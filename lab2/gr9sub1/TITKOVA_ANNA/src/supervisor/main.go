package main

import (
	"flag"
	"log"
	"os"
)

func main() {
	// Парсинг аргументов командной строки
	configPath := flag.String("config", "config.json", "Path to config file")
	workers := flag.Int("workers", 3, "Number of workers")
	flag.Parse()

	// Создаем супервизор
	supervisor, err := NewSupervisor(*configPath, *workers)
	if err != nil {
		log.Fatalf("Failed to create supervisor: %v", err)
	}

	log.Printf("Starting supervisor with config: %s, workers: %d", *configPath, *workers)

	// Запускаем супервизор
	if err := supervisor.Start(); err != nil {
		log.Fatalf("Supervisor failed: %v", err)
	}

	log.Println("Supervisor stopped gracefully")
	os.Exit(0)
}
