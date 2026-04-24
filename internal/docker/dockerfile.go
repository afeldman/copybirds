package docker

import (
	"fmt"
	"os"
	"path/filepath"
	"text/template"
)

// dockerfileTemplate ist das Go-Template für das Dockerfile.
// Das staging/-Subverzeichnis im Build-Context enthält die Anwendungsdateien.
// ldconfig wird in den Container kopiert, ist aber optional (scratch/busybox haben es nicht).
const dockerfileTemplate = `FROM {{.BaseImage}}
COPY staging/ /cb_root/
RUN ldconfig 2>/dev/null || true
ENV LD_LIBRARY_PATH=/cb_root/lib:/cb_root/usr/lib:/cb_root/usr/local/lib
ENTRYPOINT ["/cb_root/{{.Entrypoint}}"]
`

// templateParams ist die interne Template-Datenstruktur
type templateParams struct {
	BaseImage   string
	Entrypoint  string
}

// WriteDockerfile schreibt das Dockerfile basierend auf den BuildOptions in buildCtxDir.
func WriteDockerfile(buildCtxDir string, opts BuildOptions) error {
	dockerfilePath := filepath.Join(buildCtxDir, "Dockerfile")

	tmpl, err := template.New("dockerfile").Parse(dockerfileTemplate)
	if err != nil {
		return fmt.Errorf("WriteDockerfile: Dockerfile-Template kann nicht geparst werden: %w", err)
	}

	params := templateParams{
		BaseImage:  opts.BaseImage,
		Entrypoint: opts.Entrypoint,
	}

	f, err := os.Create(dockerfilePath)
	if err != nil {
		return fmt.Errorf("WriteDockerfile: Dockerfile '%s' kann nicht erstellt werden: %w", dockerfilePath, err)
	}
	defer f.Close()

	if err := tmpl.Execute(f, params); err != nil {
		return fmt.Errorf("WriteDockerfile: Template kann nicht ausgeführt werden: %w", err)
	}

	return nil
}
