package main

import (
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

// StartMemTouch запускает mem_touch как внешний процесс
func StartMemTouch(rssMB, stepMB int, limitAS int) (*exec.Cmd, error) {
	cmd := exec.Command("./mem_touch",
		"--rss-mb", fmt.Sprintf("%d", rssMB),
		"--step-mb", fmt.Sprintf("%d", stepMB),
		"--sleep-ms", "200")

	if limitAS > 0 {
		cmd.Args = append(cmd.Args, "--limit-as-mb", fmt.Sprintf("%d", limitAS))
	}

	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err := cmd.Start()
	if err != nil {
		return nil, fmt.Errorf("failed to start mem_touch: %w", err)
	}

	return cmd, nil
}

// ControlMemTouch отправляет сигналы mem_touch для управления памятью
func ControlMemTouch(pid int, increase bool) error {
	sig := syscall.SIGUSR1 // increase memory
	if !increase {
		sig = syscall.SIGUSR2 // decrease memory
	}

	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %w", pid, err)
	}

	if err := process.Signal(sig); err != nil {
		return fmt.Errorf("failed to send signal to process %d: %w", pid, err)
	}

	action := "increase"
	if !increase {
		action = "decrease"
	}
	fmt.Printf("Sent signal to mem_touch (PID %d) to %s memory\n", pid, action)
	return nil
}

// StopMemTouch корректно останавливает mem_touch
func StopMemTouch(pid int) error {
	process, err := os.FindProcess(pid)
	if err != nil {
		return fmt.Errorf("failed to find process %d: %w", pid, err)
	}

	// Сначала graceful shutdown
	if err := process.Signal(syscall.SIGTERM); err != nil {
		return fmt.Errorf("failed to send SIGTERM to process %d: %w", pid, err)
	}

	fmt.Printf("Sent SIGTERM to mem_touch (PID %d)\n", pid)
	return nil
}
