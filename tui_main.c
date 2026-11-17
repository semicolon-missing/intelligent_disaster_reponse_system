#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <ncurses.h>

#include "priority_queue.h"
#include "graph.h"
#include "rescue.h"
#include "hospital.h"
#include "db.h"
#include "scenario_import.h"

// =========================================================================
//  INTELLIGENT DISASTER RESPONSE SYSTEM (IDRS) - TACTICAL TUI COMMAND CENTER
// =========================================================================

/* Required global variables declared in headers */
MaxHeap emergencyQueue;
char lastRescuePlan[4096] = "No dispatch plan executed yet.\n";

/* UI Constants & Enums */
#define NUM_TABS 5

enum 
{
    TAB_DASHBOARD = 0,
    TAB_NETWORK,
    TAB_RESOURCES,
    TAB_SCENARIOS,
    TAB_LOG
};

enum 
{
    MODAL_NONE = 0,
    MODAL_DISPATCH_RESULT,
    MODAL_NEW_EMERGENCY,
    MODAL_BLOCK_ROAD,
    MODAL_DIJKSTRA,
    MODAL_BFS,
    MODAL_SCENARIO_SELECT,
    MODAL_HELP,
    MODAL_CONFIRM_RESET
};

enum 
{
    PAIR_DEFAULT = 1,
    PAIR_HEADER,
    PAIR_TAB_ACTIVE,
    PAIR_TAB_INACTIVE,
    PAIR_CRITICAL,
    PAIR_WARNING,
    PAIR_OK,
    PAIR_SELECTED,
    PAIR_STATUS_BAR,
    PAIR_ACCENT,
    PAIR_DIM,
    PAIR_CYAN_ON_BLACK,
    PAIR_MAGENTA_ON_BLACK
};

/* Logging System */
#define MAX_LOGS 300

typedef struct 
{
    char timestamp[12];
    char message[256];
    int  colorPair;
} LogEntry;

static LogEntry eventLogs[MAX_LOGS];
static int      logCount = 0;

static void addLog(int colorPair, const char *fmt, ...) 
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    char msgBuf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    va_end(args);

    if (logCount < MAX_LOGS) 
    {
        snprintf(eventLogs[logCount].timestamp, sizeof(eventLogs[logCount].timestamp), "%s", timeBuf);
        snprintf(eventLogs[logCount].message, sizeof(eventLogs[logCount].message), "%s", msgBuf);
        eventLogs[logCount].colorPair = colorPair;
        logCount++;
    } 
    else 
    {
        memmove(&eventLogs[0], &eventLogs[1], sizeof(LogEntry) * (MAX_LOGS - 1));
        snprintf(eventLogs[MAX_LOGS - 1].timestamp, sizeof(eventLogs[MAX_LOGS - 1].timestamp), "%s", timeBuf);
        snprintf(eventLogs[MAX_LOGS - 1].message, sizeof(eventLogs[MAX_LOGS - 1].message), "%s", msgBuf);
        eventLogs[MAX_LOGS - 1].colorPair = colorPair;
    }
}

/* Dispatch Record */
typedef struct 
{
    int   valid;
    int   emergencyId;
    char  location[50];
    char  hazardType[50];
    int   affectedPeople;
    int   peopleNeedingMedical;
    float priorityScore;
    char  teamName[50];
    char  teamBase[50];
    char  transportType[50];
    char  destName[50];
    char  destLocation[50];
    int   destOccupancy;
    int   destCapacity;
    char  routeToEmergency[256];
    int   distToEmergency;
    char  routeToDestination[256];
    int   distToDestination;
    int   totalDist;
    char  timestamp[20];
} DispatchRecord;

static DispatchRecord lastDispatch;

/* UI State */
static int  currentTab = TAB_DASHBOARD;
static int  activeModal = MODAL_NONE;
static char currentScenarioName[80] = "Multi-Hazard Unified Crisis";

static int selEmergency = 0;
static int scrollEmergency = 0;

