package fileops

import (
	"fmt"
	"os"

	"copybirds/internal/log"
	"copybirds/internal/xmlutil"
)

// Stage liest die strace-XML und kopiert alle Dateien ins Staging-Verzeichnis.
// Gibt Anzahl kopierter Dateien zurück.
//
// XML-Format (erzeugt von cb_strace_to_xml):
//
//	<copybirds version="0.8.7">
//	  <files>
//	    <file filename="/lib/x86_64/libc.so.6" pid="1"
//	          read="1" write="0" exec="1"
//	          version="" sha256="abcdef..."/>
//	  </files>
//	</copybirds>
//
// Unterstützt auch das ältere Format aus cb_meta_merge:
//
//	<content>
//	  <filelist>
//	    <file name="/etc/services" pid="1"/>
//	  </filelist>
//	</content>
func Stage(xmlPath, stagingDir string, opts CopyOptions) (int, error) {
	log.Warnf("Stage: Lese XML '%s'...", xmlPath)

	root, err := xmlutil.ReadFile(xmlPath)
	if err != nil {
		return 0, fmt.Errorf("Stage: XML-Datei '%s' kann nicht gelesen werden: %w", xmlPath, err)
	}

	// Staging-Verzeichnis erstellen
	if err := os.MkdirAll(stagingDir, 0755); err != nil {
		return 0, fmt.Errorf("Stage: Staging-Verzeichnis '%s' kann nicht erstellt werden: %w", stagingDir, err)
	}

	// Dateiliste aus XML extrahieren
	var files []string

	// Format 1: <copybirds><files><file filename="..."/>
	copybirdsNode := xmlutil.FindChild(root, "copybirds")
	if copybirdsNode != nil {
		filesNode := xmlutil.FindChild(copybirdsNode, "files")
		if filesNode != nil {
			for _, child := range filesNode.Children {
				if child.XMLName.Local == "file" {
					filename := xmlutil.GetAttr(child, "filename")
					if filename != "" {
						files = append(files, filename)
					}
				}
			}
		}
	}

	// Format 2: <content><filelist><file name="..."/>
	contentNode := xmlutil.FindChild(root, "content")
	if contentNode != nil {
		filelistNode := xmlutil.FindChild(contentNode, "filelist")
		if filelistNode != nil {
			for _, child := range filelistNode.Children {
				if child.XMLName.Local == "file" {
					filename := xmlutil.GetAttr(child, "name")
					if filename != "" {
						files = append(files, filename)
					}
				}
			}
		}
	}

	// Fallback: root selbst könnte schon <copybirds> oder <content> sein
	if copybirdsNode == nil && contentNode == nil {
		// Prüfe ob root selbst <copybirds> ist
		if root.XMLName.Local == "copybirds" {
			filesNode := xmlutil.FindChild(root, "files")
			if filesNode != nil {
				for _, child := range filesNode.Children {
					if child.XMLName.Local == "file" {
						filename := xmlutil.GetAttr(child, "filename")
						if filename != "" {
							files = append(files, filename)
						}
					}
				}
			}
		}
		// Prüfe ob root selbst <content> ist
		if root.XMLName.Local == "content" {
			filelistNode := xmlutil.FindChild(root, "filelist")
			if filelistNode != nil {
				for _, child := range filelistNode.Children {
					if child.XMLName.Local == "file" {
						filename := xmlutil.GetAttr(child, "name")
						if filename != "" {
							files = append(files, filename)
						}
					}
				}
			}
		}
	}

	if len(files) == 0 {
		return 0, fmt.Errorf("Stage: Keine Dateien in '%s' gefunden", xmlPath)
	}

	log.Warnf("Stage: %d Dateien in XML gefunden", len(files))

	// Dateien kopieren
	copied := 0
	skipped := 0
	for _, filepath := range files {
		log.Infof("Stage: Verarbeite '%s'...", filepath)

		// Blacklist-Check
		if IsBlacklisted(filepath, opts.DefaultBlacklist) {
			log.Infof("Stage: '%s' ist blacklisted -> überspringe", filepath)
			skipped++
			continue
		}

		// Zielpfad berechnen
		dst := StagingPath(filepath, stagingDir)

		// Prüfen ob die Quelle ein Verzeichnis ist
		srcInfo, errStat := os.Stat(filepath)
		if errStat != nil {
			log.Warnf("Stage: Warnung: Datei '%s' nicht gefunden -> überspringe", filepath)
			skipped++
			continue
		}

		if srcInfo.IsDir() {
			// Verzeichnis: nur Struktur anlegen (leeres Verzeichnis), nicht Inhalt rekursiv
			dstDir := StagingPath(filepath, stagingDir)
			log.Infof("Stage: Erstelle Verzeichnis '%s'", dstDir)
			_ = os.MkdirAll(dstDir, srcInfo.Mode().Perm())
			continue
		}

		// Prüfen ob AlwaysLink oder MakeLinks
		if opts.MakeLinks || IsAlwaysLink(filepath, opts.AlwaysLink) {
			log.Infof("Stage: Erstelle Symlink für '%s' -> '%s'", filepath, dst)
			if err := CopySymlink(filepath, dst); err != nil {
				log.Warnf("Stage: Fehler beim Symlink für '%s': %v", filepath, err)
				// Bei Symlink-Fehler weitermachen (Datei existiert vielleicht nicht)
				skipped++
				continue
			}
		} else {
			// Normales Kopieren
			log.Infof("Stage: Kopiere '%s' -> '%s'", filepath, dst)
			if err := CopyFile(filepath, dst); err != nil {
				log.Warnf("Stage: Fehler beim Kopieren von '%s': %v", filepath, err)
				// Bei Kopierfehler weitermachen
				skipped++
				continue
			}
		}

		copied++
	}

	log.Warnf("Stage: %d Dateien kopiert, %d übersprungen", copied, skipped)
	return copied, nil
}
