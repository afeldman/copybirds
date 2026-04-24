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
	Commands      []string // ausgeführte Kommandos
	Connections   []string // Netzwerkverbindungen
}

// PackageInfo enthält Informationen zu einem installierten Paket
type PackageInfo struct {
	Name      string
	Version   string
	Installed bool
}
