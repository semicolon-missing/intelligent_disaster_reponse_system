CC     = gcc
CFLAGS = -Wall -Wextra -O2

SRCS_COMMON = priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c

# ---------------------------------------------------------------
# Platform detection
#   Windows (MinGW): binaries get .exe, web server needs Winsock2.
#   The ncurses TUI is Linux/macOS only.
# ---------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    EXE      = .exe
    LIBS_NET = -lws2_32
else
    EXE      =
    LIBS_NET =
endif

LIBS_DB  = -lsqlite3
LIBS_TUI = -lncurses
DB_DEP   =

# If build.bat (or you) dropped the SQLite amalgamation into third_party/sqlite,
# compile it in instead of requiring a system libsqlite3.
ifneq ($(wildcard third_party/sqlite/sqlite3.c),)
    CFLAGS  += -Ithird_party/sqlite
    DB_DEP   = third_party/sqlite/sqlite3.o
    LIBS_DB  = $(DB_DEP) -lm
endif

CLI = cli$(EXE)
WEB = web$(EXE)
TUI = tui$(EXE)

ifeq ($(OS),Windows_NT)
all: $(CLI) $(WEB)
else
all: $(CLI) $(WEB) $(TUI)
endif

$(CLI): main.c $(SRCS_COMMON) *.h $(DB_DEP)
	$(CC) $(CFLAGS) -o $@ main.c $(SRCS_COMMON) $(LIBS_DB)

$(WEB): server_main.c httpserver.c $(SRCS_COMMON) *.h $(DB_DEP)
	$(CC) $(CFLAGS) -o $@ server_main.c httpserver.c $(SRCS_COMMON) $(LIBS_DB) $(LIBS_NET)

$(TUI): tui_main.c $(SRCS_COMMON) *.h $(DB_DEP)
	$(CC) $(CFLAGS) -o $@ tui_main.c $(SRCS_COMMON) $(LIBS_DB) $(LIBS_TUI)

third_party/sqlite/sqlite3.o: third_party/sqlite/sqlite3.c
	$(CC) -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c $< -o $@

# Standalone graph module self-test (no database needed)
graph_test$(EXE): graph.c graph.h
	$(CC) $(CFLAGS) -DGRAPH_TEST_MAIN -o $@ graph.c

# On Windows, let "make cli" / "make web" work too
ifeq ($(OS),Windows_NT)
cli: $(CLI)
web: $(WEB)
.PHONY: cli web
endif

clean:
	rm -f cli web tui graph_test cli.exe web.exe tui.exe graph_test.exe *.o

.PHONY: all clean
