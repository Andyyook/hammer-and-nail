# API Reference

## Health Endpoint

### `GET /health`

Returns the backend service health status.

**Response:**
```json
{
  "status": "ok"
}
```

## Request ID Propagation

All HTTP responses include an `X-Request-Id` header for request correlation across distributed services.

### Behavior

| Scenario | Behavior |
|----------|----------|
| Inbound `X-Request-Id` present, valid | Propagated as-is |
| Inbound `X-Request-Id` missing | UUID v4 generated |
| Inbound `X-Request-Id` empty | UUID v4 generated |
| Inbound `X-Request-Id` > 128 chars | UUID v4 generated |

### Validation Rules

- Must be non-empty
- Maximum length: 128 characters
- Invalid or missing headers trigger automatic UUID generation

### Log Correlation

The request ID is included in all backend log messages via the `tracing` crate, enabling end-to-end request tracing across the system.
