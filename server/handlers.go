package main

import (
	"encoding/json"
	"log"
	"net/http"
	"strconv"
	"strings"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true // Allow all origins for simplicity
	},
}

// APIKeyAuth middleware checks for valid API key
func APIKeyAuth(apiKey string) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			// Check Authorization header
			auth := r.Header.Get("Authorization")
			if auth == "" {
				// Also check X-API-Key header
				auth = r.Header.Get("X-API-Key")
			}

			// Strip "Bearer " prefix if present
			auth = strings.TrimPrefix(auth, "Bearer ")

			if auth != apiKey {
				http.Error(w, "Unauthorized", http.StatusUnauthorized)
				return
			}

			next.ServeHTTP(w, r)
		})
	}
}

// HandlePostLogs handles POST /api/logs - receive logs from scanner
func HandlePostLogs(w http.ResponseWriter, r *http.Request) {
	var batch LogBatch

	if err := json.NewDecoder(r.Body).Decode(&batch); err != nil {
		log.Printf("Error decoding log batch: %v", err)
		http.Error(w, "Invalid JSON", http.StatusBadRequest)
		return
	}

	if batch.ScannerID == "" {
		http.Error(w, "scanner_id required", http.StatusBadRequest)
		return
	}

	// Get client IP
	ip := r.Header.Get("X-Forwarded-For")
	if ip == "" {
		ip = r.RemoteAddr
	}

	// Update scanner record
	if err := UpsertScanner(batch.ScannerID, "", ip); err != nil {
		log.Printf("Error updating scanner: %v", err)
	}

	// Insert logs
	count, err := InsertLogBatch(batch)
	if err != nil {
		log.Printf("Error inserting logs: %v", err)
		http.Error(w, "Database error", http.StatusInternalServerError)
		return
	}

	log.Printf("Received %d logs from scanner %s", count, batch.ScannerID)

	// Broadcast to WebSocket clients
	if count > 0 {
		go func() {
			// Send the new logs
			BroadcastNewLogs(batch.Devices)

			// Also send updated stats
			if stats, err := GetStats(); err == nil {
				BroadcastStats(stats)
			}
		}()
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":   "ok",
		"received": count,
	})
}

// HandleGetLogs handles GET /api/logs - query logs
func HandleGetLogs(w http.ResponseWriter, r *http.Request) {
	limit := 100
	if l := r.URL.Query().Get("limit"); l != "" {
		if parsed, err := strconv.Atoi(l); err == nil && parsed > 0 {
			limit = parsed
			if limit > 1000 {
				limit = 1000
			}
		}
	}

	scannerID := r.URL.Query().Get("scanner_id")

	var logs []LogEntry
	var err error

	if scannerID != "" {
		logs, err = GetLogsByScanner(scannerID, limit)
	} else {
		logs, err = GetRecentLogs(limit)
	}

	if err != nil {
		log.Printf("Error fetching logs: %v", err)
		http.Error(w, "Database error", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(logs)
}

// HandleClearLogs handles DELETE /api/logs - clear all logs
func HandleClearLogs(w http.ResponseWriter, r *http.Request) {
	deleted, err := ClearLogs()
	if err != nil {
		log.Printf("Error clearing logs: %v", err)
		http.Error(w, "Database error", http.StatusInternalServerError)
		return
	}

	log.Printf("Cleared %d logs from database", deleted)

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status":  "ok",
		"deleted": deleted,
	})
}

// HandleGetScanners handles GET /api/scanners - list scanners
func HandleGetScanners(w http.ResponseWriter, r *http.Request) {
	scanners, err := GetScanners()
	if err != nil {
		log.Printf("Error fetching scanners: %v", err)
		http.Error(w, "Database error", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(scanners)
}

// HandleGetStats handles GET /api/stats - get dashboard stats
func HandleGetStats(w http.ResponseWriter, r *http.Request) {
	stats, err := GetStats()
	if err != nil {
		log.Printf("Error fetching stats: %v", err)
		http.Error(w, "Database error", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(stats)
}

// HandleWebSocket handles WebSocket connections for live updates
func HandleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("WebSocket upgrade error: %v", err)
		return
	}

	client := &Client{
		hub:  hub,
		conn: conn,
		send: make(chan []byte, 256),
	}

	hub.register <- client

	// Send initial data
	go func() {
		// Send recent logs
		if logs, err := GetRecentLogs(50); err == nil {
			msg := WSMessage{Type: "initial_logs", Data: logs}
			if data, err := json.Marshal(msg); err == nil {
				client.send <- data
			}
		}

		// Send stats
		if stats, err := GetStats(); err == nil {
			msg := WSMessage{Type: "stats", Data: stats}
			if data, err := json.Marshal(msg); err == nil {
				client.send <- data
			}
		}

		// Send scanners
		if scanners, err := GetScanners(); err == nil {
			msg := WSMessage{Type: "scanners", Data: scanners}
			if data, err := json.Marshal(msg); err == nil {
				client.send <- data
			}
		}
	}()

	go client.writePump()
	go client.readPump()
}

// HandleHealth handles GET /health - health check
func HandleHealth(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
}
