package fileops

import "strings"

// DefaultBlacklist enthält die Standard-Blacklist-Einträge.
// Dateien/Pfade die hier gelistet sind, werden nie kopiert.
var DefaultBlacklist = []string{
	// Sensible Systemdateien
	"/etc/passwd",
	"/etc/shadow",
	"/etc/group",
	"/etc/gshadow",
	// Temp-Dateien
	"/tmp/",
	"/var/tmp/",
	// Proc-Filesystem
	"/proc/",
	"/sys/",
	"/dev/",
}

// IsBlacklisted prüft ob eine Datei auf der Blacklist steht.
// Wenn useDefault true ist, wird die DefaultBlacklist verwendet.
// Ein Pfad gilt als blacklisted, wenn er mit einem Blacklist-Eintrag beginnt
// oder exakt einem Eintrag entspricht (wildcard-freier Prefix-Match wie im C-Original).
func IsBlacklisted(path string, useDefault bool) bool {
	var list []string
	if useDefault {
		list = DefaultBlacklist
	}
	if len(list) == 0 {
		return false
	}
	for _, entry := range list {
		if matchBlacklist(path, entry) {
			return true
		}
	}
	return false
}

// matchBlacklist prüft ob path dem Blacklist-Eintrag entry entspricht.
// Ein Eintrag wie "/tmp/" matcht alle Pfade die mit "/tmp/" beginnen.
// Ein Eintrag wie "/etc/passwd" matcht exakt "/etc/passwd".
func matchBlacklist(path, entry string) bool {
	// Wenn der Eintrag auf '/' endet (Verzeichnis-Prefix), prüfe Prefix
	if strings.HasSuffix(entry, "/") {
		return strings.HasPrefix(path, entry)
	}
	// Sonst exakter Match
	return path == entry
}
