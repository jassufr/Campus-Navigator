# Campus Navigator

Shortest-path navigation between rooms in Blocks A & B of the School of Computer
Science and Engineering, KIIT (ground to 3rd floor). The route is computed in C++
(Dijkstra) and shown on a schematic SVG map in the browser.

## Requirements
- g++ with C++17 support, and make
- Python 3 (only to generate the graph data)
- curl (to download two single-header libraries)

## Setup (once)
    curl -L -o backend/third_party/json.hpp    https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    curl -L -o backend/third_party/httplib.h   https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
    python3 tools/generate_building.py

## Run
    cd backend && make run
Open http://localhost:8080

## How it works
- `data/layout.json`: hand-built description of ONE floor (rooms, doors,
  junctions, stairs, entries, corridors). Edit this to fix the map.
- `tools/generate_building.py`: expands it into 4 floors, validates it and
  checks connectivity, then writes `data/building.json`.
- `backend/`: loads the graph, runs Dijkstra, serves `/api/route`,
  `/data/building.json` and the frontend on 127.0.0.1.
- `frontend/`: draws the map from `building.json` and calls `/api/route`.

## Tuning (in layout.json, then re-run the generator)
- `scale_m_per_px`: metres per map pixel (calibrate against a measured corridor)
- `stair_cost_m`: walking-distance equivalent of one flight of stairs

## Tests
    cd backend && make test    # Dijkstra vs Bellman-Ford oracle + property tests
    cd backend && make audit   # all-pairs routes + detour report

## Known limitations
- Floor plans have no scale; distances and times are estimates.
- Corridor topology was read from photographs and may differ from the building.
- Block C is not modelled; its entries are treated as endpoints only.
- Toilets and lifts are not modelled (the plans show no lifts).