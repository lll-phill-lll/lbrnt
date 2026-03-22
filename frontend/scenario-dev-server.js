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

app.listen(PORT, HOST, () => {
  console.log(`Scenario dev: http://${HOST}:${PORT}/  (песочница + /api/scenarios)`);
});
