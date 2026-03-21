import express from 'express';
import http from 'http';
import { Server as SocketIOServer } from 'socket.io';
import path from 'path';
import { fileURLToPath } from 'url';
import { spawn } from 'child_process';
import fs from 'fs';
import crypto from 'crypto';
import sharp from 'sharp';
import GIFEncoder from 'gif-encoder-2';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT = path.resolve(__dirname, '..');
const LAB_BIN = path.join(ROOT, 'build', 'labyrinth');
const ROOMS_DIR = path.join(ROOT, 'rooms');

fs.mkdirSync(ROOMS_DIR, { recursive: true });

// ── Input validation helpers ──
const SAFE_ID_RE = /^[a-zA-Z0-9_-]{1,40}$/;
const SAFE_NAME_RE = /^[^\s<>&"']{1,30}$/;
const MAX_PASSWORD_LEN = 64;
const MAX_CHAT_LEN = 300;

function validateRoomId(id) {
  if (typeof id !== 'string') return false;
  return SAFE_ID_RE.test(id);
}
function validateName(name) {
  if (typeof name !== 'string') return false;
  return SAFE_NAME_RE.test(name);
}
function sanitizeStr(s, maxLen) {
  return String(s || '').slice(0, maxLen);
}

const ALLOWED_ORIGINS = (process.env.ALLOWED_ORIGINS || '').split(',').filter(Boolean);

const app = express();
const server = http.createServer(app);
const io = new SocketIOServer(server, {
  cors: {
    origin: ALLOWED_ORIGINS.length > 0
      ? ALLOWED_ORIGINS
      : (origin, cb) => cb(null, origin),
    credentials: true,
  }
});

app.use(express.static(path.join(__dirname, 'public')));
app.get('/', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'lobby.html')));
app.get('/game', (_req, res) => res.sendFile(path.join(__dirname, 'public', 'game.html')));

// Rate limit for GIF export: one concurrent request per room
const gifInProgress = new Set();

app.get('/api/replay-gif', async (req, res) => {
  const room = String(req.query.room || '').trim();
  const token = String(req.query.token || '');
  if (!room || !token) return res.status(400).send('Missing params');
  if (!validateRoomId(room)) return res.status(400).send('Invalid room id');
  const meta = readMeta(room);
  if (!meta || meta.creatorToken !== token) return res.status(403).send('Forbidden');
  const sf = stateFile(room);
  if (!fs.existsSync(sf)) return res.status(404).send('Room not found');
  if (gifInProgress.has(room)) return res.status(429).send('GIF export already in progress for this room');
  gifInProgress.add(room);
  try {
    const listRes = await runLab(['replay-list', '--state', sf]);
    if (listRes.code !== 0) throw new Error(listRes.err);
    const { total } = JSON.parse(listRes.out);

    const svgs = [];
    for (let i = 0; i <= total; i++) {
      let svgText;
      if (i === total) {
        const tmpSvg = sf + '.tmp.svg';
        const r = await runLab(['export-svg', '--state', sf, '--out', tmpSvg]);
        if (r.code !== 0) throw new Error(r.err);
        svgText = fs.readFileSync(tmpSvg, 'utf8');
        fs.unlinkSync(tmpSvg);
      } else {
        const r = await runLab(['replay-svg', '--state', sf, '--step', String(i)]);
        if (r.code !== 0) throw new Error(r.err);
        svgText = r.out;
      }
      svgs.push(svgText);
    }

    const firstPng = await sharp(Buffer.from(svgs[0])).png().toBuffer();
    const firstMeta = await sharp(firstPng).metadata();
    const w = firstMeta.width, h = firstMeta.height;

    const encoder = new GIFEncoder(w, h);
    encoder.setDelay(200);
    encoder.setRepeat(0);
    encoder.start();

    for (const svg of svgs) {
      const png = await sharp(Buffer.from(svg))
        .resize(w, h)
        .flatten({ background: { r: 11, g: 13, b: 18 } })
        .ensureAlpha()
        .raw()
        .toBuffer();
      encoder.addFrame(png);
    }

    encoder.finish();
    const gifBuf = encoder.out.getData();
    res.set({ 'Content-Type': 'image/gif', 'Content-Disposition': 'attachment; filename="replay.gif"' });
    res.send(gifBuf);
  } catch (e) {
    console.error('GIF export error:', e);
    res.status(500).send('Error: ' + (e?.message || e));
  } finally {
    gifInProgress.delete(room);
  }
});

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
const broadcastActiveRooms = new Set();

