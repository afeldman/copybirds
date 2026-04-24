# Design: apt-basierte Dockerfile-Generierung mit Snapshot-Fallback

**Datum:** 2026-04-24  
**Status:** Approved

## Problem

Das aktuelle `cb docker` kopiert alle getrackten `.so`-Dateien physisch ins Image (COPY-Ansatz). Das erzeugt große Images und funktioniert nicht zuverlässig wenn das Zielsystem andere Paketversionen erwartet. Ziel: der generierte Container installiert Bibliotheken sauber via `apt-get install` mit exakten Versionen — wie auf dem Source-System.

## Lösung (Überblick)

1. `cb meta` mappt jede getrackte Datei auf ihr Debian-Paket (`dpkg -S`)
2. `cb docker` teilt Pakete auf: Standard-Repo vs. Snapshot-Repo
3. Generiertes Dockerfile: apt-Install-Blöcke + COPY nur für nicht-packaged Dateien
4. Fallback: User kann `--dockerfile` übergeben; nicht-packaged Dateien werden trotzdem eingefügt

---

## Abschnitt 1: Datenmodell

### Erweiterung SysInfo (`internal/meta/sysinfo.go`)

```go
type FilePackageEntry struct {
    File    string
    Package string
}

type SysInfo struct {
    // ... bestehende Felder ...
    FilePackages []FilePackageEntry
}
```

### Erweiterung meta.xml

```xml
<copybirds_meta version="0.9.0">
  <system distro="Debian GNU/Linux 12 (bookworm)" .../>
  <packages>
    <package name="libgtk-3-0" version="3.24.38-2" installed="1"/>
  </packages>
  <file_packages>
    <entry file="/usr/lib/x86_64-linux-gnu/libgtk-3.so.0" package="libgtk-3-0"/>
    <entry file="/usr/share/themes/Adwaita/gtk-3.0/gtk.css" package="gnome-themes-extra"/>
    <!-- Dateien ohne entry = nicht aus einem Paket, werden per COPY eingefügt -->
  </file_packages>
</copybirds_meta>
```

### Sammlung in `cb meta`

`internal/meta/collect.go`: für jede Datei im Trace-Manifest `dpkg -S <file>` ausführen. Output-Format: `package: /path/to/file`. Fehler (Datei nicht in dpkg-DB) → Datei landet nicht in `file_packages` → wird später als unpackaged behandelt.

---

## Abschnitt 2: Package Resolver (`internal/docker/resolver.go`)

Neue Datei mit zwei Funktionen:

### `ResolveBaseImage(distro string) string`

Parsed den Distro-String aus meta.xml:

| Distro-String enthält | Base Image |
|---|---|
| `bookworm` | `debian:bookworm-slim` |
| `bullseye` | `debian:bullseye-slim` |
| `buster` | `debian:buster-slim` |
| `Ubuntu 22.04` | `ubuntu:22.04` |
| `Ubuntu 24.04` | `ubuntu:24.04` |
| unbekannt | `debian:bookworm-slim` + Warnung |

### `SplitPackages(pkgs []PackageInfo, release string) (standard, snapshot []PackageInfo, snapshotDate string, err error)`

Für jedes Paket:

1. **Standard-Check:** `GET https://packages.debian.org/search?keywords=<name>&searchon=names&suite=<release>&section=all&format=json`
   - Antwort enthält verfügbare Versionen → Vergleich mit gewünschter Version
   - Match → `standard`-Liste

2. **Snapshot-Check** (nur wenn kein Standard-Match): `GET https://snapshot.debian.org/mr/package/<name>/<version>/binfiles`
   - Liefert Liste von Binaries mit `archive_name` und `first_seen`-Timestamp
   - Frühesten Timestamp als Snapshot-Datum verwenden
   - Gefunden → `snapshot`-Liste
   - Nicht gefunden → Warnung ausgeben, Paket überspringen

3. `snapshotDate`: das älteste Datum aus allen Snapshot-Paketen (ein einziger Snapshot-Source-Eintrag für alle).

