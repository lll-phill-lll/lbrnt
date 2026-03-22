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

Открыть **http://127.0.0.1:5174/** — песочница. В блоке «Сценарий» указать id (`категория/имя`), создать карту (generate, игроки, предметы) В сценарий будут записаны действия, произведенные между нажатиями **Зафиксировать начало** → **Зафиксировать конец**. Дальше этот сценарий можно тестировать через pytest.

3. Проверка:

```bash
pytest tests/test_scenarios.py -v -k '<id>'
```

## Канонизация эталона stdout

При падении только из‑за несовпадения ожидаемых сообщений с фактическим выводом движка:

```bash
pytest tests/test_scenarios.py -v --canonize
```

В репозитории: при открытии PR те же тесты гоняются в **GitHub Actions** (`.github/workflows/ci.yml`).