static int selRoad = 0;
static int scrollRoad = 0;

static int scrollLog = 0;

static int modalDijkstraSrc = 0;
static int modalDijkstraDest = 0;
static int modalBfsSrc = 0;
static int modalScenarioChoice = 0;

/* New Emergency Form State */
static int newEmLocIdx = 0;
static int newEmHazardIdx = 0;
static int newEmAffected = 150;
static int newEmMedical = 45;
static int newEmField = 0;

static const char *hazardTypesList[] = 
{
    "Flood", "Flash Flood", "Earthquake", "Cyclone",
    "Landslide", "Fire", "Tsunami", "Chemical", "Cloudburst", "Storm Surge"
};
static const int numHazardTypes = 10;

/* Road representation for table display */
typedef struct 
{
    int u;
    int v;
    int distance;
    int blocked;
} RoadEntry;

static RoadEntry roadList[MAX_LOCATIONS * 4];
static int       numRoadList = 0;

static void refreshRoadList(void) 
{
    numRoadList = 0;
    for (int i = 0; i < numLocations; i++) 
    {
        for (int j = i + 1; j < numLocations; j++) 
        {
            if (originalGraph[i][j] != INF) 
            {
                roadList[numRoadList].u        = i;
                roadList[numRoadList].v        = j;
                roadList[numRoadList].distance = originalGraph[i][j];
                roadList[numRoadList].blocked  = (graph[i][j] == INF);
                numRoadList++;
            }
        }
    }
}

/* Helpers to suppress stdout of legacy printf() functions during ncurses */
static void suppressStdoutBegin(int *savedStdout) 
{
    fflush(stdout);
    *savedStdout = dup(fileno(stdout));
    int devNull = open("/dev/null", O_WRONLY);
    
    if (devNull >= 0) 
    {
        dup2(devNull, fileno(stdout));
        close(devNull);
    }
}

static void suppressStdoutEnd(int savedStdout) 
{
    fflush(stdout);
    dup2(savedStdout, fileno(stdout));
    close(savedStdout);
}

/* Scenario Loading Routine */
static void loadScenarioById(int id) 
{
    int saved;
    suppressStdoutBegin(&saved);
    
    if (id == 1) 
    {
        if (importScenario("sample_natural_disasters.txt") != 0) 
        {
            loadAssamDemo();
        }
        strncpy(currentScenarioName, "Multi-Hazard Unified Crisis", sizeof(currentScenarioName) - 1);
    } 
    else if (id == 2) 
    {
        loadAssamDemo();
        strncpy(currentScenarioName, "Assam Brahmaputra Floods", sizeof(currentScenarioName) - 1);
    } 
    else if (id == 3) 
    {
        loadKeralaDemo();
        strncpy(currentScenarioName, "Kerala Floods & Landslides", sizeof(currentScenarioName) - 1);
    } 
    else if (id == 4) 
    {
        loadUttarakhandDemo();
        strncpy(currentScenarioName, "Uttarakhand Cloudburst", sizeof(currentScenarioName) - 1);
    } 
    else if (id == 5) 
    {
        loadOdishaDemo();
        strncpy(currentScenarioName, "Odisha Super Cyclone", sizeof(currentScenarioName) - 1);
    }
    suppressStdoutEnd(saved);

    refreshRoadList();
    
    selEmergency      = 0;
    scrollEmergency   = 0;
    selRoad           = 0;
    scrollRoad        = 0;
    modalDijkstraSrc  = 0;
    modalDijkstraDest = numLocations > 1 ? 1 : 0;
    modalBfsSrc       = 0;

    addLog(PAIR_OK, "[SCENARIO] Loaded '%s' (%d locations, %d pending alerts)",
           currentScenarioName, numLocations, emergencyQueue.size);
}

/* Simulation Reset Routine */
static void doResetSimulation(void) 
{
    resetRescueAvailabilities();
    resetDestinationsOccupancy();
    dbResetResources();
    loadAllData(&emergencyQueue);
    refreshRoadList();
    
    addLog(PAIR_OK, "[SYSTEM] Simulation reset: All teams on STANDBY, hospital occupancies cleared");
}

