/**
 * Dev: API сценариев + песочница (без лобби) на одном порту.
 * Запуск: npm run dev:scenario → http://127.0.0.1:5174/
 */
import express from 'express';
import path from 'path';
import { fileURLToPath } from 'url';
import { createScenarioApiRouter, scenarioCorsMiddleware } from './lib/scenarioHttpApi.js';
import { createSandboxApiRouter } from './lib/sandboxHttpApi.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const app = express();
app.use(scenarioCorsMiddleware);
app.use(express.json({ limit: '2mb' }));
app.use('/api', createScenarioApiRouter());
app.use('/api/sandbox', createSandboxApiRouter());

app.get('/', (_req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'dev-sandbox.html'));
});
app.use(express.static(path.join(__dirname, 'public')));

const PORT = Number(process.env.PORT || 5174);
const HOST = process.env.HOST || '127.0.0.1';

const server = app.listen(PORT, HOST, () => {
  console.log(`Scenario dev: http://${HOST}:${PORT}/  (песочница + /api/scenarios)`);
});

server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(
      `Порт ${PORT} уже занят (часто — другой dev:scenario). Варианты:\n` +
        `  • Закрыть процесс:  lsof -i :${PORT}   затем kill <PID>\n` +
        `  • Другой порт:       PORT=5175 npm run dev:scenario`,
    );
    process.exit(1);
  }
  console.error(err);
  process.exit(1);
});
