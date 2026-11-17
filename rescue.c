#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "rescue.h"
#include "graph.h"

RescueTeam teams[MAX_TEAMS];
int numTeams = 0;

Transport transports[MAX_TRANSPORTS];
int numTransports = 0;

void initRescueResources(void) {
    numTeams = 0;
    numTransports = 0;
}

int addRescueTeam(int id, const char* name, int location, int capacity, int available, int medicalKit, int boatAvailable) {
    if (numTeams >= MAX_TEAMS) return -1;
    if (id == -1) id = numTeams;
    teams[numTeams].id = id;
    strncpy(teams[numTeams].name, name, 49);
    teams[numTeams].name[49] = '\0';
    teams[numTeams].location = location;
    teams[numTeams].capacity = capacity;
    teams[numTeams].available = available;
    teams[numTeams].medicalKit = medicalKit;
    teams[numTeams].boatAvailable = boatAvailable;
    numTeams++;
    return id;
}

int addTransport(int id, const char* type, int capacity, int available, int floodSuitable) {
    if (numTransports >= MAX_TRANSPORTS) return -1;
    if (id == -1) id = numTransports;
    transports[numTransports].id = id;
    strncpy(transports[numTransports].type, type, 49);
    transports[numTransports].type[49] = '\0';
    transports[numTransports].capacity = capacity;
    transports[numTransports].available = available;
    transports[numTransports].floodSuitable = floodSuitable;
    numTransports++;
    return id;
}

RescueTeam* selectRescueTeam(Emergency e, int emergencyLocId) {
    RescueTeam* bestTeam = NULL;
    int bestScore = -999999;
    
    int needsMedical = 0;
    if (e.affectedPeople > 0) {
        float medScore = ((float)e.peopleNeedingMedical / e.affectedPeople) * 10.0f;
        if (medScore >= 4.0f) needsMedical = 1;
    }
    
    int isWaterHazard = (strcasecmp(e.hazardType, "Flood") == 0 || 
                         strcasecmp(e.hazardType, "Flash Flood") == 0 ||
                         strcasecmp(e.hazardType, "Tsunami") == 0);
    
    for (int i = 0; i < numTeams; i++) {
        if (!teams[i].available) continue;
        if (needsMedical && !teams[i].medicalKit) continue;
        /* Hard constraint: water hazards (Flood, Flash Flood, Tsunami) need a rescue boat. */
        if (isWaterHazard && !teams[i].boatAvailable) continue;
        
        int dist = 0;
        if (emergencyLocId >= 0) {
            if (!isReachable(teams[i].location, emergencyLocId)) continue;
            dist = getShortestDistance(teams[i].location, emergencyLocId);
            if (dist == INF) continue;
        }
        
        int score = 500;
        if (teams[i].capacity >= e.affectedPeople) score += 100;
        else score += (teams[i].capacity * 50) / (e.affectedPeople > 0 ? e.affectedPeople : 1);
        
        if (teams[i].medicalKit) score += 30;
        if (isWaterHazard && teams[i].boatAvailable) score += 50;
        
        // Closer teams are strongly prioritized
        score -= (dist * 2);
        
        if (score > bestScore) {
            bestScore = score;
            bestTeam = &teams[i];
        }
    }
    return bestTeam;
}

Transport* selectTransport(Emergency e) {
    Transport* bestTransport = NULL;
    int bestScore = -1;
    int isWaterHazard = (strcasecmp(e.hazardType, "Flood") == 0 || 
                         strcasecmp(e.hazardType, "Flash Flood") == 0 ||
                         strcasecmp(e.hazardType, "Tsunami") == 0);
    
    for (int i = 0; i < numTransports; i++) {
        if (!transports[i].available) continue;
        
        int score = 20;
        if (transports[i].capacity >= e.affectedPeople) score += 50;
        if (isWaterHazard && transports[i].floodSuitable) score += 40;
        else if (transports[i].floodSuitable) score += 10;
        
        if (score > bestScore) {
            bestScore = score;
            bestTransport = &transports[i];
        }
    }
    return bestTransport;
}

void resetRescueAvailabilities(void) {
    for (int i = 0; i < numTeams; i++) {
        teams[i].available = 1;
    }
    for (int i = 0; i < numTransports; i++) {
        transports[i].available = 1;
    }
}
