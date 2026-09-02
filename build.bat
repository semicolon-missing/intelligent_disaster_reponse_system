@echo off
echo ===================================================
echo  Building Intelligent Disaster Response System
echo ===================================================

echo [1/2] Compiling cli.exe (Console Decision Support System)...
gcc -Wall -Wextra -O2 -o cli.exe main.c priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c -lsqlite3
if %errorlevel% neq 0 (
    echo Error compiling cli.exe!
    exit /b %errorlevel%
)
echo cli.exe built successfully.

echo [2/2] Compiling web.exe (Tactical Web Server + REST API)...
gcc -Wall -Wextra -O2 -o web.exe server_main.c priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c httpserver.c -lsqlite3 -lws2_32
if %errorlevel% neq 0 (
    echo Error compiling web.exe!
    exit /b %errorlevel%
)
echo web.exe built successfully.

echo ===================================================
echo  Build complete!
echo  - Run "cli.exe" for interactive terminal mode.
echo  - Run "web.exe" and open http://localhost:8080 for web command center.
echo ===================================================
