@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\22"
set "OUTDIR=%ROOT%\CANONICAL\22out"
set "EXE=%OUTDIR%\rza_block22.exe"
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
echo RZA BLOCK 22
echo XAUUSD MULTI-TF FIB GEOMETRY
echo M15 M30 M60 M90 M120 M180
echo ENTRY 38.2 / SL EXTREME / TP 161.8
echo LIMIT LIFE 3xM5
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
    "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
    "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
    "%BLOCK%\22_xau_multitf.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK22 FAIL - COMPILE
    exit /b 1
)

echo.
echo [SELFTEST]
"%EXE%" --selftest
if errorlevel 1 (
    echo.
    echo BLOCK22 FAIL - SELFTEST
    exit /b 1
)
echo [PASS] SELFTEST

echo.
echo [RUN] XAUUSD multi-timeframe replay
"%EXE%" "%DATA%" "%OUTDIR%"
if errorlevel 1 (
    echo.
    echo BLOCK22 FAIL - MARKET REPLAY
    exit /b 1
)

echo.
echo EXIT=0
exit /b 0
