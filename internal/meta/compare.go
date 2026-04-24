package meta

import (
	"strings"

	"copybirds/internal/log"
)

// Compare vergleicht zwei SysInfo und gibt Unterschiede aus.
// Gibt true zurück wenn kompatibel (keine kritischen Unterschiede).
func Compare(current, stored *SysInfo) bool {
	compatible := true

	// Home-Verzeichnis vergleichen (wie in comparesys.c compare_string)
	compareString(current.UserHome, stored.UserHome, "Homeverzeichnis")

	// Distro vergleichen
	if compareString(current.Distro, stored.Distro, "Distribution") == 1 {
		// Bei gleicher Distro: kein weiterer Check nötig für Distro
	} else {
		log.Warnf("compare: Warnung: Distributionen unterscheiden sich: '%s' vs '%s'",
			stored.Distro, current.Distro)
		// Unterschiedliche Distro ist eine Warnung, aber nicht blockierend
	}

	// Architektur vergleichen (kritisch!)
	if current.Arch != stored.Arch {
		log.Errorf("compare: Fehler: Architektur unterschiedlich: '%s' vs '%s'",
			stored.Arch, current.Arch)
		compatible = false
	}

	// Kernel-Version vergleichen (wie compare_stringversions im C-Code)
	compareVersions(stored.Kernel, current.Kernel, "Kernel", 0)

	// GL-Info vergleichen (nur wenn vorhanden)
	if stored.GraphicDriver != "" || current.GraphicDriver != "" {
		compareString(current.GraphicDriver, stored.GraphicDriver, "Grafiktreiber")
	}
	if stored.GraphicCard != "" || current.GraphicCard != "" {
		compareString(current.GraphicCard, stored.GraphicCard, "Grafikkarte")
	}
	if stored.GLVersion != "" || current.GLVersion != "" {
		compareVersions(stored.GLVersion, current.GLVersion, "GL-Version", 0)
	}

	// GL-Extensions vergleichen
	if stored.GLExtensions != "" {
		if current.GLExtensions == "" {
			log.Warnf("compare: Warnung: GL-Extensions konnten auf aktuellem System nicht ermittelt werden")
		} else {
			compareExtensions(stored.GLExtensions, current.GLExtensions)
		}
	}

	// Pakete vergleichen
	comparePackages(current, stored)

	return compatible
}

// compareString vergleicht zwei Strings und gibt 1 zurück wenn gleich, 0 wenn unterschiedlich, -1 bei Fehler
// (entspricht compare_string in comparesys.c)
func compareString(v1, v2, name string) int {
	if v1 != "" && v2 != "" {
		if v1 == v2 {
			log.Infof("compareString: Info: %s sind gleich", name)
			return 1
		}
		log.Warnf("compareString: Warnung: %s unterscheiden sich: '%s' vs '%s'", name, v1, v2)
		return 0
	}
	if v1 == "" && v2 == "" {
		return 1
	}
	log.Warnf("compareString: Warnung: %s hat mindestens einen nicht gesetzten Wert", name)
	return -1
}

// compareVersions vergleicht zwei Versionsstrings (vereinfachte Version von compare_versions in comparesys.c)
// Gibt -1 zurück wenn source älter, 0 wenn gleich, 1 wenn neuer
func compareVersions(source, dest, name string, reportLevel int) {
	if source == "" || dest == "" {
		log.Infof("compareVersions: Info: Version von '%s' kann nicht verglichen werden (fehlende Daten)", name)
		return
	}
	if source == dest {
		log.Infof("compareVersions: Info: Version von '%s' ist gleich: '%s'", name, source)
		return
	}
	log.Warnf("compareVersions: Info: Version von '%s' unterscheidet sich: '%s' vs '%s'",
		name, dest, source)
}

// compareExtensions prüft ob alle Extensions aus stored auch in current vorhanden sind
// (entspricht compare_glext in comparesys.c)
func compareExtensions(stored, current string) {
	storedExts := strings.Fields(stored)
	currentExts := make(map[string]bool)
	for _, ext := range strings.Fields(current) {
		currentExts[ext] = true
	}

	var missing []string
	for _, ext := range storedExts {
		if !currentExts[ext] {
			missing = append(missing, ext)
		}
	}

	if len(missing) > 0 {
		log.Warnf("compareExtensions: Warnung: Folgende GL-Extensions werden nicht unterstützt: '%s'",
			strings.Join(missing, " "))
	} else {
		log.Infof("compareExtensions: Info: Alle GL-Extensions werden unterstützt")
	}
}

// comparePackages vergleicht die Pakete aus stored mit dem aktuellen System
// (entspricht compare_packages + package_found in comparesys.c)
func comparePackages(current, stored *SysInfo) {
	if len(stored.Packages) == 0 {
		log.Warnf("comparePackages: Keine Paketinformationen in XML-Datei gefunden")
		return
	}

	// Bauen eine Map der aktuellen Pakete für schnellen Lookup
	currentPkgMap := make(map[string]PackageInfo)
	for _, p := range current.Packages {
		currentPkgMap[p.Name] = p
	}

	for _, sp := range stored.Packages {
		cp, found := currentPkgMap[sp.Name]
		if !found {
			log.Warnf("comparePackages: Fehler: Benötigtes Paket '%s' existiert nicht auf dem aktuellen System",
				sp.Name)
			continue
		}
		if !cp.Installed {
			log.Warnf("comparePackages: Warnung: Paket '%s' ist aktuell nicht installiert",
				sp.Name)
		}
		// Versionsvergleich (wie compare_stringversions im C-Code)
		compareVersions(sp.Version, cp.Version, sp.Name, 1)
	}
}
