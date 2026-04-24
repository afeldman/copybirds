package strace

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"copybirds/internal/hashutil"
	"copybirds/internal/log"
)

// pidState speichert den Arbeitspfad pro PID (wie pidmanager.c)
type pidState struct {
	workDir string
	parent  int
}

// parser kapselt den gesamten Parsing-Zustand
type parser struct {
	ignorePIDs bool
	workDir    string // initiales Arbeitsverzeichnis

	// PID-Verwaltung (wie pidmanager.c)
	pids map[int]*pidState

	// Dateizugriffe: Map[filename+pid] -> *FileAccess
	files map[string]*FileAccess

	// Netzwerkverbindungen (dedupliziert über pid+addr+port)
	conns map[string]*Connection
}

// fileKey erzeugt einen eindeutigen Schlüssel für datei+pid
func fileKey(filename string, pid int) string {
	return fmt.Sprintf("%s|%d", filename, pid)
}

// connKey erzeugt einen eindeutigen Schlüssel für verbindung+pid
func connKey(addr string, port, pid int) string {
	return fmt.Sprintf("%s:%d|%d", addr, port, pid)
}

// newParser erzeugt einen neuen Parser
func newParser(workDir string, ignorePIDs bool) *parser {
	return &parser{
		ignorePIDs: ignorePIDs,
		workDir:    workDir,
		pids:       make(map[int]*pidState),
		files:      make(map[string]*FileAccess),
		conns:      make(map[string]*Connection),
	}
}

// ParseFile liest eine strace-Output-Datei und gibt TraceResult zurück.
// ignorePIDs: wenn true, werden alle PIDs auf 1 gesetzt (schneller bei vielen Childs)
func ParseFile(path string, ignorePIDs bool) (*TraceResult, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("konnte strace-Datei nicht öffnen: %w", err)
	}
	defer f.Close()

	// Initiales WorkDir: aktuelles Verzeichnis
	cwd, err := os.Getwd()
	if err != nil {
		cwd = "."
	}

	p := newParser(cwd, ignorePIDs)

	// Phase 1: Zeilen vorverarbeiten (unfinished/resumed zusammenführen, filtern)
	// wie preprocessing_file.c + strace_preparation()
	lines, err := preprocessLines(f)
	if err != nil {
		return nil, fmt.Errorf("vorverarbeitung fehlgeschlagen: %w", err)
	}

	// Phase 2: Jede Zeile parsen (wie process_line() in copyxmler.c)
	for _, line := range lines {
		p.processLine(line)
	}

	// Phase 3: Für jede gefundene Datei SHA256 berechnen
	log.Warnf("Berechne SHA256-Hashes...")
	for _, fa := range p.files {
		if fa.SHA256 == "" {
			hash, err := hashutil.FileHash(fa.Filename)
			if err == nil {
				fa.SHA256 = hash
			}
			// Wenn die Datei nicht existiert (z.B. weil sie im Container lag),
			// bleibt SHA256 leer - das ist okay
		}
	}

	// Ergebnisse sammeln
	result := &TraceResult{
		Files:       make([]*FileAccess, 0, len(p.files)),
		Connections: make([]*Connection, 0, len(p.conns)),
		WorkDir:     p.workDir,
	}
	for _, fa := range p.files {
		result.Files = append(result.Files, fa)
	}
	for _, c := range p.conns {
		result.Connections = append(result.Connections, c)
	}

	return result, nil
}

// ---------------------------------------------------------------------------
// Vorverarbeitung (wie preprocessing_file.c)
// ---------------------------------------------------------------------------

// fifoEntry repräsentiert einen Eintrag im Vorverarbeitungs-FIFO
type fifoEntry struct {
	text string
}

