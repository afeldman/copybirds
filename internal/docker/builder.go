package docker

import (
	"bufio"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"

	"copybirds/internal/fileops"
	"copybirds/internal/log"
	"copybirds/internal/meta"
)

// BuildOptions steuert den Docker-Build
type BuildOptions struct {
	BaseImage        string // default: aus meta.xml abgeleitet
	Tag              string // default: "cb_app"
	StagingDir       string // Pfad zum Staging-Verzeichnis
	Entrypoint       string // Relativer Pfad im Staging-Dir zum Binary
	AlpineWarning    bool   // true wenn Alpine/Busybox gewählt
	CustomDockerfile string // Pfad zu user-eigenem Dockerfile; leer = auto-generieren
	MetaFile         string // Pfad zur meta.xml; leer = kein apt-Management
}

// DefaultOptions gibt Standard-Optionen zurück
func DefaultOptions() BuildOptions {
	return BuildOptions{
		BaseImage: "debian:bookworm-slim",
		Tag:       "cb_app",
	}
}

// IsAlpineLike prüft ob das Image musl-basiert ist.
// Gibt true zurück für: alpine, busybox, wolfi, chainguard und alle Derivate.
func IsAlpineLike(image string) bool {
	// Basisname ohne Tag (z.B. "alpine:3.18" -> "alpine")
	base := strings.SplitN(image, ":", 2)[0]
	// Auch Varianten wie "alpine:3.18" oder "myrepo/alpine:latest"
	// Wenn es einen Slash gibt, nimm den letzten Teil
	if idx := strings.LastIndex(base, "/"); idx >= 0 {
		base = base[idx+1:]
	}

	switch strings.ToLower(base) {
	case "alpine", "busybox", "wolfi", "chainguard":
		return true
	}
	return false
}

// promptConfirmation fragt den Benutzer nach einer j/N Bestätigung
func promptConfirmation(msg string) bool {
	fmt.Print(msg)
	reader := bufio.NewReader(os.Stdin)
	line, _ := reader.ReadString('\n')
	line = strings.TrimSpace(line)
	return strings.EqualFold(line, "j") || strings.EqualFold(line, "ja") ||
		strings.EqualFold(line, "y") || strings.EqualFold(line, "yes")
}

// splitStaging kopiert nur nicht-packaged Dateien aus stagingDir nach unpackagedDir.
// Packaged files (in filePackages) werden übersprungen — apt installiert sie.
func splitStaging(stagingDir, unpackagedDir string, filePackages []meta.FilePackageEntry) error {
	// Set für O(1)-Lookup: Absolut-Pfad auf Source-System → true
	packaged := make(map[string]bool, len(filePackages))
	for _, fp := range filePackages {
		packaged[fp.File] = true
	}

	return filepath.Walk(stagingDir, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if info.IsDir() {
			return nil
		}

		// Relativer Pfad im Staging-Dir
		rel, err := filepath.Rel(stagingDir, path)
		if err != nil {
			return err
		}
		// Absoluter Pfad wie er auf dem Source-System war
		absPath := "/" + rel

		if packaged[absPath] {
			return nil // Paket wird von apt installiert
		}

		// Datei nach unpackagedDir kopieren
		dest := filepath.Join(unpackagedDir, rel)
		if err := os.MkdirAll(filepath.Dir(dest), 0755); err != nil {
			return err
		}
		return fileops.CopyFile(path, dest)
	})
}

