# apt-based Dockerfile Generation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `cb docker` generates a Dockerfile that installs libraries via `apt-get install` with pinned versions instead of physically copying `.so` files, using the correct Debian base image and falling back to `snapshot.debian.org` for packages not in the standard repo.

**Architecture:** `cb meta` maps each traced file to its Debian package via `dpkg -S` and writes the mapping into `meta.xml`. `cb docker` reads `meta.xml`, splits packages into standard-repo vs snapshot-repo buckets via HTTP APIs, and generates a three-block Dockerfile (standard apt, snapshot apt, COPY for non-packaged files). A `--dockerfile` flag lets users bypass generation entirely.

**Tech Stack:** Go 1.22, stdlib only (`net/http`, `encoding/json`, `encoding/xml`, `text/template`), Debian FTP Master API, snapshot.debian.org API.

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `internal/meta/sysinfo.go` | Modify | Add `FilePackageEntry` type + `FilePackages` field to `SysInfo` |
| `internal/meta/xml.go` | Modify | Serialize/deserialize `<file_packages>` block, bump version to 0.9.0 |
| `internal/meta/collect.go` | Modify | Add `CollectFilePackages(manifestFile string) ([]FilePackageEntry, error)` |
| `internal/docker/resolver.go` | Create | `ResolveBaseImage` + `SplitPackages` with HTTP calls |
| `internal/docker/dockerfile.go` | Rewrite | Three-block apt Dockerfile template |
| `internal/docker/builder.go` | Modify | `BuildOptions` expansion, staging split into `_unpackaged/`, custom Dockerfile support |
| `cmd/cb/cmd/docker.go` | Modify | Add `--dockerfile`, `--meta` flags |
| `cmd/cb/cmd/run.go` | Modify | Add `--dockerfile` flag, wire up `CollectFilePackages` after strace parse |

---

## Task 01 — SysInfo data model + XML

**Files:**
- Modify: `internal/meta/sysinfo.go`
- Modify: `internal/meta/xml.go`

- [ ] **Step 1: Add FilePackageEntry to sysinfo.go**

Replace contents of `internal/meta/sysinfo.go`:

```go
package meta

// SysInfo enthält die gesammelten System-Metadaten
type SysInfo struct {
	Distro        string // z.B. "Ubuntu 24.04"
	Kernel        string // uname -r output
	Arch          string // uname -m
	GraphicDriver string // GL_RENDERER
	GraphicCard   string // GL_VENDOR
	GLVersion     string // GL_VERSION
	GLExtensions  string // GL_EXTENSIONS (lange Liste)
	UserHome      string // $HOME
	Packages      []PackageInfo
	FilePackages  []FilePackageEntry // Datei → Paket Zuordnung
	Commands      []string           // ausgeführte Kommandos
	Connections   []string           // Netzwerkverbindungen
}

// PackageInfo enthält Informationen zu einem installierten Paket
type PackageInfo struct {
	Name      string
	Version   string
	Installed bool
}

// FilePackageEntry ordnet einen Dateipfad seinem Debian-Paket zu
type FilePackageEntry struct {
	File    string
	Package string
}
```

- [ ] **Step 2: Extend xml.go with file_packages block**

Add the following types after the existing `xmlConnections` struct in `internal/meta/xml.go`:

```go
type xmlFilePackages struct {
	List []xmlFilePackage `xml:"entry"`
}

type xmlFilePackage struct {
	File    string `xml:"file,attr"`
	Package string `xml:"package,attr"`
}
```

Add `FilePackages xmlFilePackages \`xml:"file_packages"\`` to the `xmlMeta` struct after `Connections`.

Change `const metaVersion = "0.8.7"` to `const metaVersion = "0.9.0"`.

In `WriteXML`, after the Connections block, add:
```go
for _, fp := range info.FilePackages {
    xm.FilePackages.List = append(xm.FilePackages.List, xmlFilePackage{
        File:    fp.File,
        Package: fp.Package,
    })
}
```

In `ReadXML`, after the Connections block, add:
```go
for _, fp := range xm.FilePackages.List {
    info.FilePackages = append(info.FilePackages, FilePackageEntry{
        File:    fp.File,
        Package: fp.Package,
    })
}
```

- [ ] **Step 3: Build and verify**

```bash
cd /path/to/copybirds
go build ./internal/meta/...
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add internal/meta/sysinfo.go internal/meta/xml.go
git commit -m "feat(meta): add FilePackageEntry type and file_packages XML block"
```

