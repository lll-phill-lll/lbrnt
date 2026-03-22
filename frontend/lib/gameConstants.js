/**
 * Правила валидации для сервера и scenario-dev-server.
 * Список предметов и подписи — из движка (`labyrinth list-items`), см. engineItems.js.
 */
import { loadEngineItemRegistry } from './engineItems.js';

export const SAFE_ROOM_ID_RE = /^[a-zA-Z0-9_-]{1,40}$/;

export function validateRoomId(id) {
  return typeof id === 'string' && SAFE_ROOM_ID_RE.test(id);
}

/** id сценария: `имя_папки` или `категория/имя` (tests/scenarios/…/scenario.json). */
export const SAFE_SCENARIO_ID_RE =
  /^(?:[a-zA-Z0-9_-]{1,40})(?:\/[a-zA-Z0-9_-]{1,40}){0,9}$/;

export function validateScenarioId(id) {
  if (typeof id !== 'string') return false;
  if (!SAFE_SCENARIO_ID_RE.test(id)) return false;
  if (id.includes('..')) return false;
  return true;
}

/** Имена игроков (как в server.js) */
export const SAFE_PLAYER_NAME_RE = /^[^\s<>&"']{1,30}$/;

export function validatePlayerName(name) {
  return typeof name === 'string' && SAFE_PLAYER_NAME_RE.test(name);
}

/** Направления для move / attack / use-item */
export const VALID_DIRS = new Set(['up', 'down', 'left', 'right']);

const _reg = loadEngineItemRegistry();

/** Предметы для use-item / give-item / add-item (id из движка). */
export const VALID_ITEMS = new Set(_reg.ids);

/** Короткие подписи для чата (displayName из классов Item в C++). */
export const ITEM_RU = _reg.displayNames;

/** Порядок размещения предметов при создании комнаты (циклы add-item-random). */
export const ITEM_IDS = [..._reg.placeOrder];

export const MAX_ITEM_PER_TYPE = 20;
export const MAX_ITEMS_TOTAL = 80;

/** Снаряжение лобби (без обязательного ножа на карте). */
export const WEAPON_IDS_FOR_LOBBY = [..._reg.lobbyWeapons];

/** Размер новой карты по умолчанию (лобби, песочница, серверные дефолты). */
export const DEFAULT_MAP_WIDTH = 8;
export const DEFAULT_MAP_HEIGHT = 8;

export const DIR_RU = { up: 'вверх', down: 'вниз', left: 'влево', right: 'вправо' };

/**
 * Проверка действия для manifest (те же типы, что и сокет use / CLI).
 */
export function validateManifestAction(a) {
  if (!a || typeof a !== 'object' || !a.type) return false;
  const name = a.name;
  if (typeof name !== 'string' || !name.trim()) return false;
  const t = a.type;
  if (t === 'move' || t === 'attack') {
    return VALID_DIRS.has(a.dir);
  }
  if (t === 'use-item') {
    return VALID_DIRS.has(a.dir) && VALID_ITEMS.has(a.item);
  }
  return false;
}
