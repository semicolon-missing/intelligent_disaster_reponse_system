# PROJECT SYNOPSIS

---

### 1) Project Title
**Intelligent Disaster Response System (IDRS)**  
*An Algorithmic Emergency Decision-Support, Evacuation Routing, and Resource Dispatch Command System*

---

### 2) Team Members Details

| S.No. | 10-Digit Roll No. | Name | Section (CSE-A / CSE-B) | Mobile No. |
|:---:|:---:|:---|:---:|:---:|
| 1 | `[Enter 10-Digit Roll No]` | **Vivek Kumar Pandey** *(First Member / Team Lead)* | `[CSE-A / CSE-B]` | `[Enter Mobile No]` |
| 2 | `[Enter 10-Digit Roll No]` | **Shivam Kumar** | `[CSE-A / CSE-B]` | `[Enter Mobile No]` |
| 3 | `[Enter 10-Digit Roll No]` | **Shreyansh Pant** | `[CSE-A / CSE-B]` | `[Enter Mobile No]` |

> *Note: As per submission guidelines, the first member (Vivek Kumar Pandey) will upload this synopsis on behalf of the team.*

---

### 3) Introduction (1 Page Max)

During sudden-onset natural and humanitarian catastrophes—such as riverine floods, mountain landslides, cloudbursts, earthquakes, and industrial hazmat accidents—emergency operation centers (EOCs) face critical operational bottlenecks. Emergency coordinators are routinely overwhelmed by simultaneous distress calls from geographically dispersed sectors, rapidly changing hazard intensities, and incomplete field data. 

Traditional disaster response mechanisms suffer from three foundational vulnerabilities:
1. **Unstructured Incident Triage:** Distress calls are traditionally handled on a First-Come, First-Served (FIFO) basis or through subjective human appraisal, delaying emergency interventions to high-casualty epicenters.
2. **Static Navigation Vulnerability:** Standard GPS routing solutions assume an uninterrupted road network. When bridges collapse or flash floods submerge roads, responders blindly encounter impassable dead ends, leading to catastrophic delays.
3. **Asymmetric & Unchecked Resource Allocation:** Relief teams without specialized capabilities (e.g., motorboats in flood zones or trauma kits for critical casualties) are frequently misallocated, while downstream regional hospitals quickly become overwhelmed due to a lack of live bed capacity coordination.

The **Intelligent Disaster Response System (IDRS)** is an algorithmic decision-support and dispatch command platform engineered from scratch in pure C (C11). IDRS centralizes and automates the entire disaster response lifecycle through a mathematically rigorous, low-latency computational engine:
- **Prioritized Triage:** Incoming emergencies are dynamically scored across hazard severity, total population at risk, and critical medical casualty ratio, maintaining an $O(\log k)$ binary Max-Heap so that the highest-priority incident is always addressed first.
- **Dynamic Topological Routing:** Road networks are modeled as weighted graphs where flooded or severed roads can be dynamically blocked ($w = \infty$) and unblocked in real time.
- **Topological Reachability Guarantees:** A Breadth-First Search (BFS) reachability validation routine ensures that an intact physical path exists before allocating teams or directing evacuations.
- **Turn-by-Turn Waypoint Reconstruction:** Dijkstra's shortest path algorithm identifies optimal, unblocked transit corridors and reconstructs step-by-step waypoint directions.
- **Capacity-Aware Greedy Matching:** A multi-criteria greedy allocator matches rescue squads based on proximity and equipment (e.g., mandatory boats for flood zones), while routing victims exclusively to receiving hospitals and relief shelters with verified residual bed capacity.

The system features both an interactive 25-option terminal CLI (`cli.exe`) for low-level systems control and a tactical web command center (`web.exe`) with real-time graph visualization, backed by persistent SQLite3 storage and a custom high-performance Winsock2 HTTP REST daemon.

---

### 4) Selected Algorithms

The core of IDRS is built entirely upon classical data structures and algorithms, ensuring sub-millisecond execution and deterministic behavior:

#### A. Max-Heap Priority Queue (Dynamic Emergency Prioritization)
- **Mathematical Scoring Model:** Each incoming emergency $E$ is assigned a bounded composite priority score $S(E) \in [0.0, 10.0]$:
  $$S(E) = \min\left(10.0, \; 0.40 \cdot H(h) + 0.30 \cdot P(N) + 0.30 \cdot M(m, N)\right)$$
  - $H(h)$: Hazard Severity Scale ($1.0 - 10.0$), mapping hazards like Tsunamis/Hazmat ($10.0$), Earthquakes ($9.5$), Flash Floods/Cloudbursts ($9.0$), and Fires ($8.5$).
  - $P(N) = \min(10.0, \frac{N}{1000} \times 10.0)$: Normalized population at risk (saturating at 1,000 victims).
  - $M(m, N) = \min(10.0, \frac{m}{N} \times 10.0)$: Critical medical urgency ratio.
- **Data Structure:** Array-based binary Max-Heap where each parent node satisfies $\text{Parent.Score} \ge \text{Child.Score}$.
- **Complexity:**
  - Emergency Ingestion / Insertion: $O(\log k)$ via `heapifyUp()`
  - Highest-Priority Extraction (Dispatch): $O(\log k)$ via `heapifyDown()`
  - Priority Inspection (Top of Queue): $O(1)$

#### B. Breadth-First Search (BFS) (Reachability & Topological Connectivity Verification)
- **Purpose:** Disasters isolate regions through landslides, broken bridges, and floodwaters. Before dispatching vehicles or directing evacuation convoys, the BFS algorithm traverses the road graph to confirm that the destination belongs to the same connected component:
  $$\text{Reachable}(s, d) \iff d \in \text{ConnectedComponent}(s) \text{ in } G_{\text{current}}$$
