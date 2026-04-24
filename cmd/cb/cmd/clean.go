package cmd

import (
	"bufio"
	"encoding/xml"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
)

// neverDelete is a HARDCODED list of system-critical paths that must NEVER be deleted.
var neverDelete = []string{
	"/bin", "/sbin", "/usr/bin", "/usr/sbin",
	"/lib", "/lib64", "/usr/lib",
	"/etc", "/boot", "/root", "/home",
	"/dev", "/proc", "/sys", "/run",
	"/var/log",
}

// XML-Struktur für clean (kompatibel mit cb_strace_to_xml)
type cleanXMLRoot struct {
	XMLName xml.Name `xml:"copybirds"`
	Files   cleanXMLFiles `xml:"files"`
}

type cleanXMLFiles struct {
	Files []cleanXMLFile `xml:"file"`
}

type cleanXMLFile struct {
	Filename string `xml:"filename,attr"`
	PID      int    `xml:"pid,attr"`
	Read     string `xml:"read,attr"`
	Written  string `xml:"write,attr"`
	Executed string `xml:"exec,attr"`
	Version  string `xml:"version,attr"`
	SHA256   string `xml:"sha256,attr"`
}

func newCleanCmd() *cobra.Command {
	var dryRun, force bool

	cmd := &cobra.Command{
		Use:   "clean XMLFILE",
		Short: "Remove traced files from source system",
		Long: `Removes all files listed in an XML manifest from the system.
WARNING: This deletes files from your source system. Use with caution.

Protected system directories (/bin, /etc, /lib, etc.) are never deleted
regardless of flags.`,
		Example: `  cb clean record.xml
  cb clean --dry-run record.xml
  cb clean --force record.xml`,
		Args: cobra.ExactArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			xmlFile := args[0]

			// Regel: Wenn --force UND --dry-run gleichzeitig: --dry-run gewinnt
			if dryRun && force {
				log.Warnf("Note: --dry-run overrides --force. Nothing will be deleted.")
				force = false
			}

			// 1. XML einlesen
			log.Infof("Reading XML file: %s", xmlFile)
			files, err := collectFiles(xmlFile)
			if err != nil {
				return fmt.Errorf("clean: %w", err)
			}

			if len(files) == 0 {
				log.Warnf("clean: no files found in XML.")
				return nil
			}

			// 2. Sicherheitscheck: Geschützte Pfade filtern
			var toDelete []string
			var skipped []string

			for _, f := range files {
				if isProtected(f) {
					skipped = append(skipped, f)
				} else {
					toDelete = append(toDelete, f)
				}
			}

			// 3. Interaktive Bestätigung (wenn kein --force)
			if len(toDelete) == 0 {
				log.Warnf("clean: no files to delete (all are protected or already filtered).")
				if len(skipped) > 0 {
					log.Warnf("clean: %d files were skipped due to security rules.", len(skipped))
				}
				return nil
			}

			// Geschützte Dateien warnen
			if len(skipped) > 0 {
				log.Warnf("WARNING: %d files are skipped due to security rules:", len(skipped))
				for _, f := range skipped {
					log.Warnf("  (protected) %s", f)
				}
			}

			// Anzeige der zu löschenden Dateien
			log.Warnf("WARNING: The following %d files will be deleted:", len(toDelete))
			for _, f := range toDelete {
				log.Warnf("  %s", f)
			}

			// Bestätigung oder Trockenlauf
			if !force {
				if dryRun {
					log.Infof("\n--- DRY RUN ---")
					fmt.Printf("%d files marked for deletion.\n", len(toDelete))
					log.Infof("No changes made.")
					return nil
				}

				if !promptConfirmation("\nContinue? [y/N] ") {
					log.Warnf("clean: cancelled by user.")
					return nil
				}
			} else if dryRun {
				log.Infof("--- DRY RUN ---")
				fmt.Printf("%d files marked for deletion.\n", len(toDelete))
				return nil
			}
			// 4. Dateien löschen
			deleted := 0
			removalErrors := 0

			for _, f := range toDelete {
				err := os.Remove(f)
				if err != nil {
					log.Warnf("WARNING: could not delete '%s': %v", f, err)
					removalErrors++
					continue
				}
				log.Infof("Deleted: %s", f)
				deleted++
			}

			// 5. Leere Verzeichnisse entfernen (bottom-up)
			if deleted > 0 {
				dirs := collectParentDirs(toDelete)
				removedDirs := removeEmptyDirs(dirs)
				if removedDirs > 0 {
					log.Warnf("%d empty directories removed.", removedDirs)
				}
			}

			// 6. Zusammenfassung
			log.Warnf("clean: %d files successfully deleted.", deleted)
			if removalErrors > 0 {
				log.Warnf("clean: %d files could not be deleted (warn+continue).", removalErrors)
			}
			if len(skipped) > 0 {
				log.Warnf("clean: %d files skipped due to security rules.", len(skipped))
			}

			return nil
		},
	}

	cmd.Flags().BoolVar(&dryRun, "dry-run", false, "only show what would be deleted, don't actually delete")
	cmd.Flags().BoolVar(&force, "force", false, "skip confirmation prompt (DANGEROUS)")

	return cmd
}

