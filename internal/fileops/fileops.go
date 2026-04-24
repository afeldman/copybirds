package fileops

// CopyOptions steuert das Kopieverhalten
type CopyOptions struct {
	MakeLinks        bool     // Symlinks statt Kopien
	DefaultBlacklist bool     // Standard-Blacklist aktiv
	AlwaysLink       []string // Diese Pfade immer als Symlink (default: /dev /proc /tmp /bin /sbin /sys)
}

// DefaultOptions gibt die Standard-Optionen zurück
func DefaultOptions() CopyOptions {
	return CopyOptions{
		DefaultBlacklist: true,
		AlwaysLink:       []string{"/dev", "/proc", "/tmp", "/bin", "/sbin", "/sys"},
	}
}
