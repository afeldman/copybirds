# Task 03 — Package Resolver (neue Datei)

## Kontext

Projekt: copybirds (Go, stdlib only — kein net/http aus externen Libs)
Arbeitsverzeichnis: Repo-Root

Wir erstellen `internal/docker/resolver.go`. Diese Datei:
1. Leitet aus dem Distro-String den passenden Docker-Base-Image-Tag ab
2. Teilt eine Paketliste auf in: Standard-Repo (Debian FTP Master API) vs. Snapshot (snapshot.debian.org)

## Datei

- Neu erstellen: `internal/docker/resolver.go`

## Schritt 1 — resolver.go erstellen

Erstelle `internal/docker/resolver.go` mit folgendem Inhalt:

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

// debianRelease leitet aus dem Base-Image den Debian-Release-Namen ab.
// "debian:bookworm-slim" → "bookworm"
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
// API: https://api.ftp-master.debian.org/binary/<package>/
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
		return false, fmt.Errorf("checkStandardRepo: API Status %d für %s", resp.StatusCode, pkg)
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
// API: https://snapshot.debian.org/mr/binary/<package>/<version>/binfiles
func findSnapshotDate(pkg, version string) (string, error) {
	url := fmt.Sprintf("https://snapshot.debian.org/mr/binary/%s/%s/binfiles", pkg, version)
	resp, err := httpClient.Get(url)
	if err != nil {
		return "", fmt.Errorf("findSnapshotDate: HTTP-Fehler für %s@%s: %w", pkg, version, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode == 404 {
		return "", fmt.Errorf("findSnapshotDate: %s@%s nicht in snapshot.debian.org", pkg, version)
	}
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("findSnapshotDate: snapshot API Status %d", resp.StatusCode)
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

	// Frühestes Datum (ISO-Format → lexikographisch sortierbar)
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
	SnapshotDate string             // frühestes Snapshot-Datum aller Snapshot-Pakete
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
			// Warnung, kein Hard-Fail
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

## Schritt 2 — Build prüfen

```bash
go build ./internal/docker/...
```

Erwartet: keine Fehler.

## Schritt 3 — Commit

```bash
git add internal/docker/resolver.go
git commit -m "feat(docker): add package resolver with Debian FTP Master API + snapshot.debian.org"
```
