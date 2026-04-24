# Design: goreleaser Multi-Distribution Release

**Datum:** 2026-04-24  
**Status:** Approved

## Problem

Das aktuelle goreleaser-Setup produziert nur `tar.gz`-Archive für Linux amd64/arm64. Ziel ist eine vollständige Distribution über alle gängigen Paketmanager und Container-Registries in einem einzigen Release-Workflow.

## Lösung

Alles läuft über goreleaser v2 — single source of truth für Versionen, Checksums und Cross-Referenzen. Die GitHub Action ist ein dünner Wrapper.

---

## Abschnitt 1: Dockerfile

Neues `Dockerfile` im Repo-Root. `cb` ist statisch gelinkt (`CGO_ENABLED=0`), daher reicht ein minimales Base Image:

```dockerfile
FROM debian:bookworm-slim
COPY cb /usr/local/bin/cb
ENTRYPOINT ["cb"]
```

goreleaser kopiert das bereits gebaute Binary vor `docker build` in den Build-Context — kein zweiter Go-Compile-Schritt.

---

## Abschnitt 2: goreleaser.yaml

Vollständige Konfiguration mit allen neuen Blöcken:

```yaml
version: 2

before:
  hooks:
    - go mod tidy

builds:
  - id: cb
    binary: cb
    main: ./cmd/cb
    env:
      - CGO_ENABLED=0
    goos: [linux]
    goarch: [amd64, arm64]

archives:
  - formats: [tar.gz]
    name_template: "{{ .ProjectName }}_{{ .Version }}_{{ .Os }}_{{ if eq .Arch \"amd64\" }}x86_64{{ else }}{{ .Arch }}{{ end }}"
    files:
      - LICENSE
      - README.md

nfpms:
  - package_name: copybirds
    maintainer: Anton Feldmann <anton.feldmann@gmail.com>
    description: Capture runtime file dependencies of Linux programs
    homepage: https://github.com/afeldman/copybirds
    license: Apache-2.0
    formats: [deb]
    bindir: /usr/bin

dockers:
  - image_templates:
      - "ghcr.io/afeldman/copybirds:{{ .Tag }}-amd64"
      - "ghcr.io/afeldman/copybirds:latest-amd64"
    goarch: amd64
    use: buildx
    build_flag_templates:
      - "--platform=linux/amd64"
  - image_templates:
      - "ghcr.io/afeldman/copybirds:{{ .Tag }}-arm64"
      - "ghcr.io/afeldman/copybirds:latest-arm64"
    goarch: arm64
    use: buildx
    build_flag_templates:
      - "--platform=linux/arm64"

docker_manifests:
  - name_template: "ghcr.io/afeldman/copybirds:{{ .Tag }}"
    image_templates:
      - "ghcr.io/afeldman/copybirds:{{ .Tag }}-amd64"
      - "ghcr.io/afeldman/copybirds:{{ .Tag }}-arm64"
  - name_template: "ghcr.io/afeldman/copybirds:latest"
    image_templates:
      - "ghcr.io/afeldman/copybirds:latest-amd64"
      - "ghcr.io/afeldman/copybirds:latest-arm64"

brews:
  - repository:
      owner: afeldman
      name: homebrew-tap
      token: "{{ .Env.HOMEBREW_TAP_TOKEN }}"
    homepage: https://github.com/afeldman/copybirds
    description: Capture runtime file dependencies of Linux programs
    license: Apache-2.0
    install: |
      bin.install "cb"

nix:
  - name: copybirds
    repository:
      owner: afeldman
      name: nebula-nix
      token: "{{ .Env.NIX_TOKEN }}"
    homepage: https://github.com/afeldman/copybirds
    description: Capture runtime file dependencies of Linux programs
    license: "asl20"

checksum:
  name_template: "checksums.txt"

changelog:
  sort: asc
  filters:
    exclude: ["^docs:", "^test:", "^chore:"]
```

---

## Abschnitt 3: GitHub Action

```yaml
# .github/workflows/release.yml
name: Release

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write
  packages: write

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: actions/setup-go@v5
        with:
          go-version: '1.22'
          cache: true

      - uses: docker/setup-qemu-action@v3

      - uses: docker/setup-buildx-action@v3

      - uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - uses: goreleaser/goreleaser-action@v6
        with:
          version: latest
          args: release --clean
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
          HOMEBREW_TAP_TOKEN: ${{ secrets.HOMEBREW_TAP_TOKEN }}
          NIX_TOKEN: ${{ secrets.NIX_TOKEN }}
```

---

## Abschnitt 4: Secrets

| Secret | Scope | Woher |
|---|---|---|
| `GITHUB_TOKEN` | Automatisch | GitHub stellt es bereit |
| `HOMEBREW_TAP_TOKEN` | `repo` auf `afeldman/homebrew-tap` | GitHub PAT erstellen |
| `NIX_TOKEN` | `repo` auf `afeldman/nebula-nix` | GitHub PAT erstellen |

---

## Betroffene Dateien

| Datei | Aktion |
|---|---|
| `Dockerfile` | Neu erstellen |
| `.goreleaser.yaml` | Ersetzen |
| `.github/workflows/release.yml` | Neu erstellen |

## Nicht im Scope

- APT-Repository Hosting
- Docker Hub (nur ghcr.io)
- macOS / Windows Builds
- Snapshot-Builds bei Push auf master
