package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/docker"
	"copybirds/internal/log"
)

func newDockerCmd() *cobra.Command {
	var baseImage, tag string

	cmd := &cobra.Command{
		Use:   "docker STAGING_DIR ENTRYPOINT_BINARY",
		Short: "Build Docker image from staging directory",
		Long: `Builds a Docker image from a staging directory (created by 'cb files')
with the given entrypoint binary.`,
		Example: `  cb docker /tmp/myapp_staging usr/bin/myapp
  cb docker --base alpine --tag myapp:latest /tmp/myapp_staging usr/bin/myapp`,
		Args: cobra.ExactArgs(2),
		RunE: func(cmd *cobra.Command, args []string) error {
			stagingDir := args[0]
			entrypoint := args[1]

			// BuildOptions konfigurieren
			opts := docker.DefaultOptions()
			opts.BaseImage = baseImage
			opts.Tag = tag
			opts.StagingDir = stagingDir
			opts.Entrypoint = entrypoint
			opts.AlpineWarning = true

			log.Infof("cb_docker: Configuration:")
			log.Infof("  Staging directory: %s", opts.StagingDir)
			log.Infof("  Entrypoint:        %s", opts.Entrypoint)
			log.Infof("  Base image:        %s", opts.BaseImage)
			log.Infof("  Tag:               %s", opts.Tag)

			// Docker-Build ausführen
			if err := docker.Build(opts); err != nil {
				return fmt.Errorf("docker: %w", err)
			}

			log.Warnf("cb_docker: Docker image '%s' successfully created", opts.Tag)
			return nil
		},
	}
	cmd.Flags().StringVar(&baseImage, "base", "debian:bookworm-slim", "base Docker image")
	cmd.Flags().StringVar(&tag, "tag", "cb_app", "image tag")

	return cmd
}
