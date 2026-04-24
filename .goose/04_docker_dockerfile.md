# Task 04 — Dockerfile-Generator neu schreiben

## Kontext

Projekt: copybirds (Go, stdlib only)
Arbeitsverzeichnis: Repo-Root

Task 03 hat `SplitResult`, `ResolveBaseImage` und `debianRelease` definiert.
Jetzt ersetzen wir den bestehenden Dockerfile-Generator in `internal/docker/dockerfile.go`
durch eine apt-basierte Version mit drei Blöcken.

## Datei

- Ersetzen: `internal/docker/dockerfile.go`

## Schritt 1 — dockerfile.go komplett ersetzen

Ersetze den gesamten Inhalt von `internal/docker/dockerfile.go`:

```go
package docker

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"text/template"

	"copybirds/internal/meta"
)

// DockerfileParams sind die Parameter für die Template-Generierung
type DockerfileParams struct {
	BaseImage     string
	Standard      []meta.PackageInfo
	Snapshot      []meta.PackageInfo
	SnapshotDate  string // z.B. "20231015T000000Z"
	Release       string // z.B. "bookworm"
	HasUnpackaged bool
	Entrypoint    string
	LibPath       string
}

const dockerfileTemplate = `FROM {{.BaseImage}}
{{- if .Standard}}

# Standard packages (exact version in official repo)
RUN apt-get update && apt-get install -y --no-install-recommends \
{{- range $i, $p := .Standard}}
    {{$p.Name}}={{$p.Version}}{{if not (isLast $i (pkgLen $.Standard))}} \{{end}}
{{- end}}
  && rm -rf /var/lib/apt/lists/*
{{- end}}
{{- if .Snapshot}}

# Snapshot packages (version only available via snapshot.debian.org)
RUN echo "deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/{{.SnapshotDate}} {{.Release}} main" \
    > /etc/apt/sources.list.d/snapshot.list \
  && apt-get update && apt-get install -y --no-install-recommends \
{{- range $i, $p := .Snapshot}}
    {{$p.Name}}={{$p.Version}}{{if not (isLast $i (pkgLen $.Snapshot))}} \{{end}}
{{- end}}
  && rm -rf /etc/apt/sources.list.d/snapshot.list \
  && rm -rf /var/lib/apt/lists/*
{{- end}}
{{- if .HasUnpackaged}}

# Non-packaged files (app data, configs, custom libs)
COPY _unpackaged/ /
{{- end}}

ENV LD_LIBRARY_PATH={{.LibPath}}
ENTRYPOINT ["/{{.Entrypoint}}"]
`

// WriteDockerfile schreibt das generierte Dockerfile in buildCtxDir.
func WriteDockerfile(buildCtxDir string, params DockerfileParams) error {
	dockerfilePath := filepath.Join(buildCtxDir, "Dockerfile")

	funcMap := template.FuncMap{
		"isLast": func(i, length int) bool { return i == length-1 },
		"pkgLen": func(s []meta.PackageInfo) int { return len(s) },
	}

	tmpl, err := template.New("dockerfile").Funcs(funcMap).Parse(dockerfileTemplate)
	if err != nil {
		return fmt.Errorf("WriteDockerfile: Template-Parse fehlgeschlagen: %w", err)
	}

	f, err := os.Create(dockerfilePath)
	if err != nil {
		return fmt.Errorf("WriteDockerfile: Dockerfile '%s' kann nicht erstellt werden: %w", dockerfilePath, err)
	}
	defer f.Close()

	if err := tmpl.Execute(f, params); err != nil {
		return fmt.Errorf("WriteDockerfile: Template-Ausführung fehlgeschlagen: %w", err)
	}

	return nil
}

// AppendUnpackagedCopy hängt die COPY-Zeile ans Ende eines user-eigenen Dockerfiles.
func AppendUnpackagedCopy(dockerfilePath string) error {
	f, err := os.OpenFile(dockerfilePath, os.O_APPEND|os.O_WRONLY, 0644)
	if err != nil {
		return fmt.Errorf("AppendUnpackagedCopy: Dockerfile '%s' nicht öffenbar: %w", dockerfilePath, err)
	}
	defer f.Close()

	_, err = fmt.Fprintln(f, "\n# Injected by cb: non-packaged files\nCOPY _unpackaged/ /")
	return err
}

// defaultLibPath gibt den Standard-LD_LIBRARY_PATH für eine Architektur zurück
func defaultLibPath(arch string) string {
	archPaths := map[string]string{
		"x86_64":  "/usr/lib/x86_64-linux-gnu:/usr/lib:/lib",
		"aarch64": "/usr/lib/aarch64-linux-gnu:/usr/lib:/lib",
		"arm64":   "/usr/lib/aarch64-linux-gnu:/usr/lib:/lib",
	}
	if p, ok := archPaths[strings.ToLower(arch)]; ok {
		return p
	}
	return "/usr/lib:/lib"
}
```

## Schritt 2 — Build prüfen

```bash
go build ./internal/docker/...
```

Erwartet: keine Fehler.

## Schritt 3 — Commit

```bash
git add internal/docker/dockerfile.go
git commit -m "feat(docker): rewrite dockerfile generator with apt-install + snapshot support"
```
