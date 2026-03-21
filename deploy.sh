#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-5173}"
DOMAIN="${DOMAIN:-_}"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Labyrinth deploy script (Ubuntu) ==="
echo "Project: $PROJECT_DIR"
echo ""

# ── Helpers ──
get_local_ip() {
  local ip=""

  if command -v ip >/dev/null 2>&1; then
    ip=$(ip -4 route get 1.1.1.1 2>/dev/null | awk '{
      for (i = 1; i <= NF; i++) {
        if ($i == "src") {
          print $(i + 1)
          exit
        }
      }
    }')
  fi

  if [ -z "$ip" ]; then
    ip=$(hostname -I 2>/dev/null | awk '{
      for (i = 1; i <= NF; i++) {
        if ($i ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/) {
          print $i
          exit
        }
      }
    }')
  fi

  printf '%s\n' "$ip"
}

is_valid_ipv4() {
  local ip="${1:-}"
  [[ "$ip" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || return 1

  local IFS=.
  local a b c d
  read -r a b c d <<< "$ip"

  for octet in "$a" "$b" "$c" "$d"; do
    [[ "$octet" =~ ^[0-9]+$ ]] || return 1
    [ "$octet" -ge 0 ] && [ "$octet" -le 255 ] || return 1
  done

  return 0
}

get_public_ip() {
  local ip=""
  local url

  command -v curl >/dev/null 2>&1 || return 0

  for url in \
    "https://api.ipify.org" \
    "https://ipv4.icanhazip.com" \
    "https://ifconfig.me/ip"
  do
    ip=$(curl -4 -fsS --max-time 5 "$url" 2>/dev/null | tr -d '[:space:]' || true)
    if is_valid_ipv4 "$ip"; then
      printf '%s\n' "$ip"
      return 0
    fi
  done

  printf '\n'
}

# ── 1. System dependencies ──
echo ">> Installing system packages..."
sudo apt-get update -qq
sudo apt-get install -y -qq build-essential cmake curl nginx iproute2 >/dev/null

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

# ── 9. Detect local/public IPs ──
LOCAL_IP="$(get_local_ip)"
PUBLIC_IP="$(get_public_ip)"

if is_valid_ipv4 "$PUBLIC_IP"; then
  echo "$PUBLIC_IP" > "$PROJECT_DIR/public_ip.txt"
else
  PUBLIC_IP=""
  rm -f "$PROJECT_DIR/public_ip.txt"
fi

echo ""
echo "============================================"
echo "  Labyrinth is running!"
echo ""

if [ -n "$PUBLIC_IP" ]; then
  echo "  http://$PUBLIC_IP  (public IP, port 80 — from the internet)"
  echo "  Saved: $PROJECT_DIR/public_ip.txt"
else
  echo "  Public IP: could not detect (no public egress IP, outbound HTTPS blocked, or lookup service unavailable)."
fi

if [ -n "$LOCAL_IP" ]; then
  echo "  http://$LOCAL_IP  (local network IP, port 80)"
else
  echo "  Local IP: could not detect."
fi

echo "  Internal: 127.0.0.1:$PORT"
echo ""
echo "  PID: $SERVER_PID"
echo "  Log: $PROJECT_DIR/labyrinth.log"
echo "  Stop: kill $SERVER_PID"
echo "============================================"
