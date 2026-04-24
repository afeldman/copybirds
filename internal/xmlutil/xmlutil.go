package xmlutil

import (
	"encoding/xml"
	"os"
)

// Node repräsentiert einen einfachen XML-Knoten
type Node struct {
	XMLName  xml.Name
	Attrs    []xml.Attr `xml:",any,attr"`
	Content  string     `xml:",chardata"`
	Children []*Node    `xml:",any"`
}

// ReadFile liest eine XML-Datei und gibt den Root-Node zurück
func ReadFile(path string) (*Node, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var root Node
	if err := xml.Unmarshal(data, &root); err != nil {
		return nil, err
	}
	return &root, nil
}

// WriteFile schreibt einen Node-Baum als XML-Datei
func WriteFile(path string, root *Node) error {
	data, err := xml.MarshalIndent(root, "", "  ")
	if err != nil {
		return err
	}
	data = append([]byte(xml.Header), data...)
	return os.WriteFile(path, data, 0644)
}

// GetAttr gibt den Wert eines Attributs zurück, "" wenn nicht vorhanden
func GetAttr(n *Node, key string) string {
	for _, attr := range n.Attrs {
		if attr.Name.Local == key {
			return attr.Value
		}
	}
	return ""
}

// SetAttr setzt oder überschreibt ein Attribut
func SetAttr(n *Node, key, value string) {
	for i, attr := range n.Attrs {
		if attr.Name.Local == key {
			n.Attrs[i].Value = value
			return
		}
	}
	n.Attrs = append(n.Attrs, xml.Attr{
		Name:  xml.Name{Local: key},
		Value: value,
	})
}

// FindChild sucht das erste Kind mit gegebenem Tag-Namen
func FindChild(n *Node, tag string) *Node {
	for _, child := range n.Children {
		if child.XMLName.Local == tag {
			return child
		}
	}
	return nil
}

// FindOrCreate gibt FindChild zurück oder erstellt ein neues Kind
func FindOrCreate(n *Node, tag string) *Node {
	if found := FindChild(n, tag); found != nil {
		return found
	}
	child := &Node{XMLName: xml.Name{Local: tag}}
	n.Children = append(n.Children, child)
	return child
}

// GetChildContent gibt den Text-Content eines Kinds zurück
func GetChildContent(n *Node, tag string) string {
	child := FindChild(n, tag)
	if child == nil {
		return ""
	}
	return child.Content
}
