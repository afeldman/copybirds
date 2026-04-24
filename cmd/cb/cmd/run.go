package cmd

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/spf13/cobra"

	"copybirds/internal/docker"
	"copybirds/internal/fileops"
	"copybirds/internal/log"
	"copybirds/internal/meta"
	"copybirds/internal/strace"
)

func newRunCmd() *cobra.Command {
	var outputFile, straceFile string
	var baseImage, tag string
	var customDockerfile string
	var noDocker bool

	cmd := &cobra.Command{
		Use:   "run PROGRAM [ARGS...]",
		Short: "Interactively trace a program and package it",
		Long: `Runs a program under strace, captures all file accesses,
copies the traced files to a staging directory, optionally collects
system metadata, and builds a Docker image — all in one step.`,
		Example: `  cb run /usr/bin/xclock
  cb run --tag myapp:latest /usr/bin/myapp --flag arg
  cb run --no-docker /usr/bin/ls`,
		Args: cobra.MinimumNArgs(1),
		RunE: func(cmd *cobra.Command, args []string) error {
			program := args[0]
			programArgs := args[1:]
			programName := filepath.Base(program)

			// Default output names based on program name
			if outputFile == "" {
				outputFile = programName + "_record.xml"
			}
			if straceFile == "" {
				straceFile = programName + "_strace.log"
			}
			stagingDir := programName + "_staging"

			// Step 1: Run strace
			log.Warnf("run: tracing '%s'...", program)
			pid, err := runStrace(program, programArgs, straceFile)
			if err != nil {
				return fmt.Errorf("run: strace failed: %w", err)
			}

			// Wait for the process to complete
			err = waitForProcess(pid)
			if err != nil {
				return fmt.Errorf("run: process execution failed: %w", err)
			}

			// Step 2: Parse strace output to XML
			log.Warnf("run: parsing strace output...")
			result, err := strace.ParseFile(straceFile, false)
			if err != nil {
				return fmt.Errorf("run: failed to parse strace output: %w", err)
			}

			err = strace.WriteXML(result, outputFile)
			if err != nil {
				return fmt.Errorf("run: failed to write XML: %w", err)
			}
			log.Warnf("run: wrote XML manifest to '%s'", outputFile)

			// Step 3: Copy files to staging
			log.Warnf("run: copying files to staging directory '%s'...", stagingDir)
			opts := fileops.DefaultOptions()
			count, err := fileops.Stage(outputFile, stagingDir, opts)
			if err != nil {
				return fmt.Errorf("run: file staging failed: %w", err)
			}
			log.Warnf("run: %d files copied to staging", count)

			// Step 4: Collect system metadata
 			metaFile := programName + "_meta.xml"
			log.Warnf("run: collecting system metadata...")
			sysInfo, err := meta.Collect("")
			if err != nil {
				log.Warnf("run: warning: failed to collect metadata: %v", err)
			} else {
				// Datei → Paket Zuordnung via dpkg -S
				filePackages, fpErr := meta.CollectFilePackages(outputFile)
				if fpErr != nil {
					log.Warnf("run: warning: CollectFilePackages fehlgeschlagen: %v", fpErr)
				} else {
					sysInfo.FilePackages = filePackages
					log.Infof("run: %d Dateien einem Paket zugeordnet", len(filePackages))
				}

				if err := meta.WriteXML(sysInfo, metaFile); err != nil {
					log.Warnf("run: warning: failed to write metadata: %v", err)
				} else {
					log.Warnf("run: metadata written to '%s'", metaFile)
				}
			}

			// Step 5: Build Docker image (optional)
			if !noDocker {
				log.Warnf("run: building Docker image...")
				dockerOpts := docker.DefaultOptions()
				dockerOpts.BaseImage = baseImage
				dockerOpts.Tag = tag
				dockerOpts.StagingDir = stagingDir
				dockerOpts.Entrypoint = program
				dockerOpts.AlpineWarning = true
				dockerOpts.MetaFile = metaFile
				dockerOpts.CustomDockerfile = customDockerfile

				if err := docker.Build(dockerOpts); err != nil {
					return fmt.Errorf("run: Docker build failed: %w", err)
				}
				log.Warnf("run: Docker image '%s' successfully created", dockerOpts.Tag)
			}

			log.Warnf("run: done")
			log.Warnf("run: artifacts:")
			log.Warnf("  - XML manifest: %s", outputFile)
			log.Warnf("  - Staging dir:  %s", stagingDir)
			if !noDocker {
				log.Warnf("  - Docker image: %s", tag)
			}

			return nil
		},
	}

	cmd.Flags().StringVarP(&outputFile, "output", "o", "", "XML output file (default: <program>_record.xml)")
	cmd.Flags().StringVarP(&straceFile, "strace-file", "s", "", "strace output file (default: <program>_strace.log)")
	cmd.Flags().StringVar(&baseImage, "base", "debian:bookworm-slim", "base Docker image")
	cmd.Flags().StringVar(&tag, "tag", "cb_app", "Docker image tag")
	cmd.Flags().BoolVar(&noDocker, "no-docker", false, "skip Docker image build")
	cmd.Flags().StringVar(&customDockerfile, "dockerfile", "", "eigenes Dockerfile für den Docker-Build")

	return cmd
}

// runStrace startet strace für das gegebene Programm
func runStrace(program string, args []string, outputFile string) (int, error) {
	straceArgs := []string{
		"-f",             // follow forks
		"-o", outputFile, // output file
		"-e", "trace=file,network,process", // trace file, network, process syscalls
		program,
	}
	straceArgs = append(straceArgs, args...)

	cmd := exec.Command("strace", straceArgs...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin

	err := cmd.Start()
	if err != nil {
		return 0, fmt.Errorf("failed to start strace: %w", err)
	}

	return cmd.Process.Pid, nil
}

// waitForProcess wartet auf einen Prozess mit gegebener PID
func waitForProcess(pid int) error {
	// Find the process and wait for it
	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %w", pid, err)
	}

	state, err := process.Wait()
	if err != nil {
		return fmt.Errorf("process %d wait failed: %w", pid, err)
	}

	if !state.Success() {
		return fmt.Errorf("process exited with status %d", state.ExitCode())
	}

	return nil
}

// waitForProcessByPID wartet auf einen Prozess mittels kill -0 polling
// (Fallback falls os.FindProcess nicht funktioniert)
func waitForProcessByPID(pidStr string) error {
	_, err := strconv.Atoi(pidStr)
	if err != nil {
		return fmt.Errorf("invalid PID '%s': %w", pidStr, err)
	}

	return nil
}

// isPIDsRunning prüft ob ein Prozess noch läuft
func isPIDsRunning(pid int) bool {
	process, err := os.FindProcess(pid)
	if err != nil {
		return false
	}

	// Auf Unix: Signal 0 prüft ob Prozess existiert
	return process.Signal(os.Signal(nil)) == nil
}

// getStracePID extrahiert die PID aus dem strace-Output
func getStracePID(output string) string {
	lines := strings.Split(strings.TrimSpace(output), "\n")
	if len(lines) > 0 {
		return strings.TrimSpace(lines[len(lines)-1])
	}
	return ""
}
