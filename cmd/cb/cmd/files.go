package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/fileops"
)

func newFilesCmd() *cobra.Command {
	var makeLinks, noBlacklist bool

	cmd := &cobra.Command{
		Use:   "files XMLFILE DESTDIR",
		Short: "Copy traced files to staging directory",
		Long: `Copies all files listed in an XML manifest to a staging directory,
preserving the directory structure.`,
		Example: `  cb files record.xml /tmp/myapp_staging
  cb files --links record.xml /tmp/myapp_staging`,
		Args: cobra.ExactArgs(2),
		RunE: func(cmd *cobra.Command, args []string) error {
			xmlFile := args[0]
			destDir := args[1]

			// CopyOptions konfigurieren
			opts := fileops.DefaultOptions()
			opts.MakeLinks = makeLinks
			if noBlacklist {
				opts.DefaultBlacklist = false
			}

			// Stage aufrufen
			count, err := fileops.Stage(xmlFile, destDir, opts)
			if err != nil {
				return fmt.Errorf("files: %w", err)
			}

			fmt.Printf("cb_files: %d files successfully copied\n", count)
			return nil
		},
	}

	cmd.Flags().BoolVar(&makeLinks, "links", false, "create symlinks instead of copies")
	cmd.Flags().BoolVar(&noBlacklist, "no-blacklist", false, "disable default blacklist")

	return cmd
}
