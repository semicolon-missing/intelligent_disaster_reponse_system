#ifndef GRAPH_H
#define GRAPH_H

/* =========================================================
   Member 2 — Graph & Route Optimization Engineer
   Handles: Graph representation, Dijkstra, BFS, DFS,
            road blocking, route reconstruction.
   ========================================================= */

#define MAX_LOCATIONS   50
#define MAX_NAME_LEN    50
#define INF             999999   /* "no road" sentinel, also used as unreachable distance */

/* ---------------------------------------------------------
   Location: a vertex in the disaster graph (city, hospital,
   relief camp, etc). IDs are assigned automatically by
   addLocation() starting at 0.
   --------------------------------------------------------- */
typedef struct {
    int  id;
    char name[MAX_NAME_LEN];
} Location;

/* ---------------------------------------------------------
   Global graph state (adjacency matrix representation).
   graph[i][j] = distance between location i and j.
                 INF means no direct road (or road is blocked).
   ---------------------------------------------------------
   These are defined in graph.c and shared with main.c /
   hospital.c wherever route information is needed.
   --------------------------------------------------------- */
extern int      graph[MAX_LOCATIONS][MAX_LOCATIONS];
extern int      originalGraph[MAX_LOCATIONS][MAX_LOCATIONS]; /* keeps distances so unblockRoad can restore them */
extern Location locations[MAX_LOCATIONS];
extern int      numLocations;

/* Dijkstra result buffers, filled by dijkstra() and read by printRoute() */
extern int dist_[MAX_LOCATIONS];
extern int parent[MAX_LOCATIONS];

/* ---------------------------------------------------------
   Setup
   --------------------------------------------------------- */
void initializeGraph(void);
int  addLocation(int id, const char *name);                 /* returns the new location's id, -1 if full */
void addRoad(int source, int destination, int distance);
void loadAssamSampleData(void);                      /* preloads the sample flood scenario locations/roads */

/* ---------------------------------------------------------
   Dynamic road conditions
   --------------------------------------------------------- */
void blockRoad(int source, int destination);
void unblockRoad(int source, int destination);
int  isRoadBlocked(int source, int destination);

/* ---------------------------------------------------------
   Traversal / reachability
   --------------------------------------------------------- */
void bfs(int start);                                 /* prints BFS order from start */
int  isReachable(int start, int destination);         /* 1 if destination reachable from start, else 0 */

void dfs(int start);                                  /* prints DFS order from start */
void dfsUtil(int node, int visitedArr[]);

/* ---------------------------------------------------------
   Shortest path
   --------------------------------------------------------- */
void dijkstra(int source, int destination);           /* fills dist_[] and parent[], prints summary */
int  getShortestDistance(int source, int destination); /* convenience: run dijkstra + return dist_[destination] */
int  getShortestPath(int source, int destination, int pathBuffer[], int maxPathLen); /* returns length of path */
void printRoute(int source, int destination);          /* reconstructs path from parent[] and prints it */

/* ---------------------------------------------------------
   Lookup helpers / display
   --------------------------------------------------------- */
int         getLocationId(const char *name);           /* -1 if not found */
const char *getLocationName(int id);
void        printGraph(void);
void        printLocations(void);

// #endif /* GRAPH_H *//