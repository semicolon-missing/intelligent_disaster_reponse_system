#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "priority_queue.h"
#include "graph.h"
#include "rescue.h"
#include "hospital.h"
#include "db.h"
#include "scenario_import.h"

MaxHeap emergencyQueue;
char lastRescuePlan[4096] = "No plan generated yet.\n";

void printMenu() {
    printf("\n=======================================================\n");
    printf("   INTELLIGENT DISASTER RESPONSE SYSTEM (CLI)\n");
    printf("=======================================================\n");
    printf(" [MAP & NETWORK]\n");
    printf("  1. View Locations\n");
    printf("  2. Add Location\n");
    printf("  3. View Road Network Matrix\n");
    printf("  4. Add Road Connection\n");
    printf("  5. Check Connectivity (BFS)\n");
    printf("  6. Find Shortest Route (Dijkstra)\n");
    printf("  7. Block Road\n");
    printf("  8. Unblock Road\n");
    printf("\n [RESOURCES & DESTINATIONS]\n");
    printf("  9. View Rescue Teams\n");
    printf(" 10. Add Rescue Team\n");
    printf(" 11. View Transport Fleet\n");
    printf(" 12. Add Transport Vehicle\n");
    printf(" 13. View Hospitals & Evacuation Camps\n");
    printf(" 14. Add Hospital / Camp\n");
    printf("\n [SCENARIOS & SIMULATION]\n");
    printf(" 15. Load Multi-Hazard Crisis Scenario (scenarios/sample_natural_disasters.txt)\n");
    printf(" 16. Load Assam Floods Demo\n");
    printf(" 17. Load Kerala Floods & Landslides Demo\n");
    printf(" 18. Load Uttarakhand Cloudburst Demo\n");
    printf(" 19. Load Odisha Super Cyclone Demo\n");
    printf(" 20. Import Custom Scenario from File (.txt)\n");
    printf(" 21. Reset All Resources & Clear Occupancies\n");
    printf("\n [EMERGENCIES & DISPATCH]\n");
    printf(" 22. Report / Add New Emergency\n");
    printf(" 23. View Priority Queue (Max Heap)\n");
    printf(" 24. Dispatch Highest Priority Emergency\n");
    printf(" 25. Show Last Rescue Dispatch Plan\n");
    printf("  0. Exit\n");
    printf("-------------------------------------------------------\n");
    printf("Enter choice [0-25]: ");
}

