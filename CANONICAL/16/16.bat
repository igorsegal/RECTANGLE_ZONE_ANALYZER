@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\16"
set "OUTDIR=%ROOT%\CANONICAL\16out"
set "EXE=%OUTDIR%\rza_block16.exe"
set "EVENTS=%ROOT%\CANONICAL\15out\15_EVENTS.csv"
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
echo RZA BLOCK 16
echo CORE10 CAUSAL TRADE REPLAY
echo TP=89 / SL=BEYOND RECTANGLE
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
    "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
    "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
    "%BLOCK%\16_trade_replay.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK16 FAIL - COMPILE
    exit /b 1
)

echo.
echo [SELFTEST]
"%EXE%" --selftest
if errorlevel 1 (
    echo.
    echo BLOCK16 FAIL - SELFTEST
    exit /b 1
)
echo [PASS] SELFTEST

if not exist "%EVENTS%" (
    echo.
    echo BLOCK16 FAIL - MISSING %EVENTS%
    exit /b 1
)

echo.
echo [RUN] CORE10 causal M5 replay
"%EXE%" "%EVENTS%" "%DATA%" "%OUTDIR%"
if errorlevel 1 (
    echo.
    echo BLOCK16 FAIL - MARKET REPLAY
    exit /b 1
)

echo.
echo EXIT=0
exit /b 0
