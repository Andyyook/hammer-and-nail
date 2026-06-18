#!/usr/bin/env bash
# ============================================================
# start-hermes.sh — Launch Hermes with TentOfTrials context
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CLAUDE_BIN="${CLAUDE_BIN:-claude}"
HERMES_CONTEXT="${SCRIPT_DIR}/CLAUDE.md"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║  Tent of Trials — Hermes Agent          ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo -e "  ${YELLOW}Repo:${NC}   Tent of Trials"
echo -e "  ${YELLOW}Agent:${NC}  Hermes (Claude Code)"
echo ""

if ! command -v "$CLAUDE_BIN" &>/dev/null; then
    echo -e "${RED}Claude Code not found. Install: npm install -g @anthropic-ai/claude-code${NC}"
    exit 1
fi

exec "$CLAUDE_BIN" "${@:-}"