/* Road Block / Unblock Toggle */
static void toggleRoadBlock(int roadIdx) 
{
    if (roadIdx < 0 || roadIdx >= numRoadList) return;
    
    int u = roadList[roadIdx].u;
    int v = roadList[roadIdx].v;
    int saved;
    
    suppressStdoutBegin(&saved);
    if (roadList[roadIdx].blocked) 
    {
        unblockRoad(u, v);
        suppressStdoutEnd(saved);
        addLog(PAIR_OK, "[NETWORK] Road UNBLOCKED: %s <-> %s (%d km restored)",
               getLocationName(u), getLocationName(v), roadList[roadIdx].distance);
    } 
    else 
    {
        blockRoad(u, v);
        suppressStdoutEnd(saved);
        addLog(PAIR_CRITICAL, "[NETWORK] Road BLOCKED: %s <-> %s (Hazard severance)",
               getLocationName(u), getLocationName(v));
    }
    refreshRoadList();
}

/* Dispatch Highest Priority Emergency */
static void doDispatchHighest(void) 
{
    if (isHeapEmpty(&emergencyQueue)) 
    {
        addLog(PAIR_WARNING, "[DISPATCH] Failed: Emergency Priority Queue is empty.");
        return;
    }

    Emergency e = extractHighestPriority(&emergencyQueue);
    int e_locId = getLocationId(e.location);

    if (e_locId == -1) 
    {
        addLog(PAIR_CRITICAL, "[DISPATCH] Error: Location '%s' not found in graph.", e.location);
        insertEmergency(&emergencyQueue, e);
        return;
    }

    RescueTeam  *t  = selectRescueTeam(e, e_locId);
    Transport   *tr = selectTransport(e);
    Destination *d  = selectDestination(e, e_locId);

    if (!t) 
    {
        addLog(PAIR_CRITICAL, "[DISPATCH] Failed: No suitable/reachable rescue team for incident at %s.", e.location);
        insertEmergency(&emergencyQueue, e);
        return;
    }

    if (!d) 
    {
        addLog(PAIR_CRITICAL, "[DISPATCH] Failed: No reachable hospital with available capacity for incident at %s.", e.location);
        insertEmergency(&emergencyQueue, e);
        return;
    }

    t->available = 0;
    if (tr) tr->available = 0;
    
    d->currentOccupancy += e.affectedPeople;
    e.status = STATUS_RESCUED;

    dbUpdateRescueTeamAvailability(t->id, 0);
    if (tr) dbUpdateTransportAvailability(tr->id, 0);
    
    dbUpdateDestinationOccupancy(d->id, d->currentOccupancy);
    dbUpdateEmergencyStatus(e.id, e.status);

    int distToEm   = getShortestDistance(t->location, e_locId);
    int distToDest = getShortestDistance(e_locId, d->location);

    int pathEm[MAX_LOCATIONS], pathDest[MAX_LOCATIONS];
    int pathEmLen   = getShortestPath(t->location, e_locId, pathEm, MAX_LOCATIONS);
    int pathDestLen = getShortestPath(e_locId, d->location, pathDest, MAX_LOCATIONS);

    char emRouteStr[256] = "", destRouteStr[256] = "";
    
    for (int i = 0; i < pathEmLen; i++) 
    {
        strncat(emRouteStr, getLocationName(pathEm[i]), sizeof(emRouteStr) - strlen(emRouteStr) - 1);
        if (i < pathEmLen - 1) strncat(emRouteStr, " -> ", sizeof(emRouteStr) - strlen(emRouteStr) - 1);
    }
    
    for (int i = 0; i < pathDestLen; i++) 
    {
        strncat(destRouteStr, getLocationName(pathDest[i]), sizeof(destRouteStr) - strlen(destRouteStr) - 1);
        if (i < pathDestLen - 1) strncat(destRouteStr, " -> ", sizeof(destRouteStr) - strlen(destRouteStr) - 1);
    }

    lastDispatch.valid = 1;
    lastDispatch.emergencyId = e.id;
    snprintf(lastDispatch.location, sizeof(lastDispatch.location), "%s", e.location);
    snprintf(lastDispatch.hazardType, sizeof(lastDispatch.hazardType), "%s", e.hazardType);
    lastDispatch.affectedPeople = e.affectedPeople;
    lastDispatch.peopleNeedingMedical = e.peopleNeedingMedical;
    lastDispatch.priorityScore = e.priorityScore;

    snprintf(lastDispatch.teamName, sizeof(lastDispatch.teamName), "%s", t->name);
    snprintf(lastDispatch.teamBase, sizeof(lastDispatch.teamBase), "%s", getLocationName(t->location));
    snprintf(lastDispatch.transportType, sizeof(lastDispatch.transportType), "%s", tr ? tr->type : "Tactical Ground Convoy");
    snprintf(lastDispatch.destName, sizeof(lastDispatch.destName), "%s", d->name);
    snprintf(lastDispatch.destLocation, sizeof(lastDispatch.destLocation), "%s", getLocationName(d->location));
    lastDispatch.destOccupancy = d->currentOccupancy;
    lastDispatch.destCapacity = d->capacity;

    snprintf(lastDispatch.routeToEmergency, sizeof(lastDispatch.routeToEmergency), "%s", emRouteStr);
    lastDispatch.distToEmergency = distToEm;
    snprintf(lastDispatch.routeToDestination, sizeof(lastDispatch.routeToDestination), "%s", destRouteStr);
    lastDispatch.distToDestination = distToDest;
    lastDispatch.totalDist = distToEm + distToDest;

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    snprintf(lastDispatch.timestamp, sizeof(lastDispatch.timestamp), "%02d:%02d:%02d",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    snprintf(lastRescuePlan, sizeof(lastRescuePlan),
             "MISSION DISPATCH DIRECTIVE #%d\n"
             "Incident @ %s (%s, %d victims) [Priority: %.2f]\n"
             "Assigned Team: %s (Base: %s)\n"
             "Transport: %s\n"
             "Destination: %s (%d/%d beds)\n"
             "Route In: %s (%d km)\n"
             "Route Out: %s (%d km)\n"
             "Total Transit: %d km\n",
             e.id, e.location, e.hazardType, e.affectedPeople, e.priorityScore,
             t->name, lastDispatch.teamBase, lastDispatch.transportType,
             d->name, d->currentOccupancy, d->capacity,
             emRouteStr, distToEm, destRouteStr, distToDest, lastDispatch.totalDist);

    addLog(PAIR_OK, "[DISPATCH] #%d @ %s -> Team '%s' deployed via %s (Total: %d km)",
           e.id, e.location, t->name, lastDispatch.transportType, lastDispatch.totalDist);

    activeModal = MODAL_DISPATCH_RESULT;
}

/* Signal Handler for Clean Terminal Exit */
static void handleSignal(int sig) 
{
    (void)sig;
    endwin();
    closeDB();
    printf("\nIDRS TUI Terminated Cleanly. Stay safe!\n");
    exit(0);
}

/* Render Top Banner & Status HUD */
static void drawHeader(void) 
{
    int maxRows, maxCols;
    getmaxyx(stdscr, maxRows, maxCols);
    (void)maxRows;

    // Line 0: Top title banner
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(0, 0, ' ', maxCols);
    mvprintw(0, 2, "◈ INTELLIGENT DISASTER RESPONSE SYSTEM (IDRS) :: TACTICAL COMMAND CENTER ◈");
    
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "SYSTEM ACTIVE | %02d:%02d:%02d",
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
             
    mvprintw(0, maxCols - (int)strlen(timeStr) - 2, "%s", timeStr);
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    // Line 1: Quick Status Stats
    int availTeams = 0;
    for (int i = 0; i < numTeams; i++) 
    {
        if (teams[i].available) availTeams++;
    }
        
    int availTrans = 0;
    for (int i = 0; i < numTransports; i++) 
    {
        if (transports[i].available) availTrans++;
    }
        
    int totalOccupancy = 0, totalCapacity = 0;
    for (int i = 0; i < numDestinations; i++) 
    {
        totalOccupancy += destinations[i].currentOccupancy;
        totalCapacity  += destinations[i].capacity;
    }
    
    float occPct = totalCapacity > 0 ? ((float)totalOccupancy / totalCapacity) * 100.0f : 0.0f;
    int blockedRoadCount = 0;
    for (int i = 0; i < numRoadList; i++) 
    {
        if (roadList[i].blocked) blockedRoadCount++;
    }

    attron(COLOR_PAIR(PAIR_DEFAULT));
    mvhline(1, 0, ' ', maxCols);
    mvprintw(1, 2, "SCENARIO: %s | PENDING: %d | TEAMS: %d/%d | TRANS: %d/%d | BEDS: %d/%d (%.1f%%) | BLOCKED ROADS: %d",
             currentScenarioName, emergencyQueue.size,
             availTeams, numTeams, availTrans, numTransports,
             totalOccupancy, totalCapacity, occPct, blockedRoadCount);
    attroff(COLOR_PAIR(PAIR_DEFAULT));

    // Line 2: Tab Bar
    const char *tabs[NUM_TABS] = 
    {
        "[1] DASHBOARD",
        "[2] NETWORK & ROUTING",
        "[3] RESOURCES & HOSPITALS",
        "[4] SCENARIOS & SIM",
        "[5] EVENT LOG"
    };

    mvhline(2, 0, ACS_HLINE, maxCols);
    int col = 2;
    for (int i = 0; i < NUM_TABS; i++) 
    {
        if (i == currentTab) 
        {
            attron(COLOR_PAIR(PAIR_TAB_ACTIVE) | A_BOLD);
            mvprintw(2, col, " %s ", tabs[i]);
            attroff(COLOR_PAIR(PAIR_TAB_ACTIVE) | A_BOLD);
        } 
        else 
        {
            attron(COLOR_PAIR(PAIR_TAB_INACTIVE));
            mvprintw(2, col, " %s ", tabs[i]);
            attroff(COLOR_PAIR(PAIR_TAB_INACTIVE));
        }
        col += strlen(tabs[i]) + 4;
    }
}

/* Render Bottom Shortcut Help Bar */
static void drawFooter(void) 
{
    int maxRows, maxCols;
    getmaxyx(stdscr, maxRows, maxCols);

    attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    mvhline(maxRows - 1, 0, ' ', maxCols);
    mvprintw(maxRows - 1, 1,
             "[D/Space] Dispatch Top  [N] New Alert  [B] Block Road  [P] Shortest Route  [C] Connectivity  [S] Scenarios  [R] Reset  [Tab] Switch View  [?] Help  [Q] Exit");
    attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
}

/* Helper to draw framed boxes */
static void drawBox(int y, int x, int h, int w, const char *title, int colorPair) 
{
    attron(COLOR_PAIR(colorPair));
    mvhline(y, x, ACS_HLINE, w);
    mvhline(y + h - 1, x, ACS_HLINE, w);
    mvvline(y, x, ACS_VLINE, h);
    mvvline(y, x + w - 1, ACS_VLINE, h);
    
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    
    if (title && strlen(title) > 0) 
    {
        attron(A_BOLD);
        mvprintw(y, x + 2, "┤ %s ├", title);
        attroff(A_BOLD);
    }
    attroff(COLOR_PAIR(colorPair));
}

/* Tab 1: Dashboard View */
static void drawDashboardView(int startY, int maxRows, int maxCols) 
{
    int viewH  = maxRows - startY - 1;
    int leftW  = maxCols * 55 / 100;
    int rightW = maxCols - leftW - 1;

    // Left Panel: Priority Queue
    drawBox(startY, 0, viewH, leftW, "INCIDENT PRIORITY QUEUE (MAX-HEAP ORDER)", PAIR_CYAN_ON_BLACK);

    int headerY = startY + 1;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(headerY, 2, "%-4s %-6s %-16s %-12s %-6s %-6s %-8s",
             "RNK", "SCORE", "LOCATION", "HAZARD", "AFFCT", "MEDIC", "STATUS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(headerY + 1, 1, ACS_HLINE, leftW - 2);

    int listY = headerY + 2;
    int listH = viewH - 7;
    
    if (emergencyQueue.size == 0) 
    {
        attron(COLOR_PAIR(PAIR_DIM));
        mvprintw(listY + 2, 4, "No pending emergencies in queue. All clear!");
        attroff(COLOR_PAIR(PAIR_DIM));
    } 
    else 
    {
        if (selEmergency >= emergencyQueue.size) selEmergency = emergencyQueue.size - 1;
        if (selEmergency < 0) selEmergency = 0;
        
        if (selEmergency < scrollEmergency) scrollEmergency = selEmergency;
        if (selEmergency >= scrollEmergency + listH) scrollEmergency = selEmergency - listH + 1;

        for (int i = 0; i < listH; i++) 
        {
            int idx = scrollEmergency + i;
            int curY = listY + i;
            mvhline(curY, 1, ' ', leftW - 2);
            
            if (idx >= emergencyQueue.size) break;

            Emergency e = emergencyQueue.data[idx];
            int isSelected = (idx == selEmergency);

            int scoreColor = PAIR_OK;
            if (e.priorityScore >= 8.0f) scoreColor = PAIR_CRITICAL;
            else if (e.priorityScore >= 5.0f) scoreColor = PAIR_WARNING;

            if (isSelected) 
            {
                attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
                mvhline(curY, 1, ' ', leftW - 2);
            } 
            else 
            {
                attron(COLOR_PAIR(scoreColor));
            }

            mvprintw(curY, 2, "#%-3d %-6.2f %-16.16s %-12.12s %-6d %-6d %-8s",
                     idx + 1, e.priorityScore, e.location, e.hazardType,
                     e.affectedPeople, e.peopleNeedingMedical,
                     e.status == STATUS_PENDING ? "PENDING" : (e.status == STATUS_ASSIGNED ? "ASSIGNED" : "RESCUED"));

            if (isSelected) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
            else attroff(COLOR_PAIR(scoreColor));
        }
    }

    // Selected emergency detail preview at bottom of left panel
    int detailY = startY + viewH - 4;
    mvhline(detailY, 1, ACS_HLINE, leftW - 2);
    
    if (emergencyQueue.size > 0 && selEmergency >= 0 && selEmergency < emergencyQueue.size) 
    {
        Emergency e = emergencyQueue.data[selEmergency];
        float medRatio = e.affectedPeople > 0 ? ((float)e.peopleNeedingMedical / e.affectedPeople) * 100.0f : 0.0f;
        
        attron(COLOR_PAIR(PAIR_DEFAULT) | A_BOLD);
        mvprintw(detailY + 1, 2, "Alert #%d @ %s | Hazard: %s | Severity: %.2f",
                 e.id, e.location, e.hazardType, e.priorityScore);
        mvprintw(detailY + 2, 2, "Casualty: %d souls | Critical Medical: %d (%.1f%%) | Action: Press [D] to Dispatch",
                 e.affectedPeople, e.peopleNeedingMedical, medRatio);
        attroff(COLOR_PAIR(PAIR_DEFAULT) | A_BOLD);
    } 
    else 
    {
        mvprintw(detailY + 1, 2, "Select an emergency using [Up/Down] arrows.");
    }

    // Right Top Panel: Latest Mission Directive
    int rightTopH = viewH * 55 / 100;
    drawBox(startY, leftW, rightTopH, rightW, "LATEST MISSION DISPATCH DIRECTIVE", PAIR_CYAN_ON_BLACK);

    if (lastDispatch.valid) 
    {
        int dY = startY + 1;
        attron(COLOR_PAIR(PAIR_OK) | A_BOLD);
        mvprintw(dY++, leftW + 2, "MISSION DIRECTIVE #%d [%s]", lastDispatch.emergencyId, lastDispatch.timestamp);
        attroff(COLOR_PAIR(PAIR_OK) | A_BOLD);
        
        mvprintw(dY++, leftW + 2, "Incident Site   : %s (%s, %d victims)",
                 lastDispatch.location, lastDispatch.hazardType, lastDispatch.affectedPeople);
        mvprintw(dY++, leftW + 2, "Assigned Team   : %s (Base: %s)",
                 lastDispatch.teamName, lastDispatch.teamBase);
        mvprintw(dY++, leftW + 2, "Transport Unit  : %s", lastDispatch.transportType);
        mvprintw(dY++, leftW + 2, "Evacuation Dest : %s (Beds: %d/%d)",
                 lastDispatch.destName, lastDispatch.destOccupancy, lastDispatch.destCapacity);
        dY++;
        
        attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(dY++, leftW + 2, "Route to Incident (%d km):", lastDispatch.distToEmergency);
        attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(dY++, leftW + 4, "%-.*s", rightW - 6, lastDispatch.routeToEmergency);

        attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(dY++, leftW + 2, "Route to Hospital (%d km):", lastDispatch.distToDestination);
        attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(dY++, leftW + 4, "%-.*s", rightW - 6, lastDispatch.routeToDestination);

        attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(dY++, leftW + 2, "Total Mission Transit: %d km", lastDispatch.totalDist);
        attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    } 
    else 
    {
        int dY = startY + 2;
        attron(COLOR_PAIR(PAIR_DIM));
        mvprintw(dY++, leftW + 2, "No emergency dispatched yet in this session.");
        mvprintw(dY++, leftW + 2, "Press [D] or [Space] to dispatch the highest priority crisis.");
        mvprintw(dY++, leftW + 2, "The algorithmic engine will automatically:");
        mvprintw(dY++, leftW + 4, "• Query Max-Heap for the highest urgent incident");
        mvprintw(dY++, leftW + 4, "• Greedily match capable Rescue Team + Vehicle");
        mvprintw(dY++, leftW + 4, "• Verify road reachability with BFS");
        mvprintw(dY++, leftW + 4, "• Calculate shortest path turn-by-turn with Dijkstra");
        mvprintw(dY++, leftW + 4, "• Route victims to nearest Hospital with bed capacity");
        attroff(COLOR_PAIR(PAIR_DIM));
    }

    // Right Bottom Panel: Tactical Event Feed
    int rightBotY = startY + rightTopH;
    int rightBotH = viewH - rightTopH;
    drawBox(rightBotY, leftW, rightBotH, rightW, "TACTICAL EVENT FEED", PAIR_CYAN_ON_BLACK);

    int feedLines = rightBotH - 2;
    int startIdx  = logCount - feedLines;
    
    if (startIdx < 0) startIdx = 0;
    for (int i = 0; i < feedLines; i++) 
    {
        int idx = startIdx + i;
        if (idx >= logCount) break;
        
        int curY = rightBotY + 1 + i;
        attron(COLOR_PAIR(eventLogs[idx].colorPair));
        mvprintw(curY, leftW + 2, "[%s] %-.*s",
                 eventLogs[idx].timestamp, rightW - 15, eventLogs[idx].message);
        attroff(COLOR_PAIR(eventLogs[idx].colorPair));
    }
}

/* Tab 2: Network & Routing View */
static void drawNetworkView(int startY, int maxRows, int maxCols) 
{
    int viewH  = maxRows - startY - 1;
    int leftW  = maxCols * 50 / 100;
    int rightW = maxCols - leftW - 1;

    // Left: Road Network List
    drawBox(startY, 0, viewH, leftW