function broadcastWaiting(roomId) {
  const r = rooms.get(roomId);
  if (!r) return;
  const names = r.waiting.map(w => w.name);
  io.to('lobby:' + roomId).emit('waitingList', { players: names, started: r.started });
}

io.on('connection', (socket) => {
  let myRoom = null;
  let myName = null;
  let myIsCreator = false;

  // ─── Lobby: create room ───
  socket.on('createRoom', async (payload, cb) => {
    try {
      const roomId = String(payload?.room || '').trim();
      const pw = sanitizeStr(payload?.password, MAX_PASSWORD_LEN);
      const creatorName = String(payload?.name || '').trim();
      if (!roomId || !validateRoomId(roomId)) return cb?.({ ok: false, error: 'ID комнаты: только a-z, 0-9, _, - (до 40 символов)' });
      if (!creatorName || !validateName(creatorName)) return cb?.({ ok: false, error: 'Имя: 1-30 символов, без пробелов и <>&"\'' });
      const width = Math.min(Math.max(Number(payload?.width) || 12, 4), 50);
      const height = Math.min(Math.max(Number(payload?.height) || 12, 4), 50);
      const openness = Math.min(Math.max(Number(payload?.openness) || 0.5, 0), 1);
      const seed = payload?.seed != null ? Number(payload.seed) : Math.floor(Math.random() * 100000);
      const turnActions = Math.min(Math.max(Number(payload?.turnActions) || 1, 1), 10);
      const validWeapons = ['shotgun', 'rifle', 'flashlight', 'armor'];
      const weapons = Array.isArray(payload?.weapons) ? payload.weapons.filter(w => validWeapons.includes(w)) : validWeapons;
      if (rooms.has(roomId) || fs.existsSync(stateFile(roomId))) return cb?.({ ok: false, error: 'Комната уже существует' });

      const creatorToken = crypto.randomBytes(24).toString('hex');

      await enqueue(roomId, async () => {
        writeMeta(roomId, { password: pw, creator: creatorName, creatorToken, started: false, weapons });
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
        creatorName, creatorToken,
      });
      myRoom = roomId;
      myName = creatorName;
      myIsCreator = true;
      socket.join('lobby:' + roomId);
      broadcastWaiting(roomId);
      cb?.({ ok: true, creatorToken, message: `Комната "${roomId}" создана (${width}×${height}, seed=${seed})` });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  // ─── Lobby: join room ───
  socket.on('joinRoom', async (payload, cb) => {
    try {
      const roomId = String(payload?.room || '').trim();
      const pw = sanitizeStr(payload?.password, MAX_PASSWORD_LEN);
      const name = String(payload?.name || '').trim();
      if (!roomId || !validateRoomId(roomId)) return cb?.({ ok: false, error: 'ID комнаты: только a-z, 0-9, _, - (до 40 символов)' });
      if (!name || !validateName(name)) return cb?.({ ok: false, error: 'Имя: 1-30 символов, без пробелов и <>&"\'' });

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
      if (!myIsCreator) return cb?.({ ok: false, error: 'Только создатель может начать игру' });
      if (r.started) return cb?.({ ok: false, error: 'Игра уже начата' });
      if (r.waiting.length === 0) return cb?.({ ok: false, error: 'Нет игроков' });

      await enqueue(myRoom, async () => {
        for (const w of r.waiting) {
          const res = await runLab(['add-player-random', '--state', stateFile(myRoom), '--name', w.name]);
          if (res.code !== 0) throw new Error(`Не удалось добавить ${w.name}: ${res.err}`);
        }
        // Initialize turn order so players see it immediately
        await runLab(['init-turns', '--state', stateFile(myRoom)]);
        // Snapshot base state for replay (after weapons + players are placed)
        await runLab(['init-base', '--state', stateFile(myRoom)]);
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
      const pw = sanitizeStr(payload?.password, MAX_PASSWORD_LEN);
      if (!roomId || !validateRoomId(roomId)) return cb?.({ ok: false, error: 'Invalid room id' });
      if (!name || !validateName(name)) return cb?.({ ok: false, error: 'Invalid name' });
      if (!fs.existsSync(stateFile(roomId))) return cb?.({ ok: false, error: 'Комната не найдена' });

      const meta = readMeta(roomId);
      const expectedPw = meta?.password ?? '';
      if (expectedPw && expectedPw !== pw) return cb?.({ ok: false, error: 'Неверный пароль' });

      myRoom = roomId;
      myName = name;
      socket.join('game:' + roomId);

      // Creator detection: verify by secret token (not just name)
      const suppliedToken = String(payload?.creatorToken || '');
      const storedToken = meta?.creatorToken ?? rooms.get(roomId)?.creatorToken ?? null;
      const isCreator = !!(storedToken && suppliedToken === storedToken);
      myIsCreator = isCreator;

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

  const VALID_DIRS = new Set(['up', 'down', 'left', 'right']);
  const VALID_ITEMS = new Set(['knife', 'shotgun', 'rifle', 'flashlight']);
  const DIR_RU = { up: 'вверх', down: 'вниз', left: 'влево', right: 'вправо' };
  const ITEM_RU = { knife: 'нож', shotgun: 'дробовик', rifle: 'ружьё', flashlight: 'фонарь', armor: 'броня' };

  function gameAction(argsBuilder, actionDesc, validator) {
    return async (payload, cb) => {
      if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в игре' });
      if (validator && !validator(payload)) return cb?.({ ok: false, error: 'Недопустимые параметры' });
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
        // Auto-rebroadcast map if broadcast is active
        if (broadcastActiveRooms.has(myRoom)) {
          try {
            await enqueue(myRoom, async () => {
              const res = await runLab(['export-svg', '--state', stateFile(myRoom), '--out', svgFile(myRoom)]);
              if (res.code !== 0) throw new Error(res.err || 'export-svg failed');
            });
            const svg = fs.readFileSync(svgFile(myRoom), 'utf8');
            io.to('game:' + myRoom).emit('mapRevealed', { svg });
          } catch {}
        }
        cb?.({ ok: true });
      } catch (e) {
        socket.emit('feedback', { lines: [e?.message || String(e)], who: myName });
        cb?.({ ok: false, error: e?.message || String(e) });
      }
    };
  }

  socket.on('move', gameAction(
    p => ['move', '--state', stateFile(myRoom), '--name', myName, String(p?.dir || '')],
    p => `шагнул ${DIR_RU[p?.dir] || p?.dir}`,
    p => VALID_DIRS.has(p?.dir)
  ));
  socket.on('attack', gameAction(
    p => ['attack', '--state', stateFile(myRoom), '--name', myName, String(p?.dir || '')],
    p => `ударил ножом ${DIR_RU[p?.dir] || p?.dir}`,
    p => VALID_DIRS.has(p?.dir)
  ));
  socket.on('use', gameAction(
    p => ['use-item', '--state', stateFile(myRoom), '--name', myName, '--item', String(p?.item || ''), String(p?.dir || '')],
    p => `использовал ${ITEM_RU[p?.item] || p?.item} ${DIR_RU[p?.dir] || p?.dir}`,
    p => VALID_DIRS.has(p?.dir) && VALID_ITEMS.has(p?.item)
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
    if (!myIsCreator) return cb?.({ ok: false, error: 'Только создатель' });
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
    if (!myIsCreator) return cb?.({ ok: false, error: 'Только создатель может смотреть карту' });
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
    if (!myIsCreator) return cb?.({ ok: false, error: 'Только создатель' });
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

  socket.on('broadcastMap', async (_payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false, error: 'Не в комнате' });
    if (!myIsCreator) return cb?.({ ok: false, error: 'Только создатель' });
    try {
      await enqueue(myRoom, async () => {
        const res = await runLab(['export-svg', '--state', stateFile(myRoom), '--out', svgFile(myRoom)]);
        if (res.code !== 0) throw new Error(res.err || 'export-svg failed');
      });
      const svg = fs.readFileSync(svgFile(myRoom), 'utf8');
      broadcastActiveRooms.add(myRoom);
      io.to('game:' + myRoom).emit('mapRevealed', { svg });
      cb?.({ ok: true });
    } catch (e) {
      cb?.({ ok: false, error: e?.message || String(e) });
    }
  });

  socket.on('chatMessage', (payload, cb) => {
    if (!myRoom || !myName) return cb?.({ ok: false });
    const text = sanitizeStr(payload?.text, MAX_CHAT_LEN).trim();
    if (!text) return cb?.({ ok: false });
    io.to('game:' + myRoom).emit('chatMsg', { who: myName, text });
    cb?.({ ok: true });
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
const HOST = process.env.HOST || '127.0.0.1';
server.listen(PORT, HOST, () => {
  console.log(`Labyrinth server: http://${HOST}:${PORT}`);
});
