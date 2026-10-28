(async function main() {
  const $ = (id) => document.getElementById(id);
  const svg = $('map');
  const tabsEl = $('floor-tabs');
  const toggle = $('graph-toggle');
  const status = $('status');
  const form = $('route-form');
  const fromInput = $('from-input');
  const toInput = $('to-input');
  const findBtn = $('find-btn');
  const resultEl = $('result');
  const summaryEl = $('summary');
  const noteEl = $('summary-note');
  const stepsEl = $('steps');

  let data;
  try {
    data = await Api.loadBuilding();
  } catch (err) {
    status.textContent = `${err.message}. Is the server running (cd backend && make run)?`;
    status.classList.add('error');
    return;
  }

  const map = new MapRenderer(svg, data);
  window.campusMap = map; // handy for poking around in the browser console

  const { floors } = data.meta;
  const floorName = (f) => map.floorName(f);
  const baseStatus = `${data.nodes.length} nodes, ${data.edges.length} edges loaded. Distances are estimates (scale ${data.meta.scale_m_per_px} m/px).`;

  function setStatus(msg, isError = false) {
    status.textContent = msg;
    status.classList.toggle('error', isError);
  }

  // ---------- floor tabs ----------
  const buttons = floors.map((f) => {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = floorName(f);
    b.addEventListener('click', () => selectFloor(f));
    tabsEl.appendChild(b);
    return b;
  });

  function selectFloor(f) {
    map.showFloor(f);
    buttons.forEach((b, i) => {
      const active = floors[i] === f;
      b.classList.toggle('active', active);
      b.setAttribute('aria-pressed', String(active));
    });
  }

  function markRouteFloors(path) {
    const used = new Set(path ? path.map((n) => n.floor) : []);
    buttons.forEach((b, i) => b.classList.toggle('on-route', used.has(floors[i])));
  }

  toggle.addEventListener('change', () => map.setGraphVisible(toggle.checked));

  // ---------- searchable location lists ----------
  const locations = data.nodes
    .filter((n) => n.selectable)
    .sort((a, b) => (a.type === b.type ? a.id.localeCompare(b.id) : a.type === 'entry' ? 1 : -1));

  const displayOf = (n) => (n.type === 'room' ? n.id : `${n.label} - ${floorName(n.floor)}`);
  const idByDisplay = new Map(locations.map((n) => [displayOf(n).toLowerCase(), n.id]));
  const nodeById = new Map(data.nodes.map((n) => [n.id, n]));

  const datalist = $('location-options');
  for (const n of locations) {
    const o = document.createElement('option');
    o.value = displayOf(n);
    if (n.type === 'room') o.label = `Block ${n.id[0]}, ${floorName(n.floor)}`;
    datalist.appendChild(o);
  }

  // Accepts "A306", "a306", "a 306", or the exact entry text from the list.
  function resolve(text) {
    const t = text.trim().toLowerCase();
    if (!t) return null;
    if (idByDisplay.has(t)) return idByDisplay.get(t);
    const compact = t.replace(/\s+/g, '').toUpperCase();
    const n = nodeById.get(compact);
    return n && n.selectable ? n.id : null;
  }

  const setInput = (input, id) => { input.value = displayOf(nodeById.get(id)); };

  // ---------- directions ----------
  const roundM = (d) => Math.max(5, Math.round(d / 5) * 5);
  const describe = (n) => (n.type === 'room' ? n.id : n.label || n.id);

  function buildDirections(route) {
    const p = route.path;
    const scale = data.meta.scale_m_per_px;
    const last = p.length - 1;
    const steps = [{ floor: p[0].floor, text: `Start at ${describe(p[0])}.` }];

    let walk = 0;
    let passing = [];
    let stairsFrom = null;

    const flushWalk = (toNode, arrived) => {
      if (walk <= 0) return;
      let t = `Walk about ${roundM(walk)} m`;
      if (passing.length) {
        const shown = passing.slice(0, 4).join(', ');
        t += `, passing ${shown}${passing.length > 4 ? ` and ${passing.length - 4} more` : ''}`;
      }
      t += ` to ${describe(toNode)}.`;
      if (arrived) t += ' You have arrived.';
      steps.push({ floor: toNode.floor, text: t });
      walk = 0;
      passing = [];
    };

    for (let i = 1; i <= last; i++) {
      const a = p[i - 1];
      const b = p[i];
      if (a.floor !== b.floor) {                  // a stair edge
        flushWalk(a, false);
        if (stairsFrom === null) stairsFrom = a.floor;
      } else {                                    // a walking edge
        if (stairsFrom !== null) {                // we just arrived from stairs
          steps.push({
            floor: a.floor,
            text: `Take the stairs ${a.floor > stairsFrom ? 'up' : 'down'} to the ${floorName(a.floor)}.`,
          });
          stairsFrom = null;
        }
        walk += Math.hypot(a.x - b.x, a.y - b.y) * scale;
        if (b.type === 'room' && i < last) passing.push(b.id);
      }
    }
    flushWalk(p[last], true);
    return steps;
  }

  // ---------- rendering a result ----------
  function statBox(value, label) {
    const d = document.createElement('div');
    d.className = 'stat';
    const b = document.createElement('b');
    b.textContent = value;
    const s = document.createElement('span');
    s.textContent = label;
    d.append(b, s);
    return d;
  }

  function renderResult(route) {
    const mins = Math.max(1, Math.round(route.est_seconds / 60));
    summaryEl.replaceChildren(
      statBox(`${Math.round(route.distance_m)} m`, 'total (est.)'),
      statBox(String(route.stairs), route.stairs === 1 ? 'floor change' : 'floor changes'),
      statBox(`~${mins} min`, 'walking time')
    );

    const walking = Math.max(0, route.distance_m - route.stairs * data.meta.stair_cost_m);
    noteEl.textContent =
      `About ${Math.round(walking)} m of walking` +
      (route.stairs ? ` plus ${route.stairs} flight${route.stairs === 1 ? '' : 's'} of stairs counted as ${data.meta.stair_cost_m} m each` : '') +
      '. Estimates only: the floor plans have no scale, so distances rest on assumed values. Click a step to jump to its floor.';

    stepsEl.replaceChildren();
    for (const s of buildDirections(route)) {
      const li = document.createElement('li');
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.append(document.createTextNode(s.text));
      const tag = document.createElement('span');
      tag.className = 'floor-tag';
      tag.textContent = floorName(s.floor);
      btn.appendChild(tag);
      btn.addEventListener('click', () => selectFloor(s.floor));
      li.appendChild(btn);
      stepsEl.appendChild(li);
    }

    resultEl.hidden = false;
    map.setRoute(route.path);
    markRouteFloors(route.path);
    selectFloor(route.path[0].floor);
  }

  function clearRoute() {
    map.setRoute(null);
    markRouteFloors(null);
    resultEl.hidden = true;
  }

  // ---------- finding a route ----------
  let requestCounter = 0;

  async function findRoute() {
    const fromId = resolve(fromInput.value);
    const toId = resolve(toInput.value);
    if (!fromId) { setStatus(`"${fromInput.value}" is not a known room or entry. Pick one from the list.`, true); return; }
    if (!toId) { setStatus(`"${toInput.value}" is not a known room or entry. Pick one from the list.`, true); return; }
    if (fromId === toId) { setStatus('Start and destination are the same place.', true); return; }

    const mine = ++requestCounter;   // ignore responses that arrive after a newer request
    findBtn.disabled = true;
    setStatus('Finding route...');
    try {
      const route = await Api.route(fromId, toId);
      if (mine !== requestCounter) return;
      renderResult(route);
      setStatus(`Route found: ${fromId} to ${toId}.`);
    } catch (err) {
      if (mine !== requestCounter) return;
      clearRoute();
      setStatus(err.message, true);
    } finally {
      if (mine === requestCounter) findBtn.disabled = false;
    }
  }

  form.addEventListener('submit', (ev) => { ev.preventDefault(); findRoute(); });

  $('swap-btn').addEventListener('click', () => {
    [fromInput.value, toInput.value] = [toInput.value, fromInput.value];
    if (fromInput.value.trim() && toInput.value.trim()) findRoute();
  });

  $('clear-btn').addEventListener('click', () => {
    requestCounter++;
    fromInput.value = '';
    toInput.value = '';
    findBtn.disabled = false;
    clearRoute();
    setStatus(baseStatus);
  });

  // Clicking the map: first click = From, second = To (then search), third restarts.
  map.onPick = (id) => {
    const hasFrom = fromInput.value.trim() !== '';
    const hasTo = toInput.value.trim() !== '';
    const name = displayOf(nodeById.get(id));
    if (!hasFrom) {
      setInput(fromInput, id);
      if (hasTo) findRoute();
      else setStatus(`Start set to ${name}. Now click a destination.`);
    } else if (!hasTo) {
      setInput(toInput, id);
      findRoute();
    } else {
      clearRoute();
      setInput(fromInput, id);
      toInput.value = '';
      setStatus(`Start set to ${name}. Now click a destination.`);
    }
  };

  selectFloor(floors[0]);
  setStatus(baseStatus);
})();