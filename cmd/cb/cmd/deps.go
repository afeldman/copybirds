package cmd

import (
	"fmt"
	"os/exec"
	"sort"
	"strings"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
	"copybirds/internal/meta"
	"copybirds/internal/packages"
)

func newDepsCmd() *cobra.Command {
	var xmlFile string

	cmd := &cobra.Command{
		Use:   "deps [PACKAGE...]",
		Short: "Analyze .deb package dependencies",
		Long: `Analyzes Debian package dependencies and shows transitive dependencies
as well as missing packages. Packages can be specified as arguments
or loaded from a cb_meta XML file with --xml.`,
		Example: `  cb deps curl git
  cb deps --xml record.xml
  cb deps -v gcc make`,
		Args: cobra.ArbitraryArgs,
		RunE: func(cmd *cobra.Command, args []string) error {
			// 1. Paket-Namen sammeln (aus Args oder XML)
			var packageNames []string

			if xmlFile != "" {
				log.Infof("Reading packages from XML file '%s'...", xmlFile)
				info, err := meta.ReadXML(xmlFile)
				if err != nil {
					return fmt.Errorf("error reading XML file '%s': %w", xmlFile, err)
				}
				for _, p := range info.Packages {
					packageNames = append(packageNames, p.Name)
				}
				if len(packageNames) == 0 {
					return fmt.Errorf("no packages found in XML file '%s'", xmlFile)
				}
				log.Warnf("%d packages loaded from XML file", len(packageNames))
			} else {
				if len(args) == 0 {
					return fmt.Errorf("no packages specified; use arguments or --xml flag")
				}
				packageNames = args
			}

			// 2. Prüfe ob dpkg-query und apt-cache verfügbar sind
			if err := checkCommands(); err != nil {
				return err
			}

			// 3. QueryInstalled() aufrufen
			installed, err := packages.QueryInstalled()
			if err != nil {
				return fmt.Errorf("error querying installed packages: %w", err)
			}
			log.Warnf("%d installed packages loaded", len(installed))

			// 4. ResolveDeps() aufrufen
			required, err := packages.ResolveDeps(packageNames, installed)
			if err != nil {
				return fmt.Errorf("error resolving dependencies: %w", err)
			}

			// 5. FindMissing() aufrufen
			missing := packages.FindMissing(required, installed)

			// 6. Ergebnis formatiert ausgeben
			printResult(packageNames, required, missing)

			log.Warnf("done")
			return nil
		},
	}

	cmd.Flags().StringVarP(&xmlFile, "xml", "x", "", "load packages from cb_meta XML file instead of arguments")

	return cmd
}

// checkCommands prüft ob dpkg-query und apt-cache verfügbar sind
func checkCommands() error {
	var missingTools []string

	if _, err := exec.LookPath("dpkg-query"); err != nil {
		missingTools = append(missingTools, "dpkg-query")
	}
	if _, err := exec.LookPath("apt-cache"); err != nil {
		missingTools = append(missingTools, "apt-cache")
	}

	if len(missingTools) > 0 {
		return fmt.Errorf("required tools not found: %s\ndeps requires a Debian-based system",
			strings.Join(missingTools, ", "))
	}
	return nil
}

// printResult formatiert und gibt das Ergebnis aus
func printResult(roots []string, required map[string]*packages.Package, missing []string) {
	fmt.Println("═══════════════════════════════════════════════")
	fmt.Println("  cb_deps — Dependency Analysis")
	fmt.Println("═══════════════════════════════════════════════")
	fmt.Println()

	// Angeforderte Pakete
	fmt.Println("Requested packages:")
	for _, name := range roots {
		pkg, exists := required[name]
		if exists {
			status := "✓ installed"
			if !pkg.Installed {
				status = "✗ NOT installed"
			}
			ver := pkg.Version
			if ver == "" {
				ver = "(unknown)"
			}
			fmt.Printf("  - %s (%s) — %s\n", name, ver, status)
		} else {
			fmt.Printf("  - %s — not found\n", name)
		}
	}
	fmt.Println()

	// Alle transitiven Abhängigkeiten
	fmt.Println("Transitive dependencies (including requested packages):")
	fmt.Printf("  Total: %d packages\n", len(required))
	fmt.Println()

	// Sortierte Liste aller Pakete ausgeben
	var names []string
	for name := range required {
		names = append(names, name)
	}
	sort.Strings(names)

	fmt.Println("Package list:")
	for _, name := range names {
		pkg := required[name]
		status := "✓"
		if !pkg.Installed {
			status = "✗"
		}
		ver := pkg.Version
		if ver == "" {
			ver = "(not installed)"
		}
		fmt.Printf("  %s %s (%s)\n", status, name, ver)
	}
	fmt.Println()

	// Fehlende Pakete (Warnung)
	if len(missing) > 0 {
		fmt.Println("╔══════════════════════════════════════════════════════╗")
		fmt.Println("║  ⚠ WARNING: The following packages are missing!    ║")
		fmt.Println("╚══════════════════════════════════════════════════════╝")
		sort.Strings(missing)
		for _, name := range missing {
			pkg := required[name]
			ver := pkg.Version
			if ver == "" {
				ver = "(unknown)"
			}
			fmt.Printf("  ✗ %s (%s)\n", name, ver)

			neededBy := findDependents(name, required)
			if len(neededBy) > 0 {
				fmt.Printf("    Required by: %s\n", strings.Join(neededBy, ", "))
			}
		}
		fmt.Println()
	} else {
		fmt.Println("✅ All required packages are installed.")
		fmt.Println()
	}
}

// findDependents findet alle Pakete die von name abhängen
func findDependents(name string, required map[string]*packages.Package) []string {
	var dependents []string
	for pkgName, pkg := range required {
		for _, dep := range pkg.Dependencies {
			if dep == name {
				dependents = append(dependents, pkgName)
				break
			}
		}
	}
	return dependents
}

