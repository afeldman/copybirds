package meta

import (
	"bufio"
	"os"
	"os/exec"
	"runtime"
	"strings"

	"copybirds/internal/log"
)

// Collect sammelt alle Systeminfos des aktuellen Systems
func Collect(commandFile string) (*SysInfo, error) {
	info := &SysInfo{}
	var err error

	// Distro aus /etc/os-release
	info.Distro = collectDistro()

	// Kernel via uname -r
	info.Kernel = collectKernel()

	// Architektur via uname -m
	info.Arch = collectArch()

	// UserHome
	info.UserHome, err = os.UserHomeDir()
	if err != nil {
		log.Warnf("collect: Warnung: Konnte UserHome nicht ermitteln: %v", err)
	}

	// GL-Info via glxinfo
	collectGLInfo(info)

	// Pakete via dpkg-query
	info.Packages = collectPackages()

	// Commands aus Datei lesen
	info.Commands = collectCommands(commandFile)

	// Connections (aktuell leer, kann später ergänzt werden)
	info.Connections = nil

	log.Warnf("collect: Systeminfo gesammelt: Distro='%s', Kernel='%s', Arch='%s'",
		info.Distro, info.Kernel, info.Arch)

	return info, nil
}

// collectDistro liest PRETTY_NAME aus /etc/os-release
func collectDistro() string {
	data, err := os.ReadFile("/etc/os-release")
	if err != nil {
		log.Warnf("collectDistro: Warnung: /etc/os-release nicht lesbar: %v", err)
		return ""
	}
	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		if strings.HasPrefix(line, "PRETTY_NAME=") {
			val := strings.TrimPrefix(line, "PRETTY_NAME=")
			val = strings.Trim(val, "\"")
			return val
		}
	}
	return ""
}

// collectKernel ermittelt die Kernel-Version via uname -r
func collectKernel() string {
	out, err := exec.Command("uname", "-r").Output()
	if err != nil {
		log.Warnf("collectKernel: Warnung: uname -r fehlgeschlagen: %v", err)
		return ""
	}
	return strings.TrimSpace(string(out))
}

// collectArch ermittelt die Architektur via uname -m
func collectArch() string {
	out, err := exec.Command("uname", "-m").Output()
	if err != nil {
		log.Warnf("collectArch: Warnung: uname -m fehlgeschlagen, fallback auf runtime.GOARCH: %v", err)
		return runtime.GOARCH
	}
	return strings.TrimSpace(string(out))
}

// collectGLInfo sammelt OpenGL-Informationen via glxinfo
func collectGLInfo(info *SysInfo) {
	// Prüfe ob DISPLAY gesetzt ist
	display := os.Getenv("DISPLAY")
	if display == "" {
		log.Infof("collectGLInfo: Kein Display verfügbar (DISPLAY nicht gesetzt), überspringe GL-Info")
		return
	}

	// Prüfe ob glxinfo verfügbar ist
	if _, err := exec.LookPath("glxinfo"); err != nil {
		log.Warnf("collectGLInfo: Warnung: glxinfo nicht gefunden, überspringe GL-Info")
		return
	}

	out, err := exec.Command("glxinfo").Output()
	if err != nil {
		log.Warnf("collectGLInfo: Warnung: glxinfo Ausführung fehlgeschlagen: %v", err)
		return
	}

	output := string(out)
	lines := strings.Split(output, "\n")

	for _, line := range lines {
		line = strings.TrimSpace(line)
		switch {
		case strings.HasPrefix(line, "OpenGL renderer string:"):
			info.GraphicDriver = strings.TrimSpace(strings.TrimPrefix(line, "OpenGL renderer string:"))
		case strings.HasPrefix(line, "OpenGL vendor string:"):
			info.GraphicCard = strings.TrimSpace(strings.TrimPrefix(line, "OpenGL vendor string:"))
		case strings.HasPrefix(line, "OpenGL version string:"):
			info.GLVersion = strings.TrimSpace(strings.TrimPrefix(line, "OpenGL version string:"))
		case strings.HasPrefix(line, "OpenGL extensions:"):
			ext := strings.TrimSpace(strings.TrimPrefix(line, "OpenGL extensions:"))
			// Die Extensions können über mehrere Zeilen gehen, aber glxinfo gibt sie
			// meist in einer Zeile aus. Wir nehmen den ersten Treffer.
			if info.GLExtensions == "" {
				info.GLExtensions = ext
			}
		}
	}

	log.Infof("collectGLInfo: GraphicDriver='%s', GraphicCard='%s', GLVersion='%s'",
		info.GraphicDriver, info.GraphicCard, info.GLVersion)
}

// collectPackages sammelt alle installierten Pakete via dpkg-query
func collectPackages() []PackageInfo {
	out, err := exec.Command("dpkg-query", "-W", "-f=${Package}\t${Version}\t${Status}\n").Output()
	if err != nil {
		log.Warnf("collectPackages: Warnung: dpkg-query fehlgeschlagen: %v", err)
		return nil
	}

	var packages []PackageInfo
	lines := strings.Split(string(out), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		parts := strings.Split(line, "\t")
		if len(parts) < 3 {
			continue
		}
		pkg := PackageInfo{
			Name:    parts[0],
			Version: parts[1],
		}
		// Status-Feld: "install ok installed" → installed=true
		status := parts[2]
		pkg.Installed = strings.Contains(status, "installed")
		packages = append(packages, pkg)
	}
	return packages
}

// collectCommands liest Befehle aus einer Datei (ein Befehl pro Zeile)
func collectCommands(path string) []string {
	if path == "" {
		return nil
	}
	f, err := os.Open(path)
	if err != nil {
		log.Warnf("collectCommands: Warnung: Konnte '%s' nicht öffnen: %v", path, err)
		return nil
	}
	defer f.Close()

	var commands []string
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line != "" {
			commands = append(commands, line)
		}
	}
	if err := scanner.Err(); err != nil {
		log.Warnf("collectCommands: Warnung: Fehler beim Lesen von '%s': %v", path, err)
	}
	return commands
}
