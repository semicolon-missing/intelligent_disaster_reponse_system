# Intelligent Disaster Response System (IDRS)

[![Language: C](https://img.shields.io/badge/Language-C11-00599C.svg?logo=c)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Database: SQLite3](https://img.shields.io/badge/Database-SQLite3-003B57.svg?logo=sqlite)](https://www.sqlite.org/)
[![Server: Winsock2 HTTP](https://img.shields.io/badge/Server-Winsock2_HTTP_REST-0078D6.svg)](https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page-2)
[![UI: Tactical Command Center](https://img.shields.io/badge/UI-Tactical_HUD_Vis.js-06B6D4.svg)](https://visjs.org/)

An advanced, high-performance C-based algorithmic emergency decision-support and dispatch command system. Designed to assist emergency management authorities in prioritizing simultaneous crisis incidents, dispatching optimal rescue teams and transport vehicles, routing evacuations around flooded or blocked roads using graph algorithms, and directing victims to medical centers with real-time capacity tracking.

Includes **both** an interactive 25-option console CLI (`cli.exe`) and a tactical web command center (`web.exe` at `http://localhost:8080`) backed by persistent SQLite storage.

---

## Architecture Overview

```mermaid
flowchart TD
    subgraph Data Layer
        DB[(SQLite3: disaster_v2.db)]
        SCN[Scenario Importer: Assam, Kerala, Uttarakhand, Odisha, Custom .txt]
    end

    subgraph Core Algorithmic Engine (Pure C)
        PQ[Max-Heap Priority Queue\nHazard Severity + Scale + Medical Urgency]
        DIJ[Dijkstra Shortest Path\nStep-by-step Vertex Reconstruction]
        BFS[BFS Reachability Validator\nFlood / Landslide Road Blockage Tracking]
        GREEDY[Greedy Resource Matcher\nTeam Capability + Transport + Hospital Capacity]
    end

    subgraph Presentation & Control Layer
        CLI["CLI Engine (main.c)\n24-Menu Console Dispatcher"]
        HTTP["HTTP / REST Server (server_main.c + httpserver.c)\nWinsock2 Non-blocking Socket Engine (Port 8080)"]
        WEB["Tactical Web Command Center (public/index.html)\nVis.js Interactive Graph + Real-time HUD"]
    end

    SCN --> DB
    DB <--> Core
    PQ <--> CLI
    DIJ <--> CLI
    BFS <--> CLI
    GREEDY <--> CLI
    PQ <--> HTTP
    DIJ <--> HTTP
    BFS <--> HTTP
    GREEDY <--> HTTP
    HTTP <--> WEB
```

---

## Core Algorithms & Mathematical Formulations

### 1. Emergency Priority Scoring (Max-Heap)
Disaster alerts are prioritized using a bounded composite scoring function ($S \in [0, 10]$) based on three key factors:

$$S = \min\left(10.0, \; 0.40 \cdot H + 0.30 \cdot \left(\frac{\min(N, 1000)}{100}\right) + 0.30 \cdot \left(\frac{M}{N} \cdot 10\right)\right)$$

- **$H$ (Hazard Severity Scale, $1-10$):**
  - Chemical / Hazmat: 10
  - Tsunami: 10
  - Flash Flood / Cloudburst: 9
  - Earthquake: 9
  - Cyclone / Storm Surge: 8.5
  - Major Flood / Landslide: 8
  - Fire: 7.5
- **$N$:** Total population affected.
- **$M$:** Number of victims requiring urgent critical medical care.
- **Heap Guarantee:** Ingestion and retrieval both operate in $O(\log k)$ time, guaranteeing that the most urgent emergency is always extracted first.

### 2. Turn-by-Turn Routing (Dijkstra with Path Reconstruction)
Road networks are modeled as an undirected weighted graph $G = (V, E)$, with distances $w(u, v) \in \mathbb{R}^+$.
- When roads are flooded or blocked, their edge weight is set to $\infty$.
- Dijkstra's algorithm computes the shortest unblocked path from:
  1. Selected Rescue Team Base $\to$ Emergency Incident Site.
  2. Incident Site $\to$ Selected Receiving Relief Hospital.
- An intermediate predecessor map $\pi(v)$ reconstructs the exact sequence of intermediate nodes (`[Guwahati -> Jorhat -> Dibrugarh]`).

### 3. Reachability Verification (Breadth-First Search)
Before routing or allocating assets, BFS runs in $O(|V| + |E|)$ time to verify that a connected, unblocked physical path exists between candidate teams, the incident, and candidate hospitals. If landslides or floods sever a connection, unreachable teams are disqualified immediately.

### 4. Multi-Criteria Greedy Resource Allocation
- **Rescue Team Selection:** Evaluates teams on availability, distance to incident, team capacity vs. affected population, medical kit availability (mandatory if $M/N \ge 0.40$), and rescue boat availability (mandatory for water hazards).
- **Hospital / Shelter Selection:** Evaluates facilities on road reachability, available capacity ($\text{Capacity} - \text{Occupancy} \ge N$), medical trauma support capabilities, and shortest transit distance.

---

## Disaster Scenarios Included

IDRS includes pre-configured disaster datasets ready for simulation:

| Scenario | Primary Disaster | Geographic Region | Key Nodes |
|---|---|---|---|
| **Multi-Hazard Unified Crisis** (`scenarios/sample_natural_disasters.txt`) | **ALL Natural & Crisis Disasters** (Earthquake, Cyclone, Flash Flood, Landslide, Tsunami, Chemical Hazmat, Wildfire) | Multi-Terrain: Coastal Harbor, Mountain Ridge, Central Metro, River Basin, Forest Reserve, Island Sanctuary... | 10 Locations, 14 Roads, 7 Teams, 6 Transports, 6 Hospitals & Shelters |
| **Assam Floods** | Brahmaputra River flooding & riverbank breaches | Assam (Guwahati, Jorhat, Dibrugarh, Tezpur, Barpeta, Silchar...) | 8 Locations, 13 Roads, 5 Teams, 4 Transports, 6 Hospitals |
| **Kerala Landslides** | Heavy monsoon rainfall, landslides & reservoir overflows | Western Ghats (Wayanad, Idukki, Munnar, Kozhikode, Kochi...) | 8 Locations, 9 Roads, 5 Teams, 4 Transports, 5 Hospitals |
| **Uttarakhand Cloudburst** | Glacial flash floods & mountain cloudbursts | Himalayas (Kedarnath, Chamoli, Joshimath, Rudraprayag, Rishikesh...) | 8 Locations, 8 Roads, 5 Teams, 4 Transports, 5 Hospitals |
| **Odisha Super Cyclone** | Category 5 cyclone, storm surges & coastal inundation | Bay of Bengal Coast (Paradip, Puri, Cuttack, Bhubaneswar, Balasore...) | 7 Locations, 8 Roads, 5 Teams, 4 Transports, 5 Hospitals |

### Custom Scenario Format (.txt)
Custom geographic regions can be created in plain text files using the simple section format:
```text
[LOCATIONS]
Metro_City
Harbor_Bay
North_Hill

[ROADS]
Metro_City | Harbor_Bay | 25
Metro_City | North_Hill | 15

[TEAMS]
Rapid Task Force | Metro_City | 150 | yes | yes

[TRANSPORT]
Air Force Chopper | 60 | yes

[DESTINATIONS]
Central Trauma Hospital | Metro_City | 500 | yes

[EMERGENCIES]
Harbor_Bay | Flood | 240 | 110
```

---

## REST API Reference

The embedded Winsock2 web server serves a JSON REST API on port `8080`:

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/api/metrics` | Real-time counts of active emergencies, victims, ready fleet, shelter occupancy %, and blocked roads. |
| `GET` | `/api/queue` | Complete array of queued emergencies sorted by Max-Heap priority. |
| `POST` | `/api/emergencies` | Report new emergency (`location`, `hazardType`, `affectedPeople`, `peopleNeedingMedical`). |
| `POST` | `/api/process` | Extract #1 emergency from heap, allocate team & hospital, calculate Dijkstra paths, update occupancies, and log mission. |
| `GET` | `/api/plan` | Return JSON breakdown of the latest dispatch order. |
| `GET` | `/api/history` | Historical audit log of all completed rescue missions with timestamps. |
| `GET` | `/api/graph` | Locations and road topology with distances and block states. |
| `GET` | `/api/route?from=X&to=Y` | Check BFS reachability, Dijkstra shortest distance, and full path array between two nodes. |
| `POST` | `/api/block` | Mark a road between two locations as blocked (`source`, `destination`). |
| `POST` | `/api/unblock` | Restore traffic on a blocked road (`source`, `destination`). |
| `GET` | `/api/teams` | List registered rescue teams and availability. |
| `POST` | `/api/teams` | Add a new rescue team (`name`, `location`, `capacity`, `medicalKit`, `boatAvailable`). |
| `GET` | `/api/transport` | List transport vehicles and availability. |
| `POST` | `/api/transport` | Register transport unit (`type`, `capacity`, `floodSuitable`). |
| `GET` | `/api/destinations` | List relief camps and hospitals with live occupancy. |
| `POST` | `/api/destinations` | Register new hospital / shelter (`name`, `location`, `capacity`, `medicalSupport`). |
| `POST` | `/api/scenario` | Switch scenario (`scenario=assam\|kerala\|uttarakhand\|odisha`) or import custom content (`content=...`). |
| `POST` | `/api/reset` | Clear all hospital occupancies and reset team/transport availability to Standby. |

---

## Building and Running

### Prerequisites
- **GCC** (MinGW / MinGW-w64 / MSYS2 on Windows, or standard GCC on Linux/macOS)
- **SQLite3**: optional on Windows. If no system SQLite is installed, `build.bat` downloads the official
  SQLite amalgamation once into `third_party/sqlite/` (git-ignored) and compiles it in.
  On Linux install it with `sudo apt install libsqlite3-dev`.
- **Winsock2** on Windows (`-lws2_32`, ships with MinGW)
- **ncurses** for the terminal dashboard (`tui`), Linux/macOS only: `sudo apt install libncurses-dev`

### 1. One-Click Build (Windows)
Double-click `build.bat` or run:
```cmd
build.bat
```
This produces `cli.exe` and `web.exe`.

### 2. Using Makefile
```bash
make            # Linux/macOS: builds cli, web and tui.  Windows (mingw32-make): cli.exe and web.exe
make web        # just the web server
make graph_test # standalone self-test of the graph module (no database needed)
```

> Run the programs **from the project root folder**: the web server serves `public/index.html`
> and the scenario loader reads `scenarios/` relative to the current directory.
> The SQLite database `disaster_v2.db` is created automatically on first run; delete it to start fresh.

### 3. Launching the Tactical Web Command Center
```cmd
web.exe
```
Then open your web browser to:
```
http://localhost:8080
```
- Interactive Vis.js network map with clickable road toggle.
- Real-time Max-Heap queue display.
- One-click "Dispatch Responders" with path animation and turn-by-turn routing.
- Auto-simulation mode to play through emergencies continuously.
- Live mission audit timeline.

### 4. Launching the ncurses Terminal Dashboard (Linux/macOS)
```bash
./tui
```
Keys: `D`/`Space` dispatch, `N` new alert, `B` block road, `P` shortest route, `C` connectivity,
`S` scenarios, `R` reset, `Tab`/`1`-`5` switch view, `?` help, `Q` quit.

### 5. Launching the Interactive Terminal CLI
```cmd
cli.exe
```
Provides a 25-option console menu for testing algorithms directly:
```text
=======================================================
   INTELLIGENT DISASTER RESPONSE SYSTEM (CLI)
=======================================================
 [MAP & NETWORK]
  1. View Locations
  2. Add Location
  3. View Road Network Matrix
  4. Add Road Connection
  5. Check Connectivity (BFS)
  6. Find Shortest Route (Dijkstra)
  7. Block Road
  8. Unblock Road

 [RESOURCES & DESTINATIONS]
  9. View Rescue Teams
 10. Add Rescue Team
 11. View Transport Fleet
 12. Add Transport Vehicle
 13. View Hospitals & Evacuation Camps
 14. Add Hospital / Camp

 [SCENARIOS & SIMULATION]
 15. Load Multi-Hazard Crisis Scenario (scenarios/sample_natural_disasters.txt)
 16. Load Assam Floods Demo
 17. Load Kerala Floods & Landslides Demo
 18. Load Uttarakhand Cloudburst Demo
 19. Load Odisha Super Cyclone Demo
 20. Import Custom Scenario from File (.txt)
 21. Reset All Resources & Clear Occupancies

 [EMERGENCIES & DISPATCH]
 22. Report / Add New Emergency
 23. View Priority Queue (Max Heap)
 24. Dispatch Highest Priority Emergency
 25. Show Last Rescue Dispatch Plan
  0. Exit
-------------------------------------------------------
```

---

## Project Structure

```text
intelligent_disaster_reponse_system/
├── priority_queue.c / .h     # Max-Heap priority queue & hazard priority scoring
├── graph.c / .h              # Adjacency matrix, BFS reachability, Dijkstra shortest path
├── rescue.c / .h             # Multi-criteria greedy rescue team & transport allocation
├── hospital.c / .h           # Greedy hospital / shelter selection with capacity tracking
├── db.c / .h                 # SQLite3 database persistence layer
├── scenario_import.c / .h    # Multi-scenario presets & custom text parser
├── httpserver.c / .h         # Winsock2 non-blocking HTTP file & REST server
├── server_main.c             # Web server entry point & 20 REST API endpoints
├── main.c                    # Interactive 25-menu console CLI application
├── tui_main.c                # ncurses tactical terminal dashboard (Linux/macOS)
├── public/
│   └── index.html            # Tactical HUD Web UI with Vis.js interactive network
├── scenarios/                # Pre-configured & custom scenario definitions (.txt)
│   ├── sample_natural_disasters.txt  # Multi-Hazard unified crisis dataset
│   ├── assam_scenario.txt            # Assam Floods scenario definition
│   ├── kerala_scenario.txt           # Kerala Floods & Landslides scenario definition
│   ├── uttarakhand_scenario.txt      # Uttarakhand Cloudburst scenario definition
│   └── odisha_scenario.txt           # Odisha Super Cyclone scenario definition
├── build.bat                 # Windows one-click compilation script
├── Makefile                  # Standard GNU / MinGW Makefile
└── README.md                 # Project documentation
```

---

## License
MIT License. Built for algorithmic emergency decision support.