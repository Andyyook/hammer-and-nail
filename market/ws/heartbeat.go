package ws

import (
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"go.uber.org/zap"
)

// HeartbeatConfig defines the WebSocket heartbeat parameters.
type HeartbeatConfig struct {
	Interval       time.Duration
	PongWait       time.Duration
	WriteWait      time.Duration
}

// DefaultHeartbeatConfig returns sensible defaults for heartbeat settings.
func DefaultHeartbeatConfig() HeartbeatConfig {
	return HeartbeatConfig{
		Interval:  30 * time.Second,
		PongWait:  60 * time.Second,
		WriteWait: 10 * time.Second,
	}
}

// HeartbeatConfigFromEnv reads heartbeat settings from environment variables,
// falling back to defaults if not set.
func HeartbeatConfigFromEnv() HeartbeatConfig {
	cfg := DefaultHeartbeatConfig()
	// Environment var support would be added via os.Getenv in production
	return cfg
}

// HeartbeatTracker tracks connection heartbeats for the WebSocket hub.
type HeartbeatTracker struct {
	mu           sync.RWMutex
	connections  map[*Client]time.Time
	config       HeartbeatConfig
	logger       *zap.Logger
	activeCount  int64
}

// NewHeartbeatTracker creates a new heartbeat tracker.
func NewHeartbeatTracker(config HeartbeatConfig, logger *zap.Logger) *HeartbeatTracker {
	return &HeartbeatTracker{
		connections: make(map[*Client]time.Time),
		config:      config,
		logger:      logger,
	}
}

// Register adds a client to heartbeat tracking.
func (ht *HeartbeatTracker) Register(client *Client) {
	ht.mu.Lock()
	defer ht.mu.Unlock()
	ht.connections[client] = time.Now()
	ht.activeCount = int64(len(ht.connections))
}

// Unregister removes a client from heartbeat tracking.
func (ht *HeartbeatTracker) Unregister(client *Client) {
	ht.mu.Lock()
	defer ht.mu.Unlock()
	delete(ht.connections, client)
	ht.activeCount = int64(len(ht.connections))
}

// LastPong updates the last pong timestamp for a client.
func (ht *HeartbeatTracker) LastPong(client *Client) {
	ht.mu.Lock()
	defer ht.mu.Unlock()
	ht.connections[client] = time.Now()
}

// ActiveCount returns the number of tracked connections.
func (ht *HeartbeatTracker) ActiveCount() int64 {
	ht.mu.RLock()
	defer ht.mu.RUnlock()
	return ht.activeCount
}

// StartPinger launches a background goroutine that sends ping frames
// and closes idle connections that do not respond.
func (ht *HeartbeatTracker) StartPinger(stop <-chan struct{}) {
	ticker := time.NewTicker(ht.config.Interval)
	defer ticker.Stop()

	for {
		select {
		case <-stop:
			return
		case <-ticker.C:
			ht.pingAll()
		}
	}
}

func (ht *HeartbeatTracker) pingAll() {
	ht.mu.RLock()
	defer ht.mu.RUnlock()

	now := time.Now()
	for client, lastPong := range ht.connections {
		if now.Sub(lastPong) > ht.config.PongWait {
			ht.logger.Warn("closing idle WebSocket connection",
				zap.String("remote", client.remote),
				zap.Duration("idle", now.Sub(lastPong)),
			)
			client.conn.Close()
			continue
		}

		if err := client.conn.WriteControl(websocket.PingMessage, []byte{}, time.Now().Add(ht.config.WriteWait)); err != nil {
			ht.logger.Warn("ping failed, closing connection",
				zap.String("remote", client.remote),
				zap.Error(err),
			)
			client.conn.Close()
		}
	}
}
