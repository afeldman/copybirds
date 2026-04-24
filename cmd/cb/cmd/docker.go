package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/docker"
	"copybirds/internal/log"
)

func newDockerCmd() *cobra.Command {
	var baseImage, tag, customDockerfile, metaFile string

	cmd := &cobra.Command{
		Use:   "docker STAGING_DIR ENTRYPOINT_BINARY",
		Short: "Build Docker image from staging directory",
		Long: `Builds a Docker image from a staging directory (created by 'cb files').

When --meta is provided, cb reads the system metadata and:
  - Detects the correct Debian base image from the source distro
  - Installs packaged libraries via apt-get (standard repo + snapshot.debian.org)
  - Copies only non-packaged files from staging

When --dockerfile is provided, the custom Dockerfile is used as-is and
non-packaged files are injected via a COPY instruction at the end.`,
		Example: `  cb docker /tmp/staging usr/bin/myapp
  cb docker --meta myapp_meta.xml /tmp/staging usr/bin/myapp
  cb docker --dockerfile my.Dockerfile /tmp/staging usr/bin/myapp`,
		Args: cobra.ExactArgs(2),
		RunE: func(cmd *cobra.Command, args []string) error {
			stagingDir := args[0]
			entrypoint := args[1]

			opts := docker.DefaultOptions()
			opts.BaseImage = baseImage
			opts.Tag = tag
			opts.StagingDir = stagingDir
			opts.Entrypoint = entrypoint
			opts.AlpineWarning = true
			opts.CustomDockerfile = customDockerfile
			opts.MetaFile = metaFile

			log.Infof("cb_docker: Staging=%s Entrypoint=%s Base=%s Tag=%s",
				opts.StagingDir, opts.Entrypoint, opts.BaseImage, opts.Tag)

			if err := docker.Build(opts); err != nil {
				return fmt.Errorf("docker: %w", err)
			}

			log.Warnf("cb_docker: Docker image '%s' erfolgreich erstellt", opts.Tag)
			return nil
		},
	}

	cmd.Flags().StringVar(&baseImage, "base", "debian:bookworm-slim", "base Docker image (wird durch --meta überschrieben)")
	cmd.Flags().StringVar(&tag, "tag", "cb_app", "image tag")
	cmd.Flags().StringVar(&customDockerfile, "dockerfile", "", "eigenes Dockerfile (non-packaged files werden automatisch injiziert)")
	cmd.Flags().StringVar(&metaFile, "meta", "", "Pfad zur meta.xml für apt-basiertes Package-Management")

	return cmd
}
