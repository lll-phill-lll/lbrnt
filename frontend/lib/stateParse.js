/**
 * Разбор state.txt (как в server.js parseTurnInfo + список имён из PLAYERS).
 */
import fs from 'fs';

export function parsePlayersFromStateText(txt) {
  const lines = String(txt).split('\n');
  const i = lines.findIndex((l) => l.startsWith('PLAYERS '));
  if (i === -1) return [];
  const n = Number(lines[i].split(/\s+/)[1]) || 0;
  const out = [];
  for (let k = 0; k < n; k++) {
    const line = lines[i + 1 + k];
    if (line === undefined) break;
    const name = line.trim().split(/\s+/)[0];
    if (name) out.push(name);
  }
  return out;
}

export function parseTurnInfoFromStateText(txt) {
  try {
    const arr = String(txt).split('\n');
    const ti = arr.findIndex((l) => l.startsWith('TURNS '));
    if (ti === -1) return { enforce: false, order: [], index: 0, current: null };
    const parts = arr[ti].split(/\s+/);
    const enforce = Number(parts[1]) !== 0;
    const cnt = Number(parts[3]) || 0;
    const order = [];
    for (let j = 0; j < cnt; j++) {
      const line = arr[ti + 1 + j];
      if (line !== undefined) order.push(line.trim());
    }
    const safeIdx = order.length ? (Number(parts[2]) || 0) % order.length : 0;
    return {
      enforce,
      order,
      index: safeIdx,
      current: order.length ? order[safeIdx] : null,
    };
  } catch {
    return { enforce: false, order: [], index: 0, current: null };
  }
}

export function readStateSnapshot(statePath) {
  if (!fs.existsSync(statePath)) return { players: [], turn: { enforce: false, order: [], current: null } };
  const txt = fs.readFileSync(statePath, 'utf8');
  return {
    players: parsePlayersFromStateText(txt),
    turn: parseTurnInfoFromStateText(txt),
  };
}
