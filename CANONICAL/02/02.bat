@echo off
setlocal

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%..\02out"
set "EXE=%OUTDIR%\rza_block02.exe"
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
echo RZA CANONICAL BLOCK 02
echo RECTANGLE FORMATION ATLAS
echo ============================================
echo.

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic ^
    "%ROOT%..\01\formation_detector.cpp" ^
    "%ROOT%xfbar_reader.cpp" ^
    "%ROOT%02_atlas.cpp" ^
    -static -static-libgcc -static-libstdc++ ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK02 FAIL - COMPILE
    exit /b 1
)

"%EXE%" "%DATA%" "%OUTDIR%"
set "RC=%ERRORLEVEL%"

echo.
echo EXIT=%RC%
exit /b %RC%
