@echo off
setlocal

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%..\03out"
set "EXE=%OUTDIR%\rza_block03.exe"
set "DATA=D:\AHexaTrader\1DataFiles\raw"
set "CXX=D:\AHexaTrader\COMPILER\bin\g++.exe"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

rem Remove obsolete outputs from the sampled/old-definition Block03.
del /q "%OUTDIR%\03_REACTION_SCREEN.csv" 2>nul
del /q "%OUTDIR%\03_FILE_SUMMARY.csv" 2>nul

if not exist "%CXX%" (
    where g++ >nul 2>nul
    if errorlevel 1 (
        echo [FAIL] g++.exe not found
        exit /b 1
    )
    set "CXX=g++"
)

echo ============================================
echo RZA CANONICAL BLOCK 03
echo FULL PER-INSTRUMENT ABS_TRACK TEST
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic ^
    "%ROOT%..\01\formation_detector.cpp" ^
    "%ROOT%..\02\xfbar_reader.cpp" ^
    "%ROOT%03_reaction_screen.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK03 FAIL - COMPILE
    exit /b 1
)

"%EXE%" "%DATA%" "%OUTDIR%"
set "RC=%ERRORLEVEL%"

echo.
echo EXIT=%RC%
exit /b %RC%
