package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"syscall"
)

const version = "pstat-go-1.0"

type ProcStat struct {
	Pid        int
	Comm       string
	State      string
	Ppid       int
	UTime      uint64
	STime      uint64
	NumThreads int
	RSSPages   int64
}

type ProcStatus struct {
	VmRSS                    uint64
	VmSize                   uint64
	Threads                  uint64
	VoluntaryCtxtSwitches    uint64
	NonvoluntaryCtxtSwitches uint64
}

type ProcIO struct {
	RChar      uint64
	WChar      uint64
	ReadBytes  uint64
	WriteBytes uint64
}

type SmapsRollup struct {
	Rss   uint64
	Pss   uint64
	Anon  uint64
	File  uint64
	Shmem uint64
}

func formatBytes(bytes uint64) string {
	const (
		KB = 1024
		MB = KB * 1024
		GB = MB * 1024
	)

	switch {
	case bytes >= GB:
		return fmt.Sprintf("%.2f GiB", float64(bytes)/float64(GB))
	case bytes >= MB:
		return fmt.Sprintf("%.2f MiB", float64(bytes)/float64(MB))
	case bytes >= KB:
		return fmt.Sprintf("%.1f KiB", float64(bytes)/float64(KB))
	default:
		return fmt.Sprintf("%d B", bytes)
	}
}

func readProcFile(pid int, filename string) (string, error) {
	path := filepath.Join("/proc", strconv.Itoa(pid), filename)
	content, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return string(content), nil
}

func parseProcStat(pid int) (*ProcStat, error) {
	content, err := readProcFile(pid, "stat")
	if err != nil {
		return nil, err
	}

	// Find the command name between parentheses
	start := strings.IndexByte(content, '(')
	if start == -1 {
		return nil, fmt.Errorf("invalid stat format")
	}
	end := strings.LastIndexByte(content, ')')
	if end == -1 {
		return nil, fmt.Errorf("invalid stat format")
	}

	// Extract command name and split the rest
	comm := content[start+1 : end]
	rest := strings.Fields(content[end+2:]) // Skip ") "

	// Parse fields (positions are 1-based in stat, but we have 0-based array)
	if len(rest) < 22 {
		return nil, fmt.Errorf("invalid stat format: not enough fields")
	}

	stat := &ProcStat{
		Pid:        pid,
		Comm:       comm,
		State:      rest[0],          // field 3
		Ppid:       atoi(rest[1]),    // field 4
		UTime:      atou(rest[11]),   // field 14
		STime:      atou(rest[12]),   // field 15
		NumThreads: atoi(rest[17]),   // field 20
		RSSPages:   atoi64(rest[21]), // field 24
	}

	return stat, nil
}

func parseProcStatus(pid int) (*ProcStatus, error) {
	content, err := readProcFile(pid, "status")
	if err != nil {
		return nil, err
	}

	status := &ProcStatus{}
	scanner := bufio.NewScanner(strings.NewReader(content))

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, ":", 2)
		if len(parts) != 2 {
			continue
		}

		key := strings.TrimSpace(parts[0])
		value := strings.TrimSpace(parts[1])

		switch key {
		case "VmRSS":
			status.VmRSS = parseMemoryValue(value)
		case "VmSize":
			status.VmSize = parseMemoryValue(value)
		case "Threads":
			status.Threads = atou(value)
		case "voluntary_ctxt_switches":
			status.VoluntaryCtxtSwitches = atou(value)
		case "nonvoluntary_ctxt_switches":
			status.NonvoluntaryCtxtSwitches = atou(value)
		}
	}

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return status, nil
}

func parseProcIO(pid int) (*ProcIO, error) {
	content, err := readProcFile(pid, "io")
	if err != nil {
		return nil, err
	}

	io := &ProcIO{}
	scanner := bufio.NewScanner(strings.NewReader(content))

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, ":", 2)
		if len(parts) != 2 {
			continue
		}

		key := strings.TrimSpace(parts[0])
		value := strings.TrimSpace(parts[1])

		switch key {
		case "rchar":
			io.RChar = atou(value)
		case "wchar":
			io.WChar = atou(value)
		case "read_bytes":
			io.ReadBytes = atou(value)
		case "write_bytes":
			io.WriteBytes = atou(value)
		}
	}

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return io, nil
}