---

## Task 02 — CollectFilePackages via dpkg -S

**Files:**
- Modify: `internal/meta/collect.go`

- [ ] **Step 1: Add CollectFilePackages function**

Add the following function to `internal/meta/collect.go`. It needs these additional imports: `"encoding/xml"`.

```go
// collectFilePackagesStruct ist für das Parsen des Strace-Manifests
type straceManifest struct {
	XMLName xml.Name      `xml:"copybirds"`
	Files   []straceFile  `xml:"files>file"`
}

type straceFile struct {
	Filename string `xml:"filename,attr"`
}

// CollectFilePackages liest die Dateiliste aus dem Trace-Manifest und
// ordnet jede Datei via 'dpkg -S' ihrem Debian-Paket zu.
// Dateien die keinem Paket gehören, werden nicht zurückgegeben.
func CollectFilePackages(manifestFile string) ([]FilePackageEntry, error) {
	data, err := os.ReadFile(manifestFile)
	if err != nil {
		return nil, fmt.Errorf("CollectFilePackages: Manifest '%s' nicht lesbar: %w", manifestFile, err)
	}

	var manifest straceManifest
	if err := xml.Unmarshal(data, &manifest); err != nil {
		return nil, fmt.Errorf("CollectFilePackages: XML-Parse fehlgeschlagen: %w", err)
	}

	var entries []FilePackageEntry
	seen := make(map[string]bool)

	for _, f := range manifest.Files {
		path := f.Filename
		if path == "" || seen[path] {
			continue
		}
		seen[path] = true

		out, err := exec.Command("dpkg", "-S", path).Output()
		if err != nil {
			// Datei gehört keinem Paket — überspringen
			continue
		}
		// Output: "packagename: /path/to/file"
		line := strings.SplitN(strings.TrimSpace(string(out)), ":", 2)
		if len(line) < 1 {
			continue
		}
		pkgName := strings.TrimSpace(line[0])
		if pkgName != "" {
			entries = append(entries, FilePackageEntry{File: path, Package: pkgName})
		}
	}

	log.Infof("CollectFilePackages: %d Dateien → Pakete zugeordnet (aus %d Dateien)",
		len(entries), len(manifest.Files))
	return entries, nil
}
```

Add `"encoding/xml"` and `"fmt"` to the import block in collect.go (fmt is already there if needed, check first).

- [ ] **Step 2: Build**

```bash
go build ./internal/meta/...
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add internal/meta/collect.go
git commit -m "feat(meta): add CollectFilePackages via dpkg -S"
```

---

## Task 03 — Package Resolver (new file)

**Files:**
- Create: `internal/docker/resolver.go`

- [ ] **Step 1: Create resolver.go**

Create `internal/docker/resolver.go` with this content:

