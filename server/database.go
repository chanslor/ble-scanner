package main

import (
	"database/sql"
	"log"
	"time"

	_ "github.com/mattn/go-sqlite3"
)

var db *sql.DB

func initDB(dbPath string) error {
	var err error
	db, err = sql.Open("sqlite3", dbPath+"?_journal_mode=WAL&_busy_timeout=5000")
	if err != nil {
		return err
	}

	// Create tables
	schema := `
	CREATE TABLE IF NOT EXISTS scanners (
		id TEXT PRIMARY KEY,
		name TEXT,
		last_seen DATETIME,
		ip_address TEXT,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP
	);

	CREATE TABLE IF NOT EXISTS logs (
		id INTEGER PRIMARY KEY AUTOINCREMENT,
		scanner_id TEXT NOT NULL,
		timestamp DATETIME,
		mac TEXT NOT NULL,
		name TEXT,
		rssi INTEGER,
		device_type TEXT,
		status TEXT,
		manufacturer TEXT,
		created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
		FOREIGN KEY (scanner_id) REFERENCES scanners(id)
	);

	CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp);
	CREATE INDEX IF NOT EXISTS idx_logs_scanner ON logs(scanner_id);
	CREATE INDEX IF NOT EXISTS idx_logs_mac ON logs(mac);
	CREATE INDEX IF NOT EXISTS idx_logs_created ON logs(created_at);
	`

	_, err = db.Exec(schema)
	if err != nil {
		return err
	}

	log.Println("Database initialized")
	return nil
}

// Scanner represents a BLE scanner device
type Scanner struct {
	ID        string    `json:"id"`
	Name      string    `json:"name"`
	LastSeen  time.Time `json:"last_seen"`
	IPAddress string    `json:"ip_address"`
	CreatedAt time.Time `json:"created_at"`
}

// LogEntry represents a single BLE device sighting
type LogEntry struct {
	ID           int64     `json:"id"`
	ScannerID    string    `json:"scanner_id"`
	Timestamp    time.Time `json:"timestamp"`
	MAC          string    `json:"mac"`
	Name         string    `json:"name"`
	RSSI         int       `json:"rssi"`
	DeviceType   string    `json:"device_type"`
	Status       string    `json:"status"`
	Manufacturer string    `json:"manufacturer"`
	CreatedAt    time.Time `json:"created_at"`
}

// LogBatch represents a batch of logs from a scanner
type LogBatch struct {
	ScannerID string     `json:"scanner_id"`
	Timestamp string     `json:"timestamp,omitempty"`
	Devices   []LogEntry `json:"devices"`
}

// UpsertScanner creates or updates a scanner record
func UpsertScanner(id, name, ip string) error {
	_, err := db.Exec(`
		INSERT INTO scanners (id, name, last_seen, ip_address)
		VALUES (?, ?, ?, ?)
		ON CONFLICT(id) DO UPDATE SET
			name = COALESCE(NULLIF(excluded.name, ''), scanners.name),
			last_seen = excluded.last_seen,
			ip_address = excluded.ip_address
	`, id, name, time.Now(), ip)
	return err
}

