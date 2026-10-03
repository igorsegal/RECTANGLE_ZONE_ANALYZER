@echo off
setlocal EnableExtensions

set "ROOT=D:\AHexaTrader\2026.10.02 RZA"
set "BLOCK=%ROOT%\CANONICAL\15"
set "OUTDIR=%ROOT%\CANONICAL\15out"
set "EXE=%OUTDIR%\rza_block15_coherence_audit.exe"
set "DATA=D:\AHexaTrader\1DataFiles\raw"
set "CXX=D:\AHexaTrader\COMPILER\bin\g++.exe"

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

"%CXX%" -std=c++17 -O3 -Wall -Wextra -Wpedantic -Wconversion -Wshadow ^
  "%ROOT%\CANONICAL\01\formation_detector.cpp" ^
  "%ROOT%\CANONICAL\02\xfbar_reader.cpp" ^
  "%BLOCK%\15_coherence_audit.cpp" ^
  -static -static-libgcc -static-libstdc++ ^
  -o "%EXE%"

if errorlevel 1 (
  echo COHERENCE AUDIT FAIL - COMPILE
  exit /b 1
)

"%EXE%" --selftest
if errorlevel 1 (
  echo COHERENCE AUDIT FAIL - SELFTEST
  exit /b 1
)

"%EXE%" "%DATA%" "%OUTDIR%"
exit /b %ERRORLEVEL%
