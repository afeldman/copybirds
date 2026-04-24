package meta

// SysInfo enthält die gesammelten System-Metadaten
type SysInfo struct {
	Distro        string // z.B. "Ubuntu 24.04"
	Kernel        string // uname -r output
	Arch          string // uname -m
	GraphicDriver string // GL_RENDERER
	GraphicCard   string // GL_VENDOR
	GLVersion     string // GL_VERSION
	GLExtensions  string // GL_EXTENSIONS (lange Liste)
	UserHome      string // $HOME
	Packages      []PackageInfo
	FilePackages  []FilePackageEntry // Datei → Paket Zuordnung
	Commands      []string           // ausgeführte Kommandos
	Connections   []string           // Netzwerkverbindungen
}

// PackageInfo enthält Informationen zu einem installierten Paket
type PackageInfo struct {
	Name      string
	Version   string
	Installed bool
}

// FilePackageEntry ordnet einen Dateipfad seinem Debian-Paket zu
type FilePackageEntry struct {
	File    string
	Package string
}
