CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS_TUI = -lsqlite3 -lncurses
LIBS_CLI = -lsqlite3

SRCS_COMMON = priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c

all: tui cli

tui: tui_main.c $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ tui_main.c $(SRCS_COMMON) $(LIBS_TUI)

cli: main.c $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ main.c $(SRCS_COMMON) $(LIBS_CLI)

# Windows / Cross-platform compatibility targets
tui.exe: tui
	cp tui tui.exe

cli.exe: cli
	cp cli cli.exe

clean:
	rm -f tui cli tui.exe cli.exe web.exe *.o

.PHONY: all clean