```go
package docker

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"copybirds/internal/meta"
)

// httpClient mit Timeout für alle API-Anfragen
var httpClient = &http.Client{Timeout: 10 * time.Second}

// ResolveBaseImage leitet aus dem Distro-String in meta.xml den passenden
// Docker-Image-Tag ab. Gibt "debian:bookworm-slim" als Default zurück.
func ResolveBaseImage(distro string) string {
	d := strings.ToLower(distro)
	switch {
	case strings.Contains(d, "bookworm"):
		return "debian:bookworm-slim"
	case strings.Contains(d, "bullseye"):
		return "debian:bullseye-slim"
	case strings.Contains(d, "buster"):
		return "debian:buster-slim"
	case strings.Contains(d, "ubuntu") && strings.Contains(d, "24.04"):
		return "ubuntu:24.04"
	case strings.Contains(d, "ubuntu") && strings.Contains(d, "22.04"):
		return "ubuntu:22.04"
	case strings.Contains(d, "ubuntu") && strings.Contains(d, "20.04"):
		return "ubuntu:20.04"
	default:
		return "debian:bookworm-slim"
	}
}

// debianRelease leitet aus dem Base-Image den Debian-Release-Namen ab
// (z.B. "debian:bookworm-slim" → "bookworm")
func debianRelease(baseImage string) string {
	image := strings.ToLower(baseImage)
	for _, rel := range []string{"bookworm", "bullseye", "buster", "focal", "jammy", "noble"} {
		if strings.Contains(image, rel) {
			return rel
		}
	}
	return "bookworm"
}

// ftpMasterEntry ist ein Eintrag aus der Debian FTP Master API
type ftpMasterEntry struct {
	Architecture string `json:"architecture"`
	Release      string `json:"release"`
	Version      string `json:"version"`
}

// checkStandardRepo prüft ob package@version im Standard-Repo des Release verfügbar ist.
// Nutzt die Debian FTP Master API: https://api.ftp-master.debian.org/binary/<package>/
func checkStandardRepo(pkg, version, release string) (bool, error) {
	url := fmt.Sprintf("https://api.ftp-master.debian.org/binary/%s/", pkg)
	resp, err := httpClient.Get(url)
	if err != nil {
		return false, fmt.Errorf("checkStandardRepo: HTTP-Fehler für %s: %w", pkg, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == 404 {
		return false, nil
	}
	if resp.StatusCode != 200 {
		return false, fmt.Errorf("checkStandardRepo: API lieferte Status %d für %s", resp.StatusCode, pkg)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return false, fmt.Errorf("checkStandardRepo: Body lesen fehlgeschlagen: %w", err)
	}

	var entries []ftpMasterEntry
	if err := json.Unmarshal(body, &entries); err != nil {
		return false, fmt.Errorf("checkStandardRepo: JSON-Parse fehlgeschlagen: %w", err)
	}

	for _, e := range entries {
		if strings.EqualFold(e.Release, release) && e.Version == version {
			return true, nil
		}
	}
	return false, nil
}

// snapshotBinfiles ist die Antwort der snapshot.debian.org API
type snapshotBinfiles struct {
	Result []snapshotBinfile `json:"result"`
}

type snapshotBinfile struct {
	ArchiveName string `json:"archive_name"`
	FirstSeen   string `json:"first_seen"` // Format: "20231015T000000Z"
}

// findSnapshotDate sucht das früheste Snapshot-Datum für package@version.
// Nutzt: https://snapshot.debian.org/mr/binary/<package>/<version>/binfiles
func findSnapshotDate(pkg, version string) (string, error) {
	url := fmt.Sprintf("https://snapshot.debian.org/mr/binary/%s/%s/binfiles", pkg, version)
	resp, err := httpClient.Get(url)
	if err != nil {
		return "", fmt.Errorf("findSnapshotDate: HTTP-Fehler für %s@%s: %w", pkg, version, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == 404 {
		return "", fmt.Errorf("findSnapshotDate: %s@%s nicht in snapshot.debian.org gefunden", pkg, version)
	}
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("findSnapshotDate: snapshot API lieferte Status %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", fmt.Errorf("findSnapshotDate: Body lesen fehlgeschlagen: %w", err)
	}

	var result snapshotBinfiles
	if err := json.Unmarshal(body, &result); err != nil {
		return "", fmt.Errorf("findSnapshotDate: JSON-Parse fehlgeschlagen: %w", err)
	}

	if len(result.Result) == 0 {
		return "", fmt.Errorf("findSnapshotDate: keine Ergebnisse für %s@%s", pkg, version)
	}

	// Frühestes Datum wählen (lexikographisch sortiert, da ISO-Format)
	earliest := result.Result[0].FirstSeen
	for _, r := range result.Result[1:] {
		if r.FirstSeen < earliest {
			earliest = r.FirstSeen
		}
	}
	return earliest, nil
}

// SplitResult enthält das Ergebnis der Paket-Aufteilung
type SplitResult struct {
	Standard     []meta.PackageInfo // Pakete im Standard-Repo verfügbar
	Snapshot     []meta.PackageInfo // Pakete nur via snapshot.debian.org
	SnapshotDate string             // frühestes Snapshot-Datum für alle Snapshot-Pakete
	Skipped      []meta.PackageInfo // Pakete die nirgendwo gefunden wurden
}

// SplitPackages teilt die Paketliste auf: Standard-Repo vs. Snapshot.
// release ist der Debian-Codename (z.B. "bookworm").
func SplitPackages(pkgs []meta.PackageInfo, release string) (*SplitResult, error) {
	result := &SplitResult{}
	var snapshotDates []string

	for _, pkg := range pkgs {
		if !pkg.Installed {
			continue
		}

		inStandard, err := checkStandardRepo(pkg.Name, pkg.Version, release)
		if err != nil {
			// Warnung, aber kein Hard-Fail
			result.Skipped = append(result.Skipped, pkg)
			continue
		}

		if inStandard {
			result.Standard = append(result.Standard, pkg)
			continue
		}

		// Nicht im Standard-Repo → Snapshot versuchen
		date, err := findSnapshotDate(pkg.Name, pkg.Version)
		if err != nil {
			result.Skipped = append(result.Skipped, pkg)
			continue
		}

		result.Snapshot = append(result.Snapshot, pkg)
		snapshotDates = append(snapshotDates, date)
	}

	// Frühestes Datum über alle Snapshot-Pakete
	if len(snapshotDates) > 0 {
		earliest := snapshotDates[0]
		for _, d := range snapshotDates[1:] {
			if d < earliest {
				earliest = d
			}
		}
		result.SnapshotDate = earliest
	}

	return result, nil
}
```

