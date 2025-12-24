package main

import (
	"embed"
	"io/fs"
	"log"
	"net/http"
	"os"

	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
)

//go:embed static/*
var staticFiles embed.FS

func main() {
	// Configuration from environment
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	apiKey := os.Getenv("API_KEY")
	if apiKey == "" {
		apiKey = "dev-key-change-me"
		log.Println("WARNING: Using default API key. Set API_KEY environment variable in production.")
	}

	dbPath := os.Getenv("DB_PATH")
	if dbPath == "" {
		dbPath = "/data/ble-scanner.db"
	}

	// Ensure data directory exists
	if err := os.MkdirAll("/data", 0755); err != nil {
		// Fallback to local directory for development
		dbPath = "./ble-scanner.db"
		log.Printf("Using local database: %s", dbPath)
	}

	// Initialize database
	if err := initDB(dbPath); err != nil {
		log.Fatalf("Failed to initialize database: %v", err)
	}

	// Initialize WebSocket hub
	hub = newHub()
	go hub.run()

	// Setup router
	r := chi.NewRouter()

	// Middleware
	r.Use(middleware.Logger)
	r.Use(middleware.Recoverer)
	r.Use(middleware.Compress(5))

	// Health check (no auth)
	r.Get("/health", HandleHealth)

	// WebSocket (no auth - dashboard uses it)
	r.Get("/ws", HandleWebSocket)

	// API routes (require API key)
	r.Route("/api", func(r chi.Router) {
		r.Use(APIKeyAuth(apiKey))

		r.Post("/logs", HandlePostLogs)
		r.Get("/logs", HandleGetLogs)
		r.Delete("/logs", HandleClearLogs)
		r.Get("/scanners", HandleGetScanners)
		r.Get("/stats", HandleGetStats)
	})

	// Static files (dashboard)
	staticFS, err := fs.Sub(staticFiles, "static")
	if err != nil {
		log.Fatal(err)
	}
	r.Handle("/*", http.FileServer(http.FS(staticFS)))

	// Start server
	log.Printf("BLE Scanner Server starting on port %s", port)
	log.Printf("Dashboard: http://localhost:%s/", port)
	log.Printf("API: http://localhost:%s/api/", port)

	if err := http.ListenAndServe(":"+port, r); err != nil {
		log.Fatal(err)
	}
}
