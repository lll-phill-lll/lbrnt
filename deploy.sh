#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-5173}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Labyrinth deploy script (Ubuntu) ==="
echo "Project: $PROJECT_DIR"
echo ""

# ── 1. System dependencies ──
echo ">> Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential cmake curl >/dev/null

# ── 2. Node.js (if not installed or too old) ──
NODE_MIN=18
install_node() {
  echo ">> Installing Node.js via NodeSource..."
  curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - >/dev/null 2>&1
  sudo apt-get install -y -qq nodejs >/dev/null
}

if command -v node &>/dev/null; then
  NODE_VER=$(node -v | sed 's/v//' | cut -d. -f1)
  if [ "$NODE_VER" -lt "$NODE_MIN" ]; then
    echo "   Node.js v$NODE_VER too old (need >= $NODE_MIN)"
    install_node
  else
    echo "   Node.js $(node -v) OK"
  fi
else
  install_node
fi

# ── 3. Build C++ backend ──
echo ""
echo ">> Building C++ backend..."
mkdir -p "$PROJECT_DIR/build"
cd "$PROJECT_DIR/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" >/dev/null
make -j"$(nproc)" 2>&1 | tail -3
echo "   Binary: $PROJECT_DIR/build/labyrinth"

# ── 4. Install Node.js dependencies ──
echo ""
echo ">> Installing frontend dependencies..."
cd "$PROJECT_DIR/frontend"
npm install --omit=dev --silent 2>&1

# ── 5. Create rooms directory ──
mkdir -p "$PROJECT_DIR/rooms"

# ── 6. Stop old instance if running ──
if [ -f "$PROJECT_DIR/.labyrinth.pid" ]; then
  OLD_PID=$(cat "$PROJECT_DIR/.labyrinth.pid")
  if kill -0 "$OLD_PID" 2>/dev/null; then
    echo ""
    echo ">> Stopping previous instance (PID $OLD_PID)..."
    kill "$OLD_PID" 2>/dev/null || true
    sleep 1
  fi
  rm -f "$PROJECT_DIR/.labyrinth.pid"
fi

# ── 7. Launch ──
echo ""
echo ">> Starting server on port $PORT..."
cd "$PROJECT_DIR/frontend"
PORT="$PORT" nohup node server.js > "$PROJECT_DIR/labyrinth.log" 2>&1 &
SERVER_PID=$!
echo "$SERVER_PID" > "$PROJECT_DIR/.labyrinth.pid"

sleep 1
if kill -0 "$SERVER_PID" 2>/dev/null; then
  echo ""
  echo "============================================"
  echo "  Labyrinth is running!"
  echo "  http://$(hostname -I | awk '{print $1}'):$PORT"
  echo "  PID: $SERVER_PID"
  echo "  Log: $PROJECT_DIR/labyrinth.log"
  echo "  Stop: kill $SERVER_PID"
  echo "============================================"
else
  echo "ERROR: Server failed to start. Check labyrinth.log"
  cat "$PROJECT_DIR/labyrinth.log"
  exit 1
fi
