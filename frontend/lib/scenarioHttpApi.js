/**
 * HTTP API сценариев: tests/scenarios/<категория>/<id>/scenario.json (setup + script).
 */
import fs from 'fs';
import path from 'path';
import express from 'express';
import { readMeta, stateFile } from './roomFiles.js';
import { validateRoomId, validateScenarioId, validateManifestAction } from './gameConstants.js';
import { readStateSnapshot } from './stateParse.js';
import { enqueueSandbox } from './sandboxHttpApi.js';
import {
  listScenarios,
  replayAllSteps,
  runSetupOnly,
  scenarioRootDir,
  SCENARIO_FILE,
} from './scenarioReplay.js';

/** Разрешённые type в setup[] (как в tests/scenario_lib.py build_setup_argv). */
const SETUP_STEP_TYPES = new Set([
  'generate',
  'add-player',
  'add-player-random',
  'set-turns',
  'init-turns',
  'init-base',
  'give-item',
  'give-treasure',
  'add-item',
  'add-item-random',
  'set-cell',
]);

function validateSetupArray(setup) {
  if (!Array.isArray(setup)) return 'нужен массив setup';
  if (setup.length === 0) {
    return (
      'setup пустой: в dev-песочнице шаги накапливаются при generate / игроках / предметах; ' +
      'из игры вставьте JSON массива в поле «Шаги setup»'
    );
  }
  for (let i = 0; i < setup.length; i++) {
    const s = setup[i];
    if (!s || typeof s !== 'object' || typeof s.type !== 'string') {
      return `setup[${i}]: ожидается объект с полем type`;
    }
    if (!SETUP_STEP_TYPES.has(s.type)) {
      return `setup[${i}]: неизвестный type "${s.type}"`;
    }
  }
  return null;
}

/** Команды до фиксации начала + при необходимости set-turns/init-turns (как раньше делал сервер на state комнаты). */
function buildSetupForScenario(clientSetup, enableTurns) {
  const out = [...clientSetup];
  if (enableTurns) {
    out.push({ type: 'set-turns', on: true });
    out.push({ type: 'init-turns' });
  }
  return out;
}

function verifyCapture(req, res) {
  const room = String(req.body?.room || '').trim();
  const token = String(req.body?.creatorToken || '');
  if (!validateRoomId(room)) {
    res.status(400).json({ ok: false, error: 'room invalid' });
    return null;
  }
  const meta = readMeta(room);
  if (!meta || !meta.creatorToken || meta.creatorToken !== token) {
    res.status(403).json({ ok: false, error: 'нужен creatorToken создателя комнаты' });
    return null;
  }
  const sf = stateFile(room);
  if (!fs.existsSync(sf)) {
    res.status(400).json({ ok: false, error: 'state комнаты не найден' });
    return null;
  }
  return { room, sf };
}