// preprocessLines liest alle Zeilen, führt unfinished/resumed zusammen
// und filtert irrelevante Zeilen
func preprocessLines(f *os.File) ([]string, error) {
	scanner := bufio.NewScanner(f)
	// Größeren Buffer für lange strace-Zeilen
	scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024)

	var rawLines []string
	for scanner.Scan() {
		rawLines = append(rawLines, scanner.Text())
	}
	if err := scanner.Err(); err != nil {
		return nil, fmt.Errorf("fehler beim Lesen: %w", err)
	}

	// FIFO-Puffer für unfinished/resumed-Zusammenführung
	var fifo []fifoEntry
	var result []string

	for _, line := range rawLines {
		if strings.TrimSpace(line) == "" {
			continue
		}
		fifo = append(fifo, fifoEntry{text: line})
		processFIFO(&fifo, &result, false)
	}
	// Rest leeren (flush)
	processFIFO(&fifo, &result, true)

	return result, nil
}

// syscallPatterns enthält alle relevanten Syscall-Namen
var relevantSyscalls = map[string]bool{
	"open":     true,
	"access":   true,
	"execve":   true,
	"clone":    true,
	"fork":     true,
	"vfork":    true,
	"chdir":    true,
	"stat64":   true,
	"lstat64":  true,
	"readlink": true,
	"getcwd":   true,
	"connect":  true,
}

// processFIFO verarbeitet den FIFO-Puffer (wie process_buffer in preprocessing_file.c)
func processFIFO(fifo *[]fifoEntry, result *[]string, flush bool) {
	for len(*fifo) > 0 {
		line := (*fifo)[0].text

		// Prüfen ob das eine resumed-Zeile ist (dürfte nicht die älteste sein)
		if strings.Contains(line, " resumed>") {
			log.Warnf("processFIFO: Warnung: resumed als ältestes Element: '%s'", line)
			*fifo = (*fifo)[1:]
			continue
		}

		syscall := extractSyscall(line)

		// Unfinished Zeile?
		if strings.Contains(line, "<unfinished ...>") {
			if !flush {
				// Versuchen, passende resumed-Zeile zu finden
				pid := extractPID(line)
				merged := mergeUnfinished(fifo, pid, syscall)
				if merged != "" {
					// Ersetze die aktuelle Zeile durch die gemergte
					(*fifo)[0] = fifoEntry{text: merged}
					// Fall-through: jetzt die gemergte Zeile normal verarbeiten
				} else {
					// Keine passende resumed-Zeile gefunden -> warten
					break
				}
			} else {
				// flush: einfach übernehmen
				// Fall-through zur Syscall-Prüfung
			}
		}

		// Nur relevante Syscalls behalten
		if relevantSyscalls[syscall] {
			*result = append(*result, line)
		} else {
			log.Infof("processFIFO: Entferne unbekannte Zeile: '%s'", line)
		}

		// Aus FIFO entfernen
		*fifo = (*fifo)[1:]
	}
}

// mergeUnfinished sucht nach einer passenden resumed-Zeile und merged sie
func mergeUnfinished(fifo *[]fifoEntry, pid int, syscall string) string {
	resumedMarker := fmt.Sprintf("<... %s resumed>", syscall)

	for i := 1; i < len(*fifo); i++ {
		probeLine := (*fifo)[i].text
		probePID := extractPID(probeLine)

		if probePID == pid && strings.Contains(probeLine, resumedMarker) {
			// Zusammenführen: nimm den Teil vor "<unfinished" aus der ersten Zeile
			// und den Teil nach "resumed>" aus der zweiten Zeile
			first := (*fifo)[0].text
			second := (*fifo)[i].text

			merged := fitTogether(first, second)

			// Entferne die resumed-Zeile aus dem FIFO
			*fifo = append((*fifo)[:i], (*fifo)[i+1:]...)
			return merged
		}
	}
	return ""
}

// fitTogether fügt zwei Teile einer strace-Zeile zusammen
// "foo <unfinished ...>" + "<... syscall resumed> bar" -> "foo bar"
func fitTogether(first, second string) string {
	// Alles vor dem letzten '<' in first
	idx := strings.LastIndex(first, "<")
	firstPart := first
	if idx >= 0 {
		firstPart = strings.TrimSpace(first[:idx])
	}

	// Alles nach dem ersten '>' in second
	idx = strings.Index(second, ">")
	secondPart := second
	if idx >= 0 {
		secondPart = strings.TrimSpace(second[idx+1:])
	}

	if firstPart == first || secondPart == second {
		// Konnte nicht gemergt werden -> einfach konkatenieren
		log.Warnf("fitTogether: Konnte nicht mergen: '%s' + '%s'", first, second)
		return first + " " + second
	}
	return firstPart + " " + secondPart
}

