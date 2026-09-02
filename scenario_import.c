#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "scenario_import.h"
#include "graph.h"
#include "rescue.h"
#include "hospital.h"
#include "db.h"
#include "priority_queue.h"

extern MaxHeap emergencyQueue;

static char* trim(char* str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static int parseBoolean(const char* str) {
    if (strcasecmp(str, "1") == 0 || strcasecmp(str, "yes") == 0 || strcasecmp(str, "true") == 0) return 1;
    return 0;
}

int importScenarioFromString(const char *content) {
    if (!content) return -1;
    
    dbClearAll();
    initDestinations();
    initRescueResources();
    initializeGraph();
    initHeap(&emergencyQueue);
    
    char *buf1 = strdup(content);
    if (!buf1) return -1;
    
    char *line;
    char *saveptr;
    char section[50] = "";
    
    // Pass 1: Locations
    line = strtok_r(buf1, "\r\n", &saveptr);
    while (line) {
        char *t = trim(line);
        if (t[0] != '#' && t[0] != '\0') {
            if (t[0] == '[') {
                strncpy(section, t, sizeof(section)-1);
                section[sizeof(section)-1] = '\0';
            } else if (strcmp(section, "[LOCATIONS]") == 0) {
                int id = addLocation(-1, t);
                dbAddLocation(id, t);
            }
        }
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
    free(buf1);
    
    char *buf2 = strdup(content);
    if (!buf2) return -1;
    section[0] = '\0';
    
    // Pass 2: Everything else
    static int emIdCounter = 100;
    line = strtok_r(buf2, "\r\n", &saveptr);
    while (line) {
        char *t = trim(line);
        if (t[0] != '#' && t[0] != '\0') {
            if (t[0] == '[') {
                strncpy(section, t, sizeof(section)-1);
                section[sizeof(section)-1] = '\0';
            } else if (strcmp(section, "[LOCATIONS]") != 0) {
                char *fields[10];
                int numFields = 0;
                char *tokSave;
                char *token = strtok_r(t, "|", &tokSave);
                while (token && numFields < 10) {
                    fields[numFields++] = trim(token);
                    token = strtok_r(NULL, "|", &tokSave);
                }
                
                if (strcmp(section, "[ROADS]") == 0) {
                    if (numFields >= 3) {
                        int from = getLocationId(fields[0]);
                        int to = getLocationId(fields[1]);
                        int dist = atoi(fields[2]);
                        if (from != -1 && to != -1) {
                            addRoad(from, to, dist);
                            dbAddRoad(from, to, dist);
                        }
                    }
                } else if (strcmp(section, "[TEAMS]") == 0) {
                    if (numFields >= 5) {
                        int loc = getLocationId(fields[1]);
                        if (loc != -1) {
                            RescueTeam t_team;
                            t_team.id = numTeams;
                            strncpy(t_team.name, fields[0], 49); t_team.name[49]='\0';
                            t_team.location = loc;
                            t_team.capacity = atoi(fields[2]);
                            t_team.medicalKit = parseBoolean(fields[3]);
                            t_team.boatAvailable = parseBoolean(fields[4]);
                            t_team.available = 1;
                            addRescueTeam(t_team.id, t_team.name, t_team.location, t_team.capacity, t_team.available, t_team.medicalKit, t_team.boatAvailable);
                            dbAddRescueTeam(t_team);
                        }
                    }
                } else if (strcmp(section, "[TRANSPORT]") == 0) {
                    if (numFields >= 3) {
                        Transport tr;
                        tr.id = numTransports;
                        strncpy(tr.type, fields[0], 49); tr.type[49]='\0';
                        tr.capacity = atoi(fields[1]);
                        tr.floodSuitable = parseBoolean(fields[2]);
                        tr.available = 1;
                        addTransport(tr.id, tr.type, tr.capacity, tr.available, tr.floodSuitable);
                        dbAddTransport(tr);
                    }
                } else if (strcmp(section, "[DESTINATIONS]") == 0) {
                    if (numFields >= 4) {
                        int loc = getLocationId(fields[1]);
                        if (loc != -1) {
                            Destination d;
                            d.id = numDestinations;
                            strncpy(d.name, fields[0], 49); d.name[49]='\0';
                            d.location = loc;
                            d.capacity = atoi(fields[2]);
                            d.medicalSupport = parseBoolean(fields[3]);
                            d.currentOccupancy = 0;
                            d.accessible = 1;
                            addDestination(d.id, d.name, d.location, d.capacity, d.currentOccupancy, d.medicalSupport, d.accessible);
                            dbAddDestination(d);
                        }
                    }
                } else if (strcmp(section, "[EMERGENCIES]") == 0) {
                    if (numFields >= 4) {
                        Emergency e;
                        e.id = emIdCounter++;
                        strncpy(e.location, fields[0], 49); e.location[49]='\0';
                        strncpy(e.hazardType, fields[1], 49); e.hazardType[49]='\0';
                        e.affectedPeople = atoi(fields[2]);
                        e.peopleNeedingMedical = atoi(fields[3]);
                        e.priorityScore = calculatePriority(e);
                        e.status = STATUS_PENDING;
                        dbAddEmergency(e);
                        insertEmergency(&emergencyQueue, e);
                    }
                }
            }
        }
        line = strtok_r(NULL, "\r\n", &saveptr);
    }
    free(buf2);
    return 0;
}

int importScenario(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return -1;
    }
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    int res = importScenarioFromString(content);
    free(content);
    return res;
}

void loadAssamDemo(void) {
    const char *assamScenario =
        "[LOCATIONS]\n"
        "Guwahati\n"
        "Jorhat\n"
        "Dibrugarh\n"
        "Tezpur\n"
        "Barpeta\n"
        "Silchar\n"
        "Nagaon\n"
        "Goalpara\n\n"
        "[ROADS]\n"
        "Guwahati | Jorhat | 10\n"
        "Guwahati | Barpeta | 20\n"
        "Jorhat | Tezpur | 15\n"
        "Barpeta | Tezpur | 12\n"
        "Guwahati | Tezpur | 22\n"
        "Tezpur | Dibrugarh | 30\n"
        "Jorhat | Dibrugarh | 25\n"
        "Guwahati | Nagaon | 12\n"
        "Nagaon | Jorhat | 18\n"
        "Guwahati | Goalpara | 15\n"
        "Barpeta | Goalpara | 10\n"
        "Guwahati | Silchar | 40\n"
        "Nagaon | Silchar | 35\n\n"
        "[TEAMS]\n"
        "Assam Rifles Alpha | Guwahati | 150 | yes | yes\n"
        "NDRF 1st Battalion | Jorhat | 120 | yes | yes\n"
        "SDRF Quick Response | Tezpur | 90 | yes | yes\n"
        "Army Medical Corps | Guwahati | 80 | yes | no\n"
        "Civil Defense Unit | Barpeta | 60 | no | yes\n\n"
        "[TRANSPORT]\n"
        "IAF Mi-17 Helicopter | 60 | yes\n"
        "NDRF Motorboat Fleet | 40 | yes\n"
        "Heavy All-Terrain Truck | 80 | yes\n"
        "Trauma Ambulance Fleet | 25 | no\n\n"
        "[DESTINATIONS]\n"
        "Guwahati Medical College | Guwahati | 600 | yes\n"
        "Jorhat Civil Hospital | Jorhat | 350 | yes\n"
        "Tezpur District Relief Camp | Tezpur | 400 | no\n"
        "Dibrugarh Base Hospital | Dibrugarh | 300 | yes\n"
        "Barpeta Mega Shelter | Barpeta | 450 | no\n"
        "Silchar Medical Center | Silchar | 350 | yes\n\n"
        "[EMERGENCIES]\n"
        "Dibrugarh | Flood | 320 | 240\n"
        "Barpeta | Flood | 180 | 70\n"
        "Tezpur | Landslide | 110 | 95\n"
        "Goalpara | Flood | 160 | 50\n"
        "Silchar | Flash Flood | 260 | 140\n";

    importScenarioFromString(assamScenario);
    printf("Assam Demo Scenario Loaded.\n");
}

void loadKeralaDemo(void) {
    const char *keralaScenario =
        "[LOCATIONS]\n"
        "Kochi\n"
        "Alappuzha\n"
        "Wayanad\n"
        "Kozhikode\n"
        "Thrissur\n"
        "Kottayam\n"
        "Munnar\n"
        "Idukki\n\n"
        "[ROADS]\n"
        "Kochi | Alappuzha | 53\n"
        "Kochi | Thrissur | 75\n"
        "Thrissur | Kozhikode | 115\n"
        "Kozhikode | Wayanad | 85\n"
        "Kochi | Kottayam | 60\n"
        "Kottayam | Alappuzha | 45\n"
        "Kottayam | Idukki | 80\n"
        "Thrissur | Munnar | 130\n"
        "Idukki | Munnar | 55\n\n"
        "[TEAMS]\n"
        "Backwater Coastal Rescue | Kochi | 220 | yes | yes\n"
        "Highland Mountain Rescue | Wayanad | 120 | yes | no\n"
        "NDRF 4th Battalion | Thrissur | 180 | yes | yes\n"
        "Kerala Police Disaster Unit | Alappuzha | 90 | yes | yes\n"
        "Idukki Terrain Squad | Idukki | 80 | no | no\n\n"
        "[TRANSPORT]\n"
        "Rescue Speedboats | 90 | yes\n"
        "Air Force ALH Dhruv | 50 | yes\n"
        "Heavy Offroad Trucks | 70 | yes\n"
        "Advanced Mobile Ambulances | 30 | no\n\n"
        "[DESTINATIONS]\n"
        "Kochi Aster Medcity | Kochi | 700 | yes\n"
        "Kozhikode Medical College | Kozhikode | 500 | yes\n"
        "Alappuzha General Hospital | Alappuzha | 400 | yes\n"
        "Thrissur Disaster Relief Camp | Thrissur | 550 | no\n"
        "Wayanad Base Hospital | Wayanad | 300 | yes\n\n"
        "[EMERGENCIES]\n"
        "Wayanad | Landslide | 380 | 260\n"
        "Alappuzha | Flood | 480 | 160\n"
        "Munnar | Flash Flood | 140 | 90\n"
        "Idukki | Landslide | 200 | 130\n";

    importScenarioFromString(keralaScenario);
    printf("Kerala Scenario Loaded.\n");
}

void loadUttarakhandDemo(void) {
    const char *uttarakhandScenario =
        "[LOCATIONS]\n"
        "Dehradun\n"
        "Rishikesh\n"
        "Haridwar\n"
        "Chamoli\n"
        "Rudraprayag\n"
        "Joshimath\n"
        "Kedarnath\n"
        "Uttarkashi\n\n"
        "[ROADS]\n"
        "Dehradun | Rishikesh | 45\n"
        "Rishikesh | Haridwar | 25\n"
        "Rishikesh | Rudraprayag | 140\n"
        "Rudraprayag | Chamoli | 70\n"
        "Chamoli | Joshimath | 50\n"
        "Rudraprayag | Kedarnath | 75\n"
        "Dehradun | Uttarkashi | 145\n"
        "Rishikesh | Uttarkashi | 165\n\n"
        "[TEAMS]\n"
        "ITBP High Altitude Team | Joshimath | 150 | yes | no\n"
        "SDRF Mountain Rescue | Rishikesh | 180 | yes | yes\n"
        "Garhwal Scouts Unit | Chamoli | 100 | yes | no\n"
        "Air Force Mi-17 Crew | Dehradun | 90 | yes | yes\n"
        "Civil Volunteer Corps | Haridwar | 70 | no | no\n\n"
        "[TRANSPORT]\n"
        "IAF Mi-17 V5 Helicopter | 80 | yes\n"
        "Cheetah Mountain Chopper | 20 | yes\n"
        "4x4 Mountain Rescue Jeeps | 40 | yes\n"
        "Advanced Life Support Van | 25 | no\n\n"
        "[DESTINATIONS]\n"
        "AIIMS Rishikesh | Rishikesh | 800 | yes\n"
        "Dehradun Military Hospital | Dehradun | 500 | yes\n"
        "Chamoli District Hospital | Chamoli | 250 | yes\n"
        "Haridwar Mega Relief Camp | Haridwar | 900 | no\n"
        "Joshimath Field Hospital | Joshimath | 200 | yes\n\n"
        "[EMERGENCIES]\n"
        "Kedarnath | Flash Flood | 420 | 320\n"
        "Chamoli | Cloudburst | 240 | 180\n"
        "Joshimath | Landslide | 190 | 100\n"
        "Uttarkashi | Flash Flood | 160 | 90\n";

    importScenarioFromString(uttarakhandScenario);
    printf("Uttarakhand Scenario Loaded.\n");
}

void loadOdishaDemo(void) {
    const char *odishaScenario =
        "[LOCATIONS]\n"
        "Bhubaneswar\n"
        "Puri\n"
        "Cuttack\n"
        "Paradip\n"
        "Balasore\n"
        "Berhampur\n"
        "Kendrapara\n\n"
        "[ROADS]\n"
        "Bhubaneswar | Cuttack | 28\n"
        "Bhubaneswar | Puri | 60\n"
        "Cuttack | Paradip | 82\n"
        "Cuttack | Balasore | 170\n"
        "Bhubaneswar | Berhampur | 175\n"
        "Puri | Berhampur | 160\n"
        "Cuttack | Kendrapara | 55\n"
        "Kendrapara | Paradip | 45\n\n"
        "[TEAMS]\n"
        "ODRAF Rapid Action Unit | Bhubaneswar | 250 | yes | yes\n"
        "NDRF 3rd Battalion | Paradip | 200 | yes | yes\n"
        "Coast Guard Marine Unit | Puri | 140 | yes | yes\n"
        "Cuttack Emergency Squad | Cuttack | 110 | yes | no\n"
        "Berhampur Civil Defense | Berhampur | 90 | no | yes\n\n"
        "[TRANSPORT]\n"
        "Coast Guard Hovercraft | 100 | yes\n"
        "Navy Sea King Helicopter | 60 | yes\n"
        "Heavy Amphibious Trucks | 80 | yes\n"
        "Mobile Medical Vans | 40 | no\n\n"
        "[DESTINATIONS]\n"
        "AIIMS Bhubaneswar | Bhubaneswar | 900 | yes\n"
        "SCB Medical College Cuttack | Cuttack | 750 | yes\n"
        "Paradip Port Trust Hospital | Paradip | 350 | yes\n"
        "Puri Cyclone Shelter & Clinic | Puri | 600 | yes\n"
        "Balasore District Camp | Balasore | 450 | no\n\n"
        "[EMERGENCIES]\n"
        "Paradip | Cyclone | 650 | 420\n"
        "Puri | Storm Surge | 500 | 280\n"
        "Kendrapara | Flood | 340 | 190\n"
        "Balasore | Cyclone | 300 | 130\n";

    importScenarioFromString(odishaScenario);
    printf("Odisha Scenario Loaded.\n");
}
