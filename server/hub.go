package main

import (
	"encoding/json"
	"log"
	"sync"

	"github.com/gorilla/websocket"
)

// Hub maintains active WebSocket connections and broadcasts messages
type Hub struct {
	clients    map[*Client]bool
	broadcast  chan []byte
	register   chan *Client
	unregister chan *Client
	mu         sync.RWMutex
}

// Client represents a WebSocket connection
type Client struct {
	hub  *Hub
	conn *websocket.Conn
	send chan []byte
}

// WSMessage is the structure for WebSocket messages
type WSMessage struct {
	Type string      `json:"type"`
	Data interface{} `json:"data"`
}

var hub *Hub

func newHub() *Hub {
	return &Hub{
		clients:    make(map[*Client]bool),
		broadcast:  make(chan []byte, 256),
		register:   make(chan *Client),
		unregister: make(chan *Client),
	}
}

func (h *Hub) run() {
	for {
		select {
		case client := <-h.register:
			h.mu.Lock()
			h.clients[client] = true
			h.mu.Unlock()
			log.Printf("WebSocket client connected. Total: %d", len(h.clients))

		case client := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.send)
			}
			h.mu.Unlock()
			log.Printf("WebSocket client disconnected. Total: %d", len(h.clients))

		case message := <-h.broadcast:
			h.mu.RLock()
			for client := range h.clients {
				select {
				case client.send <- message:
				default:
					close(client.send)
					delete(h.clients, client)
				}
			}
			h.mu.RUnlock()
		}
	}
}

func (c *Client) writePump() {
	defer func() {
		c.conn.Close()
	}()

	for message := range c.send {
		if err := c.conn.WriteMessage(websocket.TextMessage, message); err != nil {
			return
		}
	}
}

func (c *Client) readPump() {
	defer func() {
		c.hub.unregister <- c
		c.conn.Close()
	}()

	for {
		_, _, err := c.conn.ReadMessage()
		if err != nil {
			break
		}
		// We don't process incoming messages, just keep connection alive
	}
}

// BroadcastNewLogs sends new log entries to all connected clients
func BroadcastNewLogs(logs []LogEntry) {
	if hub == nil {
		return
	}

	msg := WSMessage{
		Type: "new_logs",
		Data: logs,
	}

	data, err := json.Marshal(msg)
	if err != nil {
		log.Printf("Error marshaling broadcast: %v", err)
		return
	}

	select {
	case hub.broadcast <- data:
	default:
		log.Println("Broadcast channel full, dropping message")
	}
}

// BroadcastStats sends updated stats to all clients
func BroadcastStats(stats map[string]interface{}) {
	if hub == nil {
		return
	}

	msg := WSMessage{
		Type: "stats",
		Data: stats,
	}

	data, err := json.Marshal(msg)
	if err != nil {
		log.Printf("Error marshaling stats: %v", err)
		return
	}

	select {
	case hub.broadcast <- data:
	default:
	}
}