func parseSmapsRollup(pid int) (*SmapsRollup, error) {
	content, err := readProcFile(pid, "smaps_rollup")
	if err != nil {
		return nil, err
	}

	smaps := &SmapsRollup{}
	scanner := bufio.NewScanner(strings.NewReader(content))

	for scanner.Scan() {
		line := scanner.Text()
		parts := strings.SplitN(line, ":", 2)
		if len(parts) != 2 {
			continue
		}

		key := strings.TrimSpace(parts[0])
		value := strings.TrimSpace(parts[1])

		switch key {
		case "Rss":
			smaps.Rss = parseMemoryValue(value)
		case "Pss":
			smaps.Pss = parseMemoryValue(value)
		case "Anonymous":
			smaps.Anon = parseMemoryValue(value)
		case "File":
			smaps.File = parseMemoryValue(value)
		case "Shared_Hugetlb":
			smaps.Shmem = parseMemoryValue(value)
		}
	}

	if err := scanner.Err(); err != nil {
		return nil, err
	}

	return smaps, nil
}

func parseMemoryValue(value string) uint64 {
	// Remove "kB" suffix and convert to bytes
	re := regexp.MustCompile(`(\d+)\s*kB`)
	matches := re.FindStringSubmatch(value)
	if len(matches) == 2 {
		return atou(matches[1]) * 1024
	}

	// If no "kB" suffix, try to parse as raw number
	fields := strings.Fields(value)
	if len(fields) > 0 {
		return atou(fields[0])
	}
	return 0
}

func atoi(s string) int {
	val, err := strconv.Atoi(s)
	if err != nil {
		return 0
	}
	return val
}

func atoi64(s string) int64 {
	val, err := strconv.ParseInt(s, 10, 64)
	if err != nil {
		return 0
	}
	return val
}

func atou(s string) uint64 {
	val, err := strconv.ParseUint(s, 10, 64)
	if err != nil {
		return 0
	}
	return val
}

func getSystemConstants() (clkTck uint64, pageSize uint64) {
	clkTck = 100 // Standard HZ value for most Linux systems
	pageSize = uint64(syscall.Getpagesize())
	return
}

func runCommand(cmd string) (string, error) {
	output, err := exec.Command("sh", "-c", cmd).CombinedOutput()
	if err != nil {
		return string(output), err
	}
	return string(output), nil
}

func runSystemComparisons(pid int) {
	commands := []struct {
		name string
		cmd  string
	}{
		{"ps", fmt.Sprintf("ps -p %d -o pid,ppid,comm,state,time,rss,vsz --no-headers", pid)},
		{"top", fmt.Sprintf("top -b -n1 -p %d 2>/dev/null | head -20", pid)},
		{"pidstat", fmt.Sprintf("pidstat -p %d 1 1 2>/dev/null || echo '(pidstat not available)'", pid)},
	}

	for _, cmd := range commands {
		fmt.Printf("\n=== %s ===\n", cmd.name)
		output, err := runCommand(cmd.cmd)
		if err != nil {
			fmt.Printf("Error: %v\n", err)
		} else {
			fmt.Print(output)
		}
	}
}

