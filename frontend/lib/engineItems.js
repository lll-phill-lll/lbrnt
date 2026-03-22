/**
 * Реестр предметов из бинарника движка (`labyrinth list-items`).
 * Единый источник id и подписей — C++ (makeItem + ITEM_REGISTRY_*).
 */
import { spawnSync } from 'child_process';
import fs from 'fs';
import { LAB_BIN } from './repoPaths.js';

let cache = null;

/**
 * @returns {{
 *   ids: string[],
 *   placeOrder: string[],
 *   lobbyWeapons: string[],
 *   displayNames: Record<string, string>
 * }}
 */
export function loadEngineItemRegistry() {
  if (cache) return cache;
  if (!fs.existsSync(LAB_BIN)) {
    throw new Error(
      `Не найден бинарник движка: ${LAB_BIN}\nСоберите проект: cmake -S . -B build && cmake --build build`,
    );
  }
  const r = spawnSync(LAB_BIN, ['list-items'], {
    encoding: 'utf8',
    maxBuffer: 1024 * 1024,
  });
  if (r.status !== 0) {
    throw new Error(`list-items failed (${r.status}): ${r.stderr || r.stdout || ''}`);
  }
  const raw = (r.stdout || '').trim();
  const data = JSON.parse(raw);
  if (!Array.isArray(data.ids) || !data.ids.length) {
    throw new Error('list-items: ожидалось непустое поле ids');
  }
  cache = data;
  return cache;
}