- [ ] **Step 2: Build**

```bash
go build ./internal/docker/...
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add internal/docker/resolver.go
git commit -m "feat(docker): add package resolver with Debian API + snapshot.debian.org"
```

---

## Task 04 — Dockerfile generator rewrite

**Files:**
- Rewrite: `internal/docker/dockerfile.go`

- [ ] **Step 1: Rewrite dockerfile.go**

Replace the entire content of `internal/docker/dockerfile.go`:

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
	BaseImage    string
	Standard     []meta.PackageInfo
	Snapshot     []meta.PackageInfo
	SnapshotDate string             // z.B. "20231015T000000Z"
	Release      string             // z.B. "bookworm"
	HasUnpackaged bool
	Entrypoint   string
	LibPath      string
}

const dockerfileTemplate = `FROM {{.BaseImage}}
{{- if .Standard}}

# Standard packages (exact version in official repo)
RUN apt-get update && apt-get install -y --no-install-recommends \
{{- range $i, $p := .Standard}}
    {{$p.Name}}={{$p.Version}}{{if not (isLast $i (len $.Standard))}} \{{end}}
{{- end}}
  && rm -rf /var/lib/apt/lists/*
{{- end}}
{{- if .Snapshot}}

# Snapshot packages (version only available via snapshot.debian.org)
RUN echo "deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/{{.SnapshotDate}} {{.Release}} main" \
    > /etc/apt/sources.list.d/snapshot.list \
  && apt-get update && apt-get install -y --no-install-recommends \
{{- range $i, $p := .Snapshot}}
    {{$p.Name}}={{$p.Version}}{{if not (isLast $i (len $.Snapshot))}} \{{end}}
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
		"len":    func(s []meta.PackageInfo) int { return len(s) },
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
		"x86_64":  "/usr/lib/x86_64-linux-gnu",
		"aarch64": "/usr/lib/aarch64-linux-gnu",
		"arm64":   "/usr/lib/aarch64-linux-gnu",
	}
	if p, ok := archPaths[arch]; ok {
		return strings.Join([]string{p, "/usr/lib", "/lib"}, ":")
	}
	return "/usr/lib:/lib"
}
```

- [ ] **Step 2: Build**

```bash
go build ./internal/docker/...
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add internal/docker/dockerfile.go
git commit -m "feat(docker): rewrite dockerfile generator with apt-install + snapshot support"
```

---

## Task 05 — Builder update: staging split + custom Dockerfile

**Files:**
- Modify: `internal/docker/builder.go`

- [ ] **Step 1: Extend BuildOptions**

Replace the `BuildOptions` struct and `DefaultOptions` in `internal/docker/builder.go`:

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

- [ ] **Step 2: Add splitStaging helper**

Add this function to `internal/docker/builder.go` (before `Build`):

```go
// splitStaging teilt das Staging-Verzeichnis auf:
// - Packaged files (in filePackages) → werden nicht kopiert (apt installiert sie)
// - Alle anderen → nach unpackagedDir kopieren
func splitStaging(stagingDir, unpackagedDir string, filePackages []meta.FilePackageEntry) error {
	// Set für schnelle Lookup: packaged files
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
			return nil // Paket überspringen
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

Add `"copybirds/internal/meta"` to the import block.

- [ ] **Step 3: Rewrite Build function**

Replace the `Build` function body. Keep steps 1–4 (docker check, staging validation, entrypoint check, alpine warning) unchanged. Replace steps 5–10 with:

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

	// 7. Staging aufteilen: packaged vs. _unpackaged
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
		// _unpackaged COPY-Zeile anhängen
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
			split, err := SplitPackages(sysInfo.Packages, params.Release)
			if err != nil {
				log.Warnf("Build: Warnung: Paket-Auflösung fehlgeschlagen, fahre ohne apt fort: %v", err)
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

- [ ] **Step 4: Build**

```bash
go build ./internal/docker/...
```

Expected: no errors.

- [ ] **Step 5: Commit**

```bash
git add internal/docker/builder.go
git commit -m "feat(docker): staging split, custom Dockerfile support, apt-based build"
```

---

## Task 06 — CLI flags

**Files:**
- Modify: `cmd/cb/cmd/docker.go`
- Modify: `cmd/cb/cmd/run.go`

- [ ] **Step 1: Update docker.go**

Replace the full content of `cmd/cb/cmd/docker.go`:

```go
package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/docker"
	"copybirds/internal/log"
)

