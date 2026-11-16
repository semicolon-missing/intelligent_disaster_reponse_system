#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "priority_queue.h"

void initHeap(MaxHeap *heap){
    heap->size = 0;
}

int isHeapEmpty(MaxHeap *heap){
    return heap->size == 0;
}

// Priority Score = 40% Hazard Severity + 30% Population Factor + 30% Medical Urgency

float calculatePriority(Emergency e){
    // Map hazard string to a 0-10 severity scale
    float hazardSeverity = 5.0f; // default
    if (strcasecmp(e.hazardType, "Tsunami") == 0) hazardSeverity = 10.0f;
    else if (strcasecmp(e.hazardType, "Chemical") == 0 || strcasecmp(e.hazardType, "Hazmat") == 0 ||
             strcasecmp(e.hazardType, "Industrial") == 0) hazardSeverity = 10.0f;
    else if (strcasecmp(e.hazardType, "Earthquake") == 0) hazardSeverity = 9.5f;
    else if (strcasecmp(e.hazardType, "Flash Flood") == 0) hazardSeverity = 9.0f;
    else if (strcasecmp(e.hazardType, "Cloudburst") == 0) hazardSeverity = 9.0f;
    else if (strcasecmp(e.hazardType, "Fire") == 0) hazardSeverity = 8.5f;
    else if (strcasecmp(e.hazardType, "Flood") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Cyclone") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Landslide") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Storm Surge") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Hurricane") == 0) hazardSeverity = 7.5f;

    float medicalScore = 0.0f;
    if (e.affectedPeople > 0) {
        medicalScore = ((float)e.peopleNeedingMedical / e.affectedPeople) * 10.0f;
        if (medicalScore > 10.0f) medicalScore = 10.0f;
    }

    float populationCap = 1000.0f;
    float populationScore = ((float)e.affectedPeople / populationCap) * 10.0f;
    if (populationScore > 10.0f) populationScore = 10.0f;

    float score = (0.40f * hazardSeverity) + (0.30f * populationScore) + (0.30f * medicalScore);
    return score;
}

void swap(Emergency *a, Emergency *b){
    Emergency temp = *a;
    *a = *b;
    *b = temp;
}

void heapifyUp(MaxHeap *heap, int index){
    while (index > 0){
        int parent = (index - 1) / 2;
        if (heap->data[index].priorityScore > heap->data[parent].priorityScore){
            swap(&heap->data[index], &heap->data[parent]);
            index = parent;
        }
        else break;
    }
}

void heapifyDown(MaxHeap *heap, int index){
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < heap->size && heap->data[left].priorityScore > heap->data[largest].priorityScore)
        largest = left;
    if (right < heap->size && heap->data[right].priorityScore > heap->data[largest].priorityScore)
        largest = right;
    if (largest != index) {
        swap(&heap->data[index], &heap->data[largest]);
        heapifyDown(heap, largest);
    }
}

void insertEmergency(MaxHeap *heap, Emergency e) {
    if (heap->size >= MAX_EMERGENCIES) {
        printf("Heap full: cannot add emergency %d\n", e.id);
        return;
    }
    e.priorityScore = calculatePriority(e);
    e.status = STATUS_PENDING;
    heap->data[heap->size] = e;
    heapifyUp(heap, heap->size);
    heap->size++;
}
 
Emergency extractHighestPriority(MaxHeap *heap) {
    Emergency top;
    memset(&top, 0, sizeof(Emergency));
    if (heap->size <= 0) return top;
    top = heap->data[0];
    heap->size--;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];
        heapifyDown(heap, 0);
    }
    return top;
}
 
void printHeap(MaxHeap *heap) {
    printf("---- Current Priority Queue (%d emergencies) ----\n", heap->size);
    for (int i = 0; i < heap->size; i++) {
        printf("ID %d | %-15s | Hazard: %-10s | Score: %.2f | People: %d | Med.Need: %d\n",
               heap->data[i].id,
               heap->data[i].location,
               heap->data[i].hazardType,
               heap->data[i].priorityScore,
               heap->data[i].affectedPeople,
               heap->data[i].peopleNeedingMedical);
    }
}
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "priority_queue.h"

