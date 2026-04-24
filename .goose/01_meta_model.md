# Task 01 — SysInfo Datenmodell + XML erweitern

## Kontext

Projekt: copybirds (Go, stdlib only, kein CGO)
Arbeitsverzeichnis: Repo-Root

Wir erweitern das Metadaten-Modell um eine Datei→Paket-Zuordnung.

## Dateien

- Modifizieren: `internal/meta/sysinfo.go`
- Modifizieren: `internal/meta/xml.go`

## Schritt 1 — sysinfo.go

Ersetze den Inhalt von `internal/meta/sysinfo.go` vollständig:

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

## Schritt 2 — xml.go: neue Typen

Füge nach dem `xmlConnections`-Block in `internal/meta/xml.go` folgende Typen ein:

```go
type xmlFilePackages struct {
	List []xmlFilePackage `xml:"entry"`
}

type xmlFilePackage struct {
	File    string `xml:"file,attr"`
	Package string `xml:"package,attr"`
}
```

Füge in der `xmlMeta`-Struct nach dem `Connections`-Feld hinzu:
```go
FilePackages xmlFilePackages `xml:"file_packages"`
```

## Schritt 3 — xml.go: metaVersion bumpen

Ändere:
```go
const metaVersion = "0.8.7"
```
zu:
```go
const metaVersion = "0.9.0"
```

## Schritt 4 — xml.go: WriteXML erweitern

Füge in `WriteXML` nach dem Connections-Block hinzu:

```go
	// FilePackages konvertieren
	for _, fp := range info.FilePackages {
		xm.FilePackages.List = append(xm.FilePackages.List, xmlFilePackage{
			File:    fp.File,
			Package: fp.Package,
		})
	}
```

## Schritt 5 — xml.go: ReadXML erweitern

Füge in `ReadXML` nach dem Connections-Block hinzu:

```go
	// FilePackages konvertieren
	for _, fp := range xm.FilePackages.List {
		info.FilePackages = append(info.FilePackages, FilePackageEntry{
			File:    fp.File,
			Package: fp.Package,
		})
	}
```

## Schritt 6 — Build prüfen

```bash
go build ./internal/meta/...
```

Erwartet: keine Fehler.

## Schritt 7 — Commit

```bash
git add internal/meta/sysinfo.go internal/meta/xml.go
git commit -m "feat(meta): add FilePackageEntry type and file_packages XML block"
```