- **Operational Guarantee:** If all connecting roads to an incident or hospital are severed ($w = \infty$), the system immediately flags the node as unreachable, preventing rescue squads from being dispatched into dead ends.
- **Complexity:** $O(|V| + |E|)$ time; implemented with a static FIFO queue and boolean visitation array.

#### C. Dijkstra’s Algorithm with Path Reconstruction (Shortest Turn-by-Turn Dynamic Routing)
- **Purpose:** Computes the shortest safe transit path between:
  1. Selected Rescue Team Base $\to$ Emergency Incident Site.
  2. Incident Site $\to$ Selected Receiving Hospital / Evacuation Shelter.
- **Dynamic Road Condition Handling:** Severed edges are assigned a weight of $\infty$ ($999999$). When a road is cleared, the system restores the surveyed distance from an immutable baseline matrix `originalGraph`.
- **Intermediate Vertex Backtracking:** Alongside minimum distances $\text{dist}[v]$, the algorithm maintains a predecessor array $\pi[v]$. A recursive backtracking routine reconstructs the exact sequence of intermediate road waypoints (`[Guwahati -> Barpeta -> Tezpur -> Jorhat]`) for responders to navigate.
- **Complexity:** $O(|V|^2)$ time utilizing adjacency matrix; $O(|V|)$ auxiliary space.

#### D. Multi-Criteria Greedy Resource Allocation
- **Rescue Squad & Fleet Matching:** Evaluates all active teams using a multi-factor fitness heuristic:
  $$Score(T) = 500 + S_{\text{capacity}}(T) + S_{\text{medical}}(T) + S_{\text{boat}}(T) - 2 \cdot \text{dist}(T.\text{loc}, E.\text{loc})$$
  - *Hard Constraints:* Responders lacking motorboats are disqualified from water hazards (Floods, Tsunamis). Teams lacking advanced medical kits are disqualified if the casualty medical ratio $\frac{m}{N} \ge 0.40$. Unreachable teams are disqualified via BFS.
- **Capacity-Constrained Hospital Allocation:** Selects the closest facility satisfying $\text{Capacity} - \text{Occupancy} \ge N$ and medical trauma capabilities, updating occupancy counters dynamically upon dispatch to prevent facility saturation.

---

### 5) Backend Used

The backend is developed as a modular, high-performance systems architecture in C:

- **Core Language:** Pure **C (C11 Standard)** compiled with GCC / MinGW-w64. Delivers deterministic execution, sub-millisecond route calculation, and a minimal memory footprint (< 15 MB RAM).
- **In-Memory Data Structures:**
  - 2D Adjacency Matrices (`graph[50][50]` and `originalGraph[50][50]`) for $O(1)$ edge lookups and instantaneous road blocking/unblocking.
  - Array-backed binary Max-Heap for emergency prioritization.
  - Cache-friendly contiguous structures for locations, rescue squads, transport fleets, and hospitals.
- **Relational Database (Persistence Layer):**
  - Embedded **SQLite3** engine (`disaster_v2.db`).
  - Stores all entities across 6 relational tables: `Location`, `Road`, `Emergency`, `RescueTeam`, `Transport`, and `Destination`.
  - Bidirectional synchronization ensures in-memory data structures are populated on boot and state changes (road cuts, dispatched missions, bed occupancy) persist across reboots.
- **Networking & RESTful Web Server:**
  - Custom multi-threaded, non-blocking HTTP daemon built with native **Windows Sockets (Winsock2 / `ws2_32.lib`)** running on port `8080`.
  - Exposes 17+ REST API endpoints handling JSON payloads (`/api/metrics`, `/api/queue`, `/api/process`, `/api/route`, `/api/block`, `/api/unblock`, `/api/teams`, `/api/destinations`, etc.).
- **Declarative Scenario Parser:**
  - Custom file tokenizer (`scenario_import.c`) supporting `.txt` scenario definitions and pre-configured real-world disaster profiles (Assam Floods, Kerala Landslides, Uttarakhand Cloudburst, Odisha Super Cyclone).

---

### 6) Frontend Used

The system provides two distinct user interfaces tailored for operational field environments and command centers:

#### A. Tactical Web Command Center (`web.exe` $\to$ `http://localhost:8080`)
- **Core Technologies:** HTML5, CSS3, JavaScript (ES6+), Tailwind CSS.
- **Interactive Topological Network Visualization (Vis.js):**
  - Displays the regional road graph as a dynamic, interactive physics network.
  - Color-codes vertices by entity type (Incident Epicenters, Rescue Stations, Hospitals, Transit Hubs).
  - Visualizes roads with real-time traversal weights; blocked roads are highlighted in dashed red, and active evacuation routes are illuminated in bold blue.
  - Allows interactive road toggling: dispatchers can click any roadway to instantly block or clear it.
- **Head-Up Display (HUD) & Telemetry Dashboard:**
  - Real-time metric cards tracking active emergencies, total victims, ready rescue fleet, shelter occupancy percentages, and blocked roads.
  - Live Max-Heap priority queue viewer with visual badges for hazard severity and casualty counts.
  - One-click "Dispatch Responders" control that executes the automated allocation pipeline, animating the turn-by-turn route and displaying the dispatch manifest.
  - Historical Mission Audit log detailing dispatched teams, chosen transport, target hospitals, and transit distances with timestamps.

#### B. Systems Console CLI (`cli.exe`)
- An interactive **25-option terminal interface** designed for technical operators, command-line environments, low-bandwidth links, and automated test script verification.
- Organized into clear functional sub-menus: Map & Network Configuration, Resource & Destination Management, Scenario Loading & Simulation, and Emergency Ingestion & Algorithmic Dispatch.