void initHeap(MaxHeap *heap){
    heap->size = 0;
}

int isHeapEmpty(MaxHeap *heap){
    return heap->size == 0;
}

// Priority Score = 40% Hazard Severity + 30% Population Factor + 30% Medical Urgency

float calculatePriority(Emergency e){
    // Map hazard string to a 0-10 severity scale
    float hazardSeverity = 5.0f; // default
    if (strcasecmp(e.hazardType, "Tsunami") == 0) hazardSeverity = 10.0f;
    else if (strcasecmp(e.hazardType, "Chemical") == 0 || strcasecmp(e.hazardType, "Hazmat") == 0 ||
             strcasecmp(e.hazardType, "Industrial") == 0) hazardSeverity = 10.0f;
    else if (strcasecmp(e.hazardType, "Earthquake") == 0) hazardSeverity = 9.5f;
    else if (strcasecmp(e.hazardType, "Flash Flood") == 0) hazardSeverity = 9.0f;
    else if (strcasecmp(e.hazardType, "Cloudburst") == 0) hazardSeverity = 9.0f;
    else if (strcasecmp(e.hazardType, "Fire") == 0) hazardSeverity = 8.5f;
    else if (strcasecmp(e.hazardType, "Flood") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Cyclone") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Landslide") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Storm Surge") == 0) hazardSeverity = 8.0f;
    else if (strcasecmp(e.hazardType, "Hurricane") == 0) hazardSeverity = 7.5f;

    float medicalScore = 0.0f;
    if (e.affectedPeople > 0) {
        medicalScore = ((float)e.peopleNeedingMedical / e.affectedPeople) * 10.0f;
        if (medicalScore > 10.0f) medicalScore = 10.0f;
    }

    float populationCap = 1000.0f;
    float populationScore = ((float)e.affectedPeople / populationCap) * 10.0f;
    if (populationScore > 10.0f) populationScore = 10.0f;

    float score = (0.40f * hazardSeverity) + (0.30f * populationScore) + (0.30f * medicalScore);
    return score;
}

void swap(Emergency *a, Emergency *b){
    Emergency temp = *a;
    *a = *b;
    *b = temp;
}

void heapifyUp(MaxHeap *heap, int index){
    while (index > 0){
        int parent = (index - 1) / 2;
        if (heap->data[index].priorityScore > heap->data[parent].priorityScore){
            swap(&heap->data[index], &heap->data[parent]);
            index = parent;
        }
        else break;
    }
}

void heapifyDown(MaxHeap *heap, int index){
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < heap->size && heap->data[left].priorityScore > heap->data[largest].priorityScore)
        largest = left;
    if (right < heap->size && heap->data[right].priorityScore > heap->data[largest].priorityScore)
        largest = right;
    if (largest != index) {
        swap(&heap->data[index], &heap->data[largest]);
        heapifyDown(heap, largest);
    }
}

void insertEmergency(MaxHeap *heap, Emergency e) {
    if (heap->size >= MAX_EMERGENCIES) {
        printf("Heap full: cannot add emergency %d\n", e.id);
        return;
    }
    e.priorityScore = calculatePriority(e);
    e.status = STATUS_PENDING;
    heap->data[heap->size] = e;
    heapifyUp(heap, heap->size);
    heap->size++;
}
 
Emergency extractHighestPriority(MaxHeap *heap) {
    Emergency top;
    memset(&top, 0, sizeof(Emergency));
    if (heap->size <= 0) return top;
    top = heap->data[0];
    heap->size--;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];
        heapifyDown(heap, 0);
    }
    return top;
}
 
void printHeap(MaxHeap *heap) {
    printf("---- Current Priority Queue (%d emergencies) ----\n", heap->size);
    for (int i = 0; i < heap->size; i++) {
        printf("ID %d | %-15s | Hazard: %-10s | Score: %.2f | People: %d | Med.Need: %d\n",
               heap->data[i].id,
               heap->data[i].location,
               heap->data[i].hazardType,
               heap->data[i].priorityScore,
               heap->data[i].affectedPeople,
               heap->data[i].peopleNeedingMedical);
    }
}
