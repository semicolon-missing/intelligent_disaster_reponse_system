#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "httpserver.h"
#include "priority_queue.h"
#include "graph.h"
#include "rescue.h"
#include "hospital.h"
#include "db.h"
#include "scenario_import.h"

MaxHeap emergencyQueue;
char lastRescuePlan[4096] = "{\"message\": \"No plan generated yet\"}";

typedef struct {
    int emergencyId;
    char location[50];
    char hazard[50];
    int affectedPeople;
    int peopleNeedingMedical;
    char team[50];
    char transport[50];
    char destination[50];
    int distToEmergency;
    int distToDestination;
    char timeStr[32];
} MissionLog;

#define MAX_MISSION_LOGS 100
static MissionLog missionHistory[MAX_MISSION_LOGS];
static int numMissionLogs = 0;

/* Mission history and the last plan describe the current simulation run;
   forget them when a new scenario is loaded or the simulation is reset. */
static void clearMissionHistory(void) {
    numMissionLogs = 0;
    snprintf(lastRescuePlan, sizeof(lastRescuePlan), "%s", "{\"message\": \"No plan generated yet\"}");
}

static void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((unsigned char)a) && isxdigit((unsigned char)b))) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = (char)(16*a+b);
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int get_param(const char *data, const char *key, char *val, int maxLen) {
    if (!data || !key || !val || maxLen <= 0) return 0;
    val[0] = '\0';
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", key);
    const char *pos = strstr(data, pattern);
    while (pos) {
        if (pos == data || *(pos - 1) == '&' || *(pos - 1) == '?' || *(pos - 1) == ' ') break;
        pos = strstr(pos + 1, pattern);
    }
    if (!pos) return 0;
    pos += strlen(pattern);
    char *raw = (char*)malloc(maxLen);
    if (!raw) return 0;
    int i = 0;
    while (*pos && *pos != '&' && *pos != ' ' && *pos != '\r' && *pos != '\n' && i < maxLen - 1) {
        raw[i++] = *pos++;
    }
    raw[i] = '\0';
    url_decode(val, raw);
    val[maxLen - 1] = '\0';
    free(raw);

    /* Values are later echoed inside JSON strings, so neutralise the characters
       that would break them. Scenario "content" keeps its line breaks. */
    int keepNewlines = (strcmp(key, "content") == 0);
    for (char *c = val; *c; c++) {
        if (*c == '"') *c = '\'';
        else if (*c == '\\') *c = '/';
        else if ((unsigned char)*c < 0x20 &&
                 !(keepNewlines && (*c == '\n' || *c == '\r' || *c == '\t'))) *c = ' ';
    }
    return 1;
}

static int get_param_or(const char *data, const char *key1, const char *key2, char *val, int maxLen) {
    if (get_param(data, key1, val, maxLen) && val[0] != '\0') return 1;
    if (key2 && get_param(data, key2, val, maxLen) && val[0] != '\0') return 1;
    return 0;
}

static int parseLocationParam(const char *str) {
    if (!str || str[0] == '\0') return -1;
    int isNumeric = 1;
    for (int i = 0; str[i]; i++) {
        if (!isdigit((unsigned char)str[i])) {
            isNumeric = 0;
            break;
        }
    }
    if (isNumeric) {
        int id = atoi(str);
        if (id >= 0 && id < numLocations) return id;
    }
    return getLocationId(str);
}

