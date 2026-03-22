(() => {
  const API = '/api';

  let session = { room: 'sandbox', creatorToken: '' };
  /** Шаги setup для scenario.json — накапливаются при generate / add-player / предметах (не из движка). */
  let scenarioSetupSteps = [];
  let scenarioRecording = false;
  let scenarioActions = [];
  let scenarioExpectStdout = '';

  const logOut = document.getElementById('logOut');
  const logErr = document.getElementById('logErr');
  const scStatus = document.getElementById('scStatus');
  const playerPanels = document.getElementById('playerPanels');

  function escapeHtml(s) {
    const d = document.createElement('div');
    d.textContent = s;
    return d.innerHTML;
  }

  function esc(s) {
    return escapeHtml(String(s ?? ''));
  }

  function logStdout(chunk) {
    if (chunk == null || chunk === '') return;
    const s = String(chunk);
    logOut.textContent += s + (s.endsWith('\n') ? '' : '\n');
    logOut.scrollTop = logOut.scrollHeight;
  }
  function logStderr(chunk) {
    if (chunk == null || chunk === '') return;
    const s = String(chunk);
    logErr.textContent += s + (s.endsWith('\n') ? '' : '\n');
    logErr.scrollTop = logErr.scrollHeight;
  }
  /** Поля stdout/stderr из ответа API (labyrinth CLI). */
  function appendCli(data) {
    if (!data || typeof data !== 'object') return;
    if (data.stdout) logStdout(data.stdout);
    if (data.stderr) logStderr(data.stderr);
  }

  function appendScenarioPiece(accum, piece) {
    const p = String(piece || '').trim();
    if (!p) return accum;
    return accum ? accum + '\n' + p : p;
  }

  function recordScenario(resp, action) {
    if (!scenarioRecording || !resp?.ok) return;
    const raw = resp.scenarioTrace?.rawStdout ?? '';
    scenarioExpectStdout = appendScenarioPiece(scenarioExpectStdout, raw);
    scenarioActions.push(action);
  }

  async function jfetch(url, opt) {
    const r = await fetch(url, opt);
    const data = await r.json().catch(() => ({}));
    appendCli(data);
    if (!r.ok || !data.ok) throw new Error(data.error || r.statusText);
    return data;
  }

  async function loadSession() {
    const d = await jfetch(API + '/sandbox/session');
    session = { room: d.room, creatorToken: d.creatorToken };
  }

  async function refreshSvg() {
    const r = await fetch(API + '/sandbox/svg');
    if (!r.ok) throw new Error('svg');
    const svg = await r.text();
    document.getElementById('mapHolder').innerHTML = svg;
  }

  function attachItemTooltip(infoEl, displayName, description, rechargeHint) {
    const tip = document.createElement('div');
    tip.className = 'tooltip';
    tip.innerHTML = `<b>${esc(displayName)}</b><br>${esc(description)}<br><br><i>${esc(rechargeHint)}</i>`;
    infoEl.appendChild(tip);
    infoEl.addEventListener('mouseenter', () => {
      const r = infoEl.getBoundingClientRect();
      let x = r.right + 8;
      let y = r.top;
      tip.style.display = 'block';
      const tw = tip.offsetWidth;
      const th = tip.offsetHeight;
      if (x + tw > window.innerWidth) x = r.left - tw - 8;
      if (y + th > window.innerHeight) y = window.innerHeight - th - 8;
      if (y < 4) y = 4;
      tip.style.left = `${x}px`;
      tip.style.top = `${y}px`;
    });
    infoEl.addEventListener('mouseleave', () => {
      tip.style.display = 'none';
    });
  }

  function renderPlayerPanels(players, playerStatuses) {
    playerPanels.innerHTML = '';
    if (!players?.length) {
      playerPanels.innerHTML = '<p class="muted">Нет игроков — добавьте слева.</p>';
      return;
    }
    const dirs = [
      ['up', '↑'],
      ['left', '←'],
      ['down', '↓'],
      ['right', '→'],
    ];
    for (const name of players) {
      const st = playerStatuses?.[name];
      const items = st?.items || [];

      const panel = document.createElement('div');
      panel.className = 'player-panel';
      panel.dataset.player = name;

      const titleRow = document.createElement('div');
      titleRow.className = 'player-panel-title-row';
      const title = document.createElement('div');
      title.className = 'player-panel-name';
      title.style.marginBottom = '0';
      title.textContent = name;
      const dumpBtn = document.createElement('button');
      dumpBtn.type = 'button';
      dumpBtn.textContent = 'dump-status';
      dumpBtn.dataset.dumpStatus = '1';
      dumpBtn.dataset.player = name;
      dumpBtn.title = 'player-status в stdout и в сценарий при записи';
      titleRow.appendChild(title);
      titleRow.appendChild(dumpBtn);
      panel.appendChild(titleRow);

      const lblMove = document.createElement('div');
      lblMove.className = 'section-label';
      lblMove.textContent = 'Движение';
      panel.appendChild(lblMove);
      const rowMove = document.createElement('div');
      rowMove.className = 'row';
      for (const [d, lab] of dirs) {
        const b = document.createElement('button');
        b.type = 'button';
        b.dataset.move = d;
        b.textContent = lab;
        rowMove.appendChild(b);
      }
      panel.appendChild(rowMove);

      // Атака ножом только через карточку «Нож» в предметах (CLI attack = use-item knife).

      const lblItems = document.createElement('div');
      lblItems.className = 'section-label';
      lblItems.textContent = 'Предметы';
      panel.appendChild(lblItems);

      if (items.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'muted';
        empty.style.fontSize = '11px';
        empty.textContent = 'Нет предметов';
        panel.appendChild(empty);
      } else {
        for (const item of items) {
          const usable = item.charges > 0 && !item.broken;
          const card = document.createElement('div');
          card.className = `item-card${usable ? '' : ' disabled'}`;

          const header = document.createElement('div');
          header.className = 'item-header';
          const nameSpan = document.createElement('span');
          nameSpan.className = 'item-name';
          nameSpan.textContent = item.displayName || item.id;
          header.appendChild(nameSpan);

          const info = document.createElement('span');
          info.className = 'info-icon';
          info.textContent = 'i';
          attachItemTooltip(
            info,
            item.displayName || item.id,
            item.description || '',
            item.rechargeHint || '',
          );
          header.appendChild(info);

          const charges = document.createElement('span');
          charges.className = 'item-charges';
          charges.textContent = item.broken ? 'сломан' : `×${item.charges}`;
          header.appendChild(charges);
          card.appendChild(header);

          const noDirectionUse = item.id === 'armor' || item.id === 'treasure';
          if (usable && !noDirectionUse) {
            const itemDirs = document.createElement('div');
            itemDirs.className = 'item-dirs';
            for (const [label, dir] of [
              ['↑', 'up'],
              ['←', 'left'],
              ['↓', 'down'],
              ['→', 'right'],
            ]) {
              const btn = document.createElement('button');
              btn.type = 'button';
              btn.textContent = label;
              btn.dataset.itemUse = '1';
              btn.dataset.player = name;
              btn.dataset.item = item.id;
              btn.dataset.dir = dir;
              itemDirs.appendChild(btn);
            }
            card.appendChild(itemDirs);
          } else if (usable && item.id === 'armor') {
            const hint = document.createElement('div');
            hint.className = 'item-hint';
            hint.textContent =
              'Направление не выбирается — броня срабатывает автоматически при ударе. Описание — в подсказке (i).';
            card.appendChild(hint);
          } else if (usable && item.id === 'treasure') {
            const hint = document.createElement('div');
            hint.className = 'item-hint';
            hint.textContent =
              'Сокровище несите до выхода; направление для «использования» не выбирается. См. подсказку (i).';
            card.appendChild(hint);
          }
          panel.appendChild(card);
        }
      }

      playerPanels.appendChild(panel);
    }
  }

  playerPanels.addEventListener('click', async (e) => {
    const dumpEl = e.target.closest('button[data-dump-status]');
    if (dumpEl) {
      const name = dumpEl.dataset.player;
      if (!name) return;
      try {
        await postDumpStatus(name);
      } catch (err) {
        logStderr(`[err] ${err.message}`);
      }
      return;
    }
    const useBtn = e.target.closest('button[data-item-use]');
    if (useBtn) {
      const name = useBtn.dataset.player;
      const item = useBtn.dataset.item;
      const dir = useBtn.dataset.dir;
      if (!name || !item || !dir) return;
      try {
        await postAction('/use', { name, item, dir }, { type: 'use-item', name, item, dir });
      } catch (err) {
        logStderr(`[err] ${err.message}`);
      }
      return;
    }
    const btn = e.target.closest('button[data-move]');
    if (!btn) return;
    const panel = btn.closest('.player-panel');
    const name = panel?.dataset.player;
    if (!name) return;
    try {
      const dir = btn.dataset.move;
      await postAction('/move', { name, dir }, { type: 'move', name, dir });
    } catch (err) {
      logStderr(`[err] ${err.message}`);
    }
  });

  function populateGivePlayerSelect(players) {
    const sel = document.getElementById('givePl');
    if (!sel) return;
    const cur = sel.value;
    sel.innerHTML = '';
    const opt0 = document.createElement('option');
    opt0.value = '';
    opt0.textContent = '— игрок —';
    sel.appendChild(opt0);
    for (const n of players || []) {
      const o = document.createElement('option');
      o.value = n;
      o.textContent = n;
      sel.appendChild(o);
    }
    if (cur && [...sel.options].some((o) => o.value === cur)) sel.value = cur;
  }

  /** В закрытом виде — только id; при открытии списка показываем id + описание. */
  function expandScenarioSelectOptions(sel) {
    if (!sel) return;
    for (const opt of sel.options) {
      if (opt.dataset && opt.dataset.full != null) opt.textContent = opt.dataset.full;
    }
  }
  function collapseScenarioSelectOptions(sel) {
    if (!sel) return;
    for (const opt of sel.options) {
      if (opt.dataset && opt.dataset.short != null) opt.textContent = opt.dataset.short;
    }
  }

  async function refreshScenarioList() {
    try {
      const r = await fetch(API + '/scenarios');
      const raw = await r.text();
      let d = {};
      try {
        d = raw ? JSON.parse(raw) : {};
      } catch {
        d = {};
      }
      if (!r.ok || !d.ok) {
        logStderr(
          'Список сценариев недоступен (HTTP ' +
            r.status +
            '). Откройте песочницу через сервер (npm run dev или npm run dev:scenario), не как file://. ' +
            (d.error ? String(d.error) : raw.slice(0, 120)) +
            '\n'
        );
        return;
      }
      const sel = document.getElementById('openScId');
      if (!sel) return;
      const cur = sel.value;
      sel.innerHTML = '';
      const o0 = document.createElement('option');
      o0.value = '';
      o0.textContent = '— выберите —';
      sel.appendChild(o0);
      for (const s of d.scenarios || []) {
        const o = document.createElement('option');
        o.value = s.id;
        const desc = s.description != null ? String(s.description).trim() : '';
        o.dataset.short = s.id;
        o.dataset.full = desc && desc !== s.id ? `${s.id} — ${desc}` : s.id;
        o.textContent = s.id;
        sel.appendChild(o);
      }
      if (cur && [...sel.options].some((x) => x.value === cur)) sel.value = cur;
      collapseScenarioSelectOptions(sel);
    } catch (e) {
      logStderr(
        'Список сценариев: ' +
          (e?.message || e) +
          ' (нужен сервер с /api/scenarios)\n'
      );
    }
  }

  async function loadScenarioSandboxInitial(id) {
    if (!id) return;
    await loadSession();
    await jfetch(API + '/scenarios/' + encodeURIComponent(id) + '/sandbox-load-initial', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ room: session.room, creatorToken: session.creatorToken }),
    });
    scenarioSetupSteps = [];
    await refreshSnapshot();
    await refreshSvg();
  }

  async function replayScenarioToSandbox(id) {
    if (!id) return;
    await loadSession();
    await jfetch(API + '/scenarios/' + encodeURIComponent(id) + '/replay-to-sandbox', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ room: session.room, creatorToken: session.creatorToken }),
    });
    scenarioSetupSteps = [];
    await refreshSnapshot();
    await refreshSvg();
  }

  async function refreshSnapshot() {
    const d = await jfetch(API + '/sandbox/snapshot');
    renderPlayerPanels(d.players, d.playerStatuses);
    populateGivePlayerSelect(d.players);
    return d;
  }

  async function postAction(path, body, manifestAction) {
    const r = await fetch(API + '/sandbox' + path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    const data = await r.json().catch(() => ({}));
    appendCli(data);
    if (!r.ok || !data.ok) throw new Error(data.error || r.statusText);
    recordScenario(data, manifestAction);
    await refreshSnapshot();
    await refreshSvg();
    return data;
  }

  async function postDumpStatus(playerName) {
    const r = await fetch(API + '/sandbox/dump-status', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ name: playerName }),
    });
    const data = await r.json().catch(() => ({}));
    appendCli(data);
    if (!r.ok || !data.ok) throw new Error(data.error || r.statusText);
    recordScenario(data, { type: 'player-status', name: playerName });
    await refreshSnapshot();
    await refreshSvg();
    return data;
  }

  document.getElementById('btnGen').onclick = async () => {
    try {
      const width = Number(document.getElementById('gW').value);
      const height = Number(document.getElementById('gH').value);
      const openness = Number(document.getElementById('gO').value);
      const seed = Number(document.getElementById('gS').value);
      const botSteps = Number(document.getElementById('gBotSteps').value);
      const turnActions = Number(document.getElementById('gTurnActions').value);
      const genBody = {
        width,
        height,
        openness,
        seed,
        enforce_turns: false,
        turn_actions: Number.isFinite(turnActions) ? turnActions : 1,
      };
      if (Number.isFinite(botSteps) && botSteps > 0) genBody.bot_steps = botSteps;
      await jfetch(API + '/sandbox/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(genBody),
      });
      const setupGen = {
        type: 'generate',
        width,
        height,
        seed,
        openness,
        turns: false,
        turn_actions: Number.isFinite(turnActions) ? Math.min(Math.max(Math.floor(turnActions), 1), 10) : 1,
      };
      if (Number.isFinite(botSteps) && botSteps > 0) {
        setupGen.bot_steps = Math.min(Math.max(Math.floor(botSteps), 1), 5);
      }
      scenarioSetupSteps = [setupGen];
      scenarioRecording = false;
      scenarioActions = [];
      scenarioExpectStdout = '';
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      logStderr(String(e.message || e));
    }
  };

  document.getElementById('btnRefreshSvg').onclick = () => refreshSvg().catch((e) => logStderr(String(e)));

  document.getElementById('btnAddPl').onclick = async () => {
    try {
      const name = document.getElementById('plName').value.trim();
      const x = document.getElementById('plX').value;
      const y = document.getElementById('plY').value;
      const body = { name };
      if (x !== '' && y !== '') {
        body.x = Number(x);
        body.y = Number(y);
      }
      await jfetch(API + '/sandbox/add-player', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      if (body.x != null && body.y != null) {
        scenarioSetupSteps.push({ type: 'add-player', name, x: body.x, y: body.y });
      } else {
        scenarioSetupSteps.push({ type: 'add-player-random', name });
      }
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      logStderr(String(e.message || e));
    }
  };

  document.getElementById('btnAddItem').onclick = async () => {
    try {
      const item = document.getElementById('selItem').value;
      const x = Number(document.getElementById('itX').value);
      const y = Number(document.getElementById('itY').value);
      await jfetch(API + '/sandbox/add-item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ item, x, y }),
      });
      scenarioSetupSteps.push({ type: 'add-item', item, x, y, charges: 1 });
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      logStderr(String(e.message || e));
    }
  };

  document.getElementById('btnGiveItem').onclick = async () => {
    try {
      const name = document.getElementById('givePl').value.trim();
      const item = document.getElementById('giveItem').value;
      if (!name) {
        logStderr('Выберите игрока\n');
        return;
      }
      await jfetch(API + '/sandbox/give-item', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name, item }),
      });
      scenarioSetupSteps.push({ type: 'give-item', name, item, charges: 1 });
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      logStderr(String(e.message || e));
    }
  };

  const openScIdEl = document.getElementById('openScId');
  openScIdEl.addEventListener('focus', () => expandScenarioSelectOptions(openScIdEl));
  openScIdEl.addEventListener('blur', () => collapseScenarioSelectOptions(openScIdEl));
  openScIdEl.addEventListener('change', async () => {
    const id = openScIdEl.value.trim();
    collapseScenarioSelectOptions(openScIdEl);
    if (!id) return;
    try {
      await loadScenarioSandboxInitial(id);
    } catch (e) {
      logStderr(String(e.message || e));
    }
  });

  document.getElementById('btnScReplay').onclick = async () => {
    const id = openScIdEl.value.trim();
    if (!id) {
      logStderr('Выберите сценарий\n');
      return;
    }
    try {
      await replayScenarioToSandbox(id);
    } catch (e) {
      logStderr(String(e.message || e));
    }
  };

  document.getElementById('scBefore').onclick = async () => {
    const id = document.getElementById('scId').value.trim();
    if (!id) {
      scStatus.textContent = 'Укажите id';
      return;
    }
    try {
      await loadSession();
      const description = document.getElementById('scDesc').value.trim();
      if (!scenarioSetupSteps.length) {
        scStatus.textContent =
          'Нет шагов setup: сначала «Сгенерировать» и при необходимости игроков/предметы (шаги пишутся в сценарий).';
        return;
      }
      await jfetch(API + '/scenarios/' + encodeURIComponent(id) + '/capture-initial', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          room: session.room,
          creatorToken: session.creatorToken,
          description,
          setup: scenarioSetupSteps,
        }),
      });
      scenarioRecording = true;
      scenarioActions = [];
      scenarioExpectStdout = '';
      scStatus.textContent =
        'Очерёдность ходов включена, запись ходов включена. Дальше — «Зафиксировать конец» (сохранит в scenario.json).';
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      scStatus.textContent = String(e.message);
    }
  };

  document.getElementById('scAfter').onclick = async () => {
    const id = document.getElementById('scId').value.trim();
    if (!id) {
      scStatus.textContent = 'Укажите id';
      return;
    }
    if (!scenarioRecording && !scenarioActions.length) {
      scStatus.textContent = 'Сначала «Зафиксировать начало».';
      return;
    }
    if (!scenarioActions.length) {
      scStatus.textContent = 'Нет записанных ходов после начала';
      return;
    }
    try {
      await loadSession();
      const description = document.getElementById('scDesc').value.trim();
      await jfetch(API + '/scenarios/' + encodeURIComponent(id) + '/capture-final', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          room: session.room,
          creatorToken: session.creatorToken,
          description,
          actions: scenarioActions,
          expect_stdout: scenarioExpectStdout,
          expect_stdout_contains: [],
        }),
      });
      scenarioRecording = false;
      scenarioActions = [];
      scenarioExpectStdout = '';
      scStatus.textContent = 'Сохранено в tests/scenarios/' + id + '/scenario.json. pytest tests/test_scenarios.py';
    } catch (e) {
      scStatus.textContent = String(e.message);
    }
  };

  (async () => {
    try {
      await loadSession();
      // Список сценариев не зависит от песочницы — грузим до ensure, иначе при падении ensure выпадающий список остаётся пустым.
      await refreshScenarioList();
      await jfetch(API + '/sandbox/ensure');
      await refreshSnapshot();
      await refreshSvg();
    } catch (e) {
      logStderr('Init: ' + (e.message || e));
      try {
        await refreshScenarioList();
      } catch (_) {
        /* ignore */
      }
    }
  })();
})();
