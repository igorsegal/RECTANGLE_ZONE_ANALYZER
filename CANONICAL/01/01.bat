@echo off
setlocal

set "ROOT=%~dp0"
set "OUTDIR=%ROOT%..\01out"
set "EXE=%OUTDIR%\formation_selftest.exe"
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
echo RZA CANONICAL BLOCK 01
echo ENGULF_2 + ENGULF_3 FORMATION SELFTEST
echo ============================================
echo.

"%CXX%" -std=c++17 -O2 -Wall -Wextra -Wpedantic ^
    "%ROOT%formation_detector.cpp" ^
    "%ROOT%01_selftest.cpp" ^
    -o "%EXE%"

if errorlevel 1 (
    echo.
    echo BLOCK01 FAIL - COMPILE
    exit /b 1
)

"%EXE%"
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
    exit /b %RC%
)

exit /b 0
