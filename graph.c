#include <stdio.h>
#include <string.h>
#include "graph.h"

/* =========================================================
   Member 2 — Graph & Route Optimization Engineer
   ========================================================= */

int      graph[MAX_LOCATIONS][MAX_LOCATIONS];
int      originalGraph[MAX_LOCATIONS][MAX_LOCATIONS];
Location locations[MAX_LOCATIONS];
int      numLocations = 0;

int dist_[MAX_LOCATIONS];
int parent[MAX_LOCATIONS];

/* ---------------------------------------------------------
   Setup
   --------------------------------------------------------- */

void initializeGraph(void) {
    numLocations = 0;
    for (int i = 0; i < MAX_LOCATIONS; i++) {
        locations[i].id = -1;
        locations[i].name[0] = '\0';
        for (int j = 0; j < MAX_LOCATIONS; j++) {
            graph[i][j]         = (i == j) ? 0 : INF;
            originalGraph[i][j] = graph[i][j];
        }
    }
}

int addLocation(int id, const char *name) {
    if (numLocations >= MAX_LOCATIONS) {
        printf("Error: Maximum number of locations reached.\n");
        return -1;
    }
    // If id is -1, auto-assign for memory (though db should provide it)
    if (id == -1) id = numLocations;
    
    locations[numLocations].id = id;
    strncpy(locations[numLocations].name, name, MAX_NAME_LEN - 1);
    locations[numLocations].name[MAX_NAME_LEN - 1] = '\0';
    numLocations++;
    return id;
}

void addRoad(int source, int destination, int distance) {
    if (source < 0 || source >= numLocations ||
        destination < 0 || destination >= numLocations) {
        printf("Error: Invalid location id in addRoad().\n");
        return;
    }
    graph[source][destination] = distance;
    graph[destination][source] = distance; /* undirected road */

    originalGraph[source][destination] = distance;
    originalGraph[destination][source] = distance;
}

void loadAssamSampleData(void) {
    initializeGraph();

    int guwahati = addLocation(-1, "Guwahati");
    int jorhat   = addLocation(-1, "Jorhat");
    int dibrugarh = addLocation(-1, "Dibrugarh");
    int tezpur   = addLocation(-1, "Tezpur");
    int barpeta  = addLocation(-1, "Barpeta");

    /* Roads from the sample scenario in the proposal, plus a
       couple of extra links so the network stays connected
       and Dijkstra has real alternate-route choices. */
    addRoad(guwahati, jorhat, 10);
    addRoad(guwahati, barpeta, 20);
    addRoad(jorhat, tezpur, 15);
    addRoad(barpeta, tezpur, 12);
    addRoad(guwahati, tezpur, 22);
    addRoad(tezpur, dibrugarh, 30);
    addRoad(jorhat, dibrugarh, 25);
}

/* ---------------------------------------------------------
   Dynamic road conditions
   --------------------------------------------------------- */

void blockRoad(int source, int destination) {
    if (source < 0 || source >= numLocations ||
        destination < 0 || destination >= numLocations) {
        printf("Error: Invalid location id in blockRoad().\n");
        return;
    }
    if (graph[source][destination] == INF) {
        printf("No direct road exists between %s and %s.\n",
               getLocationName(source), getLocationName(destination));
        return;
    }
    graph[source][destination] = INF;
    graph[destination][source] = INF;
    printf("Road blocked: %s <-> %s\n",
           getLocationName(source), getLocationName(destination));
}

void unblockRoad(int source, int destination) {
    if (source < 0 || source >= numLocations ||
        destination < 0 || destination >= numLocations) {
        printf("Error: Invalid location id in unblockRoad().\n");
        return;
    }
    if (originalGraph[source][destination] == INF) {
        printf("No road ever existed between %s and %s.\n",
               getLocationName(source), getLocationName(destination));
        return;
    }
    graph[source][destination] = originalGraph[source][destination];
    graph[destination][source] = originalGraph[destination][source];
    printf("Road unblocked: %s <-> %s\n",
           getLocationName(source), getLocationName(destination));
}

