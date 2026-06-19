package gateway

import (
	"encoding/json"
	"net/http"
	"sync"
	"time"
)

// RateLimiter implements IP-based rate limiting for the market gateway.
type RateLimiter struct {
	mu       sync.Mutex
	clients  map[string]*rateLimitEntry
	limit    int
	burst    int
	interval time.Duration
}

type rateLimitEntry struct {
	count     int
	windowStart time.Time
}

type rateLimitResponse struct {
	Error   string `json:"error"`
	Message string `json:"message"`
	RetryAfter int `json:"retry_after_seconds"`
}

// NewRateLimiter creates a rate limiter with the given limit per minute and burst.
func NewRateLimiter(limit, burst int) *RateLimiter {
	return &RateLimiter{
		clients:  make(map[string]*rateLimitEntry),
		limit:    limit,
		burst:    burst,
		interval: time.Minute,
	}
}

// Middleware returns an HTTP middleware that enforces rate limits per client IP.
func (rl *RateLimiter) Middleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ip := r.RemoteAddr
		if forwarded := r.Header.Get("X-Forwarded-For"); forwarded != "" {
			ip = forwarded
		}

		allowed, remaining := rl.allow(ip)
		w.Header().Set("X-RateLimit-Limit", rl.limit)
		w.Header().Set("X-RateLimit-Remaining", remaining)

		if !allowed {
			retryAfter := int(rl.interval.Seconds())
			w.Header().Set("Retry-After", retryAfter)
			w.Header().Set("Content-Type", "application/json")
			w.WriteHeader(http.StatusTooManyRequests)
			json.NewEncoder(w).Encode(rateLimitResponse{
				Error:   "rate_limit_exceeded",
				Message: "too many requests, please slow down",
				RetryAfter: retryAfter,
			})
			return
		}

		next.ServeHTTP(w, r)
	})
}

func (rl *RateLimiter) allow(ip string) (bool, string) {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	now := time.Now()
	entry, exists := rl.clients[ip]

	if !exists || now.Sub(entry.windowStart) > rl.interval {
		rl.clients[ip] = &rateLimitEntry{count: 1, windowStart: now}
		return true, itoa(rl.limit - 1)
	}

	entry.count++
	if entry.count > rl.limit {
		return false, "0"
	}

	return true, itoa(rl.limit - entry.count)
}

func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	s := ""
	for n > 0 {
		s = string(rune('0'+n%10)) + s
		n /= 10
	}
	return s
}
