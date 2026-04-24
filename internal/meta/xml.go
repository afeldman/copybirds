package meta

import (
	"encoding/xml"
	"fmt"
	"os"

	"copybirds/internal/log"
)

// XML-Strukturen für Serialisierung
type xmlMeta struct {
	XMLName     xml.Name       `xml:"copybirds_meta"`
	Version     string         `xml:"version,attr"`
	System      xmlSystem      `xml:"system"`
	UserHome    string         `xml:"userhome"`
	Packages    xmlPackages    `xml:"packages"`
	Commands    xmlCommands    `xml:"commands"`
	Connections xmlConnections `xml:"connections"`
	FilePackages xmlFilePackages `xml:"file_packages"`
}

type xmlSystem struct {
	Distro        string `xml:"distro,attr"`
	Kernel        string `xml:"kernel,attr"`
	Arch          string `xml:"arch,attr"`
	GraphicDriver string `xml:"graphicdriver,attr"`
	GraphicCard   string `xml:"graphiccard,attr"`
	GLVersion     string `xml:"glversion,attr"`
}

type xmlPackages struct {
	List []xmlPackage `xml:"package"`
}

type xmlPackage struct {
	Name      string `xml:"name,attr"`
	Version   string `xml:"version,attr"`
	Installed string `xml:"installed,attr"`
}

type xmlCommands struct {
	List []xmlCommand `xml:"command"`
}

type xmlCommand struct {
	Name string `xml:",chardata"`
}

type xmlConnections struct {
	List []xmlConnection `xml:"connection"`
}

type xmlConnection struct {
	Name string `xml:",chardata"`
}

type xmlFilePackages struct {
	List []xmlFilePackage `xml:"entry"`
}

type xmlFilePackage struct {
	File    string `xml:"file,attr"`
	Package string `xml:"package,attr"`
}

const metaVersion = "0.9.0"

// ReadXML liest eine gespeicherte SysInfo aus einer XML-Datei
func ReadXML(path string) (*SysInfo, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("ReadXML: Datei '%s' kann nicht gelesen werden: %w", path, err)
	}

	var xm xmlMeta
	if err := xml.Unmarshal(data, &xm); err != nil {
		return nil, fmt.Errorf("ReadXML: XML-Parsing von '%s' fehlgeschlagen: %w", path, err)
	}

	info := &SysInfo{
		Distro:        xm.System.Distro,
		Kernel:        xm.System.Kernel,
		Arch:          xm.System.Arch,
		GraphicDriver: xm.System.GraphicDriver,
		GraphicCard:   xm.System.GraphicCard,
		GLVersion:     xm.System.GLVersion,
		UserHome:      xm.UserHome,
	}

	// Pakete konvertieren
	for _, p := range xm.Packages.List {
		installed := p.Installed == "1"
		info.Packages = append(info.Packages, PackageInfo{
			Name:      p.Name,
			Version:   p.Version,
			Installed: installed,
		})
	}

	// Commands konvertieren
	for _, c := range xm.Commands.List {
		info.Commands = append(info.Commands, c.Name)
	}

	// Connections konvertieren
	for _, c := range xm.Connections.List {
		info.Connections = append(info.Connections, c.Name)
	}

	// FilePackages konvertieren
	for _, fp := range xm.FilePackages.List {
		info.FilePackages = append(info.FilePackages, FilePackageEntry{
			File:    fp.File,
			Package: fp.Package,
		})
	}

	log.Infof("ReadXML: Geladene SysInfo aus '%s': Distro='%s', Kernel='%s', %d Pakete",
		path, info.Distro, info.Kernel, len(info.Packages))

	return info, nil
}

// WriteXML schreibt SysInfo als XML-Datei
func WriteXML(info *SysInfo, path string) error {
	xm := xmlMeta{
		Version: metaVersion,
		System: xmlSystem{
			Distro:        info.Distro,
			Kernel:        info.Kernel,
			Arch:          info.Arch,
			GraphicDriver: info.GraphicDriver,
			GraphicCard:   info.GraphicCard,
			GLVersion:     info.GLVersion,
		},
		UserHome: info.UserHome,
	}

	// Pakete konvertieren
	for _, p := range info.Packages {
		installed := "0"
		if p.Installed {
			installed = "1"
		}
		xm.Packages.List = append(xm.Packages.List, xmlPackage{
			Name:      p.Name,
			Version:   p.Version,
			Installed: installed,
		})
	}

	// Commands konvertieren
	for _, c := range info.Commands {
		xm.Commands.List = append(xm.Commands.List, xmlCommand{Name: c})
	}

	// Connections konvertieren
	for _, c := range info.Connections {
		xm.Connections.List = append(xm.Connections.List, xmlConnection{Name: c})
	}

	// FilePackages konvertieren
	for _, fp := range info.FilePackages {
		xm.FilePackages.List = append(xm.FilePackages.List, xmlFilePackage{
			File:    fp.File,
			Package: fp.Package,
		})
	}

	data, err := xml.MarshalIndent(xm, "", "  ")
	if err != nil {
		return fmt.Errorf("WriteXML: XML-Marshalling fehlgeschlagen: %w", err)
	}
	data = []byte(xml.Header + string(data) + "\n")

	if err := os.WriteFile(path, data, 0644); err != nil {
		return fmt.Errorf("WriteXML: Konnte '%s' nicht schreiben: %w", path, err)
	}

	log.Warnf("WriteXML: SysInfo nach '%s' geschrieben (%d Pakete, %d Commands, %d Connections)",
		path, len(info.Packages), len(info.Commands), len(info.Connections))

	return nil
}