Fehlerbehandlung: HTTP-Fehler → Warnung, Paket wird übersprungen (kein Hard-Fail).

---

## Abschnitt 3: Dockerfile-Generator (`internal/docker/builder.go`)

### Neues BuildOptions-Feld

```go
type BuildOptions struct {
    BaseImage     string
    Tag           string
    StagingDir    string
    Entrypoint    string
    AlpineWarning bool
    CustomDockerfile string // Pfad zu user-eigenem Dockerfile; leer = auto-generieren
    MetaFile      string   // Pfad zur meta.xml für Package-Infos
}
```

### Generiertes Dockerfile (Auto-Modus)

```dockerfile
FROM debian:bookworm-slim

# Standard packages (exact version in official repo)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgtk-3-0=3.24.38-2 \
    libpango-1.0-0=1.50.12+ds-1 \
  && rm -rf /var/lib/apt/lists/*

# Snapshot packages (version only available via snapshot.debian.org)
RUN echo "deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/20231015T000000Z bookworm main" \
    > /etc/apt/sources.list.d/snapshot.list \
  && apt-get update && apt-get install -y --no-install-recommends \
    libspecial=1.2.3-old \
  && rm -rf /var/lib/apt/lists/*

# Non-packaged files (app data, configs, custom libs)
COPY _unpackaged/ /

ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
ENTRYPOINT ["/usr/bin/myapp"]
```

Wenn keine Snapshot-Pakete vorhanden: zweiter RUN-Block entfällt komplett.

### Fallback: `--dockerfile` (User-Dockerfile)

1. User-Dockerfile wird als Basis verwendet (unverändert)
2. Staging-Verzeichnis wird trotzdem in `_unpackaged/` aufgeteilt
3. Am Ende des User-Dockerfiles wird automatisch appended:
   ```dockerfile
   # Injected by cb: non-packaged files
   COPY _unpackaged/ /
   ```
4. User ist verantwortlich für apt-Zeilen im eigenen Dockerfile

---

## Abschnitt 4: Staging-Aufteilung

`internal/docker/builder.go` teilt das Staging-Verzeichnis in zwei Teile auf:

- **Packaged files**: Dateien die einem Paket zugeordnet sind → werden nicht kopiert (apt installiert sie)
- **`_unpackaged/`**: alle anderen Dateien → werden per COPY ins Image gebracht

Die Aufteilung passiert im Build-Context-Verzeichnis (temp dir), nicht im originalen Staging-Dir.

---

## Abschnitt 5: CLI-Änderungen

### `cb docker`
```
--dockerfile string   Eigenes Dockerfile verwenden (nicht-packaged Dateien werden trotzdem injiziert)
--meta string         Pfad zur meta.xml (default: <STAGING_DIR>/../<program>_meta.xml)
```

### `cb run`
```
--dockerfile string   Wird an cb docker durchgereicht
```

---

## Betroffene Dateien

| Datei | Änderung |
|---|---|
| `internal/meta/sysinfo.go` | `FilePackageEntry` Struct + Feld in `SysInfo` |
| `internal/meta/collect.go` | `dpkg -S` für jede Datei ausführen |
| `internal/meta/xml.go` | `<file_packages>` serialisieren/deserialisieren |
| `internal/docker/resolver.go` | **neu** — Base Image Detection + Package Splitting |
| `internal/docker/builder.go` | Dockerfile-Generierung, `_unpackaged/`-Splitting, Custom-Dockerfile-Fallback |
| `cmd/cb/cmd/docker.go` | `--dockerfile`, `--meta` Flags |
| `cmd/cb/cmd/run.go` | `--dockerfile` Flag durchreichen |

## Nicht im Scope

- Gentoo / andere Distros als Quelle (nur Debian/Ubuntu)
- Ubuntu-Snapshot-Support (Ubuntu hat kein snapshot.debian.org-Äquivalent — Warnung ausgeben)
- Automatisches `ldconfig` im Container (Verantwortung des Base Image)
