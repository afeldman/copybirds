package cmd

import (
	"os"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
)

var (
	verbosity int
	quiet     bool
	logFile   string
)

var rootCmd = &cobra.Command{
	Use:   "cb",
	Short: "Copybirds — capture, package, deploy",
	Long: `Copybirds traces a Linux program's runtime file dependencies
and packages them into a Docker image or self-extracting archive.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		v := verbosity
		if quiet {
			v = -1
		}
		log.Init(v, logFile)
	},
}

func Execute() {
	defer log.Sync()
	if err := rootCmd.Execute(); err != nil {
		os.Exit(1)
	}
}

func init() {
	rootCmd.PersistentFlags().CountVarP(
		&verbosity, "verbose", "v",
		"verbosity level (use -v, -vv, etc.)",
	)
	rootCmd.PersistentFlags().BoolVarP(&quiet, "quiet", "q", false, "quiet mode (errors only)")
	rootCmd.PersistentFlags().StringVar(&logFile, "log-file", "", "Logs zusätzlich in Datei schreiben (mit Rotation)")

	// Register all subcommands
	rootCmd.AddCommand(
		newTraceCmd(),
		newMetaCmd(),
		newMergeCmd(),
		newFilesCmd(),
		newDepsCmd(),
		newCleanCmd(),
		newDockerCmd(),
		newRunCmd(),
	)
}
