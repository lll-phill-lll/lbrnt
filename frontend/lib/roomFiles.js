/**
 * Пути и метаданные файлов комнаты в rooms/ — общие для game server и scenario API.
 */
import fs from 'fs';
import path from 'path';
import { ROOMS_DIR } from './repoPaths.js';

export function stateFile(room) {
  return path.join(ROOMS_DIR, `${room}.txt`);
}

export function svgFile(room) {
  return path.join(ROOMS_DIR, `${room}.svg`);
}

export function metaFile(room) {
  return path.join(ROOMS_DIR, `${room}.json`);
}

export function readMeta(room) {
  try {
    return JSON.parse(fs.readFileSync(metaFile(room), 'utf8'));
  } catch {
    return null;
  }
}

export function writeMeta(room, meta) {
  fs.writeFileSync(metaFile(room), JSON.stringify(meta, null, 2), 'utf8');
}
