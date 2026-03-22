/**
 * Пути к корню репозитория и артефактам сборки (ESM).
 */
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

/** Корень репозитория (родитель каталога frontend/) */
export const REPO_ROOT = path.resolve(__dirname, '..', '..');

export const LAB_BIN = path.join(REPO_ROOT, 'build', 'labyrinth');
export const ROOMS_DIR = path.join(REPO_ROOT, 'rooms');
export const SCENARIOS_DIR = path.join(REPO_ROOT, 'tests', 'scenarios');
