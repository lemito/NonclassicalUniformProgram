package main

import (
	"embed"
	"flag"
	"fmt"
	"io/fs"
	"log"
	"net/http"
)

//go:embed web
var webFS embed.FS

func main() {
	addr := flag.String("addr", ":8080", "HTTP listen address")
	logPath := flag.String("log", "fuzzy.log", "journal file path")
	flag.Parse()

	if !LogOpen(*logPath) {
		log.Fatalf("cannot open journal %s", *logPath)
	}
	defer LogClose()
	LogMessage(fmt.Sprintf("server start, libfuzzy %s, listen %s", Version(), *addr))

	srv := NewServer(*logPath)
	mux := http.NewServeMux()
	srv.Routes(mux)
	static, _ := fs.Sub(webFS, "web")
	mux.Handle("/", http.FileServer(http.FS(static)))

	log.Printf("fuzzy server on http://localhost%s (journal: %s)", *addr, *logPath)
	log.Fatal(http.ListenAndServe(*addr, mux))
}
