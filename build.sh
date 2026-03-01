#!/usr/bin/env bash
#
# build.sh — Sync shared header, then build.
#
# Usage:
#   ./build.sh                  # just build
#   ./build.sh install          # build + install to emulator (basalt)
#   ./build.sh phone 192.168.x  # build + install to phone
#

set -euo pipefail

HEADER="src/c/pulse_time.h"
WORKER_HEADER="worker_src/c/pulse_time.h"

# ---- Sync shared header ----
if [ -f "$HEADER" ]; then
  if ! cmp -s "$HEADER" "$WORKER_HEADER" 2>/dev/null; then
    cp "$HEADER" "$WORKER_HEADER"
    echo "✓ Synced $HEADER → $WORKER_HEADER"
  fi
else
  echo "⚠ $HEADER not found"
  exit 1
fi

# ---- Build ----
echo "Building..."
pebble build

# ---- Optional install ----
case "${1:-}" in
  install)
    PLATFORM="${2:-basalt}"
    echo "Installing to emulator ($PLATFORM)..."
    pebble install --emulator "$PLATFORM"
    ;;
  phone)
    IP="${2:?Usage: ./build.sh phone <IP>}"
    echo "Installing to phone ($IP)..."
    pebble install --phone "$IP"
    ;;
  logs)
    IP="${2:?Usage: ./build.sh logs <IP>}"
    pebble build
    pebble install --phone "$IP"
    pebble logs --phone "$IP"
    ;;
  "")
    echo "✓ Build complete. Run './build.sh install' or './build.sh phone <IP>' to deploy."
    ;;
  *)
    echo "Usage:"
    echo "  ./build.sh                     # build only"
    echo "  ./build.sh install [platform]  # build + emulator (default: basalt)"
    echo "  ./build.sh phone <IP>          # build + install to phone"
    echo "  ./build.sh logs <IP>           # build + install + tail logs"
    exit 1
    ;;
esac
