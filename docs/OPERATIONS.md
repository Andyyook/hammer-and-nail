# Operations Guide

## Configuration

### Environment Variables

The Rust backend supports configuration through environment variables with safe defaults for local development.

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `TOT_BACKEND_HOST` | string | `0.0.0.0` | HTTP server bind address |
| `TOT_BACKEND_PORT` | integer | `8080` | HTTP server listen port (0-65535) |
| `TOT_LOG_LEVEL` | string | `info` | Log level: trace, debug, info, warn, error |
| `TOT_ENABLE_EXPERIMENTAL` | boolean | `false` | Enable experimental features (true/false/1/0/yes/no) |

### Example

```bash
export TOT_BACKEND_HOST="127.0.0.1"
export TOT_BACKEND_PORT="9090"
export TOT_LOG_LEVEL="debug"
export TOT_ENABLE_EXPERIMENTAL="true"

cargo run
```

### Error Handling

- **Invalid port**: Returns descriptive error indicating the invalid value and valid range (0-65535)
- **Invalid boolean**: Returns descriptive error listing accepted values (true/false/1/0/yes/no)
- **Missing variables**: All variables fall back to safe defaults automatically

### TOML Config File

For advanced configuration, the backend also supports a TOML config file (default: `/etc/tent/config.toml`). See `config/mod.rs` for the full schema.

## Build

```bash
python3 build.py              # Build all modules
python3 build.py -m backend   # Build backend only
```

## Diagnostics

Each build generates encrypted diagnostics in `diagnostic/build-<commit>.logd`. See `diagnostic/build-<commit>.json` for the decrypt password.