// isProtected prüft, ob der Pfad auf der neverDelete-Liste steht
func isProtected(path string) bool {
	clean := filepath.Clean(path)

	for _, protected := range neverDelete {
		if clean == protected {
			return true
		}
		if strings.HasPrefix(clean, protected+"/") {
			return true
		}
	}
	return false
}

// collectFiles liest alle <file filename="..."> aus der XML-Datei
func collectFiles(xmlPath string) ([]string, error) {
	data, err := os.ReadFile(xmlPath)
	if err != nil {
		return nil, fmt.Errorf("could not read XML file: %w", err)
	}

	var root cleanXMLRoot
	if err := xml.Unmarshal(data, &root); err != nil {
		return nil, fmt.Errorf("could not parse XML: %w", err)
	}

	var files []string
	seen := make(map[string]bool)
	for _, f := range root.Files.Files {
		name := f.Filename
		if name == "" || seen[name] {
			continue
		}
		seen[name] = true
		files = append(files, name)
	}

	sort.Strings(files)
	return files, nil
}

// promptConfirmation fragt den Benutzer nach Bestätigung
func promptConfirmation(msg string) bool {
	fmt.Print(msg)
	reader := bufio.NewReader(os.Stdin)
	line, _ := reader.ReadString('\n')
	line = strings.TrimSpace(line)
	return strings.EqualFold(line, "j") || strings.EqualFold(line, "ja") ||
		strings.EqualFold(line, "y") || strings.EqualFold(line, "yes")
}

// removeEmptyDirs löscht leere Verzeichnisse bottom-up
func removeEmptyDirs(dirs []string) int {
	count := 0

	// Sortiere absteigend (tiefste zuerst) für bottom-up
	sort.Sort(sort.Reverse(sort.StringSlice(dirs)))

	removed := make(map[string]bool)
	for _, dir := range dirs {
		if removed[dir] {
			continue
		}
		current := dir
		for {
			err := os.Remove(current)
			if err != nil {
				break
			}
			log.Infof("Empty directory removed: %s", current)
			removed[current] = true
			count++
			parent := filepath.Dir(current)
			if parent == current || parent == "." || parent == "/" {
				break
			}
			current = parent
		}
	}

	return count
}

// collectParentDirs sammelt alle eindeutigen Elternverzeichnisse aus den Dateipfaden
func collectParentDirs(files []string) []string {
	dirMap := make(map[string]bool)
	for _, f := range files {
		dir := filepath.Dir(f)
		for dir != "." && dir != "/" {
			dirMap[dir] = true
			dir = filepath.Dir(dir)
		}
	}
	var dirs []string
	for d := range dirMap {
		dirs = append(dirs, d)
	}
	return dirs
}
