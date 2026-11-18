#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
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
    drawBox(startY, 0, viewH, leftW, "ROAD NETWORK & HAZARD CONDITIONS", PAIR_CYAN_ON_BLACK);

    int headerY = startY + 1;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(headerY, 2, "%-4s %-16s %-16s %-6s %-12s",
             "ID", "FROM LOCATION", "TO LOCATION", "DIST", "ROAD STATUS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(headerY + 1, 1, ACS_HLINE, leftW - 2);

    int listY = headerY + 2;
    int listH = viewH - 6;

    if (selRoad >= numRoadList) selRoad = numRoadList - 1;
    if (selRoad < 0) selRoad = 0;
    if (selRoad < scrollRoad) scrollRoad = selRoad;
    if (selRoad >= scrollRoad + listH) scrollRoad = selRoad - listH + 1;

    for (int i = 0; i < listH; i++) {
        int idx = scrollRoad + i;
        int curY = listY + i;
        mvhline(curY, 1, ' ', leftW - 2);
        if (idx >= numRoadList) break;

        RoadEntry r = roadList[idx];
        int isSelected = (idx == selRoad);

        if (isSelected) {
            attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
            mvhline(curY, 1, ' ', leftW - 2);
        }

        const char *fromName = getLocationName(r.u);
        const char *toName = getLocationName(r.v);
        char distStr[16];
        snprintf(distStr, sizeof(distStr), "%d km", r.distance);

        mvprintw(curY, 2, "#%-3d %-16.16s %-16.16s %-6s",
                 idx + 1, fromName, toName, distStr);

        if (!isSelected) {
            if (r.blocked) attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            else attron(COLOR_PAIR(PAIR_OK));
        }

        printw(" %s", r.blocked ? "✖ BLOCKED" : "✔ CLEAR");

        if (!isSelected) {
            if (r.blocked) attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            else attroff(COLOR_PAIR(PAIR_OK));
        }

        if (isSelected) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
    }

    int tipY = startY + viewH - 2;
    mvhline(tipY - 1, 1, ACS_HLINE, leftW - 2);
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(tipY, 2, "Action: Select road and press [B] or [Space] to toggle Blocked/Clear");
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);

    // Right: Interactive Route & Reachability Analyzer
    drawBox(startY, leftW, viewH, rightW, "ALGORITHMIC ROUTE & REACHABILITY ANALYZER", PAIR_CYAN_ON_BLACK);

    int rY = startY + 2;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(rY++, leftW + 2, "1. DIJKSTRA SHORTEST PATH CALCULATION");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    if (numLocations >= 2) {
        if (modalDijkstraSrc >= numLocations) modalDijkstraSrc = 0;
        if (modalDijkstraDest >= numLocations) modalDijkstraDest = numLocations - 1;

        const char *srcName = getLocationName(modalDijkstraSrc);
        const char *destName = getLocationName(modalDijkstraDest);
        mvprintw(rY++, leftW + 4, "Origin: %s (ID %d)   -->   Destination: %s (ID %d)",
                 srcName, modalDijkstraSrc, destName, modalDijkstraDest);

        int dist = getShortestDistance(modalDijkstraSrc, modalDijkstraDest);
        int path[MAX_LOCATIONS];
        int pathLen = getShortestPath(modalDijkstraSrc, modalDijkstraDest, path, MAX_LOCATIONS);

        if (dist == INF || pathLen <= 0) {
            attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            mvprintw(rY++, leftW + 4, "STATUS: UNREACHABLE (Physical road disconnected or blocked by disaster)");
            attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
        } else {
            attron(COLOR_PAIR(PAIR_OK) | A_BOLD);
            mvprintw(rY++, leftW + 4, "STATUS: CONNECTED (Shortest Distance: %d km)", dist);
            attroff(COLOR_PAIR(PAIR_OK) | A_BOLD);

            char routeStr[256] = "";
            for (int k = 0; k < pathLen; k++) {
                strncat(routeStr, getLocationName(path[k]), sizeof(routeStr) - strlen(routeStr) - 1);
                if (k < pathLen - 1) strncat(routeStr, " -> ", sizeof(routeStr) - strlen(routeStr) - 1);
            }
            mvprintw(rY++, leftW + 4, "Path: %-.*s", rightW - 10, routeStr);
        }
    }
    mvprintw(rY++, leftW + 4, "Hint: Press [P] to change start/destination pair.");
    rY++;

    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(rY++, leftW + 2, "2. BFS REACHABILITY VALIDATOR");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

    if (modalBfsSrc >= numLocations) modalBfsSrc = 0;
    const char *bfsSrcName = getLocationName(modalBfsSrc);
    mvprintw(rY++, leftW + 4, "Start Node: %s (ID %d)", bfsSrcName, modalBfsSrc);

    int reachableCount = 0;
    char reachableList[256] = "";
    char unreachableList[256] = "";

    for (int k = 0; k < numLocations; k++) {
        if (k == modalBfsSrc) continue;
        if (isReachable(modalBfsSrc, k)) {
            reachableCount++;
            if (strlen(reachableList) > 0) strncat(reachableList, ", ", sizeof(reachableList) - strlen(reachableList) - 1);
            strncat(reachableList, getLocationName(k), sizeof(reachableList) - strlen(reachableList) - 1);
        } else {
            if (strlen(unreachableList) > 0) strncat(unreachableList, ", ", sizeof(unreachableList) - strlen(unreachableList) - 1);
            strncat(unreachableList, getLocationName(k), sizeof(unreachableList) - strlen(unreachableList) - 1);
        }
    }

    attron(COLOR_PAIR(PAIR_OK));
    mvprintw(rY++, leftW + 4, "Reachable Nodes (%d): %-.*s",
             reachableCount, rightW - 30, reachableList[0] ? reachableList : "None");
    attroff(COLOR_PAIR(PAIR_OK));

    if (strlen(unreachableList) > 0) {
        attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
        mvprintw(rY++, leftW + 4, "Disconnected Isolated Nodes: %-.*s",
                 rightW - 35, unreachableList);
        attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
    } else {
        attron(COLOR_PAIR(PAIR_OK));
        mvprintw(rY++, leftW + 4, "Isolated Nodes: None (Fully Connected Graph)");
        attroff(COLOR_PAIR(PAIR_OK));
    }
    mvprintw(rY++, leftW + 4, "Hint: Press [C] to run BFS reachability analysis on another node.");
    rY++;

    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(rY++, leftW + 2, "3. GRAPH VERTEX METRICS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(rY++, leftW + 4, "Total Vertices : %d Locations", numLocations);
    mvprintw(rY++, leftW + 4, "Total Edges    : %d Road Segments", numRoadList);
    int blockedCount = 0;
    for (int k = 0; k < numRoadList; k++) if (roadList[k].blocked) blockedCount++;
    mvprintw(rY++, leftW + 4, "Blocked Roads  : %d (%s)", blockedCount,
             blockedCount > 0 ? "Alternate routing in effect" : "Optimal highway connectivity");
}

