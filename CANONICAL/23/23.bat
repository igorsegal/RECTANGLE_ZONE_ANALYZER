@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\23"
set "OUTDIR=%ROOT%\CANONICAL\23out"
set "EXE=%OUTDIR%\rza_block23.exe"
set "DATA=D:\AHexaTrader\1DataFiles\raw"
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
echo RZA BLOCK 23
echo XAUUSD LARGE-TF TARGET MATRIX
echo M90 M120 M180
echo ENTRY FIB38.2 / LIMIT LIFE 3xM5
echo SL CONFIRMATION EXTREME
echo TP 150 200 300 400 + FIB161.8 CONTROL
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
    "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
    "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
    "%BLOCK%\23_target_matrix.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK23 FAIL - COMPILE
    exit /b 1
)

echo.
echo [SELFTEST]
"%EXE%" --selftest
if errorlevel 1 (
    echo.
    echo BLOCK23 FAIL - SELFTEST
    exit /b 1
)
echo [PASS] SELFTEST

echo.
echo [RUN] XAUUSD M90/M120/M180 target comparison
"%EXE%" "%DATA%" "%OUTDIR%"
if errorlevel 1 (
    echo.
    echo BLOCK23 FAIL - MARKET REPLAY
    exit /b 1
)

echo.
echo EXIT=0
exit /b 0