// ---------------------------------------------------------------------------
// Zeilen-Parsing (wie process_line in copyxmler.c)
// ---------------------------------------------------------------------------

// processLine verarbeitet eine einzelne, bereits vorverarbeitete strace-Zeile
func (p *parser) processLine(line string) {
	// PID extrahieren
	pid := extractPID(line)
	if pid < 0 {
		return
	}

	// Syscall extrahieren
	syscall := extractSyscall(line)
	if syscall == "" {
		return
	}

	switch syscall {
	case "clone", "fork", "vfork":
		childPID := getRetValue(line)
		if childPID > 0 {
			log.Infof("main: %s called by %d, new pid is: %d", syscall, pid, childPID)
			p.addChild(pid, childPID)
		}

	case "connect":
		p.parseConnect(line, pid)

	case "open":
		filename := extractFilename(line)
		if filename == "" {
			break
		}
		ending := filenameEnding(line)
		mode := 0
		if strings.Contains(ending, "O_RDONLY") {
			mode |= FO_READ
		}
		if strings.Contains(ending, "O_WRONLY") {
			mode |= FO_WRITE
		}
		if strings.Contains(ending, "O_RDWR") {
			mode |= FO_READ | FO_WRITE
		}
		absFile := p.getAbsolutePath(pid, filename)
		p.addFile(absFile, mode, pid)

	case "stat64", "lstat64", "readlink", "getcwd":
		filename := extractFilename(line)
		if filename != "" {
			absFile := p.getAbsolutePath(pid, filename)
			p.addFile(absFile, FO_READ, pid)
		} else {
			log.Warnf("processLine: Warnung: '%s' ohne Dateinamen, Zeile: %s", syscall, line)
		}

	case "access":
		filename := extractFilename(line)
		if filename == "" {
			break
		}
		ending := filenameEnding(line)
		mode := 0
		if strings.Contains(ending, "F_OK") {
			mode |= FO_READ
		}
		if strings.Contains(ending, "R_OK") {
			mode |= FO_READ
		}
		if strings.Contains(ending, "W_OK") {
			mode |= FO_WRITE
		}
		if strings.Contains(ending, "X_OK") {
			mode |= FO_EXEC
		}
		absFile := p.getAbsolutePath(pid, filename)
		p.addFile(absFile, mode, pid)

	case "execve":
		filename := extractFilename(line)
		if filename == "" {
			break
		}
		// Für execve: erstes WDir setzen (wie im C-Code)
		absFile := p.absProgPath(pid, filename)
		p.addFile(absFile, FO_EXEC, pid)

	case "chdir":
		filename := extractFilename(line)
		if filename == "" {
			break
		}
		succ := getRetValue(line)
		log.Infof("main: chdir called by %d, changes %s, returned: %d", pid, filename, succ)
		if succ == 0 {
			p.updatePath(pid, filename)
		}
	}
}

// ---------------------------------------------------------------------------
// PID-Verwaltung (wie pidmanager.c)
// ---------------------------------------------------------------------------

// pidSeek sucht einen PID-Eintrag
func (p *parser) pidSeek(pid int) *pidState {
	if state, ok := p.pids[pid]; ok {
		return state
	}
	return nil
}

// addPID fügt einen PID-Eintrag mit initialem WorkDir hinzu
func (p *parser) addPID(pid int, workDir string) {
	p.pids[pid] = &pidState{
		workDir: workDir,
		parent:  -1,
	}
}

// addChild fügt ein Kind-PID zum Eltern-PID hinzu (kopiert WorkDir)
func (p *parser) addChild(parent, child int) {
	state := &pidState{parent: parent}
	if parentState := p.pidSeek(parent); parentState != nil {
		state.workDir = parentState.workDir
	} else {
		log.Warnf("addChild: Fehler: Eltern-PID %d nicht gefunden", parent)
	}
	p.pids[child] = state
}

