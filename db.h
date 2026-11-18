#ifndef DB_H
#define DB_H

#include "priority_queue.h"
#include "graph.h"
#include "rescue.h"
#include "hospital.h"

int initDB(const char *dbPath);
void closeDB(void);
int createTables(void);
int loadAllData(MaxHeap* emergencyQueue); // loads into memory arrays and MaxHeap

int dbAddLocation(int id, const char *name);
int dbAddRoad(int fromId, int toId, int distance);
int dbAddEmergency(Emergency e);
int dbUpdateEmergencyStatus(int id, int status);
int dbAddRescueTeam(RescueTeam t);
int dbUpdateRescueTeamAvailability(int id, int available);
int dbAddTransport(Transport t);
int dbUpdateTransportAvailability(int id, int available);
int dbAddDestination(Destination d);
int dbUpdateDestinationOccupancy(int id, int currentOccupancy);
int dbClearAll(void); // Used to clear db for loading demo
int dbResetResources(void); // Resets availability and occupancy
int dbNextEmergencyId(int minId); // max(id)+1 over all stored emergencies, at least minId

#endif // DB_H
