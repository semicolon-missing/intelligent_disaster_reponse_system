#ifndef HOSPITAL_H
#define HOSPITAL_H

#include "priority_queue.h"

#define MAX_DESTINATIONS 50

typedef struct {
    int id;
    char name[50];
    int location; // Location ID
    int capacity;
    int currentOccupancy;
    int medicalSupport;
    int accessible;
} Destination;

extern Destination destinations[MAX_DESTINATIONS];
extern int numDestinations;

void initDestinations(void);
int addDestination(int id, const char* name, int location, int capacity, int currentOccupancy, int medicalSupport, int accessible);
Destination* selectDestination(Emergency e, int emergencyLocation);
void resetDestinationsOccupancy(void);

#endif // HOSPITAL_H
