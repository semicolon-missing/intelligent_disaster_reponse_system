# Intelligent Disaster Response System

A C-based disaster-response decision-support system that uses
algorithmic techniques to prioritize emergencies, allocate rescue
resources, and determine efficient rescue routes.

## Overview

The project simulates an Assam flood scenario where multiple
emergencies may occur simultaneously.

The system can:

- Prioritize emergencies
- Select suitable rescue teams
- Select transportation
- Check location connectivity
- Find efficient routes
- Handle blocked roads
- Select hospitals/relief camps
- Generate a rescue plan

## Algorithms Used

| Algorithm | Purpose |
|---|---|
| Max Heap / Priority Queue | Emergency prioritization |
| Greedy Algorithm | Rescue team & transport selection |
| BFS | Reachability checking |
| DFS | Graph traversal |
| Dijkstra | Shortest route |
| Graph | Represent roads and locations |

## Tech Stack

- **Language:** C
- **Interface:** Console
- **Graph:** Adjacency Matrix
- **Storage:** In-memory
- **Compiler:** GCC

## Project Structure

```text
Intelligent-Disaster-Response/
├── main.c
├── priority_queue.c
├── priority_queue.h
├── rescue.c
├── rescue.h
├── graph.c
├── graph.h
├── hospital.c
├── hospital.h
└── README.md