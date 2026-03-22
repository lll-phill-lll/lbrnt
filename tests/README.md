# Тесты

## Запуск

Нужны `cmake`, компилятор C++, `pip install pytest`. Ручной `cmake --build` не обязателен: при отсутствии `build/labyrinth` pytest сам вызовет `cmake -B build` и сборку.

```bash
pip install pytest
pytest tests/test_scenarios.py -v
```

Один сценарий:

```bash
pytest tests/test_scenarios.py -v -k breathe
```

## Добавить новый сценарий

```bash
cd frontend && npm install && npm run dev:scenario
```

Открыть **http://127.0.0.1:5174/** — песочница. В блоке «Сценарий» указать id (`категория/имя`), создать карту (generate, игроки, предметы): setup и ходы накапливаются автоматически. **Сохранить сценарий** записывает `tests/scenarios/…/scenario.json` (перезапись). Дальше — `pytest tests/test_scenarios.py`.

### Очерёдность ходов и `expect_stdout`

- **`init-turns`** всегда заново строит очередь из `random_seed` (как в `generate`) и текущего набора игроков, чтобы pytest и запись в dev давали **один и тот же** порядок ходов при одном `scenario.json`.
- **`expect_stdout` в последнем шаге `script`** — это **склеенный stdout всех игровых шагов** (как накапливает dev при сохранении); pytest сравнивает с накопленным выводом, а не только с последним ходом.
- Сообщения игроку в stdout — **wire-коды** (`MOVED:down`, `HOSPITAL_ENTER`, …), как в `message.hpp` / `frontend/lib/messageParse.js`. Человекочитаемый текст — только в JS.

3. Проверка:

```bash
pytest tests/test_scenarios.py -v -k '<id>'
```

## Канонизация эталона stdout

При падении только из‑за несовпадения ожидаемых сообщений с фактическим выводом движка:

```bash
pytest tests/test_scenarios.py -v --canonize
```

