/**
 * Dev-песочница: одна комната `sandbox`, без лобби — генерация, add-player/add-item, ходы через CLI.
 */
import fs from 'fs';
import express from 'express';
import { runLab } from './runLab.js';
import { ROOMS_DIR } from './repoPaths.js';
import { stateFile, svgFile, readMeta, writeMeta } from './roomFiles.js';
import { SANDBOX_ROOM_ID, SANDBOX_CREATOR_TOKEN } from './sandboxConstants.js';
import {
  validatePlayerName,
  DEFAULT_MAP_WIDTH,
  DEFAULT_MAP_HEIGHT,
  VALID_DIRS,
  VALID_ITEMS,
} from './gameConstants.js';
import { readStateSnapshot } from './stateParse.js';

function splitLines(s) {
  if (!s) return [];
  let t = String(s).replace(/\r\n/g, '\n').replace(/\r/g, '\n');
  while (t.includes('\\n')) t = t.replace(/\\n/g, '\n');
  return t.split('\n').filter((l) => l.length > 0);
}

let sandboxQueue = Promise.resolve();

export function enqueueSandbox(task) {
  const next = sandboxQueue.then(task).catch((e) => console.error('sandbox:', e));
  sandboxQueue = next.catch(() => {});
  return next;
}

async function ensureSandbox() {
  fs.mkdirSync(ROOMS_DIR, { recursive: true });
  const sf = stateFile(SANDBOX_ROOM_ID);
  if (!fs.existsSync(sf)) {
    const res = await runLab([
      'generate',
      '--width', String(DEFAULT_MAP_WIDTH),
      '--height', String(DEFAULT_MAP_HEIGHT),
      '--out', sf,
      '--openness', '0.5',
      '--seed', '42',
      '--turn-actions', '1',
      '--turns', '0',
    ]);
    if (res.code !== 0) throw new Error(res.err || res.out || 'generate failed');
  }
  if (!readMeta(SANDBOX_ROOM_ID)) {
    writeMeta(SANDBOX_ROOM_ID, {
      password: '',
      creator: 'dev',
      creatorToken: SANDBOX_CREATOR_TOKEN,
      started: true,
    });
  }
}

/** Один цикл действия + resolve-bots; rawStdout только из stdout CLI (как в server.js). */
async function runActionWithBots(args) {
  const sf = stateFile(SANDBOX_ROOM_ID);
  const res = await runLab(args);
  let stdout = res.out || '';
  let stderr = res.err || '';
  let scenarioTrace = stdout.trim();
  let feedback = splitLines(res.out);
  if (res.code !== 0 && res.err) feedback.push(res.err);
  if (res.code === 0) {
    const r2 = await runLab(['resolve-bots', '--state', sf]);
    if (r2.code !== 0) console.error('resolve-bots:', r2.err || r2.out);
    if (r2.out) {
      stdout = stdout + (stdout && r2.out ? '\n' : '') + r2.out;
      const botPart = r2.out.trim();
      if (botPart) scenarioTrace = scenarioTrace + (scenarioTrace ? '\n' : '') + botPart;
      feedback = feedback.concat(splitLines(r2.out));
    }
    if (r2.err) stderr = stderr + (stderr && r2.err ? '\n' : '') + r2.err;
  }
  return { res, scenarioTrace, feedback, stdout, stderr };
}

