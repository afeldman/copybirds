# Task 05 — Builder: Staging-Split + Custom Dockerfile

## Kontext

Projekt: copybirds (Go, stdlib only)
Arbeitsverzeichnis: Repo-Root

Tasks 01–04 haben die Datenmodelle und Hilfsfunktionen definiert.
Jetzt erweitern wir `internal/docker/builder.go`:
- `BuildOptions` um `CustomDockerfile` und `MetaFile` erweitern
- `splitStaging` Funktion: Staging aufteilen in packaged vs. _unpackaged
- `Build` Funktion: neuen Ablauf mit apt-Generierung und Custom-Dockerfile-Fallback

Wichtig: `fileops.CopyFile` und `fileops.CopyDir` existieren bereits in
`internal/fileops/`. Prüfe die genauen Funktionsnamen bevor du sie aufrufst
(ggf. `internal/fileops/*.go` lesen).

## Datei

- Modifizieren: `internal/docker/builder.go`

## Schritt 1 — Imports erweitern

Stelle sicher dass folgende Imports in `internal/docker/builder.go` vorhanden sind:

```go
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
```

## Schritt 2 — BuildOptions erweitern

Ersetze die `BuildOptions`-Struct und `DefaultOptions`-Funktion:

```go
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
```

## Schritt 3 — splitStaging Funktion hinzufügen

Füge folgende Funktion vor `Build` ein. Prüfe zuerst ob `fileops.CopyFile`
existiert — falls die Funktion anders heißt, passe den Aufruf entsprechend an:

```go
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
```

## Schritt 4 — Build Funktion ersetzen

Ersetze den Inhalt der `Build`-Funktion. Behalte die Schritte 1–4 (docker-Check,
Staging-Validierung, Entrypoint-Check, Alpine-Warnung) unverändert.
Ersetze alles ab Schritt 5 mit:

```go
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
```

Wichtig: Die Variable `err` wird bereits zu Beginn von `Build` deklariert
(für Staging-Info-Fehler). Verwende `:=` nur wo nötig, sonst `=`.

## Schritt 5 — Build prüfen

```bash
go build ./internal/docker/...
go build ./...
```

Erwartet: keine Fehler.

## Schritt 6 — Commit

```bash
git add internal/docker/builder.go
git commit -m "feat(docker): staging split, custom Dockerfile support, apt-based image build"
```
