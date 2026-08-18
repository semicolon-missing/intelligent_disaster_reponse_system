#include <stdio.h>
#include "priority_queue.h"

void initHeap(MaxHeap *heap){
    heap->size = 0;
}

int isHeapEmpty(MaxHeap *heap){
    return heap->size == 0;
}

// Priority Score = 40% Severity + 30% Population Factor + 30% Medical Urgency

float calculatePriority(Emergency e){
    float severityScore = e.severity;         // 0 - 10
    float medicalScore = e.medicalUrgency;    // 0 - 10

    float populationCap = 1000.0f;
    float populationScore = (e.affectedPeople / populationCap) * 10.0f;

    if (populationScore > 10.0f){
        populationScore = 10.0f;
    }

    float score = (0.40f * severityScore) + (0.30f * medicalScore) + (0.30f * populationScore);

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
    Emergency top = heap->data[0]; // caller should check isHeapEmpty() first
 
    heap->size--;
    heap->data[0] = heap->data[heap->size];
    heapifyDown(heap, 0);
 
    return top;
}
 
void printHeap(MaxHeap *heap) {
    printf("---- Current Priority Queue (%d emergencies) ----\n", heap->size);
    for (int i = 0; i < heap->size; i++) {
        printf("ID %d | %-15s | Score: %.2f | Severity: %d | People: %d | Medical: %d\n",
               heap->data[i].id,
               heap->data[i].location,
               heap->data[i].priorityScore,
               heap->data[i].severity,
               heap->data[i].affectedPeople,
               heap->data[i].medicalUrgency);
    }
}