func newDockerCmd() *cobra.Command {
	var baseImage, tag, customDockerfile, metaFile string

	cmd := &cobra.Command{
		Use:   "docker STAGING_DIR ENTRYPOINT_BINARY",
		Short: "Build Docker image from staging directory",
		Long: `Builds a Docker image from a staging directory (created by 'cb files').

When --meta is provided, cb reads the system metadata and:
  - Detects the correct Debian base image from the source distro
  - Installs packaged libraries via apt-get (standard repo + snapshot.debian.org)
  - Copies only non-packaged files from staging

When --dockerfile is provided, the custom Dockerfile is used as-is and
non-packaged files are injected via a COPY instruction at the end.`,
		Example: `  cb docker /tmp/staging usr/bin/myapp
  cb docker --meta myapp_meta.xml /tmp/staging usr/bin/myapp
  cb docker --dockerfile my.Dockerfile /tmp/staging usr/bin/myapp`,
		Args: cobra.ExactArgs(2),
		RunE: func(cmd *cobra.Command, args []string) error {
			stagingDir := args[0]
			entrypoint := args[1]

			opts := docker.DefaultOptions()
			opts.BaseImage = baseImage
			opts.Tag = tag
			opts.StagingDir = stagingDir
			opts.Entrypoint = entrypoint
			opts.AlpineWarning = true
			opts.CustomDockerfile = customDockerfile
			opts.MetaFile = metaFile

			log.Infof("cb_docker: Staging=%s Entrypoint=%s Base=%s Tag=%s",
				opts.StagingDir, opts.Entrypoint, opts.BaseImage, opts.Tag)

			if err := docker.Build(opts); err != nil {
				return fmt.Errorf("docker: %w", err)
			}

			log.Warnf("cb_docker: Docker image '%s' erfolgreich erstellt", opts.Tag)
			return nil
		},
	}

	cmd.Flags().StringVar(&baseImage, "base", "debian:bookworm-slim", "base Docker image (überschrieben durch --meta)")
	cmd.Flags().StringVar(&tag, "tag", "cb_app", "image tag")
	cmd.Flags().StringVar(&customDockerfile, "dockerfile", "", "eigenes Dockerfile verwenden (non-packaged files werden trotzdem injiziert)")
	cmd.Flags().StringVar(&metaFile, "meta", "", "Pfad zur meta.xml für apt-basiertes Package-Management")

	return cmd
}
```

- [ ] **Step 2: Update run.go — add --dockerfile flag and CollectFilePackages call**

Add `customDockerfile string` to the flag variables at the top of `newRunCmd`.

After the existing `meta.Collect("")` call and before `meta.WriteXML`, add:

```go
			// Datei → Paket Zuordnung via dpkg -S
			filePackages, fpErr := meta.CollectFilePackages(outputFile)
			if fpErr != nil {
				log.Warnf("run: warning: CollectFilePackages fehlgeschlagen: %v", fpErr)
			} else {
				sysInfo.FilePackages = filePackages
				log.Infof("run: %d Dateien → Pakete zugeordnet", len(filePackages))
			}
```

In the Docker build section, add `opts.CustomDockerfile = customDockerfile` and `opts.MetaFile = metaFile` (derive `metaFile` as `programName + "_meta.xml"`).

Add the flag registration:
```go
cmd.Flags().StringVar(&customDockerfile, "dockerfile", "", "eigenes Dockerfile für den Docker-Build")
```

- [ ] **Step 3: Build everything**

```bash
go build ./...
```

Expected: no errors.

- [ ] **Step 4: Final commit**

```bash
git add cmd/cb/cmd/docker.go cmd/cb/cmd/run.go
git commit -m "feat(cmd): add --dockerfile and --meta flags to docker and run commands"
```
