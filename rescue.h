#ifndef RESCUE_H
#define RESCUE_H

#include "priority_queue.h"

#define MAX_TEAMS 50
#define MAX_TRANSPORTS 50

typedef struct {
    int id;
    char name[50];
    int location; // Location ID
    int capacity;
    int available;
    int medicalKit;
    int boatAvailable;
} RescueTeam;

typedef struct {
    int id;
    char type[50];
    int capacity;
    int available;
    int floodSuitable;
} Transport;

extern RescueTeam teams[MAX_TEAMS];
extern int numTeams;

extern Transport transports[MAX_TRANSPORTS];
extern int numTransports;

void initRescueResources(void);
int addRescueTeam(int id, const char* name, int location, int capacity, int available, int medicalKit, int boatAvailable);
int addTransport(int id, const char* type, int capacity, int available, int floodSuitable);
RescueTeam* selectRescueTeam(Emergency e, int emergencyLocId);
Transport* selectTransport(Emergency e);
void resetRescueAvailabilities(void);

#endif // RESCUE_H
