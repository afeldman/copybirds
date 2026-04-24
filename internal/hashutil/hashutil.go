package hashutil

import (
	"crypto/sha256"
	"encoding/hex"
	"hash/fnv"
	"io"
	"os"
)

// FileHash gibt den SHA256-Hash einer Datei als Hex-String zurück
func FileHash(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}

// StringHash gibt einen uint64-Hash eines Strings zurück (für interne Maps)
func StringHash(s string) uint64 {
	h := fnv.New64a()
	h.Write([]byte(s))
	return h.Sum64()
}
