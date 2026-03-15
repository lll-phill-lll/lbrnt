#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-5173}"
DOMAIN="${DOMAIN:-_}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Labyrinth deploy script (Ubuntu) ==="
echo "Project: $PROJECT_DIR"
echo ""

# ── 1. System dependencies ──
echo ">> Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential cmake curl nginx >/dev/null

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
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" >/dev/null 2>&1
if ! make -j"$(nproc)" 2>&1; then
  echo "ERROR: C++ build failed (see errors above)"
  exit 1
fi
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

# ── 7. Launch Node.js server ──
echo ""
echo ">> Starting server on port $PORT..."
cd "$PROJECT_DIR/frontend"
PORT="$PORT" nohup node server.js > "$PROJECT_DIR/labyrinth.log" 2>&1 &
SERVER_PID=$!
echo "$SERVER_PID" > "$PROJECT_DIR/.labyrinth.pid"

sleep 1
if ! kill -0 "$SERVER_PID" 2>/dev/null; then
  echo "ERROR: Server failed to start. Check labyrinth.log"
  cat "$PROJECT_DIR/labyrinth.log"
  exit 1
fi

# ── 8. Configure nginx reverse proxy ──
echo ""
echo ">> Configuring nginx..."

NGINX_CONF="/etc/nginx/sites-available/labyrinth"
sudo tee "$NGINX_CONF" > /dev/null <<NGINX
server {
    listen 80;
    server_name $DOMAIN;

    location / {
        proxy_pass http://127.0.0.1:$PORT;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_read_timeout 86400;
    }
}
NGINX

sudo ln -sf "$NGINX_CONF" /etc/nginx/sites-enabled/labyrinth
sudo rm -f /etc/nginx/sites-enabled/default

if sudo nginx -t 2>&1; then
  sudo systemctl reload nginx
  echo "   nginx configured and reloaded"
else
  echo "ERROR: nginx config test failed"
  exit 1
fi

IP=$(hostname -I | awk '{print $1}')
echo ""
echo "============================================"
echo "  Labyrinth is running!"
echo ""
echo "  http://$IP  (port 80, via nginx)"
echo "  Internal: 127.0.0.1:$PORT"
echo ""
echo "  PID: $SERVER_PID"
echo "  Log: $PROJECT_DIR/labyrinth.log"
echo "  Stop: kill $SERVER_PID"
echo "============================================"
