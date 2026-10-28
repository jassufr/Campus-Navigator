#!/usr/bin/env python3
"""Expand data/layout.json (one floor template) into data/building.json (all floors).

Run from the project root:  python3 tools/generate_building.py
"""
import json
import math
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "data" / "layout.json"
DST = ROOT / "data" / "building.json"

LONG_EDGE_PX = 350   # warn if a single corridor segment is longer than this
DOOR_FAR_PX = 50     # warn if a door point is farther than this from its room

ENTRY_LABELS = {
    "ENTRY_C_A": "Entry from C block (A side)",
    "ENTRY_C_B": "Entry from C block (B side)",
    "ENTRY_EXT_A": "Outside entrance (A side)",
    "ENTRY_EXT_B": "Outside entrance (B side)",
}


def px_dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def point_to_rect_dist(p, r):
    dx = max(r["x"] - p[0], 0, p[0] - (r["x"] + r["w"]))
    dy = max(r["y"] - p[1], 0, p[1] - (r["y"] + r["h"]))
    return math.hypot(dx, dy)


def fail(errors):
    print(f"\nFAILED with {len(errors)} error(s):")
    for e in errors:
        print("  ERROR:", e)
    sys.exit(1)


def main():
    layout = json.loads(SRC.read_text(encoding="utf-8"))
    meta = layout["meta"]
    scale = meta["scale_m_per_px"]
    stair_cost = meta["stair_cost_m"]
    floors = meta["floors"]
    rooms = layout["rooms"]
    doors = layout["doors"]
    junctions = layout["junctions"]
    stairs = layout["stairs"]
    entries = layout["entries"]
    corridors = layout["corridors"]

    errors, warnings = [], []

    # ---------- static validation ----------
    room_by_id = {r["id"]: r for r in rooms}
    if len(room_by_id) != len(rooms):
        errors.append("duplicate ids in 'rooms'")
    for rid in sorted(room_by_id.keys() - doors.keys()):
        errors.append(f"room {rid} has no door point")
    for did in sorted(doors.keys() - room_by_id.keys()):
        errors.append(f"door {did} has no room rectangle")

    all_ids = [k for g in (doors, junctions, stairs, entries) for k in g]
    for k in sorted({k for k in all_ids if all_ids.count(k) > 1}):
        errors.append(f"id '{k}' is used more than once across doors/junctions/stairs/entries")

    pos, kind = {}, {}
    for k, v in doors.items():
        pos[k], kind[k] = v, "room"
    for k, v in junctions.items():
        pos[k], kind[k] = v, "junction"
    for k, v in stairs.items():
        pos[k], kind[k] = v["pos"], "stairs"
    for k, v in entries.items():
        pos[k], kind[k] = v["pos"], "entry"

    for i, c in enumerate(corridors):
        for w in c:
            if w not in pos:
                errors.append(f"corridor #{i} references unknown id '{w}'")
    for s, v in stairs.items():
        if v["link"] not in junctions:
            errors.append(f"stairs {s} links to unknown junction '{v['link']}'")
    for e, v in entries.items():
        if "link" in v and v["link"] not in junctions:
            errors.append(f"entry {e} links to unknown junction '{v['link']}'")
    if errors:
        fail(errors)

    for rid, r in room_by_id.items():
        if rid in doors:
            d = point_to_rect_dist(doors[rid], r)
            if d > DOOR_FAR_PX:
                warnings.append(f"door of {rid} is {d:.0f}px from its room rectangle")

    def exists_on(base, f):
        return kind[base] != "entry" or f in entries[base]["floors"]

    def node_id(base, f):
        if kind[base] == "room":              # "A06" on floor 1 -> "A106"
            return f"{base[0]}{f}{base[1:]}"
        return f"{base}_F{f}"

    # ---------- build graph ----------
    nodes, edges, seen = [], [], set()

    def add_edge(a, b, w, k):
        key = frozenset((a, b))
        if key in seen:
            return
        seen.add(key)
        edges.append({"from": a, "to": b, "w": round(w, 2), "kind": k})

    for f in floors:
        for base in pos:
            if not exists_on(base, f):
                continue
            t = kind[base]
            if t == "room":
                label = node_id(base, f)
            elif t == "entry":
                label = ENTRY_LABELS.get(base, base)
            elif t == "stairs":
                label = "Stairs " + base[2:]
            else:
                label = ""
            nodes.append({
                "id": node_id(base, f), "type": t, "floor": f,
                "x": pos[base][0], "y": pos[base][1],
                "label": label, "selectable": t in ("room", "entry"),
            })

        for i, c in enumerate(corridors):
            for u, v in zip(c, c[1:]):
                if not (exists_on(u, f) and exists_on(v, f)):
                    errors.append(f"corridor #{i}: {u}->{v} uses an entry missing on floor {f}")
                    continue
                d = px_dist(pos[u], pos[v])
                if d > LONG_EDGE_PX:
                    warnings.append(f"long corridor segment {u}->{v}: {d:.0f}px")
                add_edge(node_id(u, f), node_id(v, f), d * scale, "walk")

        for s, v in stairs.items():
            d = px_dist(pos[s], pos[v["link"]])
            add_edge(node_id(s, f), node_id(v["link"], f), d * scale, "walk")

        for e, v in entries.items():
            if "link" in v and exists_on(e, f):
                d = px_dist(pos[e], pos[v["link"]])
                add_edge(node_id(e, f), node_id(v["link"], f), d * scale, "walk")

    for a, b in zip(floors, floors[1:]):
        for s in stairs:
            add_edge(node_id(s, a), node_id(s, b), stair_cost, "stairs")

    if errors:
        fail(errors)

    # ---------- connectivity check ----------
    adj = {n["id"]: [] for n in nodes}
    for e in edges:
        adj[e["from"]].append(e["to"])
        adj[e["to"]].append(e["from"])
    visited, queue = {nodes[0]["id"]}, deque([nodes[0]["id"]])
    while queue:
        u = queue.popleft()
        for v in adj[u]:
            if v not in visited:
                visited.add(v)
                queue.append(v)
    unreachable = [n["id"] for n in nodes if n["id"] not in visited]
    if unreachable:
        fail([f"{len(unreachable)} node(s) unreachable from {nodes[0]['id']}: {unreachable[:20]}"])

    # ---------- room rectangles for drawing ----------
    room_rects = [
        {"id": node_id(r["id"], f), "floor": f, "block": r["id"][0],
         "x": r["x"], "y": r["y"], "w": r["w"], "h": r["h"]}
        for f in floors for r in rooms
    ]

    out = {
        "meta": {k: meta[k] for k in
                 ("viewBox", "scale_m_per_px", "stair_cost_m", "floors", "floor_names", "block_labels")},
        "nodes": nodes,
        "edges": edges,
        "rooms": room_rects,
    }
    DST.write_text(json.dumps(out, indent=1), encoding="utf-8")

    # ---------- report ----------
    n_sel = sum(n["selectable"] for n in nodes)
    ws = [e["w"] for e in edges if e["kind"] == "walk"]
    print(f"OK  wrote {DST.relative_to(ROOT)}")
    print(f"    nodes: {len(nodes)}  (selectable: {n_sel})")
    print(f"    edges: {len(edges)}  (stairs: {sum(e['kind'] == 'stairs' for e in edges)})")
    print(f"    room rectangles: {len(room_rects)}")
    print(f"    walk edge length: min {min(ws):.1f} m, max {max(ws):.1f} m")
    print("    connectivity: all nodes reachable")
    for w in warnings:
        print("  WARNING:", w)


if __name__ == "__main__":
    main()