// InsertLog inserts a single log entry
func InsertLog(entry LogEntry) (int64, error) {
	result, err := db.Exec(`
		INSERT INTO logs (scanner_id, timestamp, mac, name, rssi, device_type, status, manufacturer)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?)
	`, entry.ScannerID, entry.Timestamp, entry.MAC, entry.Name, entry.RSSI, entry.DeviceType, entry.Status, entry.Manufacturer)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

// InsertLogBatch inserts multiple log entries in a transaction
func InsertLogBatch(batch LogBatch) (int, error) {
	tx, err := db.Begin()
	if err != nil {
		return 0, err
	}
	defer tx.Rollback()

	stmt, err := tx.Prepare(`
		INSERT INTO logs (scanner_id, timestamp, mac, name, rssi, device_type, status, manufacturer)
		VALUES (?, ?, ?, ?, ?, ?, ?, ?)
	`)
	if err != nil {
		return 0, err
	}
	defer stmt.Close()

	count := 0
	for _, entry := range batch.Devices {
		ts := entry.Timestamp
		if ts.IsZero() {
			ts = time.Now()
		}
		_, err := stmt.Exec(batch.ScannerID, ts, entry.MAC, entry.Name, entry.RSSI, entry.DeviceType, entry.Status, entry.Manufacturer)
		if err != nil {
			log.Printf("Error inserting log entry: %v", err)
			continue
		}
		count++
	}

	if err := tx.Commit(); err != nil {
		return 0, err
	}
	return count, nil
}

// GetRecentLogs returns the most recent log entries
func GetRecentLogs(limit int) ([]LogEntry, error) {
	rows, err := db.Query(`
		SELECT id, scanner_id, timestamp, mac, name, rssi, device_type, status, manufacturer, created_at
		FROM logs
		ORDER BY created_at DESC
		LIMIT ?
	`, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var logs []LogEntry
	for rows.Next() {
		var entry LogEntry
		err := rows.Scan(&entry.ID, &entry.ScannerID, &entry.Timestamp, &entry.MAC, &entry.Name,
			&entry.RSSI, &entry.DeviceType, &entry.Status, &entry.Manufacturer, &entry.CreatedAt)
		if err != nil {
			return nil, err
		}
		logs = append(logs, entry)
	}
	return logs, nil
}

// GetLogsByScanner returns logs for a specific scanner
func GetLogsByScanner(scannerID string, limit int) ([]LogEntry, error) {
	rows, err := db.Query(`
		SELECT id, scanner_id, timestamp, mac, name, rssi, device_type, status, manufacturer, created_at
		FROM logs
		WHERE scanner_id = ?
		ORDER BY created_at DESC
		LIMIT ?
	`, scannerID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var logs []LogEntry
	for rows.Next() {
		var entry LogEntry
		err := rows.Scan(&entry.ID, &entry.ScannerID, &entry.Timestamp, &entry.MAC, &entry.Name,
			&entry.RSSI, &entry.DeviceType, &entry.Status, &entry.Manufacturer, &entry.CreatedAt)
		if err != nil {
			return nil, err
		}
		logs = append(logs, entry)
	}
	return logs, nil
}

// GetScanners returns all registered scanners
func GetScanners() ([]Scanner, error) {
	rows, err := db.Query(`
		SELECT id, COALESCE(name, ''), last_seen, COALESCE(ip_address, ''), created_at
		FROM scanners
		ORDER BY last_seen DESC
	`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var scanners []Scanner
	for rows.Next() {
		var s Scanner
		err := rows.Scan(&s.ID, &s.Name, &s.LastSeen, &s.IPAddress, &s.CreatedAt)
		if err != nil {
			return nil, err
		}
		scanners = append(scanners, s)
	}
	return scanners, nil
}

// GetStats returns dashboard statistics
func GetStats() (map[string]interface{}, error) {
	stats := make(map[string]interface{})

	// Total logs
	var totalLogs int
	db.QueryRow("SELECT COUNT(*) FROM logs").Scan(&totalLogs)
	stats["total_logs"] = totalLogs

	// Unique devices
	var uniqueDevices int
	db.QueryRow("SELECT COUNT(DISTINCT mac) FROM logs").Scan(&uniqueDevices)
	stats["unique_devices"] = uniqueDevices

	// Active scanners (seen in last 5 minutes)
	var activeScanners int
	db.QueryRow("SELECT COUNT(*) FROM scanners WHERE last_seen > datetime('now', '-5 minutes')").Scan(&activeScanners)
	stats["active_scanners"] = activeScanners

	// Logs in last hour
	var logsLastHour int
	db.QueryRow("SELECT COUNT(*) FROM logs WHERE created_at > datetime('now', '-1 hour')").Scan(&logsLastHour)
	stats["logs_last_hour"] = logsLastHour

	return stats, nil
}

// ClearLogs deletes all logs from the database
func ClearLogs() (int64, error) {
	result, err := db.Exec("DELETE FROM logs")
	if err != nil {
		return 0, err
	}
	return result.RowsAffected()
}
