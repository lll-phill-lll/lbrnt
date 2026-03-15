# Labyrinth Frontend (Node + Socket.IO)

One Node.js server hosts multiple rooms. Each room maps to a `state.txt` under `rooms/` and uses the C++ CLI backend per action. Players connect via WebSocket, join a room with a name, take turns, and receive personal feedback. No map/SVG is exposed to players; they keep their own notes on a personal drawing canvas.

## Install & Run

```bash
cd frontend
npm install
npm run start
# open http://127.0.0.1:5173/
```

Requirements:
- Build the C++ binary first: `build/labyrinth` must exist at repo root under `build/`.

## Events

- `join { room, name }` → server generates state if missing and runs `add-player-random`; emits `feedback` to actor and `turn` (names only) to room.
- `move { dir }`, `attack { dir }`, `use { dir, item }` → sequentialized per room; actor gets `feedback`; room gets updated `turn`.

## Notes

- Room-to-backend access is serialized to prevent state corruption.
- Players only see textual feedback and turn info; the full map stays hidden.
- Personal canvas (notes) are stored locally in `localStorage`.

