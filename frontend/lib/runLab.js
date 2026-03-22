import { spawn } from 'child_process';
import { LAB_BIN } from './repoPaths.js';

/**
 * Запуск бинарника labyrinth; stdout/stderr обрезаются по краям как в server.js.
 */
export function runLab(args) {
  return new Promise((resolve) => {
    const p = spawn(LAB_BIN, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '';
    let err = '';
    p.stdout.on('data', (d) => { out += d.toString(); });
    p.stderr.on('data', (d) => { err += d.toString(); });
    p.on('close', (code) => resolve({ code, out: out.trim(), err: err.trim() }));
  });
}
