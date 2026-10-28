// Thin wrapper around the C++ server's endpoints.
const Api = {
  async loadBuilding() {
    const res = await fetch('/data/building.json');
    if (!res.ok) throw new Error(`Could not load building.json (HTTP ${res.status})`);
    return res.json();
  },

  // Resolves with the route object, or throws an Error carrying the server's message.
  async route(from, to) {
    const url = `/api/route?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`;
    const res = await fetch(url);
    let body = null;
    try { body = await res.json(); } catch (_) { /* non-JSON error page */ }
    if (!res.ok) throw new Error((body && body.error) || `Request failed (HTTP ${res.status})`);
    return body;
  },
};