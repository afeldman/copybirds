package fileops

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"

	"copybirds/internal/log"
)

// CopyDir kopiert ein Verzeichnis rekursiv von src nach dst.
// Erstellt alle notwendigen Unterverzeichnisse und kopiert Dateien.
// Symlinks werden als Symlinks kopiert.
func CopyDir(src, dst string) error {
	srcInfo, err := os.Stat(src)
	if err != nil {
		return fmt.Errorf("CopyDir: Quelle '%s' kann nicht gelesen werden: %w", src, err)
	}
	if !srcInfo.IsDir() {
		return fmt.Errorf("CopyDir: '%s' ist kein Verzeichnis", src)
	}

	// Zielverzeichnis erstellen
	if err := os.MkdirAll(dst, srcInfo.Mode().Perm()); err != nil {
		return fmt.Errorf("CopyDir: Zielverzeichnis '%s' kann nicht erstellt werden: %w", dst, err)
	}

	// Verzeichnisinhalt lesen
	entries, err := os.ReadDir(src)
	if err != nil {
		return fmt.Errorf("CopyDir: Verzeichnis '%s' kann nicht gelesen werden: %w", src, err)
	}

	for _, entry := range entries {
		srcPath := filepath.Join(src, entry.Name())
		dstPath := filepath.Join(dst, entry.Name())

		// Symlink-Info prüfen (lstat, nicht stat)
		info, err := os.Lstat(srcPath)
		if err != nil {
			return fmt.Errorf("CopyDir: '%s' kann nicht gelesen werden: %w", srcPath, err)
		}

		if info.Mode()&os.ModeSymlink != 0 {
			// Symlink: Zielpfad auslesen und neu erstellen
			target, err := os.Readlink(srcPath)
			if err != nil {
				return fmt.Errorf("CopyDir: Symlink '%s' kann nicht gelesen werden: %w", srcPath, err)
			}
			if err := os.Symlink(target, dstPath); err != nil {
				return fmt.Errorf("CopyDir: Symlink '%s' -> '%s' kann nicht erstellt werden: %w", dstPath, target, err)
			}
		} else if info.IsDir() {
			// Rekursiv ins Unterverzeichnis
			if err := CopyDir(srcPath, dstPath); err != nil {
				return err
			}
		} else {
			// Normale Datei kopieren
			if err := CopyFile(srcPath, dstPath); err != nil {
				return err
			}
		}
	}

	log.Infof("CopyDir: '%s' -> '%s' kopiert", src, dst)
	return nil
}

// CopyFile kopiert eine Datei von src nach dst, erstellt Verzeichnisse.
// Erhält Permissions. Folgt Symlinks beim Lesen.
func CopyFile(src, dst string) error {
	// Zielverzeichnis erstellen
	dstDir := filepath.Dir(dst)
	if err := os.MkdirAll(dstDir, 0755); err != nil {
		return fmt.Errorf("CopyFile: Verzeichnis '%s' kann nicht erstellt werden: %w", dstDir, err)
	}

	// Quelldatei öffnen (folgt Symlinks -> liest die Zieldatei)
	srcFile, err := os.Open(src)
	if err != nil {
		return fmt.Errorf("CopyFile: Quelle '%s' kann nicht geöffnet werden: %w", src, err)
	}
	defer srcFile.Close()

	// Zieldatei erstellen
	dstFile, err := os.Create(dst)
	if err != nil {
		return fmt.Errorf("CopyFile: Ziel '%s' kann nicht erstellt werden: %w", dst, err)
	}
	defer dstFile.Close()

	// Inhalt kopieren
	if _, err := io.Copy(dstFile, srcFile); err != nil {
		return fmt.Errorf("CopyFile: Kopieren von '%s' nach '%s' fehlgeschlagen: %w", src, dst, err)
	}

	// Permissions der Originaldatei übernehmen
	srcInfo, err := os.Stat(src) // Stat folgt Symlinks
	if err != nil {
		log.Warnf("CopyFile: Warnung: Kann Permissions von '%s' nicht lesen: %v", src, err)
	} else {
		if err := os.Chmod(dst, srcInfo.Mode().Perm()); err != nil {
			log.Warnf("CopyFile: Warnung: Kann Permissions für '%s' nicht setzen: %v", dst, err)
		}
	}

	log.Infof("CopyFile: '%s' -> '%s' kopiert", src, dst)
	return nil
}

// CopySymlink erstellt einen Symlink dst der auf src zeigt.
// src ist das Ziel, auf das der Symlink zeigen soll (relativer oder absoluter Pfad).
func CopySymlink(src, dst string) error {
	dstDir := filepath.Dir(dst)
	if err := os.MkdirAll(dstDir, 0755); err != nil {
		return fmt.Errorf("CopySymlink: Verzeichnis '%s' kann nicht erstellt werden: %w", dstDir, err)
	}

	// Entferne bestehendes Ziel, falls vorhanden (z.B. wenn vorher eine Datei kopiert wurde)
	if _, err := os.Lstat(dst); err == nil {
		if err := os.Remove(dst); err != nil {
			return fmt.Errorf("CopySymlink: Kann bestehendes '%s' nicht entfernen: %w", dst, err)
		}
	}

	if err := os.Symlink(src, dst); err != nil {
		return fmt.Errorf("CopySymlink: Symlink '%s' -> '%s' kann nicht erstellt werden: %w", dst, src, err)
	}

	log.Infof("CopySymlink: '%s' -> '%s' (Symlink)", dst, src)
	return nil
}

// IsAlwaysLink prüft ob ein Pfad immer als Symlink behandelt werden soll.
// Ein Pfad gilt als "always link", wenn er mit einem der Einträge in alwaysLink beginnt.
func IsAlwaysLink(path string, alwaysLink []string) bool {
	for _, prefix := range alwaysLink {
		// Prüfe ob path mit prefix beginnt (und ggf. ein '/' oder Ende folgt)
		if strings.HasPrefix(path, prefix) {
			rest := strings.TrimPrefix(path, prefix)
			if rest == "" || strings.HasPrefix(rest, "/") {
				return true
			}
		}
	}
	return false
}

// StagingPath berechnet den Zielpfad im Staging-Verzeichnis.
// z.B. src="/usr/lib/libc.so.6", stagingDir="/tmp/myapp" -> "/tmp/myapp/usr/lib/libc.so.6"
func StagingPath(src, stagingDir string) string {
	// src muss absolut sein (beginnt mit /)
	// stagingDir muss absolut sein oder wird als absolut behandelt
	if !filepath.IsAbs(src) {
		// Relativen Pfad als absolut behandeln (sollte nicht vorkommen)
		src = "/" + src
	}
	// Clean den Pfad (entfernt Doppelslash, etc.)
	src = filepath.Clean(src)
	// Entferne führenden Slash von src
	src = strings.TrimPrefix(src, "/")
	// Kombinieren
	return filepath.Join(stagingDir, src)
}
