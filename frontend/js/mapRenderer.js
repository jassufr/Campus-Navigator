// Draws one floor at a time as SVG, straight from building.json.
// Layer order (bottom to top): corridors, block letters, rooms, debug graph, markers, route.
(function () {
  const SVG_NS = 'http://www.w3.org/2000/svg';

  function el(name, attrs = {}, text) {
    const e = document.createElementNS(SVG_NS, name);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    if (text !== undefined) e.textContent = text;
    return e;
  }

  class MapRenderer {
    constructor(svg, data) {
      this.svg = svg;
      this.data = data;
      this.floor = data.meta.floors[0];
      this.graphVisible = false;
      this.route = null;     // array of path nodes from /api/route, or null
      this.onPick = null;    // callback(nodeId) when a room or entry is clicked
      this.nodeById = new Map(data.nodes.map((n) => [n.id, n]));

      // Index everything by floor once, so switching floors is cheap.
      this.byFloor = new Map(data.meta.floors.map((f) => [f, { rooms: [], nodes: [], edges: [] }]));
      for (const r of data.rooms) this.byFloor.get(r.floor).rooms.push(r);
      for (const n of data.nodes) this.byFloor.get(n.floor).nodes.push(n);
      for (const e of data.edges) {
        if (e.kind !== 'walk') continue; // stair edges join floors; nothing to draw on one floor
        const a = this.nodeById.get(e.from);
        const b = this.nodeById.get(e.to);
        this.byFloor.get(a.floor).edges.push([a, b]);
      }

      const [x, y, w, h] = data.meta.viewBox;
      svg.setAttribute('viewBox', `${x} ${y} ${w} ${h}`);
      svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');

      this.layers = {};
      for (const name of ['corridors', 'blocks', 'rooms', 'graph', 'markers', 'path']) {
        this.layers[name] = el('g', { id: `layer-${name}` });
        svg.appendChild(this.layers[name]);
      }

      // One delegated listener handles every clickable room and entry.
      svg.addEventListener('click', (ev) => {
        const target = ev.target.closest('[data-pick]');
        if (target && this.onPick) this.onPick(target.getAttribute('data-pick'));
      });
    }

    floorName(floor) {
      return this.data.meta.floor_names[this.data.meta.floors.indexOf(floor)];
    }

    showFloor(floor) {
      if (!this.byFloor.has(floor)) throw new Error(`Unknown floor ${floor}`);
      this.floor = floor;
      this._render();
    }

    setGraphVisible(visible) {
      this.graphVisible = visible;
      this._renderGraph();
    }

    // pathNodes: array from the API response, or null/empty to clear.
    setRoute(pathNodes) {
      this.route = pathNodes && pathNodes.length ? pathNodes : null;
      this._renderPath();
    }

    _render() {
      for (const layer of Object.values(this.layers)) layer.replaceChildren();
      const f = this.byFloor.get(this.floor);
      const floorName = this.floorName(this.floor);

      // Corridors: thick strokes along the walkable edges.
      for (const [a, b] of f.edges) {
        this.layers.corridors.appendChild(
          el('line', { class: 'corridor', x1: a.x, y1: a.y, x2: b.x, y2: b.y })
        );
      }

      // Big block letters (A, B, C).
      for (const b of this.data.meta.block_labels) {
        this.layers.blocks.appendChild(
          el('text', { class: 'block-label', x: b.x, y: b.y, 'text-anchor': 'middle', 'dominant-baseline': 'central' }, b.text)
        );
      }

      // Rooms (clickable).
      for (const r of f.rooms) {
        const g = el('g', { 'data-room': r.id, 'data-pick': r.id });
        g.appendChild(el('title', {}, `${r.id} - ${floorName}`));
        g.appendChild(el('rect', { class: `room block-${r.block}`, x: r.x, y: r.y, width: r.w, height: r.h, rx: 4 }));

        const cx = r.x + r.w / 2;
        const cy = r.y + r.h / 2;
        const attrs = { class: 'room-label', x: cx, y: cy, 'text-anchor': 'middle', 'dominant-baseline': 'central' };
        if (r.w < 65 && r.h >= 1.15 * r.w) attrs.transform = `rotate(-90 ${cx} ${cy})`; // narrow, tall rooms
        g.appendChild(el('text', attrs, r.id));
        this.layers.rooms.appendChild(g);
      }

      // Stairs (not clickable) and entries (clickable).
      for (const n of f.nodes) {
        if (n.type === 'stairs') {
          const g = el('g');
          g.appendChild(el('title', {}, n.label));
          g.appendChild(el('rect', { class: 'stairs-box', x: n.x - 10, y: n.y - 10, width: 20, height: 20, rx: 3 }));
          g.appendChild(el('text', { class: 'stairs-text', x: n.x, y: n.y, 'text-anchor': 'middle', 'dominant-baseline': 'central' }, 'S'));
          this.layers.markers.appendChild(g);
        } else if (n.type === 'entry') {
          const g = el('g', { 'data-pick': n.id });
          g.appendChild(el('title', {}, `${n.label} - ${floorName}`));
          g.appendChild(el('circle', { class: 'entry-dot', cx: n.x, cy: n.y, r: 11 }));
          const short = n.id.startsWith('ENTRY_C') ? 'C' : 'IN';
          g.appendChild(el('text', { class: 'entry-text', x: n.x, y: n.y, 'text-anchor': 'middle', 'dominant-baseline': 'central' }, short));
          this.layers.markers.appendChild(g);
        }
      }

      this._renderGraph();
      this._renderPath();
    }

    _renderGraph() {
      const layer = this.layers.graph;
      layer.replaceChildren();
      if (!this.graphVisible) return;
      const f = this.byFloor.get(this.floor);

      for (const [a, b] of f.edges) {
        layer.appendChild(el('line', { class: 'dbg-edge', x1: a.x, y1: a.y, x2: b.x, y2: b.y }));
      }
      for (const n of f.nodes) {
        if (n.type === 'room') {
          layer.appendChild(el('circle', { class: 'dbg-door', cx: n.x, cy: n.y, r: 4 }));
        } else if (n.type === 'junction') {
          layer.appendChild(el('circle', { class: 'dbg-junction', cx: n.x, cy: n.y, r: 6 }));
          const name = n.id.replace(/_F\d+$/, '');
          layer.appendChild(el('text', { class: 'dbg-text', x: n.x + 8, y: n.y - 8 }, name));
        }
      }
    }

    // Draws only the part of the route on the current floor.
    _renderPath() {
      const layer = this.layers.path;
      layer.replaceChildren();
      this.layers.rooms.querySelectorAll('.is-start, .is-end')
        .forEach((r) => r.classList.remove('is-start', 'is-end'));
      if (!this.route) return;

      const p = this.route;
      const f = this.floor;

      // Consecutive nodes on this floor form one polyline; a floor change breaks it.
      let run = [];
      const flushRun = () => {
        if (run.length >= 2) {
          layer.appendChild(el('polyline', {
            class: 'route-line',
            points: run.map((n) => `${n.x},${n.y}`).join(' '),
          }));
        }
        run = [];
      };
      for (const n of p) {
        if (n.floor === f) run.push(n);
        else flushRun();
      }
      flushRun();

      // Ring the staircases the route uses on this floor.
      for (const n of p) {
        if (n.type === 'stairs' && n.floor === f) {
          layer.appendChild(el('circle', { class: 'route-stair', cx: n.x, cy: n.y, r: 16 }));
        }
      }

      const start = p[0];
      const end = p[p.length - 1];
      const mark = (n, cls, tag, roomCls) => {
        if (n.floor !== f) return;
        layer.appendChild(el('circle', { class: cls, cx: n.x, cy: n.y, r: 10 }));
        layer.appendChild(el('text', { class: 'route-tag', x: n.x, y: n.y - 18 }, tag));
        const rect = this.layers.rooms.querySelector(`[data-room="${n.id}"] rect`);
        if (rect) rect.classList.add(roomCls);
      };
      mark(end, 'route-end', 'END', 'is-end');
      mark(start, 'route-start', 'START', 'is-start');
    }
  }

  window.MapRenderer = MapRenderer;
})();