# Task 01 — Dockerfile erstellen

## Kontext

Projekt: copybirds
Arbeitsverzeichnis: Repo-Root

goreleaser braucht ein Dockerfile im Repo-Root um das Container-Image zu bauen.
`cb` ist statisch gelinkt (CGO_ENABLED=0), daher kein Build-Step nötig —
goreleaser kopiert das bereits gebaute Binary in den Build-Context.

## Schritt 1 — Dockerfile erstellen

Erstelle `Dockerfile` im Repo-Root:

```dockerfile
FROM debian:bookworm-slim
COPY cb /usr/local/bin/cb
ENTRYPOINT ["cb"]
```

## Schritt 2 — Commit

```bash
git add Dockerfile
git commit -m "build: add Dockerfile for goreleaser multi-arch image build"
```
