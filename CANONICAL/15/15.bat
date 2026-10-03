@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\15"
set "OUTDIR=%ROOT%\CANONICAL\15out"
set "EXE=%OUTDIR%\rza_block15.exe"
set "DATA=D:\AHexaTrader\1DataFiles\raw"
set "CXX=D:\AHexaTrader\COMPILER\bin\g++.exe"
set "INPROGRESS=%OUTDIR%\15_IN_PROGRESS.txt"

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
echo RZA BLOCK 15 FIX3
echo DIRECT MOVE AFTER ENGULFING
echo FAIL-CLOSED DATA/CRASH CONTRACT
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
    "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
    "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
    "%BLOCK%\15_direct_move.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK15 FAIL - COMPILE
    exit /b 1
)

echo.
echo [SELFTEST] deterministic code-contract checks...
"%EXE%" --selftest
if errorlevel 1 (
    echo.
    echo BLOCK15 FAIL - SELFTEST
    exit /b 1
)
echo [PASS] SELFTEST

if exist "%INPROGRESS%" del /q "%INPROGRESS%"

echo.
echo [RUN] full market-data pass
"%EXE%" "%DATA%" "%OUTDIR%"
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
    echo.
    if exist "%INPROGRESS%" (
        set /p CRASHSYM=<"%INPROGRESS%"
        echo BLOCK15 FAIL - PROCESS_EXIT=%RC% - IN_PROGRESS_SYMBOL=%CRASHSYM%
    ) else (
        echo BLOCK15 FAIL - PROCESS_EXIT=%RC%
    )
    echo No symbol will be silently skipped.
    exit /b %RC%
)

echo.
echo EXIT=0
exit /b 0
