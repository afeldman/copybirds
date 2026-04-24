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
)

// BuildOptions steuert den Docker-Build
type BuildOptions struct {
	BaseImage     string // default: "debian:bookworm-slim"
	Tag           string // default: "cb_app"
	StagingDir    string // Pfad zum Staging-Verzeichnis
	Entrypoint    string // Relativer Pfad im Staging-Dir zum Binary
	AlpineWarning bool   // true wenn Alpine/Busybox gewählt
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

// Build erstellt das Docker-Image aus dem Staging-Verzeichnis.
//
// Ablauf:
//  1. Prüfen ob docker im PATH
//  2. Staging-Dir prüfen (existiert, nicht leer)
//  3. Entrypoint prüfen (existiert im Staging-Dir)
//  4. Alpine-Warnung (wenn Basis-Image musl-basiert)
//  5. Temp-Verzeichnis erstellen
//  6. Staging-Dir nach buildCtxDir/staging/ kopieren
//  7. Dockerfile schreiben
//  8. docker build ausführen
//  9. Temp-Dir aufräumen
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

	// Cleanup des Temp-Dirs am Ende
	defer func() {
		if err := os.RemoveAll(buildCtxDir); err != nil {
			log.Warnf("Build: Warnung: Temp-Verzeichnis '%s' kann nicht gelöscht werden: %v", buildCtxDir, err)
		} else {
			log.Infof("Build: Temp-Verzeichnis '%s' bereinigt", buildCtxDir)
		}
	}()

	// 6. Staging-Dir nach buildCtxDir/staging/ kopieren
	stagingDest := filepath.Join(buildCtxDir, "staging")
	if err := fileops.CopyDir(opts.StagingDir, stagingDest); err != nil {
		return fmt.Errorf("Build: Staging-Verzeichnis kann nicht kopiert werden: %w", err)
	}
	log.Infof("Build: Staging-Verzeichnis nach '%s' kopiert", stagingDest)

	// 7. Dockerfile schreiben
	if err := WriteDockerfile(buildCtxDir, opts); err != nil {
		return fmt.Errorf("Build: Dockerfile kann nicht geschrieben werden: %w", err)
	}
	log.Infof("Build: Dockerfile geschrieben")

	// 8. Docker build ausführen
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
