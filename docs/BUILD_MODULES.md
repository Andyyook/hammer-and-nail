# Build Module Reference

This document describes every module known to `build.py`.

| Module | Language | Build Command | Test Command | Directory |
|--------|----------|---------------|--------------|-----------|
| **backend** | Rust | `cargo build --release` | `cargo test` | `backend/` |
| **compliance** | Java | `./gradlew build` | `./gradlew test` | `compliance/` |
| **engine** | C++ | `cmake --build build` | `ctest` | `engine/` |
| **frailbox** | C | `make` | `make test` | `frailbox/` |
| **frontend** | TypeScript | `npm run build` | `npm test` | `frontend/` |
| **market** | Go | `go build ./...` | `go test ./...` | `market/` |

## Usage

```bash
# Build all modules
python3 build.py

# Build specific modules
python3 build.py --module backend,frontend

# Build with timing report
python3 build.py --timing

# Build with release mode
python3 build.py --release
```

## Adding a New Module

1. Add a `Module()` definition in `build.py`
2. Update this reference
3. Create the module directory with the required build system
