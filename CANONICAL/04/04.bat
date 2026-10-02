@echo off
setlocal

set "ROOT=%~dp0"
set "INPUT=%ROOT%..\03out\03_REACTION_SCREEN.csv"
set "OUTDIR=%ROOT%..\04out"
set "EXE=%OUTDIR%\rza_block04.exe"
set "CXX=D:\AHexaTrader\COMPILER\bin\g++.exe"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

if not exist "%CXX%" (
    where g++ >nul 2>nul
    if errorlevel 1 (
        echo [FAIL] g++.exe not found
        exit /b 1
    )
    set "CXX=g++"
)

echo ============================================
echo RZA CANONICAL BLOCK 04
echo DEVELOPMENT STATISTICS
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic ^
    "%ROOT%04_stats.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK04 FAIL - COMPILE
    exit /b 1
)

"%EXE%" "%INPUT%" "%OUTDIR%"
set "RC=%ERRORLEVEL%"

echo.
echo EXIT=%RC%
exit /b %RC%
