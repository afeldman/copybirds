package cmd

import (
	"os"
	"strings"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
	"copybirds/internal/strace"
)

// readWorkDir reads the initial working directory from a file
func readWorkDir(path string) string {
	data, err := os.ReadFile(path)
	if err != nil {
		log.Warnf("trace: warning: could not read workdir file '%s': %v", path, err)
		return ""
	}
	dir := strings.TrimSpace(string(data))
	dir = strings.TrimRight(dir, "\n\r")
	if dir == "" {
		return ""
	}
	return dir
}

func newTraceCmd() *cobra.Command {
	var straceFile, outputFile, wdirFile string
	var ignorePIDs bool

	cmd := &cobra.Command{
		Use:   "trace",
		Short: "Convert strace output to XML manifest",
		Long:  `Parses a strace log file and writes a structured XML file with all file accesses.`,
		RunE: func(cmd *cobra.Command, args []string) error {
			result, err := strace.ParseFile(straceFile, ignorePIDs)
			if err != nil {
				return err
			}

			// If working directory file was provided, read and set it
			if wdirFile != "" {
				workDir := readWorkDir(wdirFile)
				if workDir != "" {
					result.WorkDir = workDir
				}
			}

			return strace.WriteXML(result, outputFile)
		},
	}

	cmd.Flags().StringVarP(&straceFile, "strace-file", "s", "strace_temp", "strace output file")
	cmd.Flags().StringVarP(&outputFile, "output", "o", "record.xml", "XML output file")
	cmd.Flags().StringVarP(&wdirFile, "workdir-file", "p", "", "file containing working directory")
	cmd.Flags().BoolVar(&ignorePIDs, "no-pids", false, "treat all PIDs as 1")

	return cmd
}