int isRoadBlocked(int source, int destination) {
    if (source < 0 || source >= numLocations ||
        destination < 0 || destination >= numLocations) {
        return -1; /* invalid */
    }
    /* "blocked" = a road exists in original data but is currently INF */
    return (originalGraph[source][destination] != INF &&
            graph[source][destination] == INF) ? 1 : 0;
}

/* ---------------------------------------------------------
   BFS — used for reachability checks after roads change
   --------------------------------------------------------- */

void bfs(int start) {
    if (start < 0 || start >= numLocations) {
        printf("Error: Invalid start location in bfs().\n");
        return;
    }

    int visitedArr[MAX_LOCATIONS] = {0};
    int queue[MAX_LOCATIONS];
    int front = 0, rear = 0;

    visitedArr[start] = 1;
    queue[rear++] = start;

    printf("BFS from %s: ", getLocationName(start));

    while (front < rear) {
        int current = queue[front++];
        printf("%s ", getLocationName(current));

        for (int neighbor = 0; neighbor < numLocations; neighbor++) {
            if (graph[current][neighbor] != INF && !visitedArr[neighbor]) {
                visitedArr[neighbor] = 1;
                queue[rear++] = neighbor;
            }
        }
    }
    printf("\n");
}

int isReachable(int start, int destination) {
    if (start < 0 || start >= numLocations ||
        destination < 0 || destination >= numLocations) {
        return 0;
    }
    if (start == destination) return 1;

    int visitedArr[MAX_LOCATIONS] = {0};
    int queue[MAX_LOCATIONS];
    int front = 0, rear = 0;

    visitedArr[start] = 1;
    queue[rear++] = start;

    while (front < rear) {
        int current = queue[front++];
        if (current == destination) return 1;

        for (int neighbor = 0; neighbor < numLocations; neighbor++) {
            if (graph[current][neighbor] != INF && !visitedArr[neighbor]) {
                visitedArr[neighbor] = 1;
                queue[rear++] = neighbor;
            }
        }
    }
    return 0;
}

/* ---------------------------------------------------------
   DFS — additional traversal, demonstration only
   --------------------------------------------------------- */

void dfsUtil(int node, int visitedArr[]) {
    visitedArr[node] = 1;
    printf("%s ", getLocationName(node));

    for (int neighbor = 0; neighbor < numLocations; neighbor++) {
        if (graph[node][neighbor] != INF && !visitedArr[neighbor]) {
            dfsUtil(neighbor, visitedArr);
        }
    }
}

void dfs(int start) {
    if (start < 0 || start >= numLocations) {
        printf("Error: Invalid start location in dfs().\n");
        return;
    }
    int visitedArr[MAX_LOCATIONS] = {0};
    printf("DFS from %s: ", getLocationName(start));
    dfsUtil(start, visitedArr);
    printf("\n");
}

/* ---------------------------------------------------------
   Dijkstra — shortest route, O(V^2) with adjacency matrix
   --------------------------------------------------------- */

void dijkstra(int source, int destination) {
    if (source < 0 || source >= numLocations ||
        destination < 0 || destination >= numLocations) {
        printf("Error: Invalid location id in dijkstra().\n");
        return;
    }

    int visitedArr[MAX_LOCATIONS] = {0};

    for (int i = 0; i < numLocations; i++) {
        dist_[i]  = INF;
        parent[i] = -1;
    }
    dist_[source] = 0;

    for (int count = 0; count < numLocations - 1; count++) {
        /* pick the unvisited vertex with the smallest known distance */
        int minDist = INF, u = -1;
        for (int v = 0; v < numLocations; v++) {
            if (!visitedArr[v] && dist_[v] <= minDist) {
                minDist = dist_[v];
                u = v;
            }
        }

        if (u == -1) break; /* remaining vertices are unreachable */
        visitedArr[u] = 1;

        for (int v = 0; v < numLocations; v++) {
            if (!visitedArr[v] && graph[u][v] != INF &&
                dist_[u] != INF && dist_[u] + graph[u][v] < dist_[v]) {
                dist_[v]  = dist_[u] + graph[u][v];
                parent[v] = u;
            }
        }
    }

    if (dist_[destination] == INF) {
        printf("No route available: %s is unreachable from %s.\n",
               getLocationName(destination), getLocationName(source));
        return;
    }

    printf("Shortest distance from %s to %s: %d km\n",
           getLocationName(source), getLocationName(destination), dist_[destination]);
    printRoute(source, destination);
}

