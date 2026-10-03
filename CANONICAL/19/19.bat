@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\19"
set "OUTDIR=%ROOT%\CANONICAL\19out"
set "EXE=%OUTDIR%\rza_block19.exe"
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
echo RZA BLOCK 19
echo CONFIRMATION CLOSE ENTRY
echo CORE10 / TP=89 / SL=233
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
    "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
    "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
    "%BLOCK%\19_confirmation_entry.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK19 FAIL - COMPILE
    exit /b 1
)

echo.
echo [SELFTEST]
"%EXE%" --selftest
if errorlevel 1 (
    echo.
    echo BLOCK19 FAIL - SELFTEST
    exit /b 1
)
echo [PASS] SELFTEST

echo.
echo [RUN] all accepted ABS_TRACK_v2 confirmations, CORE10
"%EXE%" "%DATA%" "%OUTDIR%"
if errorlevel 1 (
    echo.
    echo BLOCK19 FAIL - MARKET REPLAY
    exit /b 1
)

echo.
echo EXIT=0
exit /b 0
