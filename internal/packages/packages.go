package packages

import (
	"fmt"
	"os/exec"
	"strings"

	"copybirds/internal/log"
)

// Package repräsentiert ein Debian-Paket
type Package struct {
	Name         string
	Version      string
	Installed    bool
	Dependencies []string // direkte Abhängigkeiten
}

// QueryInstalled gibt alle installierten Pakete zurück
// via: dpkg-query -W -f='${Package}\t${Version}\t${Status}\n'
func QueryInstalled() (map[string]*Package, error) {
	log.Infof("QueryInstalled: Sammle installierte Pakete via dpkg-query...")

	// Prüfe ob dpkg-query verfügbar ist
	if _, err := exec.LookPath("dpkg-query"); err != nil {
		return nil, fmt.Errorf("dpkg-query nicht gefunden: %w. Bitte auf einem Debian-basierten System ausführen", err)
	}

	out, err := exec.Command("dpkg-query", "-W", "-f=${Package}\t${Version}\t${Status}\n").Output()
	if err != nil {
		return nil, fmt.Errorf("dpkg-query fehlgeschlagen: %w", err)
	}

	packages := make(map[string]*Package)
	lines := strings.Split(string(out), "\n")
	count := 0
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}
		parts := strings.Split(line, "\t")
		if len(parts) < 3 {
			continue
		}
		name := parts[0]
		version := parts[1]
		status := parts[2]
		installed := strings.Contains(status, "installed")

		p := &Package{
			Name:      name,
			Version:   version,
			Installed: installed,
		}
		packages[name] = p
		count++
	}

	log.Infof("QueryInstalled: %d Pakete gefunden (%d installiert)",
		count, countInstalled(packages))
	return packages, nil
}

// countInstalled zählt die installierten Pakete
func countInstalled(pkgs map[string]*Package) int {
	count := 0
	for _, p := range pkgs {
		if p.Installed {
			count++
		}
	}
	return count
}

// QueryDeps gibt die Abhängigkeiten eines Pakets zurück
// via: apt-cache depends --installed PAKETNAME
func QueryDeps(name string) ([]string, error) {
	log.Debugf("QueryDeps: Ermittle Abhängigkeiten für '%s'...", name)

	// Prüfe ob apt-cache verfügbar ist
	if _, err := exec.LookPath("apt-cache"); err != nil {
		return nil, fmt.Errorf("apt-cache nicht gefunden: %w. Bitte auf einem Debian-basierten System ausführen", err)
	}

	// LC_ALL=C für konsistentes englisches Output
	cmd := exec.Command("apt-cache", "depends", "--installed", name)
	cmd.Env = append(cmd.Environ(), "LC_ALL=C")
	out, err := cmd.Output()
	if err != nil {
		// Bei Unbekanntem Paket gibt apt-cache einen Fehler aus
		return nil, fmt.Errorf("apt-cache depends für '%s' fehlgeschlagen: %w", name, err)
	}

	var deps []string
	seen := make(map[string]bool)
	lines := strings.Split(string(out), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// apt-cache depends --installed Output (LC_ALL=C):
		//   bash
		//     Depends: libc6
		//     Depends: libtinfo6
		//   <Eigenständiges-Paket>
		//     <keine Abhängigkeiten>
		//
		// Wir parsen Zeilen, die mit "Depends:" beginnen
		// und ignorieren Abschnitte für andere Beziehungstypen (Conflicts, Suggests, etc.)

		// Zeilenformat: "    Depends: <paketname>"
		if strings.Contains(line, "Depends:") {
			// Alles nach "Depends:" extrahieren
			idx := strings.Index(line, "Depends:")
			dep := strings.TrimSpace(line[idx+len("Depends:"):])

			// Entferne Alternativ-Marker: " | something"
			if pipeIdx := strings.Index(dep, "|"); pipeIdx >= 0 {
				dep = strings.TrimSpace(dep[:pipeIdx])
			}
			// Entferne Versionseinschränkungen: "libc6 (>= 2.34)" → "libc6"
			if parenIdx := strings.Index(dep, "("); parenIdx >= 0 {
				dep = strings.TrimSpace(dep[:parenIdx])
			}
			// Entferne Architektur-Suffix: "libc6:amd64" → "libc6"
			if colonIdx := strings.Index(dep, ":"); colonIdx >= 0 {
				// Nur wenn es wie ein Arch-Suffix aussieht (kein Doppelpunkt im Paketnamen)
				arch := dep[colonIdx+1:]
				if len(arch) <= 8 && !strings.Contains(arch, " ") {
					dep = dep[:colonIdx]
				}
			}

			dep = strings.TrimSpace(dep)
			if dep != "" && !strings.ContainsAny(dep, "<>") && !seen[dep] {
				seen[dep] = true
				deps = append(deps, dep)
			}
		}
	}

	log.Debugf("QueryDeps: '%s' hat %d Abhängigkeiten", name, len(deps))
	return deps, nil
}