int main() {
    if (initDB("disaster_v2.db") != 0) {
        printf("Failed to initialize database.\n");
        return 1;
    }
    
    initHeap(&emergencyQueue);
    loadAllData(&emergencyQueue);
    
    if (numLocations == 0) {
        printf("Database empty. Loading Comprehensive Multi-Disaster Scenario (sample_natural_disasters.txt)...\n");
        if (importScenario("sample_natural_disasters.txt") != 0) {
            loadAssamDemo();
        }
    } else {
        printf("Loaded existing scenario from database with %d locations and %d emergencies.\n", numLocations, emergencyQueue.size);
    }

    int choice;
    while (1) {
        printMenu();
        if (scanf("%d", &choice) != 1) break;
        
        if (choice == 0) {
            printf("\nExiting Disaster Response System. Stay safe!\n");
            break;
        }
        else if (choice == 1) {
            printLocations();
        }
        else if (choice == 2) {
            char name[50];
            printf("Enter Location Name: ");
            scanf("%49s", name);
            int id = addLocation(-1, name);
            if (id >= 0) {
                dbAddLocation(id, name);
                printf("Location '%s' added with ID %d.\n", name, id);
            }
        }
        else if (choice == 3) {
            printGraph();
        }
        else if (choice == 4) {
            int u, v, d;
            printf("Enter From Location ID, To Location ID, Distance (km): ");
            scanf("%d %d %d", &u, &v, &d);
            addRoad(u, v, d);
            dbAddRoad(u, v, d);
            printf("Road between %s (%d) and %s (%d) set to %d km.\n", getLocationName(u), u, getLocationName(v), v, d);
        }
        else if (choice == 5) {
            int u, v;
            printf("Enter Start Location ID, Destination Location ID: ");
            scanf("%d %d", &u, &v);
            int reachable = isReachable(u, v);
            printf("Connectivity Status: %s -> %s is %s\n", getLocationName(u), getLocationName(v), reachable ? "REACHABLE (CONNECTED)" : "DISCONNECTED (UNREACHABLE)");
            printf("Running BFS traversal from %s:\n", getLocationName(u));
            bfs(u);
        }
        else if (choice == 6) {
            int u, v;
            printf("Enter Start Location ID, Destination Location ID: ");
            scanf("%d %d", &u, &v);
            dijkstra(u, v);
        }
        else if (choice == 7) {
            int u, v;
            printf("Enter From Location ID, To Location ID to BLOCK: ");
            scanf("%d %d", &u, &v);
            blockRoad(u, v);
        }
        else if (choice == 8) {
            int u, v;
            printf("Enter From Location ID, To Location ID to UNBLOCK: ");
            scanf("%d %d", &u, &v);
            unblockRoad(u, v);
        }
        else if (choice == 9) {
            printf("\n---- Rescue Teams Fleet (%d registered) ----\n", numTeams);
            for (int i = 0; i < numTeams; i++) {
                printf("ID %-2d | %-25s | Base: %-15s | Cap: %-4d | Avail: %-7s | MedKit: %-3s | Boat: %-3s\n",
                    teams[i].id, teams[i].name, getLocationName(teams[i].location), teams[i].capacity,
                    teams[i].available ? "STANDBY" : "DEPLOYED", teams[i].medicalKit ? "YES" : "NO", teams[i].boatAvailable ? "YES" : "NO");
            }
        }
        else if (choice == 10) {
            char name[50];
            int loc, cap, med, boat;
            printf("Enter Team Name: ");
            scanf("%49s", name);
            printf("Enter Base Location ID, Capacity, MedicalKit (0/1), Boat (0/1): ");
            scanf("%d %d %d %d", &loc, &cap, &med, &boat);
            int id = addRescueTeam(-1, name, loc, cap, 1, med, boat);
            RescueTeam t = {id, "", loc, cap, 1, med, boat};
            snprintf(t.name, sizeof(t.name), "%s", name);
            dbAddRescueTeam(t);
            printf("Rescue Team '%s' registered with ID %d.\n", name, id);
        }
        else if (choice == 11) {
            printf("\n---- Transport Fleet (%d registered) ----\n", numTransports);
            for (int i = 0; i < numTransports; i++) {
                printf("ID %-2d | %-25s | Capacity: %-4d | Status: %-8s | Flood/Amphibious: %s\n",
                    transports[i].id, transports[i].type, transports[i].capacity,
                    transports[i].available ? "STANDBY" : "EN ROUTE", transports[i].floodSuitable ? "YES" : "NO");
            }
        }
        else if (choice == 12) {
            char type[50];
            int cap, flood;
            printf("Enter Transport Type Name (e.g. Helicopter, Boat): ");
            scanf("%49s", type);
            printf("Enter Capacity, FloodSuitable (0/1): ");
            scanf("%d %d", &cap, &flood);
            int id = addTransport(-1, type, cap, 1, flood);
            Transport tr = {id, "", cap, 1, flood};
            snprintf(tr.type, sizeof(tr.type), "%s", type);
            dbAddTransport(tr);
            printf("Transport '%s' registered with ID %d.\n", type, id);
        }
        else if (choice == 13) {
            printf("\n---- Evacuation Destinations & Hospitals (%d registered) ----\n", numDestinations);
            for (int i = 0; i < numDestinations; i++) {
                float occPct = destinations[i].capacity > 0 ? ((float)destinations[i].currentOccupancy / destinations[i].capacity) * 100.0f : 0.0f;
                printf("ID %-2d | %-30s | Loc: %-15s | Beds: %d/%d (%4.1f%%) | MedSupport: %-3s\n",
                    destinations[i].id, destinations[i].name, getLocationName(destinations[i].location),
                    destinations[i].currentOccupancy, destinations[i].capacity, occPct,
                    destinations[i].medicalSupport ? "YES" : "NO");
            }
        }
        else if (choice == 14) {
            char name[50];
            int loc, cap, med;
            printf("Enter Destination/Hospital Name: ");
            scanf("%49s", name);
            printf("Enter Location ID, Total Capacity, MedicalSupport (0/1): ");
            scanf("%d %d %d", &loc, &cap, &med);
            int id = addDestination(-1, name, loc, cap, 0, med, 1);
            Destination d = {id, "", loc, cap, 0, med, 1};
            snprintf(d.name, sizeof(d.name), "%s", name);
            dbAddDestination(d);
            printf("Destination '%s' registered with ID %d.\n", name, id);
        }
        else if (choice == 15) {
            if (importScenario("sample_natural_disasters.txt") == 0) {
                printf("Comprehensive Multi-Hazard Scenario successfully loaded from 'scenarios/sample_natural_disasters.txt'.\n");
            } else {
                loadAssamDemo();
                printf("Default Scenario Loaded.\n");
            }
        }
        else if (choice == 16) {
            loadAssamDemo();
            printf("Assam Floods Scenario Loaded successfully.\n");
        }
        else if (choice == 17) {
            loadKeralaDemo();
            printf("Kerala Floods & Landslides Scenario Loaded successfully.\n");
        }
        else if (choice == 18) {
            loadUttarakhandDemo();
            printf("Uttarakhand Cloudburst Scenario Loaded successfully.\n");
        }
        else if (choice == 19) {
            loadOdishaDemo();
            printf("Odisha Super Cyclone Scenario Loaded successfully.\n");
        }
        else if (choice == 20) {
            char file[128];
            printf("Enter scenario filepath (default: scenarios/sample_natural_disasters.txt): ");
            scanf("%127s", file);
            if (importScenario(file) == 0) {
                printf("Scenario successfully imported from '%s'.\n", file);
            } else {
                printf("Error: Failed to open or parse file '%s'.\n", file);
            }
        }
        else if (choice == 21) {
            resetRescueAvailabilities();
            resetDestinationsOccupancy();
            dbResetResources();
            loadAllData(&emergencyQueue);
            printf("Simulation reset: All rescue teams and transports are on STANDBY, hospital occupancies cleared.\n");
        }
        else if (choice == 22) {
            Emergency e;
            memset(&e, 0, sizeof(e));
            printf("Enter Location Name: ");
            scanf("%49s", e.location);
            printf("Enter Hazard Type (Flood, Flash Flood, Landslide, Cyclone, Fire, Earthquake, etc.): ");
            scanf("%49s", e.hazardType);
            printf("Enter Total Affected People: ");
            scanf("%d", &e.affectedPeople);
            printf("Enter Number of People Needing Critical Medical Attention: ");
            scanf("%d", &e.peopleNeedingMedical);

            static int e_id_counter = 700;
            e.id = e_id_counter++;
            e.status = STATUS_PENDING;
            e.priorityScore = calculatePriority(e);
            dbAddEmergency(e);
            insertEmergency(&emergencyQueue, e);
            printf("Emergency #%d created with calculated Priority Score: %.2f\n", e.id, e.priorityScore);
        }
        else if (choice == 23) {
            printHeap(&emergencyQueue);
        }
        else if (choice == 24) {
            if (isHeapEmpty(&emergencyQueue)) {
                printf("Priority Queue is empty. No pending emergencies to dispatch!\n");
                continue;
            }
            Emergency e = extractHighestPriority(&emergencyQueue);
            int e_locId = getLocationId(e.location);
            
            if (e_locId == -1) {
                printf("Error: Emergency location '%s' not recognized in road network.\n", e.location);
                continue;
            }

            RescueTeam* t = selectRescueTeam(e, e_locId);
            Transport* tr = selectTransport(e);
            Destination* d = selectDestination(e, e_locId);
            
            if (!t) {
                printf("Dispatch Failed: No suitable or reachable rescue team available for emergency at %s.\n", e.location);
                insertEmergency(&emergencyQueue, e);
                continue;
            }
            
            if (!d) {
                printf("Dispatch Failed: No reachable destination / hospital with available capacity for %s.\n", e.location);
                insertEmergency(&emergencyQueue, e);
                continue;
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

            int pathEm[MAX_LOCATIONS], pathDest[MAX_LOCATIONS];
            int pathEmLen = getShortestPath(t->location, e_locId, pathEm, MAX_LOCATIONS);
            int pathDestLen = getShortestPath(e_locId, d->location, pathDest, MAX_LOCATIONS);

            char emRouteStr[256] = "", destRouteStr[256] = "";
            for (int i = 0; i < pathEmLen; i++) {
                strncat(emRouteStr, getLocationName(pathEm[i]), sizeof(emRouteStr) - strlen(emRouteStr) - 1);
                if (i < pathEmLen - 1) strncat(emRouteStr, " -> ", sizeof(emRouteStr) - strlen(emRouteStr) - 1);
            }
            for (int i = 0; i < pathDestLen; i++) {
                strncat(destRouteStr, getLocationName(pathDest[i]), sizeof(destRouteStr) - strlen(destRouteStr) - 1);
                if (i < pathDestLen - 1) strncat(destRouteStr, " -> ", sizeof(destRouteStr) - strlen(destRouteStr) - 1);
            }
            
            snprintf(lastRescuePlan, sizeof(lastRescuePlan),
                "\n=======================================================\n"
                "               MISSION DISPATCH DIRECTIVE              \n"
                "=======================================================\n"
                " Emergency ID       : #%d\n"
                " Disaster Site      : %s [Hazard: %s]\n"
                " Affected Count     : %d total (%d requiring urgent medical care)\n"
                " Priority Score     : %.2f\n"
                "-------------------------------------------------------\n"
                " Assigned Team      : %s (Dispatched from %s)\n"
                " Assigned Transport : %s\n"
                " Evacuation Dest.   : %s (Occupancy: %d/%d)\n"
                "-------------------------------------------------------\n"
                " Route to Incident  : %s (%d km)\n"
                " Route to Hospital  : %s (%d km)\n"
                " Total Mission Dist : %d km\n"
                "=======================================================\n",
                e.id, e.location, e.hazardType, e.affectedPeople, e.peopleNeedingMedical, e.priorityScore,
                t->name, getLocationName(t->location), tr ? tr->type : "Ground Unit",
                d->name, d->currentOccupancy, d->capacity,
                emRouteStr, distToEm, destRouteStr, distToDest, distToEm + distToDest);
            
            printf("%s\n", lastRescuePlan);
        }
        else if (choice == 25) {
            printf("%s\n", lastRescuePlan);
        }
    }
    
    closeDB();
    return 0;
}
