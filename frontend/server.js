import express from 'express';
import http from 'http';
import { Server as SocketIOServer } from 'socket.io';
import path from 'path';
import { fileURLToPath } from 'url';
import { spawn } from 'child_process';
import fs from 'fs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT = path.resolve(__dirname, '..');
const LAB_BIN = path.join(ROOT, 'build', 'labyrinth');
const ROOMS_DIR = path.join(ROOT, 'rooms');

fs.mkdirSync(ROOMS_DIR, { recursive: true });

const app = express();
const server = http.createServer(app);
const io = new SocketIOServer(server, { cors: { origin: true, credentials: true } });

app.use(express.static(path.join(__dirname, 'public')));
app.get('/', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'lobby.html')));
app.get('/game', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'game.html')));

function splitLines(s) {
  if (!s) return [];
  let t = String(s).replace(/\r\n/g, '\n').replace(/\r/g, '\n');
  while (t.includes('\\n')) t = t.replace(/\\n/g, '\n');
  return t.split('\n').filter(l => l.length > 0);
}

function runLab(args) {
  return new Promise((resolve) => {
    const p = spawn(LAB_BIN, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '', err = '';
    p.stdout.on('data', d => { out += d.toString(); });
    p.stderr.on('data', d => { err += d.toString(); });
    p.on('close', (code) => resolve({ code, out: out.trim(), err: err.trim() }));
  });
}

const roomQueues = new Map();
function enqueue(room, task) {
  const prev = roomQueues.get(room) || Promise.resolve();
  const next = prev.catch(() => {}).then(task);
  roomQueues.set(room, next.catch(() => {}));
  return next;
}

function stateFile(room) { return path.join(ROOMS_DIR, `${room}.txt`); }
function svgFile(room) { return path.join(ROOMS_DIR, `${room}.svg`); }
function metaFile(room) { return path.join(ROOMS_DIR, `${room}.json`); }

function readMeta(room) {
  try { return JSON.parse(fs.readFileSync(metaFile(room), 'utf8')); }
  catch { return null; }
}
function writeMeta(room, meta) {
  fs.writeFileSync(metaFile(room), JSON.stringify(meta, null, 2), 'utf8');
}

function parseTurnInfo(room) {
  try {
    const txt = fs.readFileSync(stateFile(room), 'utf8');
    const arr = txt.split('\n');
    const ti = arr.findIndex(l => l.startsWith('TURNS '));
    if (ti === -1) return { enforce: false, order: [], index: 0, current: null };
    const parts = arr[ti].split(/\s+/);
    const enforce = Number(parts[1]) !== 0;
    const idx = Number(parts[2]) || 0;
    const cnt = Number(parts[3]) || 0;
    const order = [];
    for (let i = 0; i < cnt; i++) {
      const line = arr[ti + 1 + i];
      if (line !== undefined) order.push(line.trim());
    }
    return { enforce, order, index: idx < order.length ? idx : 0, current: order.length ? order[idx % order.length] : null };
  } catch { return { enforce: false, order: [], index: 0, current: null }; }
}

// In-memory room lobby state (waiting players before game starts)
const rooms = new Map();

function broadcastWaiting(roomId) {
  const r = rooms.get(roomId);
  if (!r) return;
  const names = r.waiting.map(w => w.name);
  io.to('lobby:' + roomId).emit('waitingList', { players: names, started: r.started });
}

io.on('connection', (socket) => {
  let myRoom = null;
  let myName = null;

  // ─── Lobby: create room ───
  socket.on('createRoom', async (payload, cb) => {
    try {
      const roomId = String(payload?.room || '').trim();
      const pw = String(payload?.password || '');
      const creatorName = String(payload?.name || '').trim();
      const width = Number(payload?.width) || 12;
      const height = Number(payload?.height) || 12;
      const openness = Number(payload?.openness) || 0.5;
      const seed = payload?.seed != null ? Number(payload.seed) : Math.floor(Math.random() * 100000);
      const turnActions = Number(payload?.turnActions) || 1;
      const weapons = Array.isArray(payload?.weapons) ? payload.weapons : ['shotgun', 'rifle', 'flashlight'];
      if (!roomId) return cb?.({ ok: false, error: 'Введите ID комнаты' });
      if (!creatorName) return cb?.({ ok: false, error: 'Введите имя' });
      if (rooms.has(roomId) || fs.existsSync(stateFile(roomId))) return cb?.({ ok: false, error: 'Комната уже существует' });

      await enqueue(roomId, async () => {
        writeMeta(roomId, { password: pw, creator: creatorName, started: false, weapons });
        const gen = await runLab([
          'generate', '--width', String(width), '--height', String(height),
          '--out', stateFile(roomId), '--openness', String(openness),
          '--seed', String(seed), '--turn-actions', String(turnActions),
        ]);
        if (gen.code !== 0) throw new Error(gen.err || gen.out || 'generate failed');
        // Place selected weapons on the map
        for (const w of weapons) {
          const res = await runLab(['add-item-random', '--state', stateFile(roomId), '--item', w]);
          if (res.code !== 0) console.error(`Failed to place ${w}: ${res.err}`);
        }
      });

      rooms.set(roomId, {
        password: pw, started: false,
        waiting: [{ name: creatorName, socketId: socket.id }],
        creatorName,
      });
      myRoom = roomId;
      myName = creatorName;
      socket.join('lobby:' + roomId);
      broadcastWaiting(roomId);
      cb?.({ ok: true, message: `Комната "${roomId}" создана (${width}×${height}, seed=${seed})` });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  // ─── Lobby: join room ───
  socket.on('joinRoom', async (payload, cb) => {
    try {
      const roomId = String(payload?.room || '').trim();
      const pw = String(payload?.password || '');
      const name = String(payload?.name || '').trim();
      if (!roomId) return cb?.({ ok: false, error: 'Введите ID комнаты' });
      if (!name) return cb?.({ ok: false, error: 'Введите имя' });

      const meta = readMeta(roomId);
      if (!meta && !rooms.has(roomId)) return cb?.({ ok: false, error: 'Комната не найдена' });
      const expectedPw = meta?.password ?? rooms.get(roomId)?.password ?? '';
      if (expectedPw && expectedPw !== pw) return cb?.({ ok: false, error: 'Неверный пароль' });

      // Rebuild in-memory entry from disk if needed (e.g. server restarted)
      if (!rooms.has(roomId)) {
        rooms.set(roomId, {
          password: expectedPw,
          started: meta?.started ?? false,
          waiting: [],
          creatorName: meta?.creator ?? null,
        });
      }
      const r = rooms.get(roomId);

      if (r.started) {
        // Reconnect to running game: check if player already exists in state
        const stateExists = fs.existsSync(stateFile(roomId));
        if (stateExists) {
          const txt = fs.readFileSync(stateFile(roomId), 'utf8');
          const playersLine = txt.split('\n').find(l => l.startsWith('PLAYERS '));
          const playerNames = [];
          if (playersLine) {
            const count = Number(playersLine.split(/\s+/)[1]);
            const lines = txt.split('\n');
            const idx = lines.indexOf(playersLine);
            for (let i = 1; i <= count && idx + i < lines.length; i++) {
              const pname = lines[idx + i].split(/\s+/)[0];
              if (pname) playerNames.push(pname);
            }
          }
          if (playerNames.includes(name)) {
            return cb?.({ ok: true, started: true });
          }
        }
        // New player joining a running game
        await enqueue(roomId, async () => {
          const res = await runLab(['add-player-random', '--state', stateFile(roomId), '--name', name]);
          if (res.code !== 0) throw new Error(res.err || res.out || 'Не удалось добавить игрока');
        });
        return cb?.({ ok: true, started: true });
      }

      // If same name exists but their socket is disconnected, allow reconnect
      const existing = r.waiting.find(w => w.name === name);
      if (existing) {
        const oldSocket = io.sockets.sockets.get(existing.socketId);
        if (!oldSocket || !oldSocket.connected) {
          existing.socketId = socket.id;
          myRoom = roomId;
          myName = name;
          socket.join('lobby:' + roomId);
          broadcastWaiting(roomId);
          return cb?.({ ok: true, started: false });
        }
        return cb?.({ ok: false, error: 'Имя уже занято' });
      }

      r.waiting.push({ name, socketId: socket.id });
      myRoom = roomId;
      myName = name;
      socket.join('lobby:' + roomId);
      broadcastWaiting(roomId);
      cb?.({ ok: true, started: false });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  // ─── Lobby: start game ───
  socket.on('startGame', async (_payload, cb) => {
    try {
      if (!myRoom) return cb?.({ ok: false, error: 'Не в комнате' });
      const r = rooms.get(myRoom);
      if (!r) return cb?.({ ok: false, error: 'Комната не найдена' });
      if (r.creatorName !== myName) return cb?.({ ok: false, error: 'Только создатель может начать игру' });
      if (r.started) return cb?.({ ok: false, error: 'Игра уже начата' });
      if (r.waiting.length === 0) return cb?.({ ok: false, error: 'Нет игроков' });

      await enqueue(myRoom, async () => {
        for (const w of r.waiting) {
          const res = await runLab(['add-player-random', '--state', stateFile(myRoom), '--name', w.name]);
          if (res.code !== 0) throw new Error(`Не удалось добавить ${w.name}: ${res.err}`);
        }
        // Initialize turn order so players see it immediately
        await runLab(['init-turns', '--state', stateFile(myRoom)]);
      });

      r.started = true;
      const meta = readMeta(myRoom) || {};
      meta.started = true;
      writeMeta(myRoom, meta);

      const turn = parseTurnInfo(myRoom);
      io.to('lobby:' + myRoom).emit('gameStarted', { turn });
      cb?.({ ok: true });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  // ─── Game: enter (after redirect from lobby) ───
  socket.on('enterGame', async (payload, cb) => {
    try {
      const roomId = String(payload?.room || '').trim();
      const name = String(payload?.name || '').trim();
      const pw = String(payload?.password || '');
      if (!roomId || !name) return cb?.({ ok: false, error: 'room and name required' });
      if (!fs.existsSync(stateFile(roomId))) return cb?.({ ok: false, error: 'Комната не найдена' });

      const meta = readMeta(roomId);
      const expectedPw = meta?.password ?? '';
      if (expectedPw && expectedPw !== pw) return cb?.({ ok: false, error: 'Неверный пароль' });

      myRoom = roomId;
      myName = name;
      socket.join('game:' + roomId);

      // Creator detection: check meta file (survives server restart + page reload)
      const creatorName = meta?.creator ?? rooms.get(roomId)?.creatorName ?? null;
      const isCreator = (creatorName === name);

      const turn = parseTurnInfo(roomId);
      const status = await fetchPlayerStatus(roomId, name);
      cb?.({ ok: true, turn, isCreator, playerStatus: status });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  // ─── Game actions ───
  async function fetchPlayerStatus(room, name) {
    try {
      const res = await runLab(['player-status', '--state', stateFile(room), '--name', name]);
      if (res.code === 0) return JSON.parse(res.out);
    } catch {}
    return null;
  }

  const DIR_RU = { up: 'вверх', down: 'вниз', left: 'влево', right: 'вправо' };
  const ITEM_RU = { knife: 'нож', shotgun: 'дробовик', rifle: 'ружьё', flashlight: 'фонарь' };

  function gameAction(argsBuilder, actionDesc) {
    return async (payload, cb) => {
      if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в игре' });
      try {
        let feedback;
        await enqueue(myRoom, async () => {
          const res = await runLab(argsBuilder(payload));
          feedback = splitLines(res.out);
          if (res.code !== 0 && res.err) feedback.push(res.err);
        });
        socket.emit('feedback', { lines: feedback, who: myName });
        const turn = parseTurnInfo(myRoom);
        io.to('game:' + myRoom).emit('turn', turn);
        // Broadcast action summary to other players
        const desc = typeof actionDesc === 'function' ? actionDesc(payload) : actionDesc;
        io.to('game:' + myRoom).emit('gameBroadcast', { who: myName, lines: feedback, desc });
        // Send updated inventory to acting player
        const status = await fetchPlayerStatus(myRoom, myName);
        if (status) socket.emit('playerStatus', status);
        cb?.({ ok: true });
      } catch (e) {
        socket.emit('feedback', { lines: [e?.message || String(e)], who: myName });
        cb?.({ ok: false, error: e?.message || String(e) });
      }
    };
  }

  socket.on('move', gameAction(
    p => ['move', '--state', stateFile(myRoom), '--name', myName, String(p?.dir || '')],
    p => `шагнул ${DIR_RU[p?.dir] || p?.dir}`
  ));
  socket.on('attack', gameAction(
    p => ['attack', '--state', stateFile(myRoom), '--name', myName, String(p?.dir || '')],
    p => `ударил ножом ${DIR_RU[p?.dir] || p?.dir}`
  ));
  socket.on('use', gameAction(
    p => ['use-item', '--state', stateFile(myRoom), '--name', myName, '--item', String(p?.item || ''), String(p?.dir || '')],
    p => `использовал ${ITEM_RU[p?.item] || p?.item} ${DIR_RU[p?.dir] || p?.dir}`
  ));

  // ─── Player status (inventory) ───
  socket.on('getPlayerStatus', async (_payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в игре' });
    try {
      let data;
      await enqueue(myRoom, async () => {
        const res = await runLab(['player-status', '--state', stateFile(myRoom), '--name', myName]);
        if (res.code !== 0) throw new Error(res.err || 'player-status failed');
        data = JSON.parse(res.out);
      });
      cb?.({ ok: true, ...data });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  socket.on('setTurns', async (payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в игре' });
    const meta = readMeta(myRoom);
    const creatorName = meta?.creator ?? rooms.get(myRoom)?.creatorName ?? null;
    if (creatorName !== myName) return cb?.({ ok: false, error: 'Только создатель' });
    const val = payload?.enabled ? '1' : '0';
    try {
      let msg;
      await enqueue(myRoom, async () => {
        const res = await runLab(['set-turns', '--state', stateFile(myRoom), val]);
        msg = res.out?.trim();
        if (res.code !== 0) throw new Error(res.err || 'set-turns failed');
      });
      const turn = parseTurnInfo(myRoom);
      io.to('game:' + myRoom).emit('turn', turn);
      io.to('game:' + myRoom).emit('turnsToggled', { enabled: !!payload?.enabled });
      cb?.({ ok: true, message: msg });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  socket.on('viewMap', async (_payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в комнате' });
    const meta = readMeta(myRoom);
    const creatorName = meta?.creator ?? rooms.get(myRoom)?.creatorName ?? null;
    if (creatorName !== myName) return cb?.({ ok: false, error: 'Только создатель может смотреть карту' });
    try {
      let svg, replayList;
      await enqueue(myRoom, async () => {
        const svgRes = await runLab(['export-svg', '--state', stateFile(myRoom), '--out', svgFile(myRoom)]);
        if (svgRes.code !== 0) throw new Error(svgRes.err || 'export-svg failed');
        const listRes = await runLab(['replay-list', '--state', stateFile(myRoom)]);
        if (listRes.code !== 0) throw new Error(listRes.err || 'replay-list failed');
        replayList = JSON.parse(listRes.out);
      });
      svg = fs.readFileSync(svgFile(myRoom), 'utf8');
      cb?.({ ok: true, svg, replay: replayList });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  socket.on('replaySvg', async (payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в комнате' });
    const meta = readMeta(myRoom);
    const creatorName = meta?.creator ?? rooms.get(myRoom)?.creatorName ?? null;
    if (creatorName !== myName) return cb?.({ ok: false, error: 'Только создатель' });
    const step = Number(payload?.step ?? 0);
    try {
      let svg;
      await enqueue(myRoom, async () => {
        const res = await runLab(['replay-svg', '--state', stateFile(myRoom), '--step', String(step)]);
        if (res.code !== 0) throw new Error(res.err || 'replay-svg failed');
        svg = res.out;
      });
      cb?.({ ok: true, svg });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  socket.on('disconnect', () => {
    if (!myRoom || !rooms.has(myRoom)) return;
    const r = rooms.get(myRoom);
    if (r.started) return; // don't touch waiting list after game started
    r.waiting = r.waiting.filter(w => w.socketId !== socket.id);
    broadcastWaiting(myRoom);
  });
});

const PORT = Number(process.env.PORT || 5173);
server.listen(PORT, () => {
  console.log(`Labyrinth server: http://127.0.0.1:${PORT}`);
});
