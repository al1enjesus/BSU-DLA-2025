package main

import (
	"fmt"
	"os"
	"sync"
	"time"
)

func main() {
	fmt.Println("worker: start")

	var wg sync.WaitGroup
	wg.Add(1)

	go func() {
		defer wg.Done()
		work()
	}()

	wg.Wait()
	fmt.Println("worker: done")
}

func work() {
	// Create test file in current directory
	path := "./pstat_io_test.bin"

	fmt.Printf("writing sync to %s\n", path)
	written, err := writeSync(path, 25)
	if err != nil {
		fmt.Printf("write failed: %v\n", err)
		return
	}
	fmt.Printf("written bytes: %d\n", written)

	fmt.Println("reading back")
	read, err := readBack(path)
	if err != nil {
		fmt.Printf("read failed: %v\n", err)
		return
	}
	fmt.Printf("read bytes: %d\n", read)

	fmt.Println("sleep 30s")
	time.Sleep(30 * time.Second)

	err = os.Remove(path)
	if err != nil {
		fmt.Printf("cleanup failed: %v\n", err)
	} else {
		fmt.Printf("removed %s\n", path)
	}
}

func writeSync(path string, megabytes int) (int64, error) {
	file, err := os.OpenFile(path, os.O_WRONLY|os.O_CREATE|os.O_TRUNC|os.O_SYNC, 0644)
	if err != nil {
		return 0, err
	}
	defer file.Close()

	buf := make([]byte, 1024*1024)
	for i := range buf {
		buf[i] = '0'
	}

	var total int64
	for i := 0; i < megabytes; i++ {
		n, err := file.Write(buf)
		if err != nil {
			return total, err
		}
		total += int64(n)
	}

	err = file.Sync()
	return total, err
}

func readBack(path string) (int64, error) {
	file, err := os.Open(path)
	if err != nil {
		return 0, err
	}
	defer file.Close()

	buf := make([]byte, 4*1024*1024)
	var total int64

	for {
		n, err := file.Read(buf)
		if err != nil {
			break
		}
		total += int64(n)
	}

	return total, nil
}