// ResolveDeps löst alle transitiven Abhängigkeiten auf
// Gibt Map von Paketname → Package zurück
// Zirkuläre Abhängigkeiten werden durch ein visited-Set korrekt behandelt
func ResolveDeps(roots []string, installed map[string]*Package) (map[string]*Package, error) {
	log.Infof("ResolveDeps: Löse transitive Abhängigkeiten für %d Wurzel-Pakete auf...", len(roots))

	required := make(map[string]*Package)
	visited := make(map[string]bool)

	for _, root := range roots {
		if err := resolveRecursive(root, installed, required, visited); err != nil {
			return nil, err
		}
	}

	log.Infof("ResolveDeps: %d transitive Abhängigkeiten gefunden (insgesamt %d Pakete)",
		len(required)-len(roots), len(required))
	return required, nil
}

// resolveRecursive traversiert Abhängigkeiten rekursiv mit visited-Set
func resolveRecursive(name string, installed map[string]*Package, required map[string]*Package, visited map[string]bool) error {
	// Zyklenerkennung: bereits in dieser Traversierungs-Richtung gesehen
	if visited[name] {
		log.Debugf("resolveRecursive: Zirkuläre Abhängigkeit erkannt für '%s', überspringe", name)
		return nil
	}

	// Bereits aufgelöst
	if _, exists := required[name]; exists {
		return nil
	}

	visited[name] = true
	defer func() { visited[name] = false }()

	// Existiert das Paket in der installierten Liste?
	pkg, exists := installed[name]
	if !exists {
		// Paket nicht installiert – es kann trotzdem Abhängigkeiten haben
		// Für nicht-installierte Pakete müssen wir trotzdem apt-cache fragen
		log.Debugf("resolveRecursive: '%s' ist nicht installiert, ermittle trotzdem Abhängigkeiten", name)
	}

	// Abhängigkeiten via apt-cache abfragen
	deps, err := QueryDeps(name)
	if err != nil {
		// Wenn apt-cache fehlschlägt, fügen wir das Paket trotzdem ohne Abhängigkeiten hinzu
		log.Warnf("resolveRecursive: Warnung: Konnte Abhängigkeiten für '%s' nicht ermitteln: %v", name, err)
		deps = nil
	}

	// Package-Eintrag erstellen (oder aus installed übernehmen)
	if pkg != nil {
		required[name] = &Package{
			Name:         pkg.Name,
			Version:      pkg.Version,
			Installed:    pkg.Installed,
			Dependencies: deps,
		}
	} else {
		required[name] = &Package{
			Name:         name,
			Installed:    false,
			Dependencies: deps,
		}
	}

	// Rekursiv für jede Abhängigkeit
	for _, dep := range deps {
		if err := resolveRecursive(dep, installed, required, visited); err != nil {
			return err
		}
	}

	return nil
}

// FindMissing gibt Pakete zurück die in required aber nicht in installed sind
func FindMissing(required, installed map[string]*Package) []string {
	log.Infof("FindMissing: Suche fehlende Pakete...")

	var missing []string
	for name := range required {
		pkg, exists := installed[name]
		if !exists || !pkg.Installed {
			missing = append(missing, name)
 		}
	}

	log.Infof("FindMissing: %d fehlende Pakete gefunden", len(missing))
	return missing
}
