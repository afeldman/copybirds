<p align="center">
  <img src="assets/logo.svg" alt="Copybirds" width="480"/>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache%202.0-blue.svg" alt="License"/></a>
  <a href="https://go.dev/"><img src="https://img.shields.io/badge/Go-1.22+-00ADD8?logo=go" alt="Go Version"/></a>
  <img src="https://img.shields.io/badge/platform-Linux-lightgrey?logo=linux" alt="Platform"/>
  <img src="https://img.shields.io/badge/arch-amd64%20%7C%20arm64-informational" alt="Architectures"/>
</p>

---

**Copybirds** captures every file a Linux program touches at runtime — libraries, configs, fonts, data files — and packages them into a self-contained Docker image or self-extracting archive. Run your application on any Linux machine without worrying about missing dependencies.

## How it works

```
your program
     │
     ▼
  strace                    tracks every file open/exec/access syscall
     │
     ▼
cb_strace_to_xml            parses strace output → structured XML
     │
     ▼
cb_meta                     adds system metadata (distro, packages, GL version)
     │
     ├─ cb_meta_merge       (optional) merge traces from multiple runs
     │
     ▼
cb_files                    copies all captured files to a staging directory
     │
     ├─ cb_docker           → Docker image  (debian:bookworm-slim / alpine)
     └─ cb_automatic        → self-extracting .sh archive
```

## Quick start

```bash
# Trace a program and package it interactively
cb run xclock -digital
```

`cb run` guides you through every step: tracing → metadata → copying → Docker or archive.

## Installation

### Pre-built binaries

Download the latest release for your architecture from the [Releases](../../releases) page and place the binaries in your `$PATH`.

### Build from source

```bash
git clone https://github.com/yourorg/copybirds
cd copybirds
make all          # builds bin/cb
export PATH=$PATH:$(pwd)/bin
```

Requirements: Go 1.22+, `strace` (runtime only)

### GoReleaser

```bash
goreleaser build --snapshot --clean
```

## Commands

All functionality is in a single `cb` binary with subcommands:

```
cb run      PROGRAM [ARGS...]   Interactively trace and package a program
cb trace    [FLAGS]             Convert strace output → XML manifest
cb meta     [FLAGS] XMLFILE     Collect or compare system metadata
cb merge    INPUT... OUTPUT     Merge multiple XML manifests
cb files    XMLFILE DESTDIR     Copy traced files to staging directory
cb deps     [PACKAGE...]        Analyze .deb dependency chains
cb clean    XMLFILE             Remove traced files from source system
cb docker   STAGING ENTRYPOINT Build Docker image from staging directory
```

```bash
cb --help           # overview
cb run --help       # subcommand help
cb completion bash  # shell completion
```

## Examples

```bash
# Manual trace + convert
strace -q -f -o strace.log \
  -e trace=open,access,execve,clone,fork,vfork,chdir,readlink,getcwd,connect \
  xclock
cb trace -s strace.log -o manifest.xml

# Collect system metadata
cb meta manifest.xml

# Merge traces from multiple runs
cb merge run1.xml run2.xml run3.xml merged.xml

# Copy files to staging directory
cb files manifest.xml /tmp/staging/

# Build Docker image (default: debian:bookworm-slim)
cb docker /tmp/staging usr/bin/xclock
cb docker --base alpine --tag xclock:latest /tmp/staging usr/bin/xclock

# Compare target system against manifest
cb meta --compare manifest.xml

# Analyze .deb dependency chain
cb deps libgtk-3-0 libpango-1.0-0
```

## Verbosity

Global flags work on every subcommand:

| Flag | Level |
|---|---|
| `-q` | Quiet — errors only |
| _(default)_ | Normal |
| `-v` | Verbose |
| `-vv` | Very verbose |
| `-vvv` | Debug |

```bash
cb -vv trace -s strace.log -o manifest.xml
```

## Design decisions

- **Static binaries** — `CGO_ENABLED=0`, no shared library dependencies on the built tools themselves
- **No external Go dependencies** — stdlib only
- **Linux only** — `strace` is Linux-specific; all tools target `linux/amd64` and `linux/arm64`
- **Debian-first** — package analysis uses `dpkg`/`apt`; other distros get file-level capture without package metadata

## License

Apache 2.0 — see [LICENSE](LICENSE)