export function createSandboxApiRouter() {
  const router = express.Router();

  router.get('/session', async (_req, res) => {
    try {
      await ensureSandbox();
      return res.json({
        ok: true,
        room: SANDBOX_ROOM_ID,
        creatorToken: SANDBOX_CREATOR_TOKEN,
      });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.post('/ensure', async (_req, res) => {
    try {
      await ensureSandbox();
      return res.json({ ok: true });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.get('/snapshot', async (_req, res) => {
    try {
      let payload;
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const sf = stateFile(SANDBOX_ROOM_ID);
        const snap = readStateSnapshot(sf);
        const playerStatuses = {};
        for (const name of snap.players) {
          const r = await runLab(['player-status', '--state', sf, '--name', name]);
          if (r.code === 0 && r.out) {
            try {
              const line = r.out.trim().split('\n').find((l) => l.startsWith('{')) || r.out.trim();
              playerStatuses[name] = JSON.parse(line);
            } catch (_) {
              /* ignore parse errors for one player */
            }
          }
        }
        payload = { ...snap, playerStatuses };
      });
      return res.json({ ok: true, ...payload });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.get('/svg', async (_req, res) => {
    try {
      await ensureSandbox();
      const sf = stateFile(SANDBOX_ROOM_ID);
      const out = svgFile(SANDBOX_ROOM_ID);
      const r = await runLab(['export-svg', '--state', sf, '--out', out]);
      if (r.code !== 0) return res.status(400).json({ ok: false, error: r.err || 'export-svg failed' });
      const svg = fs.readFileSync(out, 'utf8');
      res.type('image/svg+xml').send(svg);
    } catch (e) {
      res.status(500).send(String(e?.message || e));
    }
  });

  router.post('/generate', async (req, res) => {
    try {
      let lastCli = { stdout: '', stderr: '' };
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const width = Math.min(Math.max(Number(req.body?.width) || DEFAULT_MAP_WIDTH, 4), 50);
        const height = Math.min(Math.max(Number(req.body?.height) || DEFAULT_MAP_HEIGHT, 4), 50);
        const openness = Math.min(Math.max(Number(req.body?.openness) ?? 0.5, 0), 1);
        const seed = req.body?.seed != null ? Number(req.body.seed) : 42;
        const turnActions = Math.min(Math.max(Number(req.body?.turn_actions) || 1, 1), 10);
        const botSteps = Number(req.body?.bot_steps);
        const sf = stateFile(SANDBOX_ROOM_ID);
        const args = [
          'generate',
          '--width', String(width),
          '--height', String(height),
          '--out', sf,
          '--openness', String(openness),
          '--seed', String(seed),
          '--turn-actions', String(turnActions),
          /* очерёдность ходов включается при сохранении сценария (set-turns в setup) */
          '--turns',
          '0',
        ];
        if (Number.isFinite(botSteps) && botSteps > 0) {
          args.push('--bot-steps', String(Math.min(Math.max(Math.floor(botSteps), 1), 5)));
        }
        const r = await runLab(args);
        lastCli = { stdout: r.out || '', stderr: r.err || '' };
        if (r.code !== 0) throw new Error(r.err || r.out || 'generate failed');
      });
      const sf = stateFile(SANDBOX_ROOM_ID);
      const snap = readStateSnapshot(sf);
      return res.json({
        ok: true,
        stdout: lastCli.stdout,
        stderr: lastCli.stderr,
        ...snap,
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.post('/add-player', async (req, res) => {
    try {
      const name = String(req.body?.name || '').trim();
      const x = req.body?.x;
      const y = req.body?.y;
      if (!validatePlayerName(name)) {
        return res.status(400).json({ ok: false, error: 'имя: 1–30 символов, без пробелов' });
      }
      let lastCli = { stdout: '', stderr: '' };
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const sf = stateFile(SANDBOX_ROOM_ID);
        let r;
        if (x != null && y != null) {
          r = await runLab(['add-player', '--state', sf, '--name', name, '--x', String(x), '--y', String(y)]);
        } else {
          r = await runLab(['add-player-random', '--state', sf, '--name', name]);
        }
        lastCli = { stdout: r.out || '', stderr: r.err || '' };
        if (r.code !== 0) throw new Error(r.err || r.out || 'add-player failed');
      });
      const sf = stateFile(SANDBOX_ROOM_ID);
      const snap = readStateSnapshot(sf);
      return res.json({
        ok: true,
        stdout: lastCli.stdout,
        stderr: lastCli.stderr,
        ...snap,
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.post('/add-item', async (req, res) => {
    try {
      const item = String(req.body?.item || '').trim();
      const x = Number(req.body?.x);
      const y = Number(req.body?.y);
      if (!VALID_ITEMS.has(item) || !Number.isFinite(x) || !Number.isFinite(y)) {
        return res.status(400).json({ ok: false, error: 'item и целые x, y' });
      }
      let lastCli = { stdout: '', stderr: '' };
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const sf = stateFile(SANDBOX_ROOM_ID);
        const r = await runLab([
          'add-item', '--state', sf, '--item', item,
          '--x', String(Math.floor(x)), '--y', String(Math.floor(y)),
          '--charges', '1',
        ]);
        lastCli = { stdout: r.out || '', stderr: r.err || '' };
        if (r.code !== 0) throw new Error(r.err || r.out || 'add-item failed');
      });
      const sf = stateFile(SANDBOX_ROOM_ID);
      const snap = readStateSnapshot(sf);
      return res.json({
        ok: true,
        stdout: lastCli.stdout,
        stderr: lastCli.stderr,
        ...snap,
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.post('/give-item', async (req, res) => {
    try {
      const name = String(req.body?.name || '').trim();
      const item = String(req.body?.item || '').trim();
      if (!validatePlayerName(name)) {
        return res.status(400).json({ ok: false, error: 'имя: 1–30 символов, без пробелов' });
      }
      if (!VALID_ITEMS.has(item)) {
        return res.status(400).json({ ok: false, error: 'неверный предмет' });
      }
      let lastCli = { stdout: '', stderr: '' };
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const sf = stateFile(SANDBOX_ROOM_ID);
        const r = await runLab([
          'give-item',
          '--state',
          sf,
          '--name',
          name,
          '--item',
          item,
          '--charges',
          '1',
        ]);
        lastCli = { stdout: r.out || '', stderr: r.err || '' };
        if (r.code !== 0) throw new Error(r.err || r.out || 'give-item failed');
      });
      const sf = stateFile(SANDBOX_ROOM_ID);
      const snap = readStateSnapshot(sf);
      return res.json({
        ok: true,
        stdout: lastCli.stdout,
        stderr: lastCli.stderr,
        ...snap,
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  async function handleMoveAttackUse(req, res, kind) {
    try {
      const name = String(req.body?.name || '').trim();
      const dir = String(req.body?.dir || '').trim();
      const item = String(req.body?.item || '').trim();
      if (!validatePlayerName(name)) {
        return res.status(400).json({ ok: false, error: 'bad name' });
      }
      if (!VALID_DIRS.has(dir)) {
        return res.status(400).json({ ok: false, error: 'bad dir' });
      }
      const sf = stateFile(SANDBOX_ROOM_ID);
      let args;
      if (kind === 'move') {
        args = ['move', '--state', sf, '--name', name, dir];
      } else if (kind === 'attack') {
        args = ['attack', '--state', sf, '--name', name, dir];
      } else {
        if (!VALID_ITEMS.has(item)) return res.status(400).json({ ok: false, error: 'bad item' });
        args = ['use-item', '--state', sf, '--name', name, '--item', item, dir];
      }

      let payload;
      await enqueueSandbox(async () => {
        await ensureSandbox();
        payload = await runActionWithBots(args);
      });
      if (!payload) {
        return res.status(500).json({ ok: false, error: 'sandbox internal error' });
      }
      if (payload.res.code !== 0) {
        return res.status(400).json({
          ok: false,
          error: payload.res.err || payload.res.out || 'cli failed',
          stdout: payload.stdout,
          stderr: payload.stderr,
          feedback: payload.feedback,
        });
      }
      const snap = readStateSnapshot(sf);
      return res.json({
        ok: true,
        stdout: payload.stdout,
        stderr: payload.stderr,
        scenarioTrace: { rawStdout: payload.scenarioTrace },
        feedback: payload.feedback,
        ...snap,
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  }

  router.post('/move', (req, res) => handleMoveAttackUse(req, res, 'move'));
  router.post('/attack', (req, res) => handleMoveAttackUse(req, res, 'attack'));
  router.post('/use', (req, res) => handleMoveAttackUse(req, res, 'use'));

  /** Только stdout JSON `player-status` (без resolve-bots) + scenarioTrace — для записи в сценарий. */
  router.post('/dump-status', async (req, res) => {
    try {
      const name = String(req.body?.name || '').trim();
      if (!validatePlayerName(name)) {
        return res.status(400).json({ ok: false, error: 'bad name' });
      }
      const sf = stateFile(SANDBOX_ROOM_ID);
      let out = '';
      let err = '';
      /** enqueueSandbox глотает reject колбэка — проверяем код выхода сами. */
      let cliFail = null;
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const r = await runLab(['player-status', '--state', sf, '--name', name]);
        out = r.out || '';
        err = r.err || '';
        if (r.code !== 0) {
          cliFail = new Error(r.err || r.out || 'player-status failed');
        }
      });
      if (cliFail) throw cliFail;
      const raw = String(out).trim();
      return res.json({
        ok: true,
        stdout: out,
        stderr: err,
        scenarioTrace: { rawStdout: raw },
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  /** Один вызов CLI player-status (для пошагового play сценария). */
  router.post('/player-status', async (req, res) => {
    try {
      const name = String(req.body?.name || '').trim();
      if (!validatePlayerName(name)) {
        return res.status(400).json({ ok: false, error: 'bad name' });
      }
      let cli;
      await enqueueSandbox(async () => {
        await ensureSandbox();
        const sf = stateFile(SANDBOX_ROOM_ID);
        cli = await runLab(['player-status', '--state', sf, '--name', name]);
      });
      if (!cli || cli.code !== 0) {
        return res.status(400).json({
          ok: false,
          error: (cli && (cli.err || cli.out)) || 'player-status failed',
        });
      }
      return res.json({
        ok: true,
        stdout: cli.out || '',
        stderr: cli.err || '',
      });
    } catch (e) {
      return res.status(400).json({ ok: false, error: e?.message || String(e) });
    }
  });

  return router;
}