void apiHandler(const char *method, const char *raw_path, const char *body, char *res, int resSize) {
    char path[256] = {0};
    char query[1024] = {0};
    const char *qmark = strchr(raw_path, '?');
    if (qmark) {
        int pathLen = (int)(qmark - raw_path);
        if (pathLen >= (int)sizeof(path)) pathLen = sizeof(path) - 1;
        strncpy(path, raw_path, pathLen);
        path[pathLen] = '\0';
        strncpy(query, qmark + 1, sizeof(query) - 1);
    } else {
        strncpy(path, raw_path, sizeof(path) - 1);
    }

    // Combine query and body for parameter searching
    static char combinedParams[131072];   /* holds the URL-encoded body; big enough for a max-size scenario import */
    memset(combinedParams, 0, sizeof(combinedParams));
    if (query[0]) {
        snprintf(combinedParams, sizeof(combinedParams), "%s&%s", query, body ? body : "");
    } else {
        snprintf(combinedParams, sizeof(combinedParams), "%s", body ? body : "");
    }

    // 1. GET /api/metrics or /api/status
    if ((strcmp(path, "/api/metrics") == 0 || strcmp(path, "/api/status") == 0) && strcmp(method, "GET") == 0) {
        int totalEmergencies = emergencyQueue.size;
        int totalAffected = 0;
        int totalMedical = 0;
        float maxScore = 0.0f;
        for (int i = 0; i < emergencyQueue.size; i++) {
            totalAffected += emergencyQueue.data[i].affectedPeople;
            totalMedical += emergencyQueue.data[i].peopleNeedingMedical;
            if (emergencyQueue.data[i].priorityScore > maxScore) maxScore = emergencyQueue.data[i].priorityScore;
        }

        int availTeams = 0;
        for (int i = 0; i < numTeams; i++) if (teams[i].available) availTeams++;

        int availTransport = 0;
        for (int i = 0; i < numTransports; i++) if (transports[i].available) availTransport++;

        int totalDestCap = 0, currentDestOcc = 0;
        for (int i = 0; i < numDestinations; i++) {
            totalDestCap += destinations[i].capacity;
            currentDestOcc += destinations[i].currentOccupancy;
        }

        int blockedRoads = 0;
        for (int i = 0; i < numLocations; i++) {
            for (int j = i + 1; j < numLocations; j++) {
                if (isRoadBlocked(i, j) == 1) blockedRoads++;
            }
        }

        snprintf(res, resSize,
            "{\"pendingEmergencies\":%d,\"resolvedMissions\":%d,\"totalAffected\":%d,\"totalMedical\":%d,\"maxPriorityScore\":%.2f,"
            "\"availableTeams\":%d,\"totalTeams\":%d,\"availableTransports\":%d,\"totalTransports\":%d,"
            "\"destinationsCapacity\":%d,\"destinationsOccupancy\":%d,\"blockedRoads\":%d,\"totalLocations\":%d}",
            totalEmergencies, numMissionLogs, totalAffected, totalMedical, maxScore,
            availTeams, numTeams, availTransport, numTransports,
            totalDestCap, currentDestOcc, blockedRoads, numLocations);
    }
    // 2. GET /api/emergencies or /api/queue
    else if ((strcmp(path, "/api/emergencies") == 0 || strcmp(path, "/api/queue") == 0) && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"emergencies\":[");
        int first = 1;
        for (int i = 0; i < emergencyQueue.size; i++) {
            if (!first) strncat(res, ",", resSize - strlen(res) - 1);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "{\"id\":%d,\"location\":\"%s\",\"hazardType\":\"%s\",\"affectedPeople\":%d,\"peopleNeedingMedical\":%d,\"priorityScore\":%.2f,\"status\":%d}",
                     emergencyQueue.data[i].id, emergencyQueue.data[i].location, emergencyQueue.data[i].hazardType,
                     emergencyQueue.data[i].affectedPeople, emergencyQueue.data[i].peopleNeedingMedical,
                     emergencyQueue.data[i].priorityScore, emergencyQueue.data[i].status);
            strncat(res, buf, resSize - strlen(res) - 1);
            first = 0;
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 3. POST /api/emergencies
    else if (strcmp(path, "/api/emergencies") == 0 && strcmp(method, "POST") == 0) {
        Emergency e;
        memset(&e, 0, sizeof(e));
        char locName[50] = {0}, hazardType[50] = {0}, affStr[32] = {0}, medStr[32] = {0};
        get_param(combinedParams, "location", locName, sizeof(locName));
        get_param(combinedParams, "hazardType", hazardType, sizeof(hazardType));
        get_param(combinedParams, "affectedPeople", affStr, sizeof(affStr));
        get_param(combinedParams, "peopleNeedingMedical", medStr, sizeof(medStr));

        if (locName[0] == '\0') {
            snprintf(res, resSize, "{\"error\":\"Location is required\"}");
            return;
        }
        int locId = getLocationId(locName);
        if (locId == -1) {
            snprintf(res, resSize, "{\"error\":\"Unknown location '%s': pick one of the map locations\"}", locName);
            return;
        }
        if (emergencyQueue.size >= MAX_EMERGENCIES) {
            snprintf(res, resSize, "{\"error\":\"Priority queue is full (%d emergencies); dispatch some first\"}", MAX_EMERGENCIES);
            return;
        }
        if (hazardType[0] == '\0') strcpy(hazardType, "Flood");

        e.affectedPeople = atoi(affStr);
        if (e.affectedPeople <= 0) e.affectedPeople = 50;
        e.peopleNeedingMedical = atoi(medStr);
        if (e.peopleNeedingMedical < 0) e.peopleNeedingMedical = 0;
        if (e.peopleNeedingMedical > e.affectedPeople) e.peopleNeedingMedical = e.affectedPeople;

        e.id = dbNextEmergencyId(600);   /* never reuses an id already in the database */
        snprintf(e.location, sizeof(e.location), "%s", getLocationName(locId));  /* canonical spelling */
        snprintf(e.hazardType, sizeof(e.hazardType), "%s", hazardType);
        e.priorityScore = calculatePriority(e);
        e.status = STATUS_PENDING;

        dbAddEmergency(e);
        insertEmergency(&emergencyQueue, e);
        snprintf(res, resSize, "{\"status\":\"success\",\"id\":%d,\"priorityScore\":%.2f}", e.id, e.priorityScore);
    }
    // 4. POST /api/process
    else if (strcmp(path, "/api/process") == 0 && strcmp(method, "POST") == 0) {
        if (isHeapEmpty(&emergencyQueue)) {
            snprintf(res, resSize, "{\"error\":\"No active emergencies in queue\"}");
            return;
        }

        Emergency e = extractHighestPriority(&emergencyQueue);
        int e_locId = getLocationId(e.location);
        if (e_locId == -1) {
            snprintf(res, resSize, "{\"error\":\"Location '%s' does not exist in graph network\"}", e.location);
            return;
        }

        RescueTeam* t = selectRescueTeam(e, e_locId);
        Transport* tr = selectTransport(e);
        Destination* d = selectDestination(e, e_locId);

        if (!t) {
            insertEmergency(&emergencyQueue, e);
            snprintf(res, resSize, "{\"error\":\"No available rescue team for %s: none is reachable with the required gear (boat for floods/tsunamis, medical kit when 40%%+ need care)\"}", e.location);
            return;
        }

        if (!d) {
            insertEmergency(&emergencyQueue, e);
            snprintf(res, resSize, "{\"error\":\"No reachable hospital/shelter with enough free beds for all %d people at %s\"}", e.affectedPeople, e.location);
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

        int distToEm = getShortestDistance(t->location, e_locId);
        int distToDest = getShortestDistance(e_locId, d->location);

        int pathEm[MAX_LOCATIONS];
        int pathEmLen = getShortestPath(t->location, e_locId, pathEm, MAX_LOCATIONS);

        int pathDest[MAX_LOCATIONS];
        int pathDestLen = getShortestPath(e_locId, d->location, pathDest, MAX_LOCATIONS);

        time_t rawtime;
        time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", timeinfo);

        if (numMissionLogs < MAX_MISSION_LOGS) {
            missionHistory[numMissionLogs].emergencyId = e.id;
            snprintf(missionHistory[numMissionLogs].location, sizeof(missionHistory[numMissionLogs].location), "%s", e.location);
            snprintf(missionHistory[numMissionLogs].hazard, sizeof(missionHistory[numMissionLogs].hazard), "%s", e.hazardType);
            missionHistory[numMissionLogs].affectedPeople = e.affectedPeople;
            missionHistory[numMissionLogs].peopleNeedingMedical = e.peopleNeedingMedical;
            snprintf(missionHistory[numMissionLogs].team, sizeof(missionHistory[numMissionLogs].team), "%s", t->name);
            snprintf(missionHistory[numMissionLogs].transport, sizeof(missionHistory[numMissionLogs].transport), "%s", tr ? tr->type : "Standard Vehicle");
            snprintf(missionHistory[numMissionLogs].destination, sizeof(missionHistory[numMissionLogs].destination), "%s", d->name);
            missionHistory[numMissionLogs].distToEmergency = distToEm;
            missionHistory[numMissionLogs].distToDestination = distToDest;
            snprintf(missionHistory[numMissionLogs].timeStr, sizeof(missionHistory[numMissionLogs].timeStr), "%s", timeStr);
            numMissionLogs++;
        }

        // Build JSON response with path arrays
        char emPathJson[1024] = "[";
        for (int i = 0; i < pathEmLen; i++) {
            char pbuf[64];
            snprintf(pbuf, sizeof(pbuf), "%s\"%s\"", i > 0 ? "," : "", getLocationName(pathEm[i]));
            strncat(emPathJson, pbuf, sizeof(emPathJson) - strlen(emPathJson) - 1);
        }
        strncat(emPathJson, "]", sizeof(emPathJson) - strlen(emPathJson) - 1);

        char destPathJson[1024] = "[";
        for (int i = 0; i < pathDestLen; i++) {
            char pbuf[64];
            snprintf(pbuf, sizeof(pbuf), "%s\"%s\"", i > 0 ? "," : "", getLocationName(pathDest[i]));
            strncat(destPathJson, pbuf, sizeof(destPathJson) - strlen(destPathJson) - 1);
        }
        strncat(destPathJson, "]", sizeof(destPathJson) - strlen(destPathJson) - 1);

        snprintf(lastRescuePlan, sizeof(lastRescuePlan),
            "{\"emergencyId\":%d,\"location\":\"%s\",\"hazard\":\"%s\",\"affected\":%d,\"medical\":%d,"
            "\"team\":\"%s\",\"teamBase\":\"%s\",\"transport\":\"%s\",\"destination\":\"%s\","
            "\"destOccupancy\":%d,\"destCapacity\":%d,\"routeToEmergency\":%d,\"routeToDestination\":%d,"
            "\"pathEmergency\":%s,\"pathDestination\":%s,\"timestamp\":\"%s\"}",
            e.id, e.location, e.hazardType, e.affectedPeople, e.peopleNeedingMedical,
            t->name, getLocationName(t->location), tr ? tr->type : "Ground Unit", d->name,
            d->currentOccupancy, d->capacity, distToEm, distToDest,
            emPathJson, destPathJson, timeStr);

        snprintf(res, resSize, "%s", lastRescuePlan);
    }
    // 5. GET /api/plan
    else if (strcmp(path, "/api/plan") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "%s", lastRescuePlan);
    }
    // 6. GET /api/history
    else if (strcmp(path, "/api/history") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"history\":[");
        int first = 1;
        for (int i = numMissionLogs - 1; i >= 0; i--) {
            if (!first) strncat(res, ",", resSize - strlen(res) - 1);
            char buf[512];
            snprintf(buf, sizeof(buf),
                "{\"emergencyId\":%d,\"location\":\"%s\",\"hazard\":\"%s\",\"affected\":%d,\"medical\":%d,"
                "\"team\":\"%s\",\"transport\":\"%s\",\"destination\":\"%s\",\"distToEmergency\":%d,\"distToDestination\":%d,\"timestamp\":\"%s\"}",
                missionHistory[i].emergencyId, missionHistory[i].location, missionHistory[i].hazard,
                missionHistory[i].affectedPeople, missionHistory[i].peopleNeedingMedical,
                missionHistory[i].team, missionHistory[i].transport, missionHistory[i].destination,
                missionHistory[i].distToEmergency, missionHistory[i].distToDestination, missionHistory[i].timeStr);
            strncat(res, buf, resSize - strlen(res) - 1);
            first = 0;
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 7. GET /api/teams
    else if (strcmp(path, "/api/teams") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"teams\":[");
        for (int i = 0; i < numTeams; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s{\"id\":%d,\"name\":\"%s\",\"available\":%d,\"capacity\":%d,\"medicalKit\":%d,\"boatAvailable\":%d,\"locationId\":%d,\"locationName\":\"%s\"}",
                i > 0 ? "," : "", teams[i].id, teams[i].name, teams[i].available, teams[i].capacity,
                teams[i].medicalKit, teams[i].boatAvailable, teams[i].location, getLocationName(teams[i].location));
            strncat(res, buf, resSize - strlen(res) - 1);
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 8. POST /api/teams
    else if (strcmp(path, "/api/teams") == 0 && strcmp(method, "POST") == 0) {
        char name[50] = {0}, locStr[32] = {0}, capStr[32] = {0}, medStr[32] = {0}, boatStr[32] = {0};
        get_param(combinedParams, "name", name, sizeof(name));
        get_param(combinedParams, "location", locStr, sizeof(locStr));
        get_param(combinedParams, "capacity", capStr, sizeof(capStr));
        get_param(combinedParams, "medicalKit", medStr, sizeof(medStr));
        get_param(combinedParams, "boatAvailable", boatStr, sizeof(boatStr));

        int locId = parseLocationParam(locStr);
        if (locId < 0) {
            snprintf(res, resSize, "{\"error\":\"Unknown base location for team\"}");
            return;
        }

        int id = addRescueTeam(-1, name[0] ? name : "Quick Response Team", locId, atoi(capStr) > 0 ? atoi(capStr) : 100, 1, atoi(medStr), atoi(boatStr));
        if (id < 0) {
            snprintf(res, resSize, "{\"error\":\"Maximum number of rescue teams reached\"}");
            return;
        }
        RescueTeam t = {id, "", locId, atoi(capStr) > 0 ? atoi(capStr) : 100, 1, atoi(medStr), atoi(boatStr)};
        snprintf(t.name, sizeof(t.name), "%s", name[0] ? name : "Quick Response Team");
        dbAddRescueTeam(t);
        snprintf(res, resSize, "{\"status\":\"success\",\"id\":%d}", id);
    }
    // 9. GET /api/transport
    else if (strcmp(path, "/api/transport") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"transports\":[");
        for (int i = 0; i < numTransports; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s{\"id\":%d,\"type\":\"%s\",\"capacity\":%d,\"available\":%d,\"floodSuitable\":%d}",
                i > 0 ? "," : "", transports[i].id, transports[i].type, transports[i].capacity, transports[i].available, transports[i].floodSuitable);
            strncat(res, buf, resSize - strlen(res) - 1);
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 10. POST /api/transport
    else if (strcmp(path, "/api/transport") == 0 && strcmp(method, "POST") == 0) {
        char type[50] = {0}, capStr[32] = {0}, floodStr[32] = {0};
        get_param(combinedParams, "type", type, sizeof(type));
        get_param(combinedParams, "capacity", capStr, sizeof(capStr));
        get_param(combinedParams, "floodSuitable", floodStr, sizeof(floodStr));

        int id = addTransport(-1, type[0] ? type : "Emergency Van", atoi(capStr) > 0 ? atoi(capStr) : 50, 1, atoi(floodStr));
        if (id < 0) {
            snprintf(res, resSize, "{\"error\":\"Maximum number of transports reached\"}");
            return;
        }
        Transport tr = {id, "", atoi(capStr) > 0 ? atoi(capStr) : 50, 1, atoi(floodStr)};
        snprintf(tr.type, sizeof(tr.type), "%s", type[0] ? type : "Emergency Van");
        dbAddTransport(tr);
        snprintf(res, resSize, "{\"status\":\"success\",\"id\":%d}", id);
    }
    // 11. GET /api/destinations
    else if (strcmp(path, "/api/destinations") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"destinations\":[");
        for (int i = 0; i < numDestinations; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                "%s{\"id\":%d,\"name\":\"%s\",\"capacity\":%d,\"currentOccupancy\":%d,\"medicalSupport\":%d,\"accessible\":%d,\"locationId\":%d,\"locationName\":\"%s\"}",
                i > 0 ? "," : "", destinations[i].id, destinations[i].name, destinations[i].capacity,
                destinations[i].currentOccupancy, destinations[i].medicalSupport, destinations[i].accessible,
                destinations[i].location, getLocationName(destinations[i].location));
            strncat(res, buf, resSize - strlen(res) - 1);
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 12. POST /api/destinations
    else if (strcmp(path, "/api/destinations") == 0 && strcmp(method, "POST") == 0) {
        char name[50] = {0}, locStr[32] = {0}, capStr[32] = {0}, medStr[32] = {0};
        get_param(combinedParams, "name", name, sizeof(name));
        get_param(combinedParams, "location", locStr, sizeof(locStr));
        get_param(combinedParams, "capacity", capStr, sizeof(capStr));
        get_param(combinedParams, "medicalSupport", medStr, sizeof(medStr));

        int locId = parseLocationParam(locStr);
        if (locId < 0) {
            snprintf(res, resSize, "{\"error\":\"Unknown location for destination\"}");
            return;
        }

        int id = addDestination(-1, name[0] ? name : "Field Hospital", locId, atoi(capStr) > 0 ? atoi(capStr) : 400, 0, atoi(medStr), 1);
        if (id < 0) {
            snprintf(res, resSize, "{\"error\":\"Maximum number of destinations reached\"}");
            return;
        }
        Destination d = {id, "", locId, atoi(capStr) > 0 ? atoi(capStr) : 400, 0, atoi(medStr), 1};
        snprintf(d.name, sizeof(d.name), "%s", name[0] ? name : "Field Hospital");
        dbAddDestination(d);
        snprintf(res, resSize, "{\"status\":\"success\",\"id\":%d}", id);
    }
    // 13. GET /api/graph
    else if (strcmp(path, "/api/graph") == 0 && strcmp(method, "GET") == 0) {
        snprintf(res, resSize, "{\"locations\":[");
        for (int i = 0; i < numLocations; i++) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s{\"id\":%d,\"name\":\"%s\"}", i > 0 ? "," : "", locations[i].id, locations[i].name);
            strncat(res, buf, resSize - strlen(res) - 1);
        }
        strncat(res, "],\"edges\":[", resSize - strlen(res) - 1);
        int firstEdge = 1;
        for (int i = 0; i < numLocations; i++) {
            for (int j = i + 1; j < numLocations; j++) {
                if (originalGraph[i][j] != INF) {
                    int blocked = isRoadBlocked(i, j);
                    int currentDist = (blocked == 1) ? originalGraph[i][j] : graph[i][j];
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                        "%s{\"from\":%d,\"to\":%d,\"fromName\":\"%s\",\"toName\":\"%s\",\"distance\":%d,\"isBlocked\":%d}",
                        firstEdge ? "" : ",", i, j, locations[i].name, locations[j].name, currentDist, blocked == 1 ? 1 : 0);
                    strncat(res, buf, resSize - strlen(res) - 1);
                    firstEdge = 0;
                }
            }
        }
        strncat(res, "]}", resSize - strlen(res) - 1);
    }
    // 14. POST /api/locations
    else if (strcmp(path, "/api/locations") == 0 && strcmp(method, "POST") == 0) {
        char name[50] = {0};
        get_param(combinedParams, "name", name, sizeof(name));
        if (name[0] == '\0') {
            snprintf(res, resSize, "{\"error\":\"Location name is required\"}");
            return;
        }
        if (getLocationId(name) != -1) {
            snprintf(res, resSize, "{\"error\":\"Location '%s' already exists\"}", name);
            return;
        }
        int id = addLocation(-1, name);
        if (id >= 0) {
            dbAddLocation(id, name);
            snprintf(res, resSize, "{\"status\":\"success\",\"id\":%d,\"name\":\"%s\"}", id, name);
        } else {
            snprintf(res, resSize, "{\"error\":\"Failed to add location (max capacity reached)\"}");
        }
    }
    // 15. POST /api/roads
    else if (strcmp(path, "/api/roads") == 0 && strcmp(method, "POST") == 0) {
        char uStr[32] = {0}, vStr[32] = {0}, dStr[32] = {0};
        get_param_or(combinedParams, "source", "from", uStr, sizeof(uStr));
        get_param_or(combinedParams, "destination", "to", vStr, sizeof(vStr));
        get_param(combinedParams, "distance", dStr, sizeof(dStr));

        int u = parseLocationParam(uStr);
        int v = parseLocationParam(vStr);
        int d = atoi(dStr);
        if (u >= 0 && v >= 0 && u < numLocations && v < numLocations && d > 0 && addRoad(u, v, d) == 0) {
            dbAddRoad(u, v, d);
            snprintf(res, resSize, "{\"status\":\"success\",\"from\":%d,\"to\":%d,\"distance\":%d}", u, v, d);
        } else {
            snprintf(res, resSize, "{\"error\":\"Invalid road coordinates or distance\"}");
        }
    }
    // 16. POST /api/block
    else if (strcmp(path, "/api/block") == 0 && strcmp(method, "POST") == 0) {
        char uStr[32] = {0}, vStr[32] = {0};
        get_param_or(combinedParams, "source", "from", uStr, sizeof(uStr));
        get_param_or(combinedParams, "destination", "to", vStr, sizeof(vStr));
        int u = parseLocationParam(uStr);
        int v = parseLocationParam(vStr);
        if (u >= 0 && v >= 0 && u < numLocations && v < numLocations) {
            blockRoad(u, v);
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Road blocked between %s and %s\"}",
                     getLocationName(u), getLocationName(v));
        } else {
            snprintf(res, resSize, "{\"error\":\"Invalid location IDs for blocking\"}");
        }
    }
    // 17. POST /api/unblock
    else if (strcmp(path, "/api/unblock") == 0 && strcmp(method, "POST") == 0) {
        char uStr[32] = {0}, vStr[32] = {0};
        get_param_or(combinedParams, "source", "from", uStr, sizeof(uStr));
        get_param_or(combinedParams, "destination", "to", vStr, sizeof(vStr));
        int u = parseLocationParam(uStr);
        int v = parseLocationParam(vStr);
        if (u >= 0 && v >= 0 && u < numLocations && v < numLocations) {
            unblockRoad(u, v);
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Road unblocked between %s and %s\"}",
                     getLocationName(u), getLocationName(v));
        } else {
            snprintf(res, resSize, "{\"error\":\"Invalid location IDs for unblocking\"}");
        }
    }
    // 18. GET /api/route?from=X&to=Y
    else if (strcmp(path, "/api/route") == 0 && strcmp(method, "GET") == 0) {
        char fromStr[32] = {0}, toStr[32] = {0};
        get_param_or(combinedParams, "from", "source", fromStr, sizeof(fromStr));
        get_param_or(combinedParams, "to", "destination", toStr, sizeof(toStr));
        int from = parseLocationParam(fromStr);
        int to = parseLocationParam(toStr);

        if (from >= 0 && to >= 0 && from < numLocations && to < numLocations) {
            int reachable = isReachable(from, to);
            int dist = getShortestDistance(from, to);
            int pathNodes[MAX_LOCATIONS];
            int pathLen = getShortestPath(from, to, pathNodes, MAX_LOCATIONS);

            char pathJson[1024] = "[";
            for (int i = 0; i < pathLen; i++) {
                char pbuf[64];
                snprintf(pbuf, sizeof(pbuf), "%s\"%s\"", i > 0 ? "," : "", getLocationName(pathNodes[i]));
                strncat(pathJson, pbuf, sizeof(pathJson) - strlen(pathJson) - 1);
            }
            strncat(pathJson, "]", sizeof(pathJson) - strlen(pathJson) - 1);

            snprintf(res, resSize,
                "{\"from\":\"%s\",\"to\":\"%s\",\"reachable\":%d,\"distance\":%d,\"path\":%s}",
                getLocationName(from), getLocationName(to), reachable, (dist == INF ? -1 : dist), pathJson);
        } else {
            snprintf(res, resSize, "{\"error\":\"Invalid source or destination locations\"}");
        }
    }
    // 19. POST /api/scenario or /api/demo
    else if ((strcmp(path, "/api/scenario") == 0 || strcmp(path, "/api/demo") == 0) && strcmp(method, "POST") == 0) {
        char name[50] = {0};
        static char rawText[65536];
        memset(rawText, 0, sizeof(rawText));
        get_param(combinedParams, "scenario", name, sizeof(name));
        get_param(combinedParams, "content", rawText, sizeof(rawText));
        clearMissionHistory();

        if (rawText[0] != '\0') {
            importScenarioFromString(rawText);
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Custom scenario imported successfully\"}");
        } else if (strcasecmp(name, "multihazard") == 0 || strcasecmp(name, "multi") == 0 || strcasecmp(name, "sample") == 0 || strcasecmp(name, "all") == 0) {
            if (importScenario("sample_natural_disasters.txt") == 0) {
                snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Comprehensive Multi-Hazard Scenario loaded from scenarios/sample_natural_disasters.txt\"}");
            } else {
                loadAssamDemo();
                snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Loaded default disaster scenario\"}");
            }
        } else if (strcasecmp(name, "kerala") == 0) {
            loadKeralaDemo();
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Kerala flood and landslide scenario loaded\"}");
        } else if (strcasecmp(name, "uttarakhand") == 0) {
            loadUttarakhandDemo();
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Uttarakhand flash flood scenario loaded\"}");
        } else if (strcasecmp(name, "odisha") == 0) {
            loadOdishaDemo();
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Odisha super cyclone scenario loaded\"}");
        } else {
            loadAssamDemo();
            snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"Assam flood scenario loaded\"}");
        }
    }
    // 20. POST /api/reset
    else if (strcmp(path, "/api/reset") == 0 && strcmp(method, "POST") == 0) {
        resetRescueAvailabilities();
        resetDestinationsOccupancy();
        dbResetResources();
        loadAllData(&emergencyQueue);
        clearMissionHistory();
        snprintf(res, resSize, "{\"status\":\"success\",\"message\":\"All resources reset to standby and hospital occupancies cleared\"}");
    }
    else {
        snprintf(res, resSize, "{\"error\":\"Endpoint not found: %s\"}", path);
    }
}

int main(int argc, char **argv) {
    const char *port_env = getenv("PORT");
    int port = (argc > 1) ? atoi(argv[1])
             : (port_env && port_env[0] != '\0') ? atoi(port_env)
             : 8080;  
    if (port <= 0 || port > 65535) port = 8080;
    if (initDB("disaster_v2.db") != 0) {
        printf("Failed to initialize database.\n");
        return 1;
    }
    
    initHeap(&emergencyQueue);
    loadAllData(&emergencyQueue);
    if (numLocations == 0) {
        printf("Database empty. Loading Comprehensive Multi-Disaster Scenario (scenarios/sample_natural_disasters.txt)...\n");
        if (importScenario("sample_natural_disasters.txt") != 0) {
            loadAssamDemo();
        }
    } else {
        printf("Loaded existing scenario from database with %d locations.\n", numLocations);
    }
    
    startHttpServer(port, apiHandler);
    
    closeDB();
    return 0;
}