/** CORS для запросов с игры (другой порт на localhost). */
export function scenarioCorsMiddleware(req, res, next) {
  const o = req.headers.origin;
  if (o && /^https?:\/\/(localhost|127\.0\.0\.1)(:\d+)?$/.test(o)) {
    res.setHeader('Access-Control-Allow-Origin', o);
    res.setHeader('Access-Control-Allow-Credentials', 'true');
  } else {
    res.setHeader('Access-Control-Allow-Origin', '*');
  }
  res.setHeader('Access-Control-Allow-Methods', 'GET,POST,PUT,OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
  if (req.method === 'OPTIONS') return res.sendStatus(204);
  next();
}

export function createScenarioApiRouter() {
  const router = express.Router();

  router.get('/health', (_req, res) => {
    res.json({ ok: true, service: 'scenario-dev' });
  });

  router.get('/scenarios', (_req, res) => {
    try {
      return res.json({ ok: true, scenarios: listScenarios() });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  /** Содержимое scenario.json */
  router.get('/scenarios/:id/scenario-file', (req, res) => {
    try {
      const sid = String(req.params.id || '').trim();
      if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
      const p = path.join(scenarioRootDir(sid), SCENARIO_FILE);
      if (!fs.existsSync(p)) return res.status(404).json({ ok: false, error: 'нет scenario.json' });
      const raw = fs.readFileSync(p, 'utf8');
      return res.json({ ok: true, scenario: JSON.parse(raw) });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.put('/scenarios/:id/scenario-file', (req, res) => {
    try {
      const sid = String(req.params.id || '').trim();
      if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
      const d = scenarioRootDir(sid);
      if (!fs.existsSync(d)) return res.status(404).json({ ok: false, error: 'папка не найдена' });
      const body = req.body;
      if (!body || typeof body !== 'object') {
        return res.status(400).json({ ok: false, error: 'нужен JSON сценария' });
      }
      fs.writeFileSync(path.join(d, SCENARIO_FILE), JSON.stringify(body, null, 2), 'utf8');
      return res.json({ ok: true });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  /** После setup скопировать state в sandbox. */
  router.post('/scenarios/:id/sandbox-load-initial', async (req, res) => {
    try {
      const sid = String(req.params.id || '').trim();
      if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
      const v = verifyCapture(req, res);
      if (!v) return;
      const dir = scenarioRootDir(sid);
      if (!fs.existsSync(path.join(dir, SCENARIO_FILE))) {
        return res.status(404).json({ ok: false, error: 'сценарий не найден' });
      }
      let payload;
      await enqueueSandbox(async () => {
        payload = await runSetupOnly(dir);
        if (!payload.ok) return;
        fs.copyFileSync(payload.tmpPath, v.sf);
        try {
          fs.unlinkSync(payload.tmpPath);
        } catch (_) {}
      });
      if (!payload || !payload.ok) {
        return res.status(400).json({
          ok: false,
          error: payload?.error || 'setup failed',
          stdout: payload?.stdout,
          stderr: payload?.stderr,
        });
      }
      const snap = readStateSnapshot(v.sf);
      return res.json({ ok: true, loaded: 'setup', ...snap });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  /** Полный прогон script → итоговый state в sandbox. */
  router.post('/scenarios/:id/replay-to-sandbox', async (req, res) => {
    try {
      const sid = String(req.params.id || '').trim();
      if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
      const v = verifyCapture(req, res);
      if (!v) return;
      const dir = scenarioRootDir(sid);
      if (!fs.existsSync(path.join(dir, SCENARIO_FILE))) {
        return res.status(404).json({ ok: false, error: 'сценарий не найден' });
      }
      let payload;
      await enqueueSandbox(async () => {
        payload = await replayAllSteps(dir);
        if (!payload.ok) return;
        fs.copyFileSync(payload.finalTmpPath, v.sf);
        try {
          fs.unlinkSync(payload.finalTmpPath);
        } catch (_) {}
      });
      if (!payload || !payload.ok) {
        return res.status(400).json({
          ok: false,
          error: payload?.error || 'replay failed',
          scriptIndex: payload?.scriptIndex,
          setupIndex: payload?.setupIndex,
          stdout: payload?.stdout,
          stderr: payload?.stderr,
        });
      }
      const snap = readStateSnapshot(v.sf);
      return res.json({
        ok: true,
        stdout: payload.stdout,
        stderr: payload.stderr,
        id: payload.id,
        description: payload.description,
        ...snap,
      });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  router.post('/scenarios', (req, res) => {
    const id = String(req.body?.id || '').trim();
    const description = String(req.body?.description ?? req.body?.title ?? '').trim() || id;
    if (!validateScenarioId(id)) {
      return res.status(400).json({
        ok: false,
        error: 'id: латиница, цифры, _-, опционально категория/имя (например weapons/knife_test)',
      });
    }
    const d = scenarioRootDir(id);
    if (fs.existsSync(d)) {
      return res.status(409).json({ ok: false, error: 'папка уже существует' });
    }
    fs.mkdirSync(d, { recursive: true });
    const scenario = { description, setup: [], script: [] };
    fs.writeFileSync(path.join(d, SCENARIO_FILE), JSON.stringify(scenario, null, 2), 'utf8');
    return res.json({ ok: true, scenario });
  });

  /**
   * Одно сохранение: setup + script + описание. Папка создаётся при необходимости, файл перезаписывается.
   * В setup[] передаётся то, что накопила песочница (без set-turns/init-turns) — сервер добавит их при enableTurnsInSetup.
   */
  router.post('/scenarios/:id/save-scenario', (req, res) => {
    try {
      const sid = String(req.params.id || '').trim();
      if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
      const v = verifyCapture(req, res);
      if (!v) return;

      const rawSetup = req.body?.setup;
      const errSetup = validateSetupArray(rawSetup);
      if (errSetup) {
        return res.status(400).json({ ok: false, error: errSetup });
      }

      const actions = req.body?.actions;
      if (!Array.isArray(actions)) {
        return res.status(400).json({ ok: false, error: 'нужен массив actions (можно [])' });
      }
      for (const a of actions) {
        if (!validateManifestAction(a)) {
          return res.status(400).json({
            ok: false,
            error: 'некорректное действие в actions (типы: move, attack, use-item, player-status)',
          });
        }
      }

      const expectStdout =
        typeof req.body?.expect_stdout === 'string' ? req.body.expect_stdout : '';
      let expectContains = req.body?.expect_stdout_contains;
      if (expectContains == null) expectContains = [];
      if (!Array.isArray(expectContains)) expectContains = [];

      const description = String(req.body?.description ?? '').trim();
      const enableTurns = req.body?.enableTurnsInSetup !== false;

      const d = scenarioRootDir(sid);
      fs.mkdirSync(d, { recursive: true });

      const script = actions.map((a, i) => {
        const row = { ...a };
        const last = i === actions.length - 1;
        if (last) {
          row.expect_stdout = expectStdout;
          if (expectContains.length) row.expect_stdout_contains = expectContains;
        }
        return row;
      });

      const scenario = {
        description: description || sid,
        setup: buildSetupForScenario(rawSetup, enableTurns),
        script,
      };
      fs.writeFileSync(path.join(d, SCENARIO_FILE), JSON.stringify(scenario, null, 2), 'utf8');
      return res.json({ ok: true, scenario });
    } catch (e) {
      return res.status(500).json({ ok: false, error: e?.message || String(e) });
    }
  });

  /** Сохранить записанные действия в scenario.json (ожидание — на последнем действии). */
  router.post('/scenarios/:id/capture-final', (req, res) => {
    const sid = String(req.params.id || '').trim();
    if (!validateScenarioId(sid)) return res.status(400).json({ ok: false, error: 'bad id' });
    const v = verifyCapture(req, res);
    if (!v) return;

    const actions = req.body?.actions;
    if (!Array.isArray(actions) || actions.length === 0) {
      return res.status(400).json({ ok: false, error: 'нужен непустой массив actions' });
    }
    for (const a of actions) {
      if (!validateManifestAction(a)) {
        return res.status(400).json({
          ok: false,
          error:
            'некорректное действие в actions (типы: move, attack, use-item, player-status)',
        });
      }
    }

    const expectStdout = req.body?.expect_stdout;
    if (typeof expectStdout !== 'string') {
      return res.status(400).json({ ok: false, error: 'нужна строка expect_stdout' });
    }

    let expectContains = req.body?.expect_stdout_contains;
    if (expectContains == null) expectContains = [];
    if (!Array.isArray(expectContains)) expectContains = [];

    const description = String(req.body?.description ?? '').trim();

    const d = scenarioRootDir(sid);
    const scenarioPath = path.join(d, SCENARIO_FILE);
    if (!fs.existsSync(scenarioPath)) return res.status(404).json({ ok: false, error: 'scenario.json не найден' });

    const scenario = JSON.parse(fs.readFileSync(scenarioPath, 'utf8'));
    const errSetup = validateSetupArray(scenario.setup || []);
    if (errSetup) {
      return res.status(400).json({ ok: false, error: `Сначала «Зафиксировать начало» с непустым setup: ${errSetup}` });
    }
    if (description) scenario.description = description;

    const script = actions.map((a, i) => {
      const row = { ...a };
      const last = i === actions.length - 1;
      if (last) {
        row.expect_stdout = expectStdout;
        if (expectContains.length) row.expect_stdout_contains = expectContains;
      }
      return row;
    });
    scenario.script = script;

    fs.writeFileSync(scenarioPath, JSON.stringify(scenario, null, 2), 'utf8');
    return res.json({ ok: true, scenario });
  });

  return router;
}
