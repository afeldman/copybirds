package cmd

import (
	"fmt"

	"github.com/spf13/cobra"

	"copybirds/internal/log"
	"copybirds/internal/meta"
)

// Modus-Konstanten (wie in copymeta.c)
const (
	modeCompare          = iota // default: vergleichen
	modeUpdate                  // --update
	modePrintPackages           // --print-packages
	modePrintComment            // --print-comment
	modePrintUserHome           // --print-userhome
	modePrintCommands           // --print-commands
	modePrintConnections        // --print-connections
)

func newMetaCmd() *cobra.Command {
	var mode int
	var commandFile string
	var xmlFile string

	cmd := &cobra.Command{
		Use:   "meta [XMLFILE]",
		Short: "Collect or compare system metadata",
		Long: `Collects system metadata (packages, kernel, distro info) and writes it to an XML file,
or compares current system state with a previously saved XML file.
Also supports printing specific metadata fields.`,
		Example: `  cb meta                     # compare with record.xml (default)
  cb meta --update             # update record.xml
  cb meta --update sysinfo.xml # update specific file
  cb meta --print-packages     # list packages from XML
  cb meta --compare sysinfo.xml`,
		Args: cobra.MaximumNArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			// Determine XML filename
			if len(args) == 1 {
				xmlFile = args[0]
			} else if xmlFile == "" {
				xmlFile = "record.xml"
			}

			switch mode {
			case modeCompare:
				return runCompare(xmlFile, commandFile)
			case modeUpdate:
				return runUpdate(xmlFile, commandFile)
			case modePrintPackages:
				return runPrintPackages(xmlFile)
			case modePrintComment:
				return runPrintComment(xmlFile)
			case modePrintUserHome:
				return runPrintUserHome(xmlFile)
			case modePrintCommands:
				return runPrintCommands(xmlFile)
			case modePrintConnections:
				return runPrintConnections(xmlFile)
			default:
				return runCompare(xmlFile, commandFile)
			}
		},
	}

	// All modes as flags
	cmd.Flags().Bool("update", false, "update existing XML file with current system info")
	cmd.Flags().Bool("compare", false, "compare current system with XML file (default)")
	cmd.Flags().Bool("print-packages", false, "print package list from XML file")
	cmd.Flags().Bool("print-comment", false, "print system comment from XML file")
	cmd.Flags().Bool("print-userhome", false, "print user home directory from XML file")
	cmd.Flags().Bool("print-commands", false, "print recorded commands from XML file")
	cmd.Flags().Bool("print-connections", false, "print network connections from XML file")
	cmd.Flags().StringVarP(&commandFile, "command-file", "c", "", "file with executed commands")

	// Mark the flags as mutually exclusive via a pre-run check
	cmd.PreRunE = func(cmd *cobra.Command, args []string) error {
		modeFlags := []string{"update", "compare", "print-packages", "print-comment",
			"print-userhome", "print-commands", "print-connections"}
		activeMode := ""
		for _, f := range modeFlags {
			v, _ := cmd.Flags().GetBool(f)
			if v {
				if activeMode != "" {
					return fmt.Errorf("flags --%s and --%s are mutually exclusive", activeMode, f)
				}
				switch f {
				case "update":
					mode = modeUpdate
				case "compare":
					mode = modeCompare
				case "print-packages":
					mode = modePrintPackages
				case "print-comment":
					mode = modePrintComment
				case "print-userhome":
					mode = modePrintUserHome
				case "print-commands":
					mode = modePrintCommands
				case "print-connections":
					mode = modePrintConnections
				}
				activeMode = f
			}
		}
		// default: compare
		if activeMode == "" {
			mode = modeCompare
		}
		return nil
	}

	return cmd
}

// runCompare sammelt aktuelle Systeminfo und vergleicht mit gespeicherter
func runCompare(xmlfilename, commandFile string) error {
	current, err := meta.Collect(commandFile)
	if err != nil {
		return fmt.Errorf("error collecting system info: %w", err)
	}

	stored, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}

	compatible := meta.Compare(current, stored)
	if compatible {
		log.Warnf("System is compatible.")
	} else {
		log.Warnf("System is NOT compatible (critical differences found).")
	}
	return nil
}

// runUpdate sammelt aktuelle Systeminfo und schreibt sie in die XML-Datei
func runUpdate(xmlfilename, commandFile string) error {
	current, err := meta.Collect(commandFile)
	if err != nil {
		return fmt.Errorf("error collecting system info: %w", err)
	}

	if err := meta.WriteXML(current, xmlfilename); err != nil {
		return fmt.Errorf("error writing '%s': %w", xmlfilename, err)
	}
	log.Warnf("meta: updated '%s'", xmlfilename)
	return nil
}

// runPrintPackages gibt alle Pakete aus der XML-Datei aus
func runPrintPackages(xmlfilename string) error {
	info, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}
	if len(info.Packages) == 0 {
		log.Warnf("print-packages: no package information found")
		return nil
	}
	for _, p := range info.Packages {
		fmt.Printf("%s ", p.Name)
	}
	fmt.Println()
	return nil
}

// runPrintComment gibt einen Kommentar aus
func runPrintComment(xmlfilename string) error {
	info, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}
	fmt.Printf("System-Metadaten von Distro '%s', Kernel '%s'\n", info.Distro, info.Kernel)
	return nil
}

// runPrintUserHome gibt das Home-Verzeichnis aus der XML-Datei aus
func runPrintUserHome(xmlfilename string) error {
	info, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}
	if info.UserHome == "" {
		return fmt.Errorf("no home directory set in '%s'", xmlfilename)
	}
	fmt.Printf("%s\n", info.UserHome)
	return nil
}

// runPrintCommands gibt alle Befehle aus der XML-Datei aus
func runPrintCommands(xmlfilename string) error {
	info, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}
	log.Warnf("print-commands: showing all recorded commands")
	if len(info.Commands) == 0 {
		log.Warnf("print-commands: warning: no command information found")
		return nil
	}
	for _, c := range info.Commands {
		fmt.Printf("%s\n", c)
	}
	return nil
}

// runPrintConnections gibt alle Verbindungen aus der XML-Datei aus
func runPrintConnections(xmlfilename string) error {
	info, err := meta.ReadXML(xmlfilename)
	if err != nil {
		return fmt.Errorf("error reading '%s': %w", xmlfilename, err)
	}
	if err != nil {
		return fmt.Errorf("no connection information found in '%s'", xmlfilename)
	}
	log.Warnf("print-connections: the archived application made the following connections:")
	for _, c := range info.Connections {
		fmt.Printf("Connection: %s\n", c)
	}
	return nil
}
