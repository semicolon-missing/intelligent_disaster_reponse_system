#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#define STATUS_PENDING     0
#define STATUS_ASSIGNED    1
#define STATUS_IN_PROGRESS 2
#define STATUS_RESCUED     3

#define MAX_EMERGENCIES 100

typedef struct {
    int id;
    char location[50];
    char hazardType[50]; // e.g. "Flood", "Earthquake", "Fire"
    int affectedPeople;
    int peopleNeedingMedical;
    float priorityScore;
    int status;
} Emergency;

typedef struct 
{
    Emergency data[MAX_EMERGENCIES];
    int size;
} MaxHeap;

void initHeap(MaxHeap *heap);
float calculatePriority(Emergency e);
void insertEmergency(MaxHeap *heap, Emergency e);
Emergency extractHighestPriority(MaxHeap *heap);
void heapifyUp(MaxHeap *heap, int index);
void heapifyDown(MaxHeap *heap, int index);
int isHeapEmpty(MaxHeap *heap);
void printHeap(MaxHeap *heap);

#endif // PRIORITY_QUEUE_H
