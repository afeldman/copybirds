# Task 03 — GitHub Action erstellen

## Kontext

Projekt: copybirds
Arbeitsverzeichnis: Repo-Root

Wir erstellen den Release-Workflow der bei jedem Tag-Push (`v*`) ausgelöst wird.
QEMU ist nötig damit Docker Buildx arm64-Images auf dem amd64-Runner bauen kann.

## Schritt 1 — Verzeichnis anlegen

```bash
mkdir -p .github/workflows
```

## Schritt 2 — release.yml erstellen

Erstelle `.github/workflows/release.yml`:

```yaml
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

## Schritt 3 — Commit

```bash
git add .github/workflows/release.yml
git commit -m "ci: add goreleaser release workflow triggered on v* tags"
```

## Hinweis nach dem Commit

Folgende Secrets müssen im GitHub-Repo unter
`Settings → Secrets and variables → Actions` angelegt werden:

| Secret | Beschreibung |
|---|---|
| `HOMEBREW_TAP_TOKEN` | GitHub PAT mit `repo`-Scope für `afeldman/homebrew-tap` |
| `NIX_TOKEN` | GitHub PAT mit `repo`-Scope für `afeldman/nebula-nix` |

`GITHUB_TOKEN` ist automatisch vorhanden — kein Setup nötig.