// updatePath aktualisiert das Arbeitsverzeichnis einer PID (wie pid_update_path)
func (p *parser) updatePath(pid int, newDir string) {
	log.Infof("updatePath: %s", newDir)
	proc := p.pidSeek(pid)
	if proc != nil {
		if filepath.IsAbs(newDir) {
			proc.workDir = newDir
		} else {
			if proc.workDir != "" {
				proc.workDir = filepath.Join(proc.workDir, newDir)
				proc.workDir = filepath.Clean(proc.workDir)
			} else {
				log.Warnf("updatePath: Fehler: Prozess %d hat kein Arbeitsverzeichnis", pid)
			}
		}
	} else {
		log.Warnf("updatePath: Fehler: Kein Prozess mit PID %d gefunden", pid)
	}
}

// getAbsolutePath gibt den absoluten Pfad zu filename relativ zum PID-WorkDir
func (p *parser) getAbsolutePath(pid int, filename string) string {
	if !filepath.IsAbs(filename) {
		if p.ignorePIDs {
			pid = 1
		}
		proc := p.pidSeek(pid)
		if proc != nil && proc.workDir != "" {
			absPath := filepath.Join(proc.workDir, filename)
			absPath = filepath.Clean(absPath)
			log.Infof("getAbsolutePath: Absoluter Pfad von %s ist %s", filename, absPath)
			return absPath
		}
		log.Warnf("getAbsolutePath: Fehler: Prozess %d hat kein WorkDir", pid)
	}
	return filename
}

// absProgPath sucht den absoluten Pfad einer ausführbaren Datei
// (wie abs_prog_path in copyxmler.c - vereinfacht ohne which)
func (p *parser) absProgPath(pid int, command string) string {
	if filepath.IsAbs(command) {
		return command
	}

	// Prüfen ob wir das WorkDir für diese PID setzen müssen (erster execve)
	proc := p.pidSeek(pid)
	if proc == nil || proc.workDir == "" {
		// Fallback auf globales WorkDir
		if p.workDir != "" {
			p.addPID(pid, p.workDir)
			proc = p.pidSeek(pid)
		}
	}

	// Relativen Pfad gegen WorkDir auflösen
	if proc != nil && proc.workDir != "" {
		absPath := filepath.Join(proc.workDir, command)
		absPath = filepath.Clean(absPath)
		return absPath
	}

	return command
}

// ---------------------------------------------------------------------------
// Dateiverwaltung (wie filemanager.c)
// ---------------------------------------------------------------------------

// Flags für Dateizugriffe
const (
	FO_READ = 1 << iota
	FO_WRITE
	FO_EXEC
)

// addFile fügt einen Dateizugriff hinzu oder aktualisiert ihn
func (p *parser) addFile(filename string, mode, pid int) {
	if filename == "" {
		log.Warnf("addFile: Fehler: Dateiname ist leer")
		return
	}
	if p.ignorePIDs {
		pid = 1
	}

	key := fileKey(filename, pid)
	fa, exists := p.files[key]
	if !exists {
		fa = &FileAccess{
			Filename: filename,
			PID:      pid,
		}
		p.files[key] = fa
	}
	if mode&FO_READ != 0 {
		fa.Read = true
	}
	if mode&FO_WRITE != 0 {
		fa.Written = true
	}
	if mode&FO_EXEC != 0 {
		fa.Executed = true
	}
}

// ---------------------------------------------------------------------------
// Connect-Parsing (wie connectionmanager.c + string_helper.c)
// ---------------------------------------------------------------------------

