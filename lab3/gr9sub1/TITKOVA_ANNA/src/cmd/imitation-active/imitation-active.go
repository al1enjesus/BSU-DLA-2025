package main

import (
	"fmt"
	"os"
	"sync"
	"time"
	"runtime/debug"
)

func main() {
    debug.SetMemoryLimit(100 * 1024 * 1024) // 100MB
    fmt.Println("=== ACTIVE IMITATION PROCESS ===")
    fmt.Printf("PID: %d\n", os.Getpid())

    var wg sync.WaitGroup
    wg.Add(2)

    // Горутина 1: Периодическая CPU нагрузка
    go func() {
        defer wg.Done()
        fmt.Println("Starting CPU workload...")
        for i := 0; i < 5; i++ {  // ← Уменьшить с 30 до 5
            _ = fibonacci(1000)
            time.Sleep(1 * time.Second)
        }
    }()

    // Горутина 2: Периодическая IO нагрузка
    go func() {
        defer wg.Done()
        fmt.Println("Starting IO workload...")
        for i := 0; i < 2; i++ {  // ← Уменьшить с 5 до 2
            path := fmt.Sprintf("test_file_%d.bin", i)
            writeSync(path, 1)  // ← Уменьшить с 5MB до 1MB
            readBack(path)
            os.Remove(path)
            time.Sleep(2 * time.Second)  // ← Уменьшить паузу
        }
    }()

    wg.Wait()
    fmt.Println("Active imitation completed")
}

// Функция для создания CPU нагрузки
func fibonacci(n int) int {
	if n <= 1 {
		return n
	}
	return fibonacci(n-1) + fibonacci(n-2)
}

func writeSync(path string, megabytes int) int64 {
	// Create file with sync flags
	file, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_TRUNC|os.O_SYNC, 0644)
	if err != nil {
		panic(err)
	}
	defer file.Close()

	// Write data in 1MB chunks
	buf := make([]byte, 1024*1024) // 1MB
	for i := range buf {
		buf[i] = '0'
	}

	var total int64
	for i := 0; i < megabytes; i++ {
		n, err := file.Write(buf)
		if err != nil {
			panic(err)
		}
		total += int64(n)
	}

	// Sync to disk
	err = file.Sync()
	if err != nil {
		panic(err)
	}

	return total
}

func readBack(path string) int64 {
	file, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	defer file.Close()

	buf := make([]byte, 4*1024*1024) // 4MB chunks
	var total int64

	for {
		n, err := file.Read(buf)
		if err != nil {
			break
		}
		total += int64(n)
	}

	return total
}
