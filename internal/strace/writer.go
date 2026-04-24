package strace

import (
	"encoding/xml"
	"fmt"
	"os"
)

// writeXMLRoot ist die äußere XML-Struktur
type writeXMLRoot struct {
	XMLName     xml.Name      `xml:"copybirds"`
	Version     string        `xml:"version,attr"`
	Files       writeXMLFiles `xml:"files"`
	Connections writeXMLConns `xml:"connections"`
}

type writeXMLFiles struct {
	Files []writeXMLFile `xml:"file"`
}

type writeXMLFile struct {
	Filename string `xml:"filename,attr"`
	PID      int    `xml:"pid,attr"`
	Read     string `xml:"read,attr"`
	Written  string `xml:"write,attr"`
	Executed string `xml:"exec,attr"`
	Version  string `xml:"version,attr"`
	SHA256   string `xml:"sha256,attr"`
}

type writeXMLConns struct {
	Conns []writeXMLConn `xml:"connection"`
}

type writeXMLConn struct {
	PID     int    `xml:"pid,attr"`
	Address string `xml:"address,attr"`
	Port    int    `xml:"port,attr"`
}

// boolToIntStr wandelt einen boolschen Wert in "1" oder "0" um (kompatibel zum C-Original)
func boolToIntStr(b bool) string {
	if b {
		return "1"
	}
	return "0"
}

// WriteXML schreibt ein TraceResult als XML-Datei.
// XML-Struktur (kompatibel mit dem Original):
//
// <copybirds version="0.8.7">
//
//	<files>
//	  <file filename="/lib/x86_64/libc.so.6" pid="1"
//	        read="1" write="0" exec="1"
//	        version="" sha256="abcdef..."/>
//	</files>
//	<connections>
//	  <connection pid="1" address="192.168.1.1" port="80"/>
//	</connections>
//
// </copybirds>
func WriteXML(result *TraceResult, outPath string) error {
	root := writeXMLRoot{
		Version: "0.8.7",
	}

	// Dateien
	for _, fa := range result.Files {
		root.Files.Files = append(root.Files.Files, writeXMLFile{
			Filename: fa.Filename,
			PID:      fa.PID,
			Read:     boolToIntStr(fa.Read),
			Written:  boolToIntStr(fa.Written),
			Executed: boolToIntStr(fa.Executed),
			Version:  fa.Version,
			SHA256:   fa.SHA256,
		})
	}

	// Verbindungen
	for _, c := range result.Connections {
		root.Connections.Conns = append(root.Connections.Conns, writeXMLConn{
			PID:     c.PID,
			Address: c.Address,
			Port:    c.Port,
		})
	}

	// XML schreiben
	data, err := xml.MarshalIndent(root, "", "  ")
	if err != nil {
		return fmt.Errorf("xml marshalling fehlgeschlagen: %w", err)
	}
	data = append([]byte(xml.Header), data...)

	// Zeilenumbruch am Ende hinzufügen
	data = append(data, '\n')

	return os.WriteFile(outPath, data, 0644)
}
