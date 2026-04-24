# Task 02 — CollectFilePackages via dpkg -S

## Kontext

Projekt: copybirds (Go, stdlib only)
Arbeitsverzeichnis: Repo-Root

Task 01 hat `FilePackageEntry` in sysinfo.go definiert. Jetzt fügen wir die
Funktion hinzu, die das Strace-Manifest liest und jede Datei via `dpkg -S`
ihrem Paket zuordnet.

## Datei

- Modifizieren: `internal/meta/collect.go`

## Schritt 1 — Imports prüfen

Stelle sicher dass folgende Imports in `internal/meta/collect.go` vorhanden sind:

```go
import (
    "bufio"
    "encoding/xml"
    "fmt"
    "os"
    "os/exec"
    "runtime"
    "strings"

    "copybirds/internal/log"
)
```

Falls `encoding/xml` und `fmt` noch nicht drin sind, ergänzen.

## Schritt 2 — Hilfstructs für XML-Parsing

Füge am Anfang von `internal/meta/collect.go` (nach dem package-Statement, vor
`Collect`) folgende Structs ein:

```go
// straceManifest ist ein minimaler Wrapper zum Lesen des Trace-Manifests
type straceManifest struct {
	XMLName xml.Name     `xml:"copybirds"`
	Files   []straceFile `xml:"files>file"`
}

type straceFile struct {
	Filename string `xml:"filename,attr"`
}
```

## Schritt 3 — CollectFilePackages Funktion

Füge am Ende von `internal/meta/collect.go` folgende Funktion hinzu:

```go
// CollectFilePackages liest die Dateiliste aus dem Trace-Manifest und
// ordnet jede Datei via 'dpkg -S' ihrem Debian-Paket zu.
// Dateien die keinem Paket gehören werden nicht zurückgegeben.
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
		// Output-Format: "packagename: /path/to/file"
		line := strings.SplitN(strings.TrimSpace(string(out)), ":", 2)
		if len(line) < 1 {
			continue
		}
		pkgName := strings.TrimSpace(line[0])
		if pkgName != "" {
			entries = append(entries, FilePackageEntry{File: path, Package: pkgName})
		}
	}

	log.Infof("CollectFilePackages: %d von %d Dateien einem Paket zugeordnet",
		len(entries), len(manifest.Files))
	return entries, nil
}
```

## Schritt 4 — Build prüfen

```bash
go build ./internal/meta/...
```

Erwartet: keine Fehler.

## Schritt 5 — Commit

```bash
git add internal/meta/collect.go
git commit -m "feat(meta): add CollectFilePackages via dpkg -S"
```
