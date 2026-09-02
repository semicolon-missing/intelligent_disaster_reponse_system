CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS_CLI = -lsqlite3
LIBS_WEB = -lsqlite3 -lws2_32

SRCS_COMMON = priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c

all: cli.exe web.exe

cli.exe: main.c $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ main.c $(SRCS_COMMON) $(LIBS_CLI)

web.exe: server_main.c httpserver.c $(SRCS_COMMON)
	$(CC) $(CFLAGS) -o $@ server_main.c httpserver.c $(SRCS_COMMON) $(LIBS_WEB)

clean:
	rm -f cli.exe web.exe *.o

.PHONY: all clean
