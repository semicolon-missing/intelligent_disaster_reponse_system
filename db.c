#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "db.h"

static sqlite3 *db = NULL;

int initDB(const char *dbPath) {
    if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return createTables();
}

void closeDB(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

int createTables(void) {
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS Location (id INTEGER PRIMARY KEY, name TEXT);"
        "CREATE TABLE IF NOT EXISTS Road (fromId INTEGER, toId INTEGER, distance INTEGER, PRIMARY KEY (fromId, toId));"
        "CREATE TABLE IF NOT EXISTS Emergency (id INTEGER PRIMARY KEY, location TEXT, hazardType TEXT, affectedPeople INTEGER, peopleNeedingMedical INTEGER, priorityScore REAL, status INTEGER);"
        "CREATE TABLE IF NOT EXISTS RescueTeam (id INTEGER PRIMARY KEY, name TEXT, location INTEGER, capacity INTEGER, available INTEGER, medicalKit INTEGER, boatAvailable INTEGER);"
        "CREATE TABLE IF NOT EXISTS Transport (id INTEGER PRIMARY KEY, type TEXT, capacity INTEGER, available INTEGER, floodSuitable INTEGER);"
        "CREATE TABLE IF NOT EXISTS Destination (id INTEGER PRIMARY KEY, name TEXT, location INTEGER, capacity INTEGER, currentOccupancy INTEGER, medicalSupport INTEGER, accessible INTEGER);";
    char *errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int dbClearAll(void) {
    const char *sql = "DELETE FROM Location; DELETE FROM Road; DELETE FROM Emergency; DELETE FROM RescueTeam; DELETE FROM Transport; DELETE FROM Destination;";
    char *errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error clearing db: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}

int loadAllData(MaxHeap* emergencyQueue) {
    sqlite3_stmt *stmt;
    // Load Locations
    if (sqlite3_prepare_v2(db, "SELECT id, name FROM Location", -1, &stmt, NULL) == SQLITE_OK) {
        initializeGraph();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            addLocation(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1));
        }
        sqlite3_finalize(stmt);
    }
    // Load Roads
    if (sqlite3_prepare_v2(db, "SELECT fromId, toId, distance FROM Road", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            addRoad(sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1), sqlite3_column_int(stmt, 2));
        }
        sqlite3_finalize(stmt);
    }
    // Load Rescue Teams
    if (sqlite3_prepare_v2(db, "SELECT id, name, location, capacity, available, medicalKit, boatAvailable FROM RescueTeam", -1, &stmt, NULL) == SQLITE_OK) {
        initRescueResources();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            addRescueTeam(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
        }
        sqlite3_finalize(stmt);
    }
    // Load Transport
    if (sqlite3_prepare_v2(db, "SELECT id, type, capacity, available, floodSuitable FROM Transport", -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            addTransport(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4));
        }
        sqlite3_finalize(stmt);
    }
    // Load Destinations
    if (sqlite3_prepare_v2(db, "SELECT id, name, location, capacity, currentOccupancy, medicalSupport, accessible FROM Destination", -1, &stmt, NULL) == SQLITE_OK) {
        initDestinations();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            addDestination(sqlite3_column_int(stmt, 0), (const char*)sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3), sqlite3_column_int(stmt, 4), sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6));
        }
        sqlite3_finalize(stmt);
    }
    // Load Emergencies
    if (sqlite3_prepare_v2(db, "SELECT id, location, hazardType, affectedPeople, peopleNeedingMedical, priorityScore, status FROM Emergency WHERE status != 3", -1, &stmt, NULL) == SQLITE_OK) {
        initHeap(emergencyQueue);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Emergency e;
            e.id = sqlite3_column_int(stmt, 0);
            strncpy(e.location, (const char*)sqlite3_column_text(stmt, 1), 49); e.location[49] = '\0';
            strncpy(e.hazardType, (const char*)sqlite3_column_text(stmt, 2), 49); e.hazardType[49] = '\0';
            e.affectedPeople = sqlite3_column_int(stmt, 3);
            e.peopleNeedingMedical = sqlite3_column_int(stmt, 4);
            /* Recompute instead of trusting the stored value, so rows saved under
               older hazard weights are re-ranked with the current formula. */
            e.priorityScore = calculatePriority(e);
            e.status = sqlite3_column_int(stmt, 6);
            
            // Insert directly to bypass status modification in insertEmergency
            if (emergencyQueue->size < MAX_EMERGENCIES) {
                emergencyQueue->data[emergencyQueue->size] = e;
                heapifyUp(emergencyQueue, emergencyQueue->size);
                emergencyQueue->size++;
            }
        }
        sqlite3_finalize(stmt);
    }
    return 0;
}

