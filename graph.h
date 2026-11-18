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
   relief camp, etc). A location's id is always its index in
   locations[] / graph[][] (0, 1, 2, ...). addLocation() assigns
   the next free index when given -1, and rejects any other id
   that does not equal that next index.
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
int  addLocation(int id, const char *name);                 /* returns the new location's id, -1 if full or id is out of sequence */
int  addRoad(int source, int destination, int distance);    /* distance must be in 1..INF-1; returns 0 on success, -1 if rejected */
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
int  getShortestDistance(int source, int destination); /* silent; returns INF if unreachable or ids invalid */
int  getShortestPath(int source, int destination, int pathBuffer[], int maxPathLen); /* silent; returns length of path, 0 if none */
void printRoute(int source, int destination);          /* reconstructs path from the last dijkstra() run and prints it */

/* ---------------------------------------------------------
   Lookup helpers / display
   --------------------------------------------------------- */
int         getLocationId(const char *name);           /* exact match first, then case-insensitive; -1 if not found */
const char *getLocationName(int id);
void        printGraph(void);
void        printLocations(void);

#endif /* GRAPH_H */
