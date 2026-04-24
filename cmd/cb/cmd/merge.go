package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
	"copybirds/internal/meta"
)

func newMergeCmd() *cobra.Command {
	cmd := &cobra.Command{
		Use:   "merge INPUT1.xml INPUT2.xml [...] OUTPUT.xml",
		Short: "Merge multiple XML manifests",
		Long:  `Merges multiple XML metadata files into a single combined manifest.`,
		Example: `  cb merge trace1.xml trace2.xml merged.xml
  cb merge -v result1.xml result2.xml final.xml`,
		Args: cobra.MinimumNArgs(3),
		RunE: func(cmd *cobra.Command, args []string) error {
			// Letztes Argument ist die Output-Datei
			outputFile := args[len(args)-1]
			inputFiles := args[:len(args)-1]

			log.Warnf("merge: parsing and merging %d XML input file(s)...", len(inputFiles))

			// Alle Input-Dateien laden
			var infos []*meta.SysInfo
			for _, inputFile := range inputFiles {
				info, err := meta.ReadXML(inputFile)
				if err != nil {
					return fmt.Errorf("error reading '%s': %w", inputFile, err)
				}
				infos = append(infos, info)
			}

			// Merge durchführen
			result := meta.Merge(infos)

			// Ergebnis schreiben
			log.Warnf("merge: writing result to '%s'...", outputFile)
			if err := meta.WriteXML(result, outputFile); err != nil {
				return fmt.Errorf("error writing '%s': %w", outputFile, err)
			}

			log.Warnf("merge: done")
			return nil
		},
	}

	return cmd
}