func printSummary(pid int, noCompare bool) {
	fmt.Printf("pstat-go v%s — quick snapshot for PID %d\n\n", version, pid)

	// Get system constants
	clkTck, pageSize := getSystemConstants()
	fmt.Printf("system: CLK_TCK=%d  PAGE_SIZE=%d bytes\n", clkTck, pageSize)

	// Parse proc files
	stat, err := parseProcStat(pid)
	if err != nil {
		fmt.Printf("Error reading stat: %v\n", err)
		return
	}

	status, err := parseProcStatus(pid)
	if err != nil {
		fmt.Printf("Error reading status: %v\n", err)
		return
	}

	io, err := parseProcIO(pid)
	if err != nil {
		fmt.Printf("Error reading io: %v\n", err)
	}

	smaps, err := parseSmapsRollup(pid)
	if err != nil {
		fmt.Printf("Note: smaps_rollup not available: %v\n", err)
	}

	// Calculate derived values
	cpuTimeSec := float64(stat.UTime+stat.STime) / float64(clkTck)
	rssBytes := uint64(stat.RSSPages) * pageSize

	// Print process summary
	fmt.Println("--- process summary ---")
	fmt.Printf("PID/Name : %d / %s\n", stat.Pid, stat.Comm)
	fmt.Printf("PPid     : %d    Threads: %d    State: %s\n", stat.Ppid, stat.NumThreads, stat.State)
	fmt.Printf("CPU(ticks): utime=%d stime=%d   CPU seconds=%d/%d = %.3fs\n",
		stat.UTime, stat.STime, stat.UTime+stat.STime, clkTck, cpuTimeSec)

	// Memory information
	fmt.Println("\n--- memory ---")
	fmt.Printf("Memory RSS : %d kB (%s)\n", status.VmRSS/1024, formatBytes(status.VmRSS))
	fmt.Printf("Stat RSS   : %d bytes (%s)\n", rssBytes, formatBytes(rssBytes))

	if status.VmSize > 0 {
		perc := float64(status.VmRSS) / float64(status.VmSize) * 100
		fmt.Printf("VmSize     : %d kB (%s)   RSS/VmSize = %.1f%%\n",
			status.VmSize/1024, formatBytes(status.VmSize), perc)
	}

	// Smaps information
	if smaps != nil {
		fmt.Println("\n--- smaps (rollup) ---")
		fmt.Printf("  Rss: %d kB (%s)\n", smaps.Rss/1024, formatBytes(smaps.Rss))
		fmt.Printf("  Anonymous: %d kB (%s)\n", smaps.Anon/1024, formatBytes(smaps.Anon))
		fmt.Printf("  File: %d kB (%s)\n", smaps.File/1024, formatBytes(smaps.File))
		fmt.Printf("  Shared: %d kB (%s)\n", smaps.Shmem/1024, formatBytes(smaps.Shmem))
	}

	// IO information
	if io != nil {
		fmt.Println("\n--- IO ---")
		fmt.Printf("rchar/wchar : %d / %d  (syscall bytes)\n", io.RChar, io.WChar)
		fmt.Printf("read_bytes/write_bytes: %d / %d  (%s / %s)\n",
			io.ReadBytes, io.WriteBytes, formatBytes(io.ReadBytes), formatBytes(io.WriteBytes))
	}

	// Context switches
	fmt.Println("\n--- context switches ---")
	fmt.Printf("voluntary   : %d\n", status.VoluntaryCtxtSwitches)
	fmt.Printf("non-voluntary: %d\n", status.NonvoluntaryCtxtSwitches)

	// System tools comparison
	if !noCompare {
		fmt.Println("\n--- comparison with system tools ---")
		runSystemComparisons(pid)
	}
}

func main() {
	var noCompare bool
	flag.BoolVar(&noCompare, "no-compare", false, "don't run system tool comparisons")
	flag.Usage = func() {
		fmt.Fprintf(flag.CommandLine.Output(), "Usage: %s [flags] <pid>\n", os.Args[0])
		fmt.Fprintf(flag.CommandLine.Output(), "Flags:\n")
		flag.PrintDefaults()
	}
	flag.Parse()

	if flag.NArg() != 1 {
		flag.Usage()
		os.Exit(1)
	}

	pid, err := strconv.Atoi(flag.Arg(0))
	if err != nil {
		fmt.Printf("Invalid PID: %s\n", flag.Arg(0))
		os.Exit(1)
	}

	printSummary(pid, noCompare)
}