/* Tab 3: Resources & Facilities View */
static void drawResourcesView(int startY, int maxRows, int maxCols) {
    int viewH = maxRows - startY - 1;
    int topH = viewH * 48 / 100;
    int botH = viewH - topH;

    // Top Panel: Rescue Teams & Transport
    drawBox(startY, 0, topH, maxCols, "RESCUE FLEET & TRANSPORT VEHICLES", PAIR_CYAN_ON_BLACK);

    int tColW = maxCols * 60 / 100;
    int trColW = maxCols - tColW - 1;

    // Teams Table
    int headY = startY + 1;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(headY, 2, "%-4s %-25s %-16s %-5s %-5s %-5s %-9s",
             "ID", "RESCUE TEAM", "BASE LOCATION", "CAP", "MED", "BOAT", "STATUS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(headY + 1, 1, ACS_HLINE, tColW - 2);

    int tListY = headY + 2;
    int tListH = topH - 4;
    for (int i = 0; i < tListH && i < numTeams; i++) {
        int curY = tListY + i;
        RescueTeam t = teams[i];
        int col = t.available ? PAIR_OK : PAIR_CRITICAL;
        attron(COLOR_PAIR(col));
        mvprintw(curY, 2, "#%-3d %-25.25s %-16.16s %-5d %-5s %-5s %-9s",
                 t.id, t.name, getLocationName(t.location), t.capacity,
                 t.medicalKit ? "YES" : "NO", t.boatAvailable ? "YES" : "NO",
                 t.available ? "STANDBY" : "DEPLOYED");
        attroff(COLOR_PAIR(col));
    }

    // Divider between teams and transport
    for (int y = startY + 1; y < startY + topH - 1; y++) {
        mvaddch(y, tColW, ACS_VLINE);
    }

    // Transports Table
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(headY, tColW + 2, "%-4s %-22s %-6s %-8s %-8s",
             "ID", "VEHICLE TYPE", "CAP", "AMPHIB", "STATUS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(headY + 1, tColW + 1, ACS_HLINE, trColW - 2);

    for (int i = 0; i < tListH && i < numTransports; i++) {
        int curY = tListY + i;
        Transport tr = transports[i];
        int col = tr.available ? PAIR_OK : PAIR_WARNING;
        attron(COLOR_PAIR(col));
        mvprintw(curY, tColW + 2, "#%-3d %-22.22s %-6d %-8s %-8s",
                 tr.id, tr.type, tr.capacity,
                 tr.floodSuitable ? "YES" : "NO",
                 tr.available ? "STANDBY" : "EN ROUTE");
        attroff(COLOR_PAIR(col));
    }

    // Bottom Panel: Hospitals & Shelters
    int botY = startY + topH;
    drawBox(botY, 0, botH, maxCols, "EMERGENCY HOSPITALS & EVACUATION DESTINATIONS", PAIR_CYAN_ON_BLACK);

    int hHeadY = botY + 1;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(hHeadY, 2, "%-4s %-30s %-16s %-14s %-22s %-8s %-8s",
             "ID", "FACILITY NAME", "LOCATION", "BEDS (OCC/CAP)", "CAPACITY LOAD BAR", "TRAUMA", "STATUS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(hHeadY + 1, 1, ACS_HLINE, maxCols - 2);

    int hListY = hHeadY + 2;
    int hListH = botH - 4;
    for (int i = 0; i < hListH && i < numDestinations; i++) {
        int curY = hListY + i;
        Destination d = destinations[i];
        float pct = d.capacity > 0 ? ((float)d.currentOccupancy / d.capacity) * 100.0f : 0.0f;
        int barPairs = PAIR_OK;
        if (pct >= 85.0f) barPairs = PAIR_CRITICAL;
        else if (pct >= 50.0f) barPairs = PAIR_WARNING;

        char bedStr[24];
        snprintf(bedStr, sizeof(bedStr), "%d / %d", d.currentOccupancy, d.capacity);

        // Progress bar: 15 chars
        int barLen = 15;
        int filled = (int)((pct / 100.0f) * barLen);
        if (filled > barLen) filled = barLen;
        char barStr[32];
        int pos = 0;
        barStr[pos++] = '[';
        for (int b = 0; b < barLen; b++) {
            barStr[pos++] = (b < filled) ? '|' : ' ';
        }
        barStr[pos++] = ']';
        snprintf(barStr + pos, sizeof(barStr) - pos, " %4.1f%%", pct);

        mvprintw(curY, 2, "#%-3d %-30.30s %-16.16s %-14s ",
                 d.id, d.name, getLocationName(d.location), bedStr);

        attron(COLOR_PAIR(barPairs) | A_BOLD);
        printw("%-22s ", barStr);
        attroff(COLOR_PAIR(barPairs) | A_BOLD);

        attron(COLOR_PAIR(d.medicalSupport ? PAIR_OK : PAIR_DIM));
        printw("%-8s ", d.medicalSupport ? "YES" : "NO");
        attroff(COLOR_PAIR(d.medicalSupport ? PAIR_OK : PAIR_DIM));

        attron(COLOR_PAIR(d.accessible ? PAIR_OK : PAIR_CRITICAL));
        printw("%-8s", d.accessible ? "OPEN" : "CUT OFF");
        attroff(COLOR_PAIR(d.accessible ? PAIR_OK : PAIR_CRITICAL));
    }
}

/* Tab 4: Scenarios & Crisis Simulator View */
static void drawScenariosView(int startY, int maxRows, int maxCols) {
    int viewH = maxRows - startY - 1;
    drawBox(startY, 0, viewH, maxCols, "DISASTER SCENARIOS & CRISIS SIMULATOR", PAIR_CYAN_ON_BLACK);

    int sY = startY + 2;
    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(sY++, 4, "SELECT PRE-CONFIGURED CRISIS SCENARIO OR IMPORT CUSTOM TOPOLOGY");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    sY++;

    struct ScenarioCard {
        int id;
        const char *key;
        const char *name;
        const char *desc;
        const char *stats;
    } scenarios[5] = {
        {1, "[1]", "Multi-Hazard Unified Crisis",
         "Unified disaster testbed covering Cyclones, Earthquakes, Flash Floods, Landslides, Tsunamis, and Hazmat emergencies across diverse terrain.",
         "10 Locations | 14 Roads | 7 Teams | 6 Transport | 6 Hospitals | 8 High-Priority Emergencies"},
        {2, "[2]", "Assam Brahmaputra Floods",
         "Major monsoon riverbank breaches along the Brahmaputra River. Requires amphibious boats and helicopter evacuations across Guwahati, Tezpur, Dibrugarh.",
         "8 Locations | 13 Roads | 5 Teams | 4 Transport | 6 Hospitals | 5 Water Emergencies"},
        {3, "[3]", "Kerala Monsoons & Landslides",
         "Hilly terrain crisis in the Western Ghats (Wayanad, Idukki, Munnar). Severe road blockages, mudslides, and dam reservoir overflows.",
         "8 Locations | 9 Roads | 5 Teams | 4 Transport | 5 Hospitals | 4 Landslide Emergencies"},
        {4, "[4]", "Uttarakhand Cloudburst & Flash Floods",
         "High-altitude mountain flash flood crisis in Chamoli, Kedarnath, and Joshimath. Highway washouts isolating mountain pilgrimage centers.",
         "8 Locations | 8 Roads | 5 Teams | 4 Transport | 5 Hospitals | 4 Alpine Emergencies"},
        {5, "[5]", "Odisha Super Cyclone",
         "Category 5 coastal cyclone and storm surge hitting Paradip, Puri, and Balasore. Large-scale coastal evacuation to inland trauma centers.",
         "7 Locations | 8 Roads | 5 Teams | 4 Transport | 5 Hospitals | 4 Coastal Emergencies"}
    };

    for (int i = 0; i < 5; i++) {
        int isCurrent = (strcmp(currentScenarioName, scenarios[i].name) == 0);
        int pair = isCurrent ? PAIR_OK : PAIR_DEFAULT;

        attron(COLOR_PAIR(pair) | A_BOLD);
        mvprintw(sY++, 4, "%s %s %s",
                 scenarios[i].key, scenarios[i].name, isCurrent ? "★ (ACTIVE)" : "");
        attroff(COLOR_PAIR(pair) | A_BOLD);

        attron(COLOR_PAIR(PAIR_DEFAULT));
        mvprintw(sY++, 8, "%-.*s", maxCols - 12, scenarios[i].desc);
        attron(COLOR_PAIR(PAIR_DIM));
        mvprintw(sY++, 8, "Topology: %s", scenarios[i].stats);
        attroff(COLOR_PAIR(PAIR_DIM));
        attroff(COLOR_PAIR(PAIR_DEFAULT));
        sY++;
    }

    mvhline(sY++, 2, ACS_HLINE, maxCols - 4);
    attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
    mvprintw(sY++, 4, "SIMULATION CONTROLS:");
    mvprintw(sY++, 6, "• Press [1-5] to instantly load that disaster scenario.");
    mvprintw(sY++, 6, "• Press [R] to reset all resource availabilities and clear hospital occupancies.");
    mvprintw(sY++, 6, "• Press [N] to inject a new custom crisis emergency at any time.");
    attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
}

/* Tab 5: Event Log View */
static void drawLogView(int startY, int maxRows, int maxCols) {
    int viewH = maxRows - startY - 1;
    drawBox(startY, 0, viewH, maxCols, "SYSTEM TACTICAL EVENT LOG & AUDIT TRAIL", PAIR_CYAN_ON_BLACK);

    int logLines = viewH - 4;
    if (scrollLog < 0) scrollLog = 0;
    if (scrollLog > logCount - logLines) scrollLog = logCount - logLines;
    if (scrollLog < 0) scrollLog = 0;

    attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvprintw(startY + 1, 2, "%-10s %s", "TIMESTAMP", "EVENT DETAILS");
    attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
    mvhline(startY + 2, 1, ACS_HLINE, maxCols - 2);

    int curY = startY + 3;
    for (int i = 0; i < logLines; i++) {
        int idx = scrollLog + i;
        if (idx >= logCount) break;
        attron(COLOR_PAIR(eventLogs[idx].colorPair));
        mvprintw(curY++, 2, "[%s]  %-.*s",
                 eventLogs[idx].timestamp, maxCols - 18, eventLogs[idx].message);
        attroff(COLOR_PAIR(eventLogs[idx].colorPair));
    }

    attron(COLOR_PAIR(PAIR_DIM));
    mvprintw(startY + viewH - 1, 4,
             "Showing events %d - %d of %d | Use [Up/Down/PageUp/PageDown] to scroll",
             scrollLog + 1, (scrollLog + logLines > logCount ? logCount : scrollLog + logLines), logCount);
    attroff(COLOR_PAIR(PAIR_DIM));
}

/* Render Active Modals */
static void drawModals(int maxRows, int maxCols) {
    if (activeModal == MODAL_NONE) return;

    if (activeModal == MODAL_DISPATCH_RESULT) {
        int mW = maxCols > 76 ? 74 : maxCols - 4;
        int mH = 21;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "MISSION DISPATCH DIRECTIVE", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        attron(COLOR_PAIR(PAIR_OK) | A_BOLD);
        mvprintw(line++, mX + 3, "MISSION AUTHORIZED :: CRISIS DIRECTIVE #%d", lastDispatch.emergencyId);
        attroff(COLOR_PAIR(PAIR_OK) | A_BOLD);
        mvprintw(line++, mX + 3, "Execution Time      : %s", lastDispatch.timestamp);
        mvprintw(line++, mX + 3, "Incident Site       : %s [Hazard: %s]", lastDispatch.location, lastDispatch.hazardType);
        mvprintw(line++, mX + 3, "Casualty Scale      : %d affected (%d requiring critical care)",
                 lastDispatch.affectedPeople, lastDispatch.peopleNeedingMedical);
        mvprintw(line++, mX + 3, "Priority Score      : %.2f / 10.0", lastDispatch.priorityScore);
        mvhline(line++, mX + 2, ACS_HLINE, mW - 4);

        attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(line++, mX + 3, "RESOURCES COMMITTED");
        attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(line++, mX + 3, "Assigned Rescue Team: %s (Base: %s)", lastDispatch.teamName, lastDispatch.teamBase);
        mvprintw(line++, mX + 3, "Transport Vehicle   : %s", lastDispatch.transportType);
        mvprintw(line++, mX + 3, "Receiving Facility  : %s (Location: %s)", lastDispatch.destName, lastDispatch.destLocation);
        mvprintw(line++, mX + 3, "Hospital Bed Load   : %d / %d beds (Updated)",
                 lastDispatch.destOccupancy, lastDispatch.destCapacity);
        mvhline(line++, mX + 2, ACS_HLINE, mW - 4);

        attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(line++, mX + 3, "DIJKSTRA TRANSIT CORRIDORS");
        attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(line++, mX + 3, "Route to Incident   : %-.*s (%d km)", mW - 32, lastDispatch.routeToEmergency, lastDispatch.distToEmergency);
        mvprintw(line++, mX + 3, "Route to Hospital   : %-.*s (%d km)", mW - 32, lastDispatch.routeToDestination, lastDispatch.distToDestination);
        attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(line++, mX + 3, "Total Mission Dist  : %d km", lastDispatch.totalDist);
        attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + (mW - 28) / 2, " [ Press Enter / Esc to Close ] ");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
    else if (activeModal == MODAL_NEW_EMERGENCY) {
        int mW = maxCols > 66 ? 64 : maxCols - 4;
        int mH = 17;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "REPORT NEW CRISIS EMERGENCY", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        mvprintw(line++, mX + 3, "Enter incident parameters to calculate priority score & queue:");
        line++;

        // Field 0: Location
        if (newEmField == 0) attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
        mvprintw(line++, mX + 3, "1. Location     : < %-20.20s >", getLocationName(newEmLocIdx));
        if (newEmField == 0) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);

        // Field 1: Hazard Type
        if (newEmField == 1) attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
        mvprintw(line++, mX + 3, "2. Hazard Type  : < %-20.20s >", hazardTypesList[newEmHazardIdx]);
        if (newEmField == 1) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);

        // Field 2: Affected People
        if (newEmField == 2) attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
        mvprintw(line++, mX + 3, "3. Affected Souls : [ %-6d ] (Left/Right to adjust)", newEmAffected);
        if (newEmField == 2) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);

        // Field 3: People Needing Medical
        if (newEmField == 3) attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
        mvprintw(line++, mX + 3, "4. Urgent Medical : [ %-6d ] (Left/Right to adjust)", newEmMedical);
        if (newEmField == 3) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);

        // Live preview of calculated priority
        Emergency tempE;
        memset(&tempE, 0, sizeof(tempE));
        strncpy(tempE.hazardType, hazardTypesList[newEmHazardIdx], sizeof(tempE.hazardType) - 1);
        tempE.affectedPeople = newEmAffected;
        tempE.peopleNeedingMedical = newEmMedical;
        float previewScore = calculatePriority(tempE);

        line++;
        mvhline(line++, mX + 2, ACS_HLINE, mW - 4);
        attron(COLOR_PAIR(PAIR_OK) | A_BOLD);
        mvprintw(line++, mX + 3, "Calculated Composite Priority Score : %.2f / 10.0", previewScore);
        attroff(COLOR_PAIR(PAIR_OK) | A_BOLD);

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + 4, "[Up/Down] Field  [Left/Right] Value  [Enter] Confirm  [Esc] Cancel");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
    else if (activeModal == MODAL_DIJKSTRA) {
        int mW = maxCols > 66 ? 64 : maxCols - 4;
        int mH = 16;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "DIJKSTRA SHORTEST PATH ROUTE QUERY", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        mvprintw(line++, mX + 3, "Select Origin and Destination to compute shortest unblocked path:");
        line++;

        mvprintw(line++, mX + 3, "Origin Node      [Left/Right] : < %s >", getLocationName(modalDijkstraSrc));
        mvprintw(line++, mX + 3, "Destination Node [Up/Down]    : < %s >", getLocationName(modalDijkstraDest));
        mvhline(line++, mX + 2, ACS_HLINE, mW - 4);

        int dist = getShortestDistance(modalDijkstraSrc, modalDijkstraDest);
        int path[MAX_LOCATIONS];
        int pathLen = getShortestPath(modalDijkstraSrc, modalDijkstraDest, path, MAX_LOCATIONS);

        if (dist == INF || pathLen <= 0) {
            attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            mvprintw(line++, mX + 3, "STATUS: UNREACHABLE (No clear path exists)");
            attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
        } else {
            attron(COLOR_PAIR(PAIR_OK) | A_BOLD);
            mvprintw(line++, mX + 3, "Shortest Distance : %d km", dist);
            attroff(COLOR_PAIR(PAIR_OK) | A_BOLD);
            char rStr[256] = "";
            for (int k = 0; k < pathLen; k++) {
                strncat(rStr, getLocationName(path[k]), sizeof(rStr) - strlen(rStr) - 1);
                if (k < pathLen - 1) strncat(rStr, " -> ", sizeof(rStr) - strlen(rStr) - 1);
            }
            mvprintw(line++, mX + 3, "Path: %-.*s", mW - 12, rStr);
        }

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + (mW - 30) / 2, " [ Press Enter / Esc to Close ] ");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
    else if (activeModal == MODAL_BFS) {
        int mW = maxCols > 66 ? 64 : maxCols - 4;
        int mH = 16;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "BFS REACHABILITY & CLUSTER VALIDATOR", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        mvprintw(line++, mX + 3, "Select Starting Node to verify flood reachability [Left/Right]:");
        line++;

        mvprintw(line++, mX + 3, "Root Location: < %s (ID %d) >", getLocationName(modalBfsSrc), modalBfsSrc);
        mvhline(line++, mX + 2, ACS_HLINE, mW - 4);

        char reachableList[256] = "";
        char unreachableList[256] = "";
        int reachableCount = 0;

        for (int k = 0; k < numLocations; k++) {
            if (k == modalBfsSrc) continue;
            if (isReachable(modalBfsSrc, k)) {
                reachableCount++;
                if (strlen(reachableList) > 0) strncat(reachableList, ", ", sizeof(reachableList) - strlen(reachableList) - 1);
                strncat(reachableList, getLocationName(k), sizeof(reachableList) - strlen(reachableList) - 1);
            } else {
                if (strlen(unreachableList) > 0) strncat(unreachableList, ", ", sizeof(unreachableList) - strlen(unreachableList) - 1);
                strncat(unreachableList, getLocationName(k), sizeof(unreachableList) - strlen(unreachableList) - 1);
            }
        }

        attron(COLOR_PAIR(PAIR_OK));
        mvprintw(line++, mX + 3, "Connected Nodes (%d): %-.*s", reachableCount, mW - 28, reachableList[0] ? reachableList : "None");
        attroff(COLOR_PAIR(PAIR_OK));

        if (strlen(unreachableList) > 0) {
            attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            mvprintw(line++, mX + 3, "Isolated / Severed: %-.*s", mW - 28, unreachableList);
            attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
        } else {
            attron(COLOR_PAIR(PAIR_OK));
            mvprintw(line++, mX + 3, "Fully connected to all network locations.");
            attroff(COLOR_PAIR(PAIR_OK));
        }

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + (mW - 30) / 2, " [ Press Enter / Esc to Close ] ");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
    else if (activeModal == MODAL_SCENARIO_SELECT) {
        int mW = maxCols > 66 ? 64 : maxCols - 4;
        int mH = 15;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "QUICK SCENARIO SWITCHER", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        mvprintw(line++, mX + 3, "Select scenario number to reload simulation:");
        line++;

        const char *scNames[5] = {
            "[1] Multi-Hazard Unified Crisis (Sample)",
            "[2] Assam Brahmaputra Floods",
            "[3] Kerala Floods & Landslides",
            "[4] Uttarakhand Cloudburst",
            "[5] Odisha Super Cyclone"
        };

        for (int i = 0; i < 5; i++) {
            if (modalScenarioChoice == i) attron(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
            mvprintw(line++, mX + 4, "%s", scNames[i]);
            if (modalScenarioChoice == i) attroff(COLOR_PAIR(PAIR_SELECTED) | A_BOLD);
        }

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + 3, " [1-5/Up/Down] Select  [Enter] Load  [Esc] Cancel ");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
    else if (activeModal == MODAL_HELP) {
        int mW = maxCols > 76 ? 74 : maxCols - 4;
        int mH = 21;
        int mX = (maxCols - mW) / 2;
        int mY = (maxRows - mH) / 2;

        drawBox(mY, mX, mH, mW, "IDRS COMMAND CENTER USER MANUAL & SHORTCUTS", PAIR_TAB_ACTIVE);

        int line = mY + 2;
        attron(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(line++, mX + 3, "KEYBOARD SHORTCUTS & NAVIGATION");
        attroff(COLOR_PAIR(PAIR_HEADER) | A_BOLD);
        mvprintw(line++, mX + 3, "[1 - 5] or [Tab]  : Switch between HUD Views");
        mvprintw(line++, mX + 3, "[D] or [Space]    : Dispatch highest priority incident (Max-Heap)");
        mvprintw(line++, mX + 3, "[N]               : Report new disaster emergency (Interactive Form)");
        mvprintw(line++, mX + 3, "[B]               : Toggle road blockage on selected road");
        mvprintw(line++, mX + 3, "[P]               : Calculate shortest unblocked route (Dijkstra)");
        mvprintw(line++, mX + 3, "[C]               : Test network reachability & connectivity (BFS)");
        mvprintw(line++, mX + 3, "[S]               : Quick scenario switcher");
        mvprintw(line++, mX + 3, "[R]               : Reset simulation resources and occupancies");
        mvprintw(line++, mX + 3, "[Up / Down]       : Navigate lists and table cursors");
        mvprintw(line++, mX + 3, "[Q] or [Esc]      : Quit / Exit modal");
        line++;

        attron(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(line++, mX + 3, "ALGORITHMIC GUARANTEES");
        attroff(COLOR_PAIR(PAIR_ACCENT) | A_BOLD);
        mvprintw(line++, mX + 3, "• Priority Queue : Max-Heap O(log N) composite hazard score");
        mvprintw(line++, mX + 3, "• Shortest Path  : Dijkstra with intermediate vertex reconstruction");
        mvprintw(line++, mX + 3, "• Reachability   : BFS O(V + E) flood disconnect validation");
        mvprintw(line++, mX + 3, "• Resource Match : Multi-criteria greedy team & hospital optimizer");

        attron(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
        mvprintw(mY + mH - 2, mX + (mW - 28) / 2, " [ Press Enter / Esc to Close ] ");
        attroff(COLOR_PAIR(PAIR_STATUS_BAR) | A_BOLD);
    }
}

/* Main Loop */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    // Initialize Database
    if (initDB("disaster_v2.db") != 0) {
        fprintf(stderr, "Failed to initialize SQLite database 'disaster_v2.db'.\n");
        return 1;
    }

    initHeap(&emergencyQueue);
    loadAllData(&emergencyQueue);

    if (numLocations == 0) {
        if (importScenario("sample_natural_disasters.txt") != 0) {
            loadAssamDemo();
        }
        strncpy(currentScenarioName, "Multi-Hazard Unified Crisis", sizeof(currentScenarioName) - 1);
    } else {
        strncpy(currentScenarioName, "Persistent SQLite Scenario", sizeof(currentScenarioName) - 1);
    }

    refreshRoadList();

    // Initialize Curses
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(500); // 500ms timeout for non-blocking clock update

    if (has_colors()) {
        start_color();
        use_default_colors();

        init_pair(PAIR_DEFAULT, COLOR_WHITE, -1);
        init_pair(PAIR_HEADER, COLOR_CYAN, -1);
        init_pair(PAIR_TAB_ACTIVE, COLOR_BLACK, COLOR_CYAN);
        init_pair(PAIR_TAB_INACTIVE, COLOR_WHITE, -1);
        init_pair(PAIR_CRITICAL, COLOR_RED, -1);
        init_pair(PAIR_WARNING, COLOR_YELLOW, -1);
        init_pair(PAIR_OK, COLOR_GREEN, -1);
        init_pair(PAIR_SELECTED, COLOR_BLACK, COLOR_WHITE);
        init_pair(PAIR_STATUS_BAR, COLOR_BLACK, COLOR_CYAN);
        init_pair(PAIR_ACCENT, COLOR_MAGENTA, -1);
        init_pair(PAIR_DIM, COLOR_BLUE, -1);
        init_pair(PAIR_CYAN_ON_BLACK, COLOR_CYAN, -1);
        init_pair(PAIR_MAGENTA_ON_BLACK, COLOR_MAGENTA, -1);
    }

    addLog(PAIR_OK, "[SYSTEM] IDRS Tactical Command TUI initialized successfully");
    addLog(PAIR_OK, "[SYSTEM] Active Scenario: %s (%d locations, %d roads, %d pending incidents)",
           currentScenarioName, numLocations, numRoadList, emergencyQueue.size);

    int ch;
    int running = 1;

    while (running) {
        // Location pickers cycle with "% numLocations": never open them on an empty map
        if (numLocations == 0 &&
            (activeModal == MODAL_DIJKSTRA || activeModal == MODAL_BFS || activeModal == MODAL_NEW_EMERGENCY)) {
            addLog(PAIR_WARNING, "[SYSTEM] No locations loaded. Press [S] to load a scenario first.");
            activeModal = MODAL_NONE;
        }

        int maxRows, maxCols;
        getmaxyx(stdscr, maxRows, maxCols);
        erase();

        if (maxRows < 20 || maxCols < 75) {
            attron(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            mvprintw(maxRows / 2, (maxCols - 45) / 2 > 0 ? (maxCols - 45) / 2 : 0,
                     "TERMINAL TOO SMALL: PLEASE ENLARGE WINDOW (MIN 80x24)");
            attroff(COLOR_PAIR(PAIR_CRITICAL) | A_BOLD);
            refresh();
            ch = getch();
            if (ch == 'q' || ch == 'Q') break;
            continue;
        }

        drawHeader();
        drawFooter();

        int startY = 3;
        switch (currentTab) {
            case TAB_DASHBOARD:
                drawDashboardView(startY, maxRows, maxCols);
                break;
            case TAB_NETWORK:
                drawNetworkView(startY, maxRows, maxCols);
                break;
            case TAB_RESOURCES:
                drawResourcesView(startY, maxRows, maxCols);
                break;
            case TAB_SCENARIOS:
                drawScenariosView(startY, maxRows, maxCols);
                break;
            case TAB_LOG:
                drawLogView(startY, maxRows, maxCols);
                break;
        }

        if (activeModal != MODAL_NONE) {
            drawModals(maxRows, maxCols);
        }

        refresh();

        ch = getch();
        if (ch == ERR) continue; // timeout expired, redraw clock & stats

        // Modal Input Handling
        if (activeModal != MODAL_NONE) {
            if (ch == 27 || ch == 'q' || ch == 'Q') { // Esc or Q closes modal
                activeModal = MODAL_NONE;
                continue;
            }

            if (activeModal == MODAL_DISPATCH_RESULT ||
                activeModal == MODAL_DIJKSTRA ||
                activeModal == MODAL_BFS ||
                activeModal == MODAL_HELP) {
                if (ch == '\n' || ch == KEY_ENTER || ch == ' ') {
                    activeModal = MODAL_NONE;
                }
                // Allow changing Dijkstra inputs inside modal
                if (activeModal == MODAL_DIJKSTRA) {
                    if (ch == KEY_LEFT) {
                        modalDijkstraSrc = (modalDijkstraSrc - 1 + numLocations) % numLocations;
                    } else if (ch == KEY_RIGHT) {
                        modalDijkstraSrc = (modalDijkstraSrc + 1) % numLocations;
                    } else if (ch == KEY_UP) {
                        modalDijkstraDest = (modalDijkstraDest - 1 + numLocations) % numLocations;
                    } else if (ch == KEY_DOWN) {
                        modalDijkstraDest = (modalDijkstraDest + 1) % numLocations;
                    }
                } else if (activeModal == MODAL_BFS) {
                    if (ch == KEY_LEFT) {
                        modalBfsSrc = (modalBfsSrc - 1 + numLocations) % numLocations;
                    } else if (ch == KEY_RIGHT) {
                        modalBfsSrc = (modalBfsSrc + 1) % numLocations;
                    }
                }
                continue;
            }

            if (activeModal == MODAL_SCENARIO_SELECT) {
                if (ch >= '1' && ch <= '5') {
                    loadScenarioById(ch - '0');
                    activeModal = MODAL_NONE;
                } else if (ch == KEY_UP) {
                    modalScenarioChoice = (modalScenarioChoice - 1 + 5) % 5;
                } else if (ch == KEY_DOWN) {
                    modalScenarioChoice = (modalScenarioChoice + 1) % 5;
                } else if (ch == '\n' || ch == KEY_ENTER) {
                    loadScenarioById(modalScenarioChoice + 1);
                    activeModal = MODAL_NONE;
                }
                continue;
            }

            if (activeModal == MODAL_NEW_EMERGENCY) {
                if (ch == KEY_UP) {
                    newEmField = (newEmField - 1 + 4) % 4;
                } else if (ch == KEY_DOWN) {
                    newEmField = (newEmField + 1) % 4;
                } else if (ch == KEY_LEFT) {
                    if (newEmField == 0) newEmLocIdx = (newEmLocIdx - 1 + numLocations) % numLocations;
                    else if (newEmField == 1) newEmHazardIdx = (newEmHazardIdx - 1 + numHazardTypes) % numHazardTypes;
                    else if (newEmField == 2) { if (newEmAffected > 10) newEmAffected -= 10; }
                    else if (newEmField == 3) { if (newEmMedical > 5) newEmMedical -= 5; }
                } else if (ch == KEY_RIGHT) {
                    if (newEmField == 0) newEmLocIdx = (newEmLocIdx + 1) % numLocations;
                    else if (newEmField == 1) newEmHazardIdx = (newEmHazardIdx + 1) % numHazardTypes;
                    else if (newEmField == 2) newEmAffected += 10;
                    else if (newEmField == 3) { if (newEmMedical < newEmAffected) newEmMedical += 5; }
                } else if (ch == '\n' || ch == KEY_ENTER) {
                    // Create Emergency
                    if (emergencyQueue.size >= MAX_EMERGENCIES) {
                        addLog(PAIR_WARNING, "[INCIDENT] Queue full (%d). Dispatch some alerts first.", MAX_EMERGENCIES);
                        activeModal = MODAL_NONE;
                        continue;
                    }
                    Emergency ne;
                    memset(&ne, 0, sizeof(ne));
                    ne.id = dbNextEmergencyId(800);   /* never reuses an id already in the database */
                    strncpy(ne.location, getLocationName(newEmLocIdx), sizeof(ne.location) - 1);
                    strncpy(ne.hazardType, hazardTypesList[newEmHazardIdx], sizeof(ne.hazardType) - 1);
                    ne.affectedPeople = newEmAffected;
                    ne.peopleNeedingMedical = newEmMedical;
                    ne.status = STATUS_PENDING;
                    ne.priorityScore = calculatePriority(ne);

                    dbAddEmergency(ne);
                    insertEmergency(&emergencyQueue, ne);
                    addLog(PAIR_OK, "[INCIDENT] Emergency #%d registered at %s [%s] (Pri: %.2f)",
                           ne.id, ne.location, ne.hazardType, ne.priorityScore);
                    activeModal = MODAL_NONE;
                }
                continue;
            }
        }

        // Global Navigation & Actions
        if (ch == 'q' || ch == 'Q') {
            running = 0;
            break;
        } else if (ch == '\t') {
            currentTab = (currentTab + 1) % NUM_TABS;
        } else if (ch == '1') {
            currentTab = TAB_DASHBOARD;
        } else if (ch == '2') {
            currentTab = TAB_NETWORK;
        } else if (ch == '3') {
            currentTab = TAB_RESOURCES;
        } else if (ch == '4') {
            currentTab = TAB_SCENARIOS;
        } else if (ch == '5') {
            currentTab = TAB_LOG;
        } else if (ch == 'd' || ch == 'D' || (ch == ' ' && currentTab == TAB_DASHBOARD)) {
            doDispatchHighest();
        } else if (ch == 'n' || ch == 'N') {
            activeModal = MODAL_NEW_EMERGENCY;
        } else if (ch == 'b' || ch == 'B') {
            if (currentTab == TAB_NETWORK) {
                toggleRoadBlock(selRoad);
            } else {
                currentTab = TAB_NETWORK;
            }
        } else if (ch == ' ' && currentTab == TAB_NETWORK) {
            toggleRoadBlock(selRoad);
        } else if (ch == 'p' || ch == 'P') {
            activeModal = MODAL_DIJKSTRA;
        } else if (ch == 'c' || ch == 'C') {
            activeModal = MODAL_BFS;
        } else if (ch == 's' || ch == 'S') {
            activeModal = MODAL_SCENARIO_SELECT;
        } else if (ch == 'r' || ch == 'R') {
            doResetSimulation();
        } else if (ch == '?' || ch == 'h' || ch == 'H') {
            activeModal = MODAL_HELP;
        }
        // Arrow Keys in Views
        else if (currentTab == TAB_DASHBOARD) {
            if (ch == KEY_UP) {
                if (selEmergency > 0) selEmergency--;
            } else if (ch == KEY_DOWN) {
                if (selEmergency < emergencyQueue.size - 1) selEmergency++;
            }
        } else if (currentTab == TAB_NETWORK) {
            if (ch == KEY_UP) {
                if (selRoad > 0) selRoad--;
            } else if (ch == KEY_DOWN) {
                if (selRoad < numRoadList - 1) selRoad++;
            }
        } else if (currentTab == TAB_LOG) {
            if (ch == KEY_UP) {
                if (scrollLog > 0) scrollLog--;
            } else if (ch == KEY_DOWN) {
                if (scrollLog < logCount - 1) scrollLog++;
            } else if (ch == KEY_PPAGE) {
                scrollLog -= 10;
                if (scrollLog < 0) scrollLog = 0;
            } else if (ch == KEY_NPAGE) {
                scrollLog += 10;
                if (scrollLog > logCount - 1) scrollLog = logCount - 1;
            }
        }
    }

    endwin();
    closeDB();
    printf("\nIDRS TUI Terminated Cleanly. Stay safe!\n");
    return 0;
}
