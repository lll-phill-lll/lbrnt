(() => {
  const $ = id => document.getElementById(id);

  // ── Session from URL or sessionStorage ──
  const params = new URLSearchParams(window.location.search);
  let session = { room: params.get('room'), name: params.get('name'), password: params.get('password') || '' };
  if (!session.room || !session.name) {
    try { session = JSON.parse(sessionStorage.getItem('lab_session') || '{}'); } catch {}
  }
  if (!session.room || !session.name) {
    window.location.href = '/';
    return;
  }
  sessionStorage.setItem('lab_session', JSON.stringify(session));
  // Clean URL
  if (window.location.search) history.replaceState(null, '', '/game');

  let isCreator = false;

  // ── DOM ──
  const canvas       = $('drawCanvas');
  const ctx          = canvas.getContext('2d', { alpha: false });
  const coordLabel   = $('coordLabel');
  const zoomLabelEl  = $('zoomLabel');
  const turnInfo     = $('turnInfo');
  const playerLabel  = $('playerLabel');
  const itemsPanel   = $('itemsPanel');
  let currentItems = [];
  let selectedItemId = null;
  const drawColorEl  = $('drawColor');
  const WALL_THIN = 3, WALL_THICK = 6, TRAIL_WIDTH = 2, LINE_WIDTH = 2;
  const clearDrawEl  = $('clearDraw');
  const paletteEl    = $('palette');
  const creatorCard  = $('creatorCard');
  const viewMapBtn   = $('viewMapBtn');
  const broadcastMapBtn = $('broadcastMapBtn');
  const mapOverlay   = $('mapOverlay');
  const mapContent   = $('mapContent');
  const closeMap     = $('closeMap');
  const roomTitle    = $('roomTitle');
  const toastContainer = $('toastContainer');

  const helpOverlay = $('helpOverlay');
  $('helpBtn').addEventListener('click', () => helpOverlay.classList.remove('hidden'));
  $('closeHelp').addEventListener('click', () => helpOverlay.classList.add('hidden'));
  helpOverlay.addEventListener('click', e => { if (e.target === helpOverlay) helpOverlay.classList.add('hidden'); });

  roomTitle.textContent = `Комната: ${session.room}`;
  playerLabel.textContent = session.name;

  function showToast(text, cls, duration) {
    const el = document.createElement('div');
    el.className = 'toast ' + (cls || '');
    el.innerHTML = text;
    toastContainer.appendChild(el);
    const ttl = duration || 3500;
    setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 250); }, ttl);
    while (toastContainer.children.length > 6) toastContainer.firstChild.remove();
  }
  function toastFeedback(lines, who) {
    const isSelf = who === session.name;
    const prefix = who ? `<span class="toast-who">${who}:</span> ` : '';
    const body = (Array.isArray(lines) ? lines : [lines]).join('<br>');
    showToast(prefix + body, isSelf ? 'self' : 'other');
  }
  function toastSystem(text) { showToast(text, 'system', 2500); }

  // ── Socket ──
  const socket = io('/', { transports: ['websocket'] });

  socket.on('connect', () => {
    toastSystem('Подключено');
    socket.emit('enterGame', { room: session.room, name: session.name, password: session.password }, resp => {
      if (resp?.ok) {
        isCreator = !!resp.isCreator;
        if (isCreator) creatorCard.classList.remove('hidden');
        if (resp.turn) updateTurn(resp.turn);
        if (resp.playerStatus) handlePlayerStatus(resp.playerStatus);
        else refreshStatus();
      } else {
        toastSystem('Ошибка: ' + (resp?.error || ''));
      }
    });
  });
  socket.on('disconnect', () => toastSystem('Отключено'));
  socket.on('feedback', msg => {
    const lines = Array.isArray(msg?.lines) ? msg.lines : [String(msg?.lines ?? msg ?? '')];
    toastFeedback(lines, msg?.who || session.name);
    refreshStatus();
  });
  socket.on('gameBroadcast', msg => {
    if (msg?.who === session.name) return;
    toastFeedback(msg.lines || [], msg.who);
  });
  socket.on('turn', t => updateTurn(t));

  // ── Chat ──
  const chatInput = $('chatInput');
  const chatSend  = $('chatSend');
  function sendChat() {
    const text = chatInput.value.trim();
    if (!text) return;
    chatInput.value = '';
    socket.emit('chatMessage', { text }, () => {});
  }
  chatSend.addEventListener('click', sendChat);
  chatInput.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); sendChat(); } });
  socket.on('chatMsg', msg => {
    if (!msg?.who || !msg?.text) return;
    toastFeedback([msg.text], msg.who);
  });

  const turnOrderEl = $('turnOrder');
  function updateTurn(t) {
    const enforce = t?.enforce !== false;
    const cur = t?.current;
    const order = Array.isArray(t?.order) ? t.order : [];

    if (!enforce) {
      turnInfo.textContent = 'Свободный режим — ходят все';
      turnInfo.style.color = '#9aa4b2';
      turnOrderEl.innerHTML = '';
      turnsToggle.checked = false;
      return;
    }
    turnsToggle.checked = true;
    if (!cur) { turnInfo.textContent = 'Ожидание...'; turnOrderEl.innerHTML = ''; return; }
    const isMe = cur === session.name;
    turnInfo.textContent = isMe ? '>>> Ваш ход! <<<' : `Ходит: ${cur}`;
    turnInfo.style.color = isMe ? '#51cf66' : '#9aa4b2';
    turnOrderEl.innerHTML = '';
    for (const name of order) {
      const badge = document.createElement('span');
      badge.className = 'turn-badge' + (name === cur ? ' current' : '') + (name === session.name ? ' me' : '');
      badge.textContent = name;
      turnOrderEl.appendChild(badge);
    }
  }

  function emit(ev, payload) { return new Promise(resolve => socket.emit(ev, payload, resp => resolve(resp))); }

  // ── Game actions ──
  async function doMove(dir)   { await emit('move', { dir }); }
  async function doAttack(dir) { await emit('attack', { dir }); }
  async function doUse(dir, itemId) { await emit('use', { dir, item: itemId || selectedItemId }); }

  document.querySelectorAll('[data-move]').forEach(b => b.addEventListener('click', () => doMove(b.dataset.move)));
  document.querySelectorAll('[data-attack]').forEach(b => b.addEventListener('click', () => doAttack(b.dataset.attack)));

  function renderItemsPanel(items) {
    currentItems = items || [];
    itemsPanel.innerHTML = '';
    if (currentItems.length === 0) {
      itemsPanel.innerHTML = '<div class="muted">Нет предметов</div>';
      return;
    }
    for (const item of currentItems) {
      const card = document.createElement('div');
      const usable = item.charges > 0 && !item.broken;
      card.className = 'item-card' + (usable ? '' : ' disabled');

      const header = document.createElement('div');
      header.className = 'item-header';
      const nameSpan = document.createElement('span');
      nameSpan.className = 'item-name';
      nameSpan.textContent = item.displayName;
      header.appendChild(nameSpan);

      const info = document.createElement('span');
      info.className = 'info-icon';
      info.textContent = 'i';
      const tip = document.createElement('div');
      tip.className = 'tooltip';
      tip.innerHTML = '<b>' + item.displayName + '</b><br>' + item.description + '<br><br><i>' + item.rechargeHint + '</i>';
      info.appendChild(tip);
      info.addEventListener('mouseenter', () => {
        const r = info.getBoundingClientRect();
        let x = r.right + 8, y = r.top;
        tip.style.display = 'block';
        const tw = tip.offsetWidth, th = tip.offsetHeight;
        if (x + tw > window.innerWidth) x = r.left - tw - 8;
        if (y + th > window.innerHeight) y = window.innerHeight - th - 8;
        if (y < 4) y = 4;
        tip.style.left = x + 'px';
        tip.style.top = y + 'px';
      });
      info.addEventListener('mouseleave', () => { tip.style.display = 'none'; });
      header.appendChild(info);

      const charges = document.createElement('span');
      charges.className = 'item-charges';
      charges.textContent = item.broken ? 'сломан' : ('×' + item.charges);
      header.appendChild(charges);
      card.appendChild(header);

      if (usable) {
        const dirs = document.createElement('div');
        dirs.className = 'item-dirs';
        for (const [label, dir] of [['↑','up'],['←','left'],['↓','down'],['→','right']]) {
          const btn = document.createElement('button');
          btn.textContent = label;
          btn.addEventListener('click', () => doUse(dir, item.id));
          dirs.appendChild(btn);
        }
        card.appendChild(dirs);
      }
      itemsPanel.appendChild(card);
    }
    if (currentItems.length > 0 && !selectedItemId) selectedItemId = currentItems[0].id;
  }

  function handlePlayerStatus(data) {
    if (data?.items) renderItemsPanel(data.items);
    if (data?.nearbyBreathing) toastSystem('Вы чувствуете чьё-то дыхание поблизости...');
  }

  socket.on('playerStatus', data => handlePlayerStatus(data));

  function refreshStatus() {
    emit('getPlayerStatus', {}).then(resp => {
      if (resp?.ok) handlePlayerStatus(resp);
    });
  }

  window.addEventListener('keydown', e => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
    const dirs = { ArrowUp:'up', ArrowDown:'down', ArrowLeft:'left', ArrowRight:'right' };
    const dir = dirs[e.key];
    if (!dir) return;
    if (!selectedShape) return;
    e.preventDefault();
    pushUndo();
    const oldCX = selectedShape.cx, oldCY = selectedShape.cy;
    const step = e.shiftKey ? 5 : 1;
    if (dir === 'left') selectedShape.cx -= step;
    if (dir === 'right') selectedShape.cx += step;
    if (dir === 'up') selectedShape.cy -= step;
    if (dir === 'down') selectedShape.cy += step;
    if (trailDraw) addTrail(oldCX,oldCY,selectedShape.cx,selectedShape.cy,selectedShape.color);
    render(); saveDraw();
  }, { passive: false });

  window.addEventListener('keydown', e => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
    if ((e.key === 'Delete' || e.key === 'Backspace') && selectedShape) {
      e.preventDefault();
      pushUndo();
      shapes = shapes.filter(s => s !== selectedShape);
      selectedShape = null;
      render(); saveDraw();
    }
    if ((e.ctrlKey || e.metaKey) && e.key === 'z') {
      e.preventDefault();
      popUndo();
    }
  }, { passive: false });

  // ── View map + replay (creator) ──
  let replayEntries = [];
  let replayTotal = 0;
  let replayCurrentStep = 0;
  let replayLoading = false;
  const replayStepLabel = $('replayStep');
  const replayLogEntry  = $('replayLogEntry');
  const replayFullLog   = $('replayFullLog');

  function renderReplayStep(step) {
    replayStepLabel.textContent = `${step} / ${replayTotal}`;
    replayLogEntry.textContent = step > 0 && step <= replayEntries.length
      ? `${step}. ${replayEntries[step - 1]}` : step === 0 ? 'Начальное состояние' : '';
    replayFullLog.querySelectorAll('.log-line').forEach((el, i) => {
      el.classList.toggle('active', i === step - 1);
    });
    const activeLine = replayFullLog.querySelector('.log-line.active');
    if (activeLine) activeLine.scrollIntoView({ block: 'nearest' });
  }

  async function loadReplaySvg(step) {
    if (replayLoading) return;
    replayLoading = true;
    if (step === replayTotal) {
      const resp = await emit('viewMap', {});
      replayLoading = false;
      if (resp?.ok && resp.svg) {
        mapContent.innerHTML = resp.svg;
        if (resp.replay) { replayEntries = resp.replay.entries; replayTotal = resp.replay.total; }
      }
      return;
    }
    const resp = await emit('replaySvg', { step });
    replayLoading = false;
    if (resp?.ok && resp.svg) mapContent.innerHTML = resp.svg;
    else toastSystem('Ошибка: ' + (resp?.error || ''));
  }

  async function gotoStep(step) {
    step = Math.max(0, Math.min(step, replayTotal));
    if (step === replayCurrentStep) return;
    replayCurrentStep = step;
    renderReplayStep(step);
    await loadReplaySvg(step);
  }

  viewMapBtn.addEventListener('click', async () => {
    $('replayControls').style.display = '';
    $('replayLogEntry').style.display = '';
    const det = mapOverlay.querySelector('details');
    if (det) det.style.display = '';
    viewMapBtn.textContent = 'Загрузка...';
    const resp = await emit('viewMap', {});
    viewMapBtn.textContent = 'Показать лабиринт';
    if (resp?.ok && resp.svg) {
      replayEntries = resp.replay?.entries || [];
      replayTotal = resp.replay?.total || 0;
      replayCurrentStep = replayTotal;
      mapContent.innerHTML = resp.svg;
      renderReplayStep(replayCurrentStep);
      replayFullLog.innerHTML = replayEntries.map((e, i) =>
        `<div class="log-line${i === replayCurrentStep - 1 ? ' active' : ''}" data-step="${i + 1}" style="cursor:pointer;padding:2px 4px;">${i + 1}. ${e}</div>`
      ).join('');
      replayFullLog.querySelectorAll('.log-line').forEach(el => {
        el.addEventListener('click', () => gotoStep(Number(el.dataset.step)));
      });
      mapOverlay.classList.remove('hidden');
    } else {
      toastSystem('Ошибка: ' + (resp?.error || ''));
    }
  });
  $('replayFirst').addEventListener('click', () => gotoStep(0));
  $('replayPrev').addEventListener('click', () => gotoStep(replayCurrentStep - 1));
  $('replayNext').addEventListener('click', () => gotoStep(replayCurrentStep + 1));
  $('replayLast').addEventListener('click', () => gotoStep(replayTotal));
  closeMap.addEventListener('click', () => mapOverlay.classList.add('hidden'));
  mapOverlay.addEventListener('click', e => { if (e.target === mapOverlay) mapOverlay.classList.add('hidden'); });
  document.addEventListener('keydown', e => {
    if (mapOverlay.classList.contains('hidden')) return;
    if (e.key === 'ArrowLeft') { e.preventDefault(); gotoStep(replayCurrentStep - 1); }
    else if (e.key === 'ArrowRight') { e.preventDefault(); gotoStep(replayCurrentStep + 1); }
    else if (e.key === 'Home') { e.preventDefault(); gotoStep(0); }
    else if (e.key === 'End') { e.preventDefault(); gotoStep(replayTotal); }
    else if (e.key === 'Escape') { e.preventDefault(); mapOverlay.classList.add('hidden'); }
  });

  // ── Broadcast map to all players (creator) ──
  broadcastMapBtn.addEventListener('click', async () => {
    broadcastMapBtn.textContent = 'Отправка...';
    const resp = await emit('broadcastMap', {});
    broadcastMapBtn.textContent = 'Показать всем';
    if (resp?.ok) toastSystem('Карта отправлена всем игрокам');
    else toastSystem('Ошибка: ' + (resp?.error || ''));
  });

  socket.on('mapRevealed', data => {
    if (data?.svg) {
      mapContent.innerHTML = data.svg;
      // Hide replay controls for broadcast view
      $('replayControls').style.display = 'none';
      $('replayLogEntry').style.display = 'none';
      const details = mapOverlay.querySelector('details');
      if (details) details.style.display = 'none';
      mapOverlay.classList.remove('hidden');
    }
  });

  // ── Turns toggle (creator only) ──
  const turnsToggle = $('turnsToggle');
  turnsToggle.addEventListener('change', async () => {
    const resp = await emit('setTurns', { enabled: turnsToggle.checked });
    if (!resp?.ok) {
      toastSystem('Ошибка: ' + (resp?.error || ''));
      turnsToggle.checked = !turnsToggle.checked;
    }
  });
  socket.on('turnsToggled', data => {
    turnsToggle.checked = !!data?.enabled;
    toastSystem(data?.enabled ? 'Очерёдность ходов включена' : 'Очерёдность ходов отключена — ходят все');
  });

  // ══════════════════════════════════════════
  //  DRAWING ENGINE
  // ══════════════════════════════════════════

  const COLORS = [
    "#ffcc66","#ff6b6b","#f06595","#cc5de8","#845ef7","#5c7cfa",
    "#339af0","#22b8cf","#20c997","#51cf66","#94d82d","#fcc419",
    "#ffffff","#ced4da","#868e96","#495057","#212529","#000000",
  ];
  let activeColor = COLORS[0];
  const STORAGE_KEY = 'lab_draw_v2_' + session.room + '_' + session.name;

  function renderPalette() {
    paletteEl.innerHTML = '';
    for (const c of COLORS) {
      const d = document.createElement('div');
      d.className = 'sw' + (c.toLowerCase() === activeColor.toLowerCase() ? ' active' : '');
      d.style.background = c;
      d.addEventListener('click', () => { activeColor = c; drawColorEl.value = c; renderPalette(); });
      paletteEl.appendChild(d);
    }
  }
  drawColorEl.addEventListener('input', () => { activeColor = drawColorEl.value; renderPalette(); });
  renderPalette();

  let drawMode = 'draw';
  const EDGE_ZONE = 0.22;
  let drawLock = null;
  let cellSize = 28, zoom = 1.0, panX = 0, panY = 0, dpr = 1;
  const cells = new Map();
  let shapes = [], selectedShape = null, lines = [], lineStart = null;
  let pointerDown = false, panning = false, lastPX = 0, lastPY = 0, draggingShape = null, hover = null, borderPreview = null;
  let trailDraw = true, dragPrevCX = 0, dragPrevCY = 0;

  // Undo stack: each entry is a snapshot { cells, shapes, lines }
  const undoStack = [];
  const MAX_UNDO = 80;
  function pushUndo() {
    const cellsSnap = {};
    for (const [k,v] of cells.entries()) cellsSnap[k] = JSON.parse(JSON.stringify(v));
    undoStack.push({ cells: cellsSnap, shapes: JSON.parse(JSON.stringify(shapes)), lines: JSON.parse(JSON.stringify(lines)) });
    if (undoStack.length > MAX_UNDO) undoStack.shift();
  }
  function popUndo() {
    if (undoStack.length === 0) return;
    const snap = undoStack.pop();
    cells.clear();
    for (const k of Object.keys(snap.cells)) cells.set(k, snap.cells[k]);
    shapes = snap.shapes; lines = snap.lines;
    selectedShape = null; lineStart = null;
    render(); saveDraw();
  }

  const trailToggleEl = $('trailToggle');
  trailToggleEl.addEventListener('click', () => {
    trailDraw = !trailDraw;
    trailToggleEl.classList.toggle('active', trailDraw);
  });

  const drawInfoIcon = $('drawInfoIcon'), drawInfoTip = $('drawInfoTip');
  drawInfoIcon.addEventListener('mouseenter', () => {
    const r = drawInfoIcon.getBoundingClientRect();
    let x = r.right + 8, y = r.top;
    drawInfoTip.style.display = 'block';
    const tw = drawInfoTip.offsetWidth, th = drawInfoTip.offsetHeight;
    if (x + tw > window.innerWidth) x = r.left - tw - 8;
    if (y + th > window.innerHeight) y = window.innerHeight - th - 8;
    if (y < 4) y = 4;
    drawInfoTip.style.left = x + 'px';
    drawInfoTip.style.top = y + 'px';
  });
  drawInfoIcon.addEventListener('mouseleave', () => { drawInfoTip.style.display = 'none'; });

  document.querySelectorAll('[data-mode]').forEach(btn => {
    btn.addEventListener('click', () => {
      drawMode = btn.dataset.mode;
      document.querySelectorAll('[data-mode]').forEach(b => b.classList.toggle('active', b.dataset.mode === drawMode));
      canvas.style.cursor = drawMode === 'erase' ? 'not-allowed' : 'crosshair';
      if (drawMode !== 'line') lineStart = null;
      borderPreview = null;
      render();
    });
  });

  function sizePx() { return cellSize * zoom; }
  function keyOf(cx,cy) { return `${cx},${cy}`; }
  function parseKey(k) { const [x,y]=k.split(',').map(Number); return {x,y}; }
  function makeId() { return crypto?.randomUUID?.() || Date.now().toString(36)+Math.random().toString(36).slice(2); }
  function screenToCell(sx,sy) { const s=sizePx(), wx=(sx-panX)/s, wy=(sy-panY)/s, cx=Math.floor(wx), cy=Math.floor(wy); return {cx,cy,fx:wx-cx,fy:wy-cy}; }
  function worldToScreen(wx,wy) { const s=sizePx(); return {sx:panX+wx*s, sy:panY+wy*s}; }
  function cellCenter(cx,cy) { return worldToScreen(cx+.5,cy+.5); }
  function pickEdge(fx,fy) { const dT=fy,dB=1-fy,dL=fx,dR=1-fx; let e='top',b=dT; if(dR<b){b=dR;e='right';}if(dB<b){b=dB;e='bottom';}if(dL<b){b=dL;e='left';} return e; }
  function pastelFill(hex) {
    let r=parseInt(hex.slice(1,3),16)/255, g=parseInt(hex.slice(3,5),16)/255, b=parseInt(hex.slice(5,7),16)/255;
    const mx=Math.max(r,g,b), mn=Math.min(r,g,b), d=mx-mn;
    let h=0, s=0, l=(mx+mn)/2;
    if(d>0){s=l>.5?d/(2-mx-mn):d/(mx+mn);if(mx===r)h=((g-b)/d+(g<b?6:0))/6;else if(mx===g)h=((b-r)/d+2)/6;else h=((r-g)/d+4)/6;}
    s=s*0.5; l=0.22;
    const hue2rgb=(p,q,t)=>{if(t<0)t+=1;if(t>1)t-=1;if(t<1/6)return p+(q-p)*6*t;if(t<1/2)return q;if(t<2/3)return p+(q-p)*(2/3-t)*6;return p;};
    const q=l<.5?l*(1+s):l+s-l*s, p=2*l-q;
    return `rgb(${Math.round(hue2rgb(p,q,h+1/3)*255)},${Math.round(hue2rgb(p,q,h)*255)},${Math.round(hue2rgb(p,q,h-1/3)*255)})`;
  }
  function ensureCell(cx,cy) { const k=keyOf(cx,cy); let c=cells.get(k); if(!c){c={};cells.set(k,c);} return c; }
  function deleteCellIfEmpty(cx,cy) { const k=keyOf(cx,cy),c=cells.get(k); if(!c)return; if(!c.fill&&!(c.edges&&(c.edges.top||c.edges.right||c.edges.bottom||c.edges.left))) cells.delete(k); }
  function applyPaint(cx,cy) { ensureCell(cx,cy).fill=activeColor; render(); }
  function neighborEdge(cx,cy,edge) {
    if(edge==='right')  return {nx:cx+1,ny:cy,ne:'left'};
    if(edge==='left')   return {nx:cx-1,ny:cy,ne:'right'};
    if(edge==='bottom') return {nx:cx,ny:cy+1,ne:'top'};
    if(edge==='top')    return {nx:cx,ny:cy-1,ne:'bottom'};
  }
  function getWall(cx,cy,edge) {
    const c=cells.get(keyOf(cx,cy));
    const cur=c&&c.edges&&c.edges[edge];
    if(cur) return cur;
    const{nx,ny,ne}=neighborEdge(cx,cy,edge);
    const n=cells.get(keyOf(nx,ny));
    return n&&n.edges&&n.edges[ne] || null;
  }
  function setWall(cx,cy,edge,val) {
    const c=ensureCell(cx,cy); if(!c.edges)c.edges={};
    c.edges[edge]=val;
    const{nx,ny,ne}=neighborEdge(cx,cy,edge);
    const n=cells.get(keyOf(nx,ny));
    if(n&&n.edges&&n.edges[ne]) { n.edges[ne]=val; }
  }
  function removeWall(cx,cy,edge) {
    const c=cells.get(keyOf(cx,cy));
    if(c&&c.edges) { delete c.edges[edge]; deleteCellIfEmpty(cx,cy); }
    const{nx,ny,ne}=neighborEdge(cx,cy,edge);
    const n=cells.get(keyOf(nx,ny));
    if(n&&n.edges) { delete n.edges[ne]; deleteCellIfEmpty(nx,ny); }
  }
  function applyBorder(cx,cy,edge,cycle) {
    const cur=getWall(cx,cy,edge);
    if(cycle) {
      if(!cur) setWall(cx,cy,edge,{w:WALL_THIN,c:activeColor});
      else if(cur.w<WALL_THICK) setWall(cx,cy,edge,{w:WALL_THICK,c:activeColor});
      else removeWall(cx,cy,edge);
    } else {
      if(!cur) setWall(cx,cy,edge,{w:WALL_THIN,c:activeColor});
    }
    render();
  }
  function applyErase(cx,cy,fx,fy) { const c=cells.get(keyOf(cx,cy)); if(!c)return; delete c.fill; if(c.edges){const edge=pickEdge(fx,fy),close=Math.min(fx,1-fx,fy,1-fy); if(close<.18)delete c.edges[edge]; else delete c.edges; if(c.edges&&!c.edges.top&&!c.edges.right&&!c.edges.bottom&&!c.edges.left)delete c.edges;} deleteCellIfEmpty(cx,cy); render(); }
  function shapeAt(cx,cy) { for(let i=shapes.length-1;i>=0;i--) if(shapes[i].cx===cx&&shapes[i].cy===cy) return shapes[i]; return null; }

  function hasTrail(ax,ay,bx,by,color) {
    for (const ln of lines) {
      if (ln.color !== color) continue;
      if ((ln.ax===ax && ln.ay===ay && ln.bx===bx && ln.by===by) ||
          (ln.ax===bx && ln.ay===by && ln.bx===ax && ln.by===ay)) return true;
    }
    return false;
  }
  function addTrail(ax,ay,bx,by,color) {
    if (ax===bx && ay===by) return;
    if (hasTrail(ax,ay,bx,by,color)) return;
    lines.push({id:makeId(),ax,ay,bx,by,color,type:'solid',w:TRAIL_WIDTH});
  }

  function resize() {
    dpr=Math.max(1,Math.min(2,window.devicePixelRatio||1));
    const rect=canvas.getBoundingClientRect();
    canvas.width=Math.floor(rect.width*dpr); canvas.height=Math.floor(rect.height*dpr);
    ctx.setTransform(dpr,0,0,dpr,0,0); render();
  }

  function render() {
    const rect=canvas.getBoundingClientRect(), w=rect.width, h=rect.height, s=sizePx();
    ctx.fillStyle='#0b0d12'; ctx.fillRect(0,0,w,h);
    const left=Math.floor(-panX/s)-1, top=Math.floor(-panY/s)-1, right=Math.floor((w-panX)/s)+1, bottom=Math.floor((h-panY)/s)+1;
    ctx.strokeStyle='rgba(255,255,255,0.06)';ctx.lineWidth=1;ctx.setLineDash([]);ctx.beginPath();
    for(let x=left;x<=right;x++){const sx=panX+x*s;ctx.moveTo(sx,0);ctx.lineTo(sx,h);}
    for(let y=top;y<=bottom;y++){const sy=panY+y*s;ctx.moveTo(0,sy);ctx.lineTo(w,sy);}
    ctx.stroke();
    ctx.strokeStyle='rgba(255,255,255,0.12)';ctx.beginPath();
    for(let x=left;x<=right;x++){if(x%5)continue;const sx=panX+x*s;ctx.moveTo(sx,0);ctx.lineTo(sx,h);}
    for(let y=top;y<=bottom;y++){if(y%5)continue;const sy=panY+y*s;ctx.moveTo(0,sy);ctx.lineTo(w,sy);}
    ctx.stroke();
    ctx.strokeStyle='rgba(122,162,255,0.25)';ctx.beginPath();ctx.moveTo(panX,0);ctx.lineTo(panX,h);ctx.moveTo(0,panY);ctx.lineTo(w,panY);ctx.stroke();
    for(const [k,c] of cells.entries()){
      const{x:cx,y:cy}=parseKey(k); if(cx<left||cx>right||cy<top||cy>bottom)continue;
      const x=panX+cx*s, y=panY+cy*s;
      if(c.fill){ctx.fillStyle=pastelFill(c.fill);ctx.fillRect(x,y,s,s);}
      if(c.edges){const e=c.edges;const dr=(edge,x1,y1,x2,y2)=>{if(!edge)return;ctx.strokeStyle=edge.c||'#fff';ctx.lineWidth=Math.max(1,edge.w||1);ctx.setLineDash([]);ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2,y2);ctx.stroke();};dr(e.top,x,y,x+s,y);dr(e.right,x+s,y,x+s,y+s);dr(e.bottom,x,y+s,x+s,y+s);dr(e.left,x,y,x,y+s);}
    }
    // Break all lines into cell-to-cell segments, then group & offset overlapping ones
    const segs = [];
    for (const ln of lines) {
      const dx = ln.bx - ln.ax, dy = ln.by - ln.ay;
      const steps = Math.max(Math.abs(dx), Math.abs(dy));
      if (steps === 0) continue;
      const sx = dx / steps, sy = dy / steps;
      for (let t = 0; t < steps; t++) {
        const x0 = Math.round(ln.ax + sx * t), y0 = Math.round(ln.ay + sy * t);
        const x1 = Math.round(ln.ax + sx * (t + 1)), y1 = Math.round(ln.ay + sy * (t + 1));
        segs.push({ ax: x0, ay: y0, bx: x1, by: y1, color: ln.color, w: ln.w, type: ln.type });
      }
    }
    const segGroups = new Map();
    for (let i = 0; i < segs.length; i++) {
      const sg = segs[i];
      const k = (sg.ax < sg.bx || (sg.ax === sg.bx && sg.ay <= sg.by))
        ? `${sg.ax},${sg.ay}-${sg.bx},${sg.by}` : `${sg.bx},${sg.by}-${sg.ax},${sg.ay}`;
      let g = segGroups.get(k); if (!g) { g = []; segGroups.set(k, g); } g.push(i);
    }
    const drawnGroups = new Set();
    for (let i = 0; i < segs.length; i++) {
      const sg = segs[i];
      const k = (sg.ax < sg.bx || (sg.ax === sg.bx && sg.ay <= sg.by))
        ? `${sg.ax},${sg.ay}-${sg.bx},${sg.by}` : `${sg.bx},${sg.by}-${sg.ax},${sg.ay}`;
      if (drawnGroups.has(k)) continue;
      drawnGroups.add(k);
      const group = segGroups.get(k);
      const colors = []; const seen = new Set();
      for (const idx of group) {
        const c = segs[idx].color;
        if (!seen.has(c)) { seen.add(c); colors.push(c); }
      }
      const a = cellCenter(sg.ax, sg.ay), b = cellCenter(sg.bx, sg.by);
      const lw = Math.max(1, sg.w);
      if (colors.length === 1) {
        ctx.save(); ctx.strokeStyle = colors[0]; ctx.lineWidth = lw; ctx.lineCap = 'round';
        ctx.setLineDash(sg.type === 'dashed' ? [10, 6] : sg.type === 'dotted' ? [2, 6] : []);
        ctx.beginPath(); ctx.moveTo(a.sx, a.sy); ctx.lineTo(b.sx, b.sy); ctx.stroke(); ctx.restore();
      } else {
        const dx = b.sx - a.sx, dy = b.sy - a.sy;
        const totalLen = Math.sqrt(dx * dx + dy * dy) || 1;
        const dashLen = Math.max(4, Math.min(sizePx() * 0.25, 10));
        const n = colors.length;
        let drawn = 0;
        let ci = 0;
        while (drawn < totalLen) {
          const segLen = Math.min(dashLen, totalLen - drawn);
          const t0 = drawn / totalLen, t1 = (drawn + segLen) / totalLen;
          ctx.save(); ctx.strokeStyle = colors[ci % n]; ctx.lineWidth = lw + 1; ctx.lineCap = 'butt';
          ctx.beginPath();
          ctx.moveTo(a.sx + dx * t0, a.sy + dy * t0);
          ctx.lineTo(a.sx + dx * t1, a.sy + dy * t1);
          ctx.stroke(); ctx.restore();
          drawn += segLen;
          ci++;
        }
      }
    }
    if(drawMode==='line'&&lineStart&&hover){const a=cellCenter(lineStart.cx,lineStart.cy),b=cellCenter(hover.cx,hover.cy);ctx.save();ctx.globalAlpha=.5;ctx.strokeStyle=activeColor;ctx.lineWidth=2;ctx.setLineDash([6,4]);ctx.lineCap='round';ctx.beginPath();ctx.moveTo(a.sx,a.sy);ctx.lineTo(b.sx,b.sy);ctx.stroke();ctx.restore();}
    const byCell=new Map();
    for(const sh of shapes){const k=keyOf(sh.cx,sh.cy);let a=byCell.get(k);if(!a){a=[];byCell.set(k,a);}a.push(sh);}
    for(const [k,arr] of byCell.entries()){
      const{x:cx,y:cy}=parseKey(k), x0=panX+cx*s, y0=panY+cy*s, n=arr.length, grid=Math.ceil(Math.sqrt(n)), sub=1/grid, pad=.12;
      for(let i=0;i<n;i++){const sh=arr[i],gx=i%grid,gy=Math.floor(i/grid),sx0=x0+gx*s*sub,sy0=y0+gy*s*sub,sw=s*sub,shh=s*sub,px=sw*pad,py=shh*pad,rx=sx0+px,ry=sy0+py,rw=sw-2*px,rh=shh-2*py;ctx.save();ctx.fillStyle=sh.color;ctx.strokeStyle='rgba(0,0,0,0.35)';ctx.lineWidth=Math.max(1,1.2*zoom);ctx.setLineDash([]);if(sh.kind==='rect'){ctx.beginPath();ctx.rect(rx,ry,rw,rh);ctx.fill();ctx.stroke();}else if(sh.kind==='circle'){const r=Math.min(rw,rh)/2;ctx.beginPath();ctx.arc(rx+rw/2,ry+rh/2,r,0,Math.PI*2);ctx.fill();ctx.stroke();}else if(sh.kind==='tri'){ctx.beginPath();ctx.moveTo(rx+rw/2,ry);ctx.lineTo(rx+rw,ry+rh);ctx.lineTo(rx,ry+rh);ctx.closePath();ctx.fill();ctx.stroke();}if(sh===selectedShape){ctx.strokeStyle='rgba(255,255,255,0.75)';ctx.lineWidth=Math.max(1,2*zoom);ctx.setLineDash([6,4]);ctx.strokeRect(rx,ry,rw,rh);}ctx.restore();}
    }
    if(hover){const x=panX+hover.cx*s,y=panY+hover.cy*s;ctx.strokeStyle='rgba(255,255,255,0.3)';ctx.lineWidth=1;ctx.setLineDash([]);ctx.strokeRect(x+.5,y+.5,s-1,s-1);}
    if(drawMode==='draw'&&borderPreview){const{cx,cy,edge}=borderPreview,x=panX+cx*s,y=panY+cy*s;ctx.save();ctx.globalAlpha=.35;ctx.strokeStyle=activeColor;ctx.lineWidth=WALL_THIN;ctx.setLineDash([]);ctx.lineCap='butt';ctx.beginPath();if(edge==='top'){ctx.moveTo(x,y);ctx.lineTo(x+s,y);}if(edge==='right'){ctx.moveTo(x+s,y);ctx.lineTo(x+s,y+s);}if(edge==='bottom'){ctx.moveTo(x,y+s);ctx.lineTo(x+s,y+s);}if(edge==='left'){ctx.moveTo(x,y);ctx.lineTo(x,y+s);}ctx.stroke();ctx.restore();}
  }

  canvas.addEventListener('contextmenu', e => e.preventDefault());
  canvas.addEventListener('wheel', e => {
    e.preventDefault();
    const rect=canvas.getBoundingClientRect(), mx=e.clientX-rect.left, my=e.clientY-rect.top;
    const old=zoom, factor=Math.exp(-e.deltaY*.0015), nz=Math.max(.15,Math.min(8,old*factor));
    const sOld=cellSize*old, sNew=cellSize*nz, wx=(mx-panX)/sOld, wy=(my-panY)/sOld;
    panX=mx-wx*sNew; panY=my-wy*sNew; zoom=nz;
    zoomLabelEl.textContent=zoom.toFixed(2)+'x'; render(); saveDraw();
  }, {passive:false});

  canvas.addEventListener('pointerdown', e => {
    canvas.setPointerCapture(e.pointerId);
    const rect=canvas.getBoundingClientRect(), mx=e.clientX-rect.left, my=e.clientY-rect.top;
    pointerDown=true; lastPX=mx; lastPY=my;
    if(e.button===1||e.button===2){panning=true;return;}
    const{cx,cy,fx,fy}=screenToCell(mx,my);
    if(drawMode==='draw'){
      const nearEdge = Math.min(fx,1-fx,fy,1-fy) < EDGE_ZONE;
      const sh = shapeAt(cx,cy);
      if(sh){drawLock='shape';pushUndo();selectedShape=sh;draggingShape=sh;dragPrevCX=sh.cx;dragPrevCY=sh.cy;render();}
      else if(nearEdge){drawLock='border';pushUndo();applyBorder(cx,cy,pickEdge(fx,fy),true);}
      else{drawLock='paint';pushUndo();applyPaint(cx,cy);}
    }
    else if(drawMode==='erase'){pushUndo();applyErase(cx,cy,fx,fy);}
    else if(drawMode==='line'){if(!lineStart){lineStart={cx,cy};}else{pushUndo();lines.push({id:makeId(),ax:lineStart.cx,ay:lineStart.cy,bx:cx,by:cy,color:activeColor,type:'solid',w:LINE_WIDTH});lineStart=null;render();saveDraw();}}
  });
  canvas.addEventListener('pointermove', e => {
    const rect=canvas.getBoundingClientRect(), mx=e.clientX-rect.left, my=e.clientY-rect.top;
    const cell=screenToCell(mx,my); hover={cx:cell.cx,cy:cell.cy};
    coordLabel.textContent=`(${cell.cx}, ${cell.cy})`;
    if(drawMode==='draw'&&!pointerDown) {
      const nearEdge = Math.min(cell.fx,1-cell.fx,cell.fy,1-cell.fy) < EDGE_ZONE;
      const sh = shapeAt(cell.cx,cell.cy);
      if(sh) { borderPreview=null; canvas.style.cursor='grab'; }
      else if(nearEdge) { borderPreview={cx:cell.cx,cy:cell.cy,edge:pickEdge(cell.fx,cell.fy)}; canvas.style.cursor='crosshair'; }
      else { borderPreview=null; canvas.style.cursor='cell'; }
    } else if(drawMode!=='draw') borderPreview=null;
    if(!pointerDown){render();return;}
    if(panning||e.buttons===4){panX+=mx-lastPX;panY+=my-lastPY;lastPX=mx;lastPY=my;render();return;}
    if(draggingShape){if(trailDraw&&(cell.cx!==dragPrevCX||cell.cy!==dragPrevCY)){addTrail(dragPrevCX,dragPrevCY,cell.cx,cell.cy,draggingShape.color);dragPrevCX=cell.cx;dragPrevCY=cell.cy;}else if(!trailDraw){dragPrevCX=cell.cx;dragPrevCY=cell.cy;}draggingShape.cx=cell.cx;draggingShape.cy=cell.cy;render();lastPX=mx;lastPY=my;return;}
    if(e.buttons&1){
      if(drawMode==='draw'){
        if(drawLock==='border') applyBorder(cell.cx,cell.cy,pickEdge(cell.fx,cell.fy));
        else if(drawLock==='paint') applyPaint(cell.cx,cell.cy);
      } else if(drawMode==='erase') applyErase(cell.cx,cell.cy,cell.fx,cell.fy);
    }

    lastPX=mx;lastPY=my;
  });
  canvas.addEventListener('pointerup', e => { pointerDown=false;panning=false;draggingShape=null;drawLock=null;canvas.releasePointerCapture(e.pointerId);saveDraw(); });
  canvas.addEventListener('dblclick', e => {
    if(drawMode!=='draw') return;
    const rect=canvas.getBoundingClientRect(), mx=e.clientX-rect.left, my=e.clientY-rect.top;
    const{cx,cy}=screenToCell(mx,my);
    if(!shapeAt(cx,cy)){
      popUndo(); popUndo();
      pushUndo();
      const ns={id:makeId(),kind:'circle',cx,cy,color:activeColor};shapes.push(ns);selectedShape=ns;render();saveDraw();
    }
  });

  clearDrawEl.addEventListener('click', () => { if(!confirm('Очистить все рисунки?'))return; pushUndo();cells.clear();shapes=[];selectedShape=null;lines=[];lineStart=null;render();saveDraw(); });

  function saveDraw() {
    const obj={}; for(const [k,v] of cells.entries()) obj[k]=v;
    localStorage.setItem(STORAGE_KEY, JSON.stringify({v:2,panX,panY,zoom,cellSize,cells:obj,shapes,lines}));
  }
  function loadDraw() {
    try{const s=localStorage.getItem(STORAGE_KEY);if(!s)return;const d=JSON.parse(s);panX=d.panX??0;panY=d.panY??0;zoom=d.zoom??1;cellSize=d.cellSize??28;cells.clear();if(d.cells)for(const k of Object.keys(d.cells))cells.set(k,d.cells[k]);shapes=Array.isArray(d.shapes)?d.shapes:[];lines=Array.isArray(d.lines)?d.lines:[];selectedShape=null;lineStart=null;}catch{}
  }

  loadDraw();
  window.addEventListener('resize', resize);
  resize();
  if(!localStorage.getItem(STORAGE_KEY)){const rect=canvas.getBoundingClientRect();panX=rect.width/2;panY=rect.height/2;}
  zoomLabelEl.textContent=zoom.toFixed(2)+'x';
  render();
})();
