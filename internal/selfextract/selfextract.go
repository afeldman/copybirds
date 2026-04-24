// Package selfextract erstellt selbstextrahierende Shell-Archive.
//
// Ein selbstextrahierendes Archiv ist ein Shell-Script, das einen tar.gz-
// Base64-kodierten Anhang enthält. Beim Ausführen entpackt es sich selbst
// nach /tmp/cb_install_XXX und führt das enthaltene Binary aus.
package selfextract

import (
	"bytes"
	"compress/gzip"
	"encoding/base64"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

// ShellScriptHeader ist das Template für das extrahierende Shell-Script.
// Der Archiv-Datenstrom beginnt nach der __ARCHIVE__-Markierung.
const shellScriptHeader = `#!/bin/bash
# cb_selfextract - generiert von cb_automatic (copybirds)
# Dieses Archiv entpackt sich selbst und führt das Programm aus.
set -e
TMPDIR=$(mktemp -d /tmp/cb_install_XXXXXX)
ARCHIVE=$(awk '/^__ARCHIVE__/{print NR+1; exit}' "$0")
tail -n +$ARCHIVE "$0" | base64 -d | tar -xzp -C "$TMPDIR" 2>/dev/null || \
tail -n +$ARCHIVE "$0" | base64 --decode 2>/dev/null | tar -xzp -C "$TMPDIR" || {
	echo "Fehler: Entpacken fehlgeschlagen. Benötigt: base64, tar, gzip"
	exit 1
}
export LD_LIBRARY_PATH="$TMPDIR/lib:$TMPDIR/usr/lib:$LD_LIBRARY_PATH"
echo "Starte %s ..."
exec "$TMPDIR/%s" "$@"
__ARCHIVE__
`

// Build erstellt ein selbstextrahierendes Shell-Archiv.
//
//	outPath:     Pfad zur Output-Datei (.sh)
//	stagingDir:  Verzeichnis mit den kopierten Dateien (von cb_files)
//	entrypoint:  Relativer Pfad zum Binary im stagingDir (z.B. "usr/bin/xclock")
func Build(outPath, stagingDir, entrypoint string) error {
	// Relative Pfade im Archiv sollen von stagingDir ausgehen
	stagingDir = filepath.Clean(stagingDir)
	entrypoint = filepath.Clean(entrypoint)

	// Prüfen, ob das Staging-Verzeichnis existiert
	info, err := os.Stat(stagingDir)
	if err != nil {
		return fmt.Errorf("Staging-Verzeichnis '%s' nicht gefunden: %w", stagingDir, err)
	}
	if !info.IsDir() {
		return fmt.Errorf("'%s' ist kein Verzeichnis", stagingDir)
	}

	// Prüfen, ob der Entrypoint existiert
	epPath := filepath.Join(stagingDir, entrypoint)
	if _, err := os.Stat(epPath); err != nil {
		return fmt.Errorf("Entrypoint '%s' nicht in '%s' gefunden: %w", entrypoint, stagingDir, err)
	}

	// Header generieren
	header := fmt.Sprintf(shellScriptHeader, entrypoint, entrypoint)

	// tar.gz-Archiv von stagingDir erstellen
	// Wir müssen das Elternverzeichnis von stagingDir als Basis nehmen,
	// damit der relativen Pfad im Archiv stimmt.
	// Z.B. stagingDir = /tmp/staging/usr/bin/xclock
	// Wir wollen: tar czf - -C /tmp/staging .
	// Also das Elternverzeichnis von stagingDir bestimmen...
	// Nein, stagingDir IST bereits das Staging-Verzeichnis (z.B. /tmp/myapp_staging).
	// Wir packen den gesamten Inhalt relativ zu stagingDir.
	tarBuf := new(bytes.Buffer)
	gzipWriter := gzip.NewWriter(tarBuf)

	tarCmd := exec.Command("tar", "czf", "-", "-C", stagingDir, ".")
	tarCmd.Stdout = gzipWriter
	tarCmd.Stderr = os.Stderr

	if err := tarCmd.Run(); err != nil {
		return fmt.Errorf("tar von '%s' fehlgeschlagen: %w", stagingDir, err)
	}
	gzipWriter.Close()

	// Base64-kodieren
	encoded := base64.StdEncoding.EncodeToString(tarBuf.Bytes())

	// Ausgabe-Datei schreiben
	outFile, err := os.Create(outPath)
	if err != nil {
		return fmt.Errorf("kann '%s' nicht erstellen: %w", outPath, err)
	}
	defer outFile.Close()

	// Header schreiben
	if _, err := outFile.WriteString(header); err != nil {
		return fmt.Errorf("schreiben des Headers fehlgeschlagen: %w", err)
	}
	outFile.WriteString("\n")

	// Base64-Daten in Blöcken zu 76 Zeichen schreiben
	const lineWidth = 76
	for i := 0; i < len(encoded); i += lineWidth {
		end := i + lineWidth
		if end > len(encoded) {
			end = len(encoded)
		}
		if _, err := outFile.WriteString(encoded[i:end] + "\n"); err != nil {
			return fmt.Errorf("schreiben der Archiv-Daten fehlgeschlagen: %w", err)
		}
	}

	outFile.Close()

	// Ausführbar machen
	if err := os.Chmod(outPath, 0755); err != nil {
		return fmt.Errorf("chmod +x '%s' fehlgeschlagen: %w", outPath, err)
	}

	return nil
}

// EntryFromStagingDir ermittelt den Entrypoint (relativ zu stagingDir)
// anhand des übergebenen Programm-Pfads (z.B. /usr/bin/xclock).
// Dabei wird der übergebene absolute Pfad relativ zum stagingDir gemacht.
func EntryFromStagingDir(stagingDir, programPath string) (string, error) {
	stagingDir = filepath.Clean(stagingDir)
	programPath = filepath.Clean(programPath)

	// Entweder ist programPath ein absoluter Pfad, den wir relativ machen
	// oder es ist der reine Programmname (dann suchen wir im stagingDir)
	if !filepath.IsAbs(programPath) {
		// Reiner Name: suchen
		found := findFileByName(stagingDir, programPath)
		if found == "" {
			return "", fmt.Errorf("Programm '%s' nicht im Staging-Verzeichnis gefunden", programPath)
		}
		rel, err := filepath.Rel(stagingDir, found)
		if err != nil {
			return "", err
		}
		return rel, nil
	}

	// Absoluter Pfad: relativ zu stagingDir
	rel, err := filepath.Rel(stagingDir, programPath)
	if err != nil {
		return "", fmt.Errorf("kann Pfad '%s' nicht relativ zu '%s' machen: %w", programPath, stagingDir, err)
	}
	if strings.HasPrefix(rel, "..") {
		return "", fmt.Errorf("Programm-Pfad '%s' liegt nicht im Staging-Verzeichnis '%s'", programPath, stagingDir)
	}
	return rel, nil
}

// findFileByName sucht eine Datei mit dem Namen name im Verzeichnis tree.
// Gibt den vollen Pfad zurück oder "" wenn nicht gefunden.
func findFileByName(tree, name string) string {
	var found string
	filepath.WalkDir(tree, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return nil // Verzeichnisse mit Fehlern überspringen
		}
		if !d.IsDir() && d.Name() == name {
			found = path
			return io.EOF // WalkDir stoppen (EOF wird als Stop-Signal behandelt)
		}
		return nil
	})
	return found
}
