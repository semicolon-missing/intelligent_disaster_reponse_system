@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo ===================================================
echo  Building Intelligent Disaster Response System
echo ===================================================

where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc was not found on PATH.
    echo Install MinGW-w64 or MSYS2 and add its "bin" folder to PATH.
    exit /b 1
)

set "CFLAGS=-Wall -Wextra -O2"
set "SRCS=priority_queue.c rescue.c graph.c hospital.c db.c scenario_import.c"
set "SQLITE_DIR=third_party\sqlite"
set "SQLITE_VER=3460100"
set "SQLITE_URL=https://www.sqlite.org/2024/sqlite-amalgamation-%SQLITE_VER%.zip"

rem ---------------------------------------------------------------
rem  SQLite: prefer an already-downloaded copy, then a system install,
rem  otherwise download the official single-file amalgamation once.
rem ---------------------------------------------------------------
if exist "%SQLITE_DIR%\sqlite3.c" goto :bundled_sqlite

echo #include ^<sqlite3.h^> > "%TEMP%\idrs_sqlite_probe.c"
echo int main(void){return sqlite3_libversion_number()==0;} >> "%TEMP%\idrs_sqlite_probe.c"
gcc "%TEMP%\idrs_sqlite_probe.c" -o "%TEMP%\idrs_sqlite_probe.exe" -lsqlite3 >nul 2>&1
if not errorlevel 1 (
    del /q "%TEMP%\idrs_sqlite_probe.c" "%TEMP%\idrs_sqlite_probe.exe" >nul 2>&1
    echo [SQLite] Using system SQLite library ^(-lsqlite3^).
    set "DB_INC="
    set "DB_LINK=-lsqlite3"
    goto :build
)
del /q "%TEMP%\idrs_sqlite_probe.c" >nul 2>&1

echo [SQLite] No system SQLite found. Downloading the official amalgamation...
if not exist "%SQLITE_DIR%" mkdir "%SQLITE_DIR%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;" ^
  "Invoke-WebRequest -UseBasicParsing -Uri '%SQLITE_URL%' -OutFile '%SQLITE_DIR%\sqlite.zip';" ^
  "Expand-Archive -Force '%SQLITE_DIR%\sqlite.zip' '%SQLITE_DIR%\tmp';" ^
  "Copy-Item '%SQLITE_DIR%\tmp\sqlite-amalgamation-%SQLITE_VER%\sqlite3.c','%SQLITE_DIR%\tmp\sqlite-amalgamation-%SQLITE_VER%\sqlite3.h' '%SQLITE_DIR%';" ^
  "Remove-Item -Recurse -Force '%SQLITE_DIR%\tmp','%SQLITE_DIR%\sqlite.zip'"
if not exist "%SQLITE_DIR%\sqlite3.c" (
    echo ERROR: Could not download SQLite. Either install SQLite for MinGW
    echo        ^(e.g. MSYS2: pacman -S mingw-w64-x86_64-sqlite3^), or download
    echo        %SQLITE_URL%
    echo        and put sqlite3.c + sqlite3.h into %SQLITE_DIR%\
    exit /b 1
)

:bundled_sqlite
if not exist "%SQLITE_DIR%\sqlite3.o" (
    echo [SQLite] Compiling bundled SQLite once ^(this takes a minute^)...
    gcc -O2 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -c "%SQLITE_DIR%\sqlite3.c" -o "%SQLITE_DIR%\sqlite3.o"
    if errorlevel 1 (
        echo ERROR: Failed to compile SQLite.
        exit /b 1
    )
)
echo [SQLite] Using bundled SQLite from %SQLITE_DIR%.
set "DB_INC=-I%SQLITE_DIR%"
set "DB_LINK=%SQLITE_DIR%\sqlite3.o"

:build
echo [1/2] Compiling cli.exe (Console Decision Support System)...
gcc %CFLAGS% %DB_INC% -o cli.exe main.c %SRCS% %DB_LINK%
if errorlevel 1 (
    echo Error compiling cli.exe!
    exit /b 1
)
echo cli.exe built successfully.

echo [2/2] Compiling web.exe (Tactical Web Server + REST API)...
gcc %CFLAGS% %DB_INC% -o web.exe server_main.c httpserver.c %SRCS% %DB_LINK% -lws2_32
if errorlevel 1 (
    echo Error compiling web.exe!
    exit /b 1
)
echo web.exe built successfully.

echo ===================================================
echo  Build complete!
echo  - Run "cli.exe" for interactive terminal mode.
echo  - Run "web.exe" and open http://localhost:8080 for web command center.
echo  (The ncurses TUI is for Linux/macOS: run "make tui" there.)
echo ===================================================
endlocal
exit /b 0