int dbAddLocation(int id, const char *name) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Location (id, name) VALUES (?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return id;
}

int dbAddRoad(int fromId, int toId, int distance) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Road (fromId, toId, distance) VALUES (?, ?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, fromId);
    sqlite3_bind_int(stmt, 2, toId);
    sqlite3_bind_int(stmt, 3, distance);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int dbAddEmergency(Emergency e) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Emergency (id, location, hazardType, affectedPeople, peopleNeedingMedical, priorityScore, status) VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, e.id);
    sqlite3_bind_text(stmt, 2, e.location, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, e.hazardType, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, e.affectedPeople);
    sqlite3_bind_int(stmt, 5, e.peopleNeedingMedical);
    sqlite3_bind_double(stmt, 6, e.priorityScore);
    sqlite3_bind_int(stmt, 7, e.status);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return e.id;
}

int dbUpdateEmergencyStatus(int id, int status) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE Emergency SET status = ? WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, status);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int dbAddRescueTeam(RescueTeam t) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO RescueTeam (id, name, location, capacity, available, medicalKit, boatAvailable) VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, t.id);
    sqlite3_bind_text(stmt, 2, t.name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, t.location);
    sqlite3_bind_int(stmt, 4, t.capacity);
    sqlite3_bind_int(stmt, 5, t.available);
    sqlite3_bind_int(stmt, 6, t.medicalKit);
    sqlite3_bind_int(stmt, 7, t.boatAvailable);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return t.id;
}

int dbUpdateRescueTeamAvailability(int id, int available) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE RescueTeam SET available = ? WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, available);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int dbAddTransport(Transport t) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Transport (id, type, capacity, available, floodSuitable) VALUES (?, ?, ?, ?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, t.id);
    sqlite3_bind_text(stmt, 2, t.type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, t.capacity);
    sqlite3_bind_int(stmt, 4, t.available);
    sqlite3_bind_int(stmt, 5, t.floodSuitable);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return t.id;
}

int dbUpdateTransportAvailability(int id, int available) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE Transport SET available = ? WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, available);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int dbAddDestination(Destination d) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO Destination (id, name, location, capacity, currentOccupancy, medicalSupport, accessible) VALUES (?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, d.id);
    sqlite3_bind_text(stmt, 2, d.name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, d.location);
    sqlite3_bind_int(stmt, 4, d.capacity);
    sqlite3_bind_int(stmt, 5, d.currentOccupancy);
    sqlite3_bind_int(stmt, 6, d.medicalSupport);
    sqlite3_bind_int(stmt, 7, d.accessible);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return d.id;
}

int dbUpdateDestinationOccupancy(int id, int currentOccupancy) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE Destination SET currentOccupancy = ? WHERE id = ?", -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, currentOccupancy);
    sqlite3_bind_int(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}

int dbResetResources(void) {
    const char *sql = 
        "UPDATE RescueTeam SET available = 1;"
        "UPDATE Transport SET available = 1;"
        "UPDATE Destination SET currentOccupancy = 0;"
        "UPDATE Emergency SET status = 0;";
    char *errMsg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error resetting resources: %s\n", errMsg);
        sqlite3_free(errMsg);
        return -1;
    }
    return 0;
}
v