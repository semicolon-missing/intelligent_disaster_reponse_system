#include <stdio.h>
#include <string.h>
#include "hospital.h"
#include "graph.h"

Destination destinations[MAX_DESTINATIONS];
int numDestinations = 0;

void initDestinations(void) {
    numDestinations = 0;
}

int addDestination(int id, const char* name, int location, int capacity, int currentOccupancy, int medicalSupport, int accessible) {
    if (numDestinations >= MAX_DESTINATIONS) return -1;
    if (id == -1) id = numDestinations;
    destinations[numDestinations].id = id;
    strncpy(destinations[numDestinations].name, name, 49);
    destinations[numDestinations].name[49] = '\0';
    destinations[numDestinations].location = location;
    destinations[numDestinations].capacity = capacity;
    destinations[numDestinations].currentOccupancy = currentOccupancy;
    destinations[numDestinations].medicalSupport = medicalSupport;
    destinations[numDestinations].accessible = accessible;
    numDestinations++;
    return id;
}

Destination* selectDestination(Emergency e, int emergencyLocation) {
    Destination* bestDest = NULL;
    int shortestDist = INF;
    Destination* fallbackDest = NULL;
    int maxRemaining = -1;
    
    int needsMedical = 0;
    if (e.affectedPeople > 0) {
        float medScore = ((float)e.peopleNeedingMedical / e.affectedPeople) * 10.0f;
        if (medScore >= 4.0f) needsMedical = 1;
    }
    
    for (int i = 0; i < numDestinations; i++) {
        if (!destinations[i].accessible) continue;
        if (needsMedical && !destinations[i].medicalSupport) continue;
        
        int remainingCapacity = destinations[i].capacity - destinations[i].currentOccupancy;
        if (remainingCapacity <= 0) continue;
        
        int dist = 0;
        if (emergencyLocation >= 0) {
            if (!isReachable(emergencyLocation, destinations[i].location)) continue;
            dist = getShortestDistance(emergencyLocation, destinations[i].location);
            if (dist == INF) continue;
        }
        
        if (remainingCapacity > maxRemaining) {
            maxRemaining = remainingCapacity;
            fallbackDest = &destinations[i];
        }
        
        if (remainingCapacity >= e.affectedPeople) {
            if (dist < shortestDist) {
                shortestDist = dist;
                bestDest = &destinations[i];
            }
        }
    }
    if (bestDest) return bestDest;
    return fallbackDest;
}

void resetDestinationsOccupancy(void) {
    for (int i = 0; i < numDestinations; i++) {
        destinations[i].currentOccupancy = 0;
    }
}
