package meta

import (
	"copybirds/internal/log"
)

// Merge kombiniert mehrere SysInfo-Strukturen zu einer.
// Regeln:
//   - Pakete: Union aller Pakete (kein Duplikat nach Name)
//   - Commands: Union aller Befehle (keine Duplikate)
//   - Connections: Union aller Connections (keine Duplikate)
//   - System-Felder (Distro, Kernel etc.): vom ersten Input übernehmen,
//     bei Abweichung in späteren Inputs: Warnung loggen
func Merge(infos []*SysInfo) *SysInfo {
	if len(infos) == 0 {
		return &SysInfo{}
	}

	result := &SysInfo{
		// System-Felder vom ersten Input übernehmen
		Distro:        infos[0].Distro,
		Kernel:        infos[0].Kernel,
		Arch:          infos[0].Arch,
		GraphicDriver: infos[0].GraphicDriver,
		GraphicCard:   infos[0].GraphicCard,
		GLVersion:     infos[0].GLVersion,
		GLExtensions:  infos[0].GLExtensions,
		UserHome:      infos[0].UserHome,
	}

	// Pakete: Union (keine Duplikate nach Name)
	pkgSeen := make(map[string]bool)
	for _, p := range infos[0].Packages {
		pkgSeen[p.Name] = true
		result.Packages = append(result.Packages, p)
	}
	for i := 1; i < len(infos); i++ {
		for _, p := range infos[i].Packages {
			if !pkgSeen[p.Name] {
				pkgSeen[p.Name] = true
				result.Packages = append(result.Packages, p)
			}
		}
	}

	// Commands: Union (keine Duplikate)
	cmdSeen := make(map[string]bool)
	for _, c := range infos[0].Commands {
		cmdSeen[c] = true
		result.Commands = append(result.Commands, c)
	}
	for i := 1; i < len(infos); i++ {
		for _, c := range infos[i].Commands {
			if !cmdSeen[c] {
				cmdSeen[c] = true
				result.Commands = append(result.Commands, c)
			}
		}
	}

	// Connections: Union (keine Duplikate)
	connSeen := make(map[string]bool)
	for _, c := range infos[0].Connections {
		connSeen[c] = true
		result.Connections = append(result.Connections, c)
	}
	for i := 1; i < len(infos); i++ {
		for _, c := range infos[i].Connections {
			if !connSeen[c] {
				connSeen[c] = true
				result.Connections = append(result.Connections, c)
			}
		}
	}

	// System-Felder der weiteren Inputs prüfen und bei Abweichung warnen
	for i := 1; i < len(infos); i++ {
		other := infos[i]
		warnOnDiff("Distro", result.Distro, other.Distro, i)
		warnOnDiff("Kernel", result.Kernel, other.Kernel, i)
		warnOnDiff("Arch", result.Arch, other.Arch, i)
		warnOnDiff("GraphicDriver", result.GraphicDriver, other.GraphicDriver, i)
		warnOnDiff("GraphicCard", result.GraphicCard, other.GraphicCard, i)
		warnOnDiff("GLVersion", result.GLVersion, other.GLVersion, i)
		warnOnDiff("UserHome", result.UserHome, other.UserHome, i)
	}

	log.Warnf("Merge: %d SysInfo-Strukturen zusammengeführt -> %d Pakete, %d Commands, %d Connections",
		len(infos), len(result.Packages), len(result.Commands), len(result.Connections))

	return result
}

// warnOnDiff loggt eine Warnung wenn zwei Werte unterschiedlich sind und beide nicht-leer.
func warnOnDiff(field, v1, v2 string, index int) {
	if v1 != v2 && v1 != "" && v2 != "" {
		log.Warnf("Merge: Warnung: '%s' unterscheidet sich in Input %d: '%s' vs '%s' (verwende '%s')",
			field, index+1, v1, v2, v1)
	}
}
