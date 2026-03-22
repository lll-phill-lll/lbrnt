/**
 * Проигрывание сценария из tests/scenarios/<категория>/<id>/scenario.json (setup + script).
 */
import fs from 'fs';
import path from 'path';
import os from 'os';
import { SCENARIOS_DIR } from './repoPaths.js';
import { runLab } from './runLab.js';
import { validateManifestAction } from './gameConstants.js';

export const SCENARIO_FILE = 'scenario.json';

export function scenarioRootDir(id) {
  const parts = String(id || '')
    .split('/')
    .map((p) => p.trim())
    .filter((p) => p && p !== '.' && p !== '..');
  return path.join(SCENARIOS_DIR, ...parts);
}

/** Относительный id папки сценария: `weapons/knife_to_hospital`. */
export function scenarioIdFromDirPath(scenarioDirPath) {
  const rel = path.relative(SCENARIOS_DIR, scenarioDirPath);
  if (rel.startsWith('..')) return path.basename(scenarioDirPath);
  return rel.split(path.sep).join('/');
}

export function listScenarios() {
  if (!fs.existsSync(SCENARIOS_DIR)) return [];
  const out = [];
  function walk(absDir, relId) {
    const sf = path.join(absDir, SCENARIO_FILE);
    if (fs.existsSync(sf) && relId) {
      let description = relId;
      try {
        const m = JSON.parse(fs.readFileSync(sf, 'utf8'));
        description = String(m.description || m.title || relId);
      } catch {
        description = relId;
      }
      const category = relId.includes('/') ? relId.slice(0, relId.indexOf('/')) : '';
      out.push({ id: relId, description, category });
    }
    let entries;
    try {
      entries = fs.readdirSync(absDir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const ent of entries) {
      if (!ent.isDirectory() || ent.name.startsWith('.')) continue;
      const subAbs = path.join(absDir, ent.name);
      const subRel = relId ? `${relId}/${ent.name}` : ent.name;
      walk(subAbs, subRel);
    }
  }
  walk(SCENARIOS_DIR, '');
  return out.sort((a, b) => a.id.localeCompare(b.id));
}

function normStdout(s) {
  return String(s || '')
    .replace(/\r\n/g, '\n')
    .replace(/\r/g, '\n')
    .split('\n')
    .map((l) => l.replace(/\s+$/, ''))
    .join('\n')
    .trim();
}

export function buildSetupCmd(statePath, action) {
  const t = action.type;
  if (t === 'generate') {
    const args = [
      'generate',
      '--width',
      String(Number(action.width)),
      '--height',
      String(Number(action.height)),
      '--out',
      statePath,
    ];
    if (action.openness != null) args.push('--openness', String(Number(action.openness)));
    if (action.seed != null) args.push('--seed', String(Number(action.seed)));
    if (action.turns != null) args.push('--turns', action.turns ? '1' : '0');
    if (action.turn_actions != null) args.push('--turn-actions', String(Number(action.turn_actions)));
    if (action.bot_steps) args.push('--bot-steps', String(Number(action.bot_steps)));
    return args;
  }
  if (t === 'add-player') {
    return [
      'add-player',
      '--state',
      statePath,
      '--name',
      String(action.name),
      '--x',
      String(Number(action.x)),
      '--y',
      String(Number(action.y)),
    ];
  }
  if (t === 'add-player-random') {
    return ['add-player-random', '--state', statePath, '--name', String(action.name)];
  }
  if (t === 'set-turns') {
    return ['set-turns', '--state', statePath, action.on !== false ? '1' : '0'];
  }
  if (t === 'init-turns') return ['init-turns', '--state', statePath];
  if (t === 'init-base') return ['init-base', '--state', statePath];
  if (t === 'give-item') {
    const args = ['give-item', '--state', statePath, '--name', String(action.name), '--item', String(action.item)];
    if (action.charges != null) args.push('--charges', String(Number(action.charges)));
    return args;
  }
  if (t === 'add-item') {
    const args = [
      'add-item',
      '--state',
      statePath,
      '--item',
      String(action.item),
      '--x',
      String(Number(action.x)),
      '--y',
      String(Number(action.y)),
    ];
    if (action.charges != null) args.push('--charges', String(Number(action.charges)));
    return args;
  }
  if (t === 'add-item-random') {
    const args = ['add-item-random', '--state', statePath, '--item', String(action.item)];
    if (action.charges != null) args.push('--charges', String(Number(action.charges)));
    return args;
  }
  if (t === 'set-cell') {
    return [
      'set-cell',
      '--state',
      statePath,
      '--x',
      String(Number(action.x)),
      '--y',
      String(Number(action.y)),
      String(action.cell),
    ];
  }
  throw new Error(`unknown setup type: ${t}`);
}

export function buildActionCmd(statePath, action) {
  const t = action.type;
  if (t === 'move') {
    return ['move', '--state', statePath, '--name', action.name, action.dir];
  }
  if (t === 'attack') {
    return ['attack', '--state', statePath, '--name', action.name, action.dir];
  }
  if (t === 'use-item') {
    return [
      'use-item',
      '--state',
      statePath,
      '--name',
      action.name,
      '--item',
      action.item,
      action.dir,
    ];
  }
  if (t === 'player-status') {
    return ['player-status', '--state', statePath, '--name', String(action.name)];
  }
  throw new Error(`unknown action type: ${t}`);
}

function appendStepOut(accum, piece) {
  const p = String(piece || '').trim();
  if (!p) return accum;
  return accum ? `${accum}\n${p}` : p;
}

function stepHasStdoutExpect(step) {
  if (step.expect_stdout != null) return true;
  const subs = step.expect_stdout_contains;
  return Array.isArray(subs) && subs.length > 0;
}

function checkExpect(stdoutAcc, stderrAcc, step) {
  const checks = [];
  let ok = true;
  if (step.expect_stdout != null) {
    if (normStdout(stdoutAcc) !== normStdout(String(step.expect_stdout))) {
      checks.push('expect_stdout mismatch');
      ok = false;
    }
  } else {
    const subs = Array.isArray(step.expect_stdout_contains) ? step.expect_stdout_contains : [];
    for (const sub of subs) {
      if (sub && !stdoutAcc.includes(sub)) {
        checks.push(`stdout missing: ${sub}`);
        ok = false;
      }
    }
  }
  const esubs = Array.isArray(step.expect_stderr_contains) ? step.expect_stderr_contains : [];
  for (const sub of esubs) {
    if (sub && !stderrAcc.includes(sub)) {
      checks.push(`stderr missing: ${sub}`);
      ok = false;
    }
  }
  return { ok, checks };
}

/**
 * Выполнить setup + script; проверки только по выводу.
 */
export async function runScenario(scenarioDirPath) {
  const sf = path.join(scenarioDirPath, SCENARIO_FILE);
  if (!fs.existsSync(sf)) return { ok: false, error: `нет ${SCENARIO_FILE}` };
  let data;
  try {
    data = JSON.parse(fs.readFileSync(sf, 'utf8'));
  } catch (e) {
    return { ok: false, error: String(e?.message || e) };
  }
  const desc = data.description || data.title || scenarioIdFromDirPath(scenarioDirPath);
  const setup = Array.isArray(data.setup) ? data.setup : [];
  const script = Array.isArray(data.script) ? data.script : [];

  const tmp = path.join(
    os.tmpdir(),
    `lab_scn_${Date.now()}_${Math.random().toString(36).slice(2)}.txt`,
  );
  function fail(payload) {
    try {
      fs.unlinkSync(tmp);
    } catch (_) {}
    return payload;
  }
  let accumulatedStdout = '';
  let accumulatedStderr = '';
  /** Накопленный вывод script[] — как expect_stdout при «Зафиксировать конец». */
  let scriptStdoutAcc = '';
  let scriptStderrAcc = '';
  function appendCliLog(out, err) {
    if (out) accumulatedStdout = accumulatedStdout ? `${accumulatedStdout}\n${out}` : out;
    if (err) accumulatedStderr = accumulatedStderr ? `${accumulatedStderr}\n${err}` : err;
  }
  try {
    for (let i = 0; i < setup.length; i++) {
      const step = setup[i];
      const cmd = buildSetupCmd(tmp, step);
      const r = await runLab(cmd);
      appendCliLog(r.out, r.err);
      if (r.code !== 0) {
        return fail({
          ok: false,
          error: `setup[${i}] failed`,
          exitCode: r.code,
          stdout: accumulatedStdout || r.out,
          stderr: accumulatedStderr || r.err,
          setupIndex: i,
        });
      }
    }

    for (let j = 0; j < script.length; j++) {
      const step = script[j];
      if (!validateManifestAction(step)) {
        return fail({ ok: false, error: `script[${j}]: некорректное действие`, scriptIndex: j });
      }
      const argv = buildActionCmd(tmp, step);
      const r = await runLab(argv);
      if (r.code !== 0) {
        appendCliLog(r.out, r.err);
        return fail({
          ok: false,
          error: `script[${j}] cli failed`,
          exitCode: r.code,
          stdout: accumulatedStdout,
          stderr: accumulatedStderr,
          scriptIndex: j,
        });
      }
      let accOut;
      let accErr;
      if (step.type === 'player-status') {
        accOut = r.out || '';
        accErr = r.err || '';
      } else {
        const r2 = await runLab(['resolve-bots', '--state', tmp]);
        if (r2.code !== 0) {
          appendCliLog(r.out, r.err);
          appendCliLog(r2.out, r2.err);
          return fail({
            ok: false,
            error: `script[${j}] resolve-bots failed`,
            exitCode: r2.code,
            stdout: accumulatedStdout,
            stderr: accumulatedStderr,
            scriptIndex: j,
          });
        }
        accOut = r.out || '';
        if (r2.out) accOut = accOut + (accOut ? '\n' : '') + r2.out;
        accErr = r.err || '';
        if (r2.err) accErr = accErr + (accErr ? '\n' : '') + r2.err;
      }

      scriptStdoutAcc = appendStepOut(scriptStdoutAcc, accOut);
      scriptStderrAcc = appendStepOut(scriptStderrAcc, accErr);
      const checkOut = stepHasStdoutExpect(step) ? scriptStdoutAcc : accOut;
      const checkErr = stepHasStdoutExpect(step) ? scriptStderrAcc : accErr;
      const { ok, checks } = checkExpect(checkOut, checkErr, step);
      appendCliLog(accOut, accErr);
      if (!ok) {
        return fail({
          ok: false,
          error: checks.join('; ') || 'expectation failed',
          scriptIndex: j,
          stdout: accumulatedStdout,
          stderr: accumulatedStderr,
          checks,
        });
      }
    }

    return {
      ok: true,
      id: scenarioIdFromDirPath(scenarioDirPath),
      description: desc,
      finalStatePath: tmp,
      stdout: accumulatedStdout,
      stderr: accumulatedStderr,
    };
  } catch (e) {
    try {
      fs.unlinkSync(tmp);
    } catch (_) {}
    return { ok: false, error: e?.message || String(e) };
  }
}

/** Только setup → временный state (для загрузки в песочницу). */
export async function runSetupOnly(scenarioDirPath) {
  const sf = path.join(scenarioDirPath, SCENARIO_FILE);
  if (!fs.existsSync(sf)) return { ok: false, error: `нет ${SCENARIO_FILE}` };
  const data = JSON.parse(fs.readFileSync(sf, 'utf8'));
  const setup = Array.isArray(data.setup) ? data.setup : [];
  const tmp = path.join(
    os.tmpdir(),
    `lab_setup_${Date.now()}_${Math.random().toString(36).slice(2)}.txt`,
  );
  try {
    for (let i = 0; i < setup.length; i++) {
      const step = setup[i];
      const r = await runLab(buildSetupCmd(tmp, step));
      if (r.code !== 0) {
        try {
          fs.unlinkSync(tmp);
        } catch (_) {}
        return {
          ok: false,
          error: `setup[${i}] failed`,
          stdout: r.out,
          stderr: r.err,
        };
      }
    }
    return { ok: true, tmpPath: tmp, description: data.description || data.title };
  } catch (e) {
    try {
      fs.unlinkSync(tmp);
    } catch (_) {}
    return { ok: false, error: e?.message || String(e) };
  }
}

/** Полный прогон (как pytest): для совместимости с replay-to-sandbox. */
export async function replayAllSteps(scenarioDirPath) {
  const r = await runScenario(scenarioDirPath);
  if (!r.ok) return r;
  return {
    ok: true,
    stdout: r.stdout || '',
    stderr: r.stderr || '',
    finalTmpPath: r.finalStatePath,
    id: r.id,
    description: r.description,
  };
}