// parseConnect parst eine connect()-Zeile
func (p *parser) parseConnect(line string, pid int) {
	if p.ignorePIDs {
		pid = 1
	}

	connType := extractBetween(line, "sa_family=", ",")
	if connType == "" {
		log.Warnf("parseConnect: Fehler: Kein Verbindungstyp in '%s'", line)
		return
	}

	switch connType {
	case "AF_INET":
		addr := extractBetween(line, " sin_addr=inet_addr(\"", "\"")
		portStr := extractBetween(line, "sin_port=htons(", ")")
		port, _ := strconv.Atoi(portStr)
		if addr != "" {
			key := connKey(addr, port, pid)
			if _, exists := p.conns[key]; !exists {
				p.conns[key] = &Connection{
					PID:     pid,
					Address: addr,
					Port:    port,
				}
			}
		}
	case "AF_INET6":
		pton := extractBetween(line, "inet_pton(", ")")
		addr := extractBetween(pton, ", \"", "\"")
		portStr := extractBetween(line, "sin6_port=htons(", ")")
		port, _ := strconv.Atoi(portStr)
		if addr != "" {
			key := connKey(addr, port, pid)
			if _, exists := p.conns[key]; !exists {
				p.conns[key] = &Connection{
					PID:     pid,
					Address: addr,
					Port:    port,
				}
			}
		}
	case "AF_FILE":
		// Unix-Sockets ignorieren
	default:
		log.Warnf("parseConnect: Warnung: '%s' Verbindungstyp nicht behandelt", connType)
	}
}

// ---------------------------------------------------------------------------
// Hilfsfunktionen für String-Extraktion (wie string_helper.c)
// ---------------------------------------------------------------------------

// Regex für strace-Zeilen: "PID  SYSCALL(...)"
var lineRegex = regexp.MustCompile(`^\s*(\d+)\s+<\.\.\.\s+(\w+)\s+resumed>`)
var lineRegex2 = regexp.MustCompile(`^\s*(\d+)\s+(\w+)\(`)

// extractPID extrahiert die PID aus einer strace-Zeile
func extractPID(line string) int {
	// Format: "12345 syscall(...)" oder "12345 <... syscall resumed>"
	if matches := lineRegex.FindStringSubmatch(line); len(matches) >= 2 {
		pid, _ := strconv.Atoi(matches[1])
		return pid
	}
	if matches := lineRegex2.FindStringSubmatch(line); len(matches) >= 2 {
		pid, _ := strconv.Atoi(matches[1])
		return pid
	}
	log.Debugf("extractPID: Warnung: keine PID in '%s'", line)
	return -1
}

// extractSyscall extrahiert den Syscall-Namen aus einer strace-Zeile
func extractSyscall(line string) string {
	// Für resumed lines
	if matches := lineRegex.FindStringSubmatch(line); len(matches) >= 3 {
		return matches[2]
	}
	// Für normale lines: "PID  syscall("
	if matches := lineRegex2.FindStringSubmatch(line); len(matches) >= 3 {
		return matches[2]
	}
	log.Warnf("extractSyscall: Fehler: Kein Syscall in '%s'", line)
	return ""
}

// extractFilename extrahiert den ersten String in Anführungszeichen
func extractFilename(line string) string {
	filename := extractBetween(line, "\"", "\"")
	if filename == "" {
		log.Warnf("extractFilename: Fehler: Konnte keinen Dateinamen extrahieren aus: %s", line)
	}
	return filename
}

// filenameEnding gibt den Teil nach dem schließenden Anführungszeichen des Dateinamens zurück
func filenameEnding(line string) string {
	// Finde das erste Paar Anführungszeichen
	start := strings.Index(line, "\"")
	if start < 0 {
		return line
	}
	end := strings.Index(line[start+1:], "\"")
	if end < 0 {
		return line
	}
	return line[start+1+end+1:]
}

// getRetValue extrahiert den Rückgabewert (Zahl nach letztem '=')
func getRetValue(line string) int {
	idx := strings.LastIndex(line, "=")
	if idx < 0 {
		return -1
	}
	rest := strings.TrimSpace(line[idx+1:])
	// Nimm die erste Zahl
	parts := strings.Fields(rest)
	if len(parts) == 0 {
		return -1
	}
	val, err := strconv.Atoi(parts[0])
	if err != nil {
		return -1
	}
	return val
}

// extractBetween extrahiert den Text zwischen erstem Vorkommen von start und erstem Vorkommen von end danach
func extractBetween(s, start, end string) string {
	i := strings.Index(s, start)
	if i < 0 {
		return ""
	}
	i += len(start)
	j := strings.Index(s[i:], end)
	if j < 0 {
		return ""
	}
	return s[i : i+j]
}
