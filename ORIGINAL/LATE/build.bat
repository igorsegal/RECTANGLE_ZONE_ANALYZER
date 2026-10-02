@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM Жесткое переключение консоли в UTF-8 для полной ликвидации кракозябр
REM ============================================================================
chcp 65001 >nul

REM ============================================================================
REM Определение корня проекта
REM ============================================================================
set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

REM ============================================================================
REM Жесткие и прямые пути (ИСПРАВЛЕНО: Добавлена закрывающая кавычка в COMPILER_BIN)
REM ============================================================================
set "COMPILER_BIN=D:\AHexaTrader\COMPILER\bin"
set "SRC=D:\AHexaTrader\2026.08.18 RECTANGLE_ZONE_ANALYZER\src"
set "BUILD_DIR=D:\AHexaTrader\2026.08.18 RECTANGLE_ZONE_ANALYZER\build"
set "OBJ_DIR=D:\AHexaTrader\2026.08.18 RECTANGLE_ZONE_ANALYZER\build\obj"
set "OUT=D:\AHexaTrader\2026.08.18 RECTANGLE_ZONE_ANALYZER\build\analyzer.exe"

echo ============================================================
echo   RectangleZoneAnalyzer - Build Script [STABLE]
echo ============================================================
echo.

REM Проверка компилятора
if not exist "%COMPILER_BIN%\g++.exe" (
    echo [ERROR] g++.exe not found in %COMPILER_BIN%
    echo Please make sure minGW64 is extracted to D:\AHexaTrader\COMPILER
    goto :final_pause
)

REM Добавляем компилятор в системный путь
set "PATH=%COMPILER_BIN%;%PATH%"

REM Создаём необходимые директории
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

REM ============================================================================
REM Сбор списка всех .cpp файлов
REM ============================================================================
echo [1/3] Collecting source files...
set "FILE_COUNT=0"
set "OBJ_FILES="

if not exist "%SRC%" (
    echo [ERROR] Source directory does not exist: %SRC%
    goto :final_pause
)

for /f "delims=" %%F in ('dir /b /s "%SRC%\*.cpp" 2^>nul') do (
    set /a FILE_COUNT+=1
)

if "%FILE_COUNT%"=="0" (
    echo [ERROR] No .cpp files found in %SRC%
    goto :final_pause
)

echo Found %FILE_COUNT% source file(s).
echo.

REM ============================================================================
REM Компиляция каждого файла (Упрощенный стабильный вывод)
REM ============================================================================
echo [2/3] Compiling RectangleZoneAnalyzer...
echo.

set "CURRENT=0"
set "COMPILE_FAILED=0"

for /f "delims=" %%F in ('dir /b /s "%SRC%\*.cpp" 2^>nul') do (
    set /a CURRENT+=1
    
    set "FILENAME=%%~nF"
    set "FILEPATH=%%F"
    
    set "OBJ_FILE=%OBJ_DIR%\!FILENAME!_!CURRENT!.o"
    set "OBJ_FILES=!OBJ_FILES! "!OBJ_FILE!""
    
    echo   [!CURRENT!/%FILE_COUNT%] Compiling: !FILENAME!.cpp
    
    REM Безопасная однопоточная компиляция
    g++ -c "!FILEPATH!" -O3 -std=c++17 -Wall -Wextra -Wno-unused-parameter -I"%SRC%" -o "!OBJ_FILE!"
    
    if errorlevel 1 (
        echo [ERROR] Compilation failed on file: !FILENAME!.cpp
        set "COMPILE_FAILED=1"
        goto :compile_error
    )
)

:compile_error
if "%COMPILE_FAILED%"=="1" (
    echo.
    echo [ERROR] Build aborted due to compilation errors.
    goto :final_pause
)

echo.
echo [3/3] Linking...

REM Линковка
g++ %OBJ_FILES% -o "%OUT%" -static -static-libgcc -static-libstdc++

if errorlevel 1 (
    echo.
    echo [ERROR] Linking failed!
    goto :final_pause
)

echo.
echo ============================================================
echo   [OK] Build successful!
echo   Output: %OUT%
echo ============================================================
echo.

REM Запуск расчетного ядра
if exist "%OUT%" (
    echo Launching engine...
    "%OUT%"
) else (
    echo [ERROR] Executable file not found after linking!
)

:final_pause
echo.
echo [SYSTEM] Execution paused. Press any key to close this window...
pause
exit /b 0
