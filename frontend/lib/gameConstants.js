/**
 * Единый источник правил игры для сервера и scenario-dev-server.
 * Добавили предмет / направление — правим здесь (и при необходимости в CLI/Python тестах).
 */

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

/** Предметы для use-item (как в CLI use-item --item) */
export const VALID_ITEMS = new Set(['knife', 'shotgun', 'rifle', 'flashlight', 'armor']);

export const DIR_RU = { up: 'вверх', down: 'вниз', left: 'влево', right: 'вправо' };
export const ITEM_RU = {
  knife: 'нож',
  shotgun: 'дробовик',
  rifle: 'ружьё',
  flashlight: 'фонарь',
  armor: 'броня',
};

/** Размещение предметов при создании комнаты (порядок — циклы add-item-random) */
export const ITEM_IDS = ['shotgun', 'rifle', 'flashlight', 'armor', 'knife'];

export const MAX_ITEM_PER_TYPE = 20;
export const MAX_ITEMS_TOTAL = 80;

/** Размер новой карты по умолчанию (лобби, песочница, серверные дефолты). */
export const DEFAULT_MAP_WIDTH = 8;
export const DEFAULT_MAP_HEIGHT = 8;

/** Только «оружие» для legacy payload.weapons (без обязательного ножа на карте) */
export const WEAPON_IDS_FOR_LOBBY = ['shotgun', 'rifle', 'flashlight', 'armor'];

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