int getShortestDistance(int source, int destination) {
    dijkstra(source, destination);
    return dist_[destination];
}

int getShortestPath(int source, int destination, int pathBuffer[], int maxPathLen) {
    dijkstra(source, destination);
    if (dist_[destination] == INF || source < 0 || destination < 0 || source >= numLocations || destination >= numLocations) {
        return 0;
    }
    int temp[MAX_LOCATIONS];
    int count = 0;
    int current = destination;
    while (current != -1 && count < MAX_LOCATIONS) {
        temp[count++] = current;
        if (current == source) break;
        current = parent[current];
    }
    int outLen = 0;
    for (int i = count - 1; i >= 0 && outLen < maxPathLen; i--) {
        pathBuffer[outLen++] = temp[i];
    }
    return outLen;
}

void printRoute(int source, int destination) {
    if (dist_[destination] == INF) {
        printf("No route to display.\n");
        return;
    }

    int path[MAX_LOCATIONS];
    int count = 0;
    int current = destination;

    while (current != -1) {
        path[count++] = current;
        if (current == source) break;
        current = parent[current];
    }

    printf("Route: ");
    for (int i = count - 1; i >= 0; i--) {
        printf("%s", getLocationName(path[i]));
        if (i > 0) printf(" -> ");
    }
    printf("\n");
}

/* ---------------------------------------------------------
   Lookup helpers / display
   --------------------------------------------------------- */

int getLocationId(const char *name) {
    for (int i = 0; i < numLocations; i++) {
        if (strcmp(locations[i].name, name) == 0) return i;
    }
    return -1;
}

const char *getLocationName(int id) {
    if (id < 0 || id >= numLocations) return "UNKNOWN";
    return locations[id].name;
}

void printLocations(void) {
    printf("---- Locations ----\n");
    for (int i = 0; i < numLocations; i++) {
        printf("%d: %s\n", locations[i].id, locations[i].name);
    }
}

void printGraph(void) {
    printf("---- Road Network (adjacency matrix, INF = no road) ----\n");
    printf("%12s", "");
    for (int j = 0; j < numLocations; j++) printf("%12s", locations[j].name);
    printf("\n");

    for (int i = 0; i < numLocations; i++) {
        printf("%12s", locations[i].name);
        for (int j = 0; j < numLocations; j++) {
            if (graph[i][j] == INF) printf("%12s", "INF");
            else printf("%12d", graph[i][j]);
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------
   Standalone test driver.
   Compile with:  gcc -DGRAPH_TEST_MAIN graph.c -o graph_test
   Not compiled when graph.c is linked into the full project,
   since main.c (Member 3) provides the real main().
   --------------------------------------------------------- */
#ifdef GRAPH_TEST_MAIN
int main(void) {
    loadAssamSampleData();
    printLocations();
    printGraph();

    int guwahati = getLocationId("Guwahati");
    int jorhat   = getLocationId("Jorhat");
    int dibrugarh = getLocationId("Dibrugarh");

    printf("\n-- Reachability & traversal --\n");
    bfs(guwahati);
    dfs(guwahati);

    printf("\n-- Shortest route test --\n");
    dijkstra(guwahati, jorhat);

    printf("\n-- Road blocking test --\n");
    blockRoad(guwahati, jorhat);
    printf("Reachable after block? %s\n",
           isReachable(guwahati, jorhat) ? "YES" : "NO");
    dijkstra(guwahati, jorhat); /* should find an alternate route */

    unblockRoad(guwahati, jorhat);
    dijkstra(guwahati, jorhat); /* back to the direct route */

    printf("\n-- Unreachable location test --\n");
    printf("Dibrugarh reachable from Guwahati? %s\n",
           isReachable(guwahati, dibrugarh) ? "YES" : "NO");

    return 0;
}
#endif