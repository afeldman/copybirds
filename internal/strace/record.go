package strace

// FileAccess beschreibt eine Datei die vom Programm verwendet wurde
type FileAccess struct {
	Filename string
	PID      int
	Read     bool
	Written  bool
	Executed bool
	Version  string // z.B. ELF-Versionstring
	SHA256   string // Hash der Datei
}

// Connection beschreibt eine Netzwerkverbindung
type Connection struct {
	PID     int
	Address string
	Port    int
}

// TraceResult ist das Ergebnis eines vollständigen strace-Parse-Laufs
type TraceResult struct {
	Files       []*FileAccess
	Connections []*Connection
	WorkDir     string
}