// Build erstellt das Docker-Image aus dem Staging-Verzeichnis.
//
// Ablauf:
//  1. Prüfen ob docker im PATH
//  2. Staging-Dir prüfen (existiert, nicht leer)
//  3. Entrypoint prüfen (existiert im Staging-Dir)
//  4. Alpine-Warnung (wenn Basis-Image musl-basiert)
//  5. Temp-Verzeichnis erstellen
//  6. Meta-Datei lesen (falls vorhanden)
//  7. Staging aufteilen: packaged → überspringen, Rest → _unpackaged/
//  8. Dockerfile schreiben (custom oder auto-generiert)
//  9. docker build ausführen
// 10. Image-ID ausgeben
func Build(opts BuildOptions) error {
	// 1. Docker im PATH prüfen
	if _, err := exec.LookPath("docker"); err != nil {
		return fmt.Errorf("Build: Docker ist nicht im PATH.\n"+
			"  Bitte installiere Docker (z.B. 'brew install docker' oder 'apt install docker.io')\n"+
			"  und stelle sicher, dass die Docker-Engine läuft.")
	}
	log.Infof("Build: docker gefunden")

	// 2. Staging-Dir prüfen
	stagingInfo, err := os.Stat(opts.StagingDir)
	if err != nil {
		return fmt.Errorf("Build: Staging-Verzeichnis '%s' existiert nicht: %w", opts.StagingDir, err)
	}
	if !stagingInfo.IsDir() {
		return fmt.Errorf("Build: '%s' ist kein Verzeichnis", opts.StagingDir)
	}
	// Prüfen ob nicht leer
	entries, err := os.ReadDir(opts.StagingDir)
	if err != nil {
		return fmt.Errorf("Build: Staging-Verzeichnis '%s' kann nicht gelesen werden: %w", opts.StagingDir, err)
	}
	if len(entries) == 0 {
		return fmt.Errorf("Build: Staging-Verzeichnis '%s' ist leer", opts.StagingDir)
	}
	log.Infof("Build: Staging-Verzeichnis '%s' OK (%d Einträge)", opts.StagingDir, len(entries))

	// 3. Entrypoint prüfen
	entrypointPath := filepath.Join(opts.StagingDir, opts.Entrypoint)
	epInfo, err := os.Stat(entrypointPath)
	if err != nil {
		return fmt.Errorf("Build: Entrypoint '%s' nicht gefunden: %w", entrypointPath, err)
	}
	if epInfo.IsDir() {
		return fmt.Errorf("Build: Entrypoint '%s' ist ein Verzeichnis, kein Binary", entrypointPath)
	}
	// Prüfen ob ausführbar
	if epInfo.Mode()&0111 == 0 {
		log.Warnf("WARNUNG: Entrypoint '%s' ist nicht als ausführbar markiert", entrypointPath)
	}
	log.Infof("Build: Entrypoint '%s' OK", entrypointPath)

	// 4. Alpine-Warnung
	if opts.AlpineWarning && IsAlpineLike(opts.BaseImage) {
		log.Warnf("WARNUNG: %s nutzt musl-libc.", opts.BaseImage)
		log.Warnf("Glibc-Binaries aus dem Staging-Dir laufen moeglicherweise nicht korrekt.")
		if !promptConfirmation("Fortfahren? [j/N] ") {
			return fmt.Errorf("Build: Abgebrochen durch Benutzer (Alpine-Warnung)")
		}
	}

	// 5. Temp-Verzeichnis erstellen
	buildCtxDir, err := os.MkdirTemp("", "cb_docker_*")
	if err != nil {
		return fmt.Errorf("Build: Temp-Verzeichnis kann nicht erstellt werden: %w", err)
	}
	log.Infof("Build: Build-Context: %s", buildCtxDir)
	defer func() {
		if err := os.RemoveAll(buildCtxDir); err != nil {
			log.Warnf("Build: Warnung: Temp-Verzeichnis '%s' kann nicht gelöscht werden: %v", buildCtxDir, err)
		}
	}()

	// 6. Meta-Datei lesen (falls vorhanden)
	var sysInfo *meta.SysInfo
	if opts.MetaFile != "" {
		sysInfo, err = meta.ReadXML(opts.MetaFile)
		if err != nil {
			log.Warnf("Build: Warnung: meta.xml '%s' nicht lesbar, fahre ohne apt-Management fort: %v", opts.MetaFile, err)
		}
	}

	// 7. Staging aufteilen: packaged → überspringen, Rest → _unpackaged/
	unpackagedDir := filepath.Join(buildCtxDir, "_unpackaged")
	if err := os.MkdirAll(unpackagedDir, 0755); err != nil {
		return fmt.Errorf("Build: _unpackaged-Verzeichnis kann nicht erstellt werden: %w", err)
	}

	var filePackages []meta.FilePackageEntry
	if sysInfo != nil {
		filePackages = sysInfo.FilePackages
	}
	if err := splitStaging(opts.StagingDir, unpackagedDir, filePackages); err != nil {
		return fmt.Errorf("Build: Staging-Aufteilung fehlgeschlagen: %w", err)
	}
	log.Infof("Build: Staging aufgeteilt → _unpackaged/")

	// 8. Dockerfile schreiben
	if opts.CustomDockerfile != "" {
		// User-Dockerfile verwenden
		customContent, err := os.ReadFile(opts.CustomDockerfile)
		if err != nil {
			return fmt.Errorf("Build: Custom-Dockerfile '%s' nicht lesbar: %w", opts.CustomDockerfile, err)
		}
		destDockerfile := filepath.Join(buildCtxDir, "Dockerfile")
		if err := os.WriteFile(destDockerfile, customContent, 0644); err != nil {
			return fmt.Errorf("Build: Dockerfile kopieren fehlgeschlagen: %w", err)
		}
		if err := AppendUnpackagedCopy(destDockerfile); err != nil {
			return fmt.Errorf("Build: COPY-Injection fehlgeschlagen: %w", err)
		}
		log.Infof("Build: Custom-Dockerfile verwendet, _unpackaged/ injiziert")
	} else {
		// Dockerfile auto-generieren
		params := DockerfileParams{
			BaseImage:     opts.BaseImage,
			Entrypoint:    opts.Entrypoint,
			HasUnpackaged: true,
			LibPath:       defaultLibPath("x86_64"),
		}

		if sysInfo != nil {
			params.BaseImage = ResolveBaseImage(sysInfo.Distro)
			params.Release = debianRelease(params.BaseImage)

			log.Infof("Build: Löse Pakete auf (Standard-Repo + Snapshot)...")
			split, splitErr := SplitPackages(sysInfo.Packages, params.Release)
			if splitErr != nil {
				log.Warnf("Build: Warnung: Paket-Auflösung fehlgeschlagen, fahre ohne apt fort: %v", splitErr)
			} else {
				params.Standard = split.Standard
				params.Snapshot = split.Snapshot
				params.SnapshotDate = split.SnapshotDate
				log.Infof("Build: %d Standard-Pakete, %d Snapshot-Pakete, %d übersprungen",
					len(split.Standard), len(split.Snapshot), len(split.Skipped))
			}
		}

		if err := WriteDockerfile(buildCtxDir, params); err != nil {
			return fmt.Errorf("Build: Dockerfile kann nicht geschrieben werden: %w", err)
		}
		log.Infof("Build: Dockerfile generiert")
	}

	// 9. docker build ausführen
	log.Warnf("Build: Führe 'docker build -t %s %s' aus...", opts.Tag, buildCtxDir)
	cmd := exec.Command("docker", "build", "-t", opts.Tag, buildCtxDir)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("Build: Docker-Build fehlgeschlagen: %w", err)
	}

	// 10. Image-ID ausgeben
	id, err := getImageID(opts.Tag)
	if err != nil {
		log.Warnf("Build: Warnung: Image-ID konnte nicht ermittelt werden: %v", err)
	} else {
		fmt.Printf("Image-ID: %s\n", id)
	}

	return nil
}
// getImageID ermittelt die Image-ID eines Docker-Images über den Tag.
func getImageID(tag string) (string, error) {
	cmd := exec.Command("docker", "inspect", "--format={{.Id}}", tag)
	output, err := cmd.Output()
	if err != nil {
		return "", fmt.Errorf("Image-ID kann nicht ermittelt werden: %w", err)
	}
	id := strings.TrimSpace(string(output))
	if id == "" {
		return "", fmt.Errorf("Image-ID ist leer")
	}
	return id, nil
}
