@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM Определение корня проекта (где лежит этот .bat файл)
REM ============================================================================
set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"

REM ============================================================================
REM Настройка путей
REM ============================================================================
set "MSYS2_ROOT=D:\msys64"
set "COMPILER_BIN=%MSYS2_ROOT%\ucrt64\bin"
set "SRC=%PROJECT_ROOT%\src"
set "BUILD_DIR=%PROJECT_ROOT%\build"
set "OBJ_DIR=%BUILD_DIR%\obj"
set "OUT=%BUILD_DIR%\analyzer.exe"

REM Проверка наличия компилятора
if not exist "%COMPILER_BIN%\g++.exe" (
    echo [ERROR] g++.exe not found in %COMPILER_BIN%
    echo Please install MSYS2 packages:
    echo   pacman -S mingw-w64-ucrt-x86_64-gcc
    pause
    exit /b 1
)

REM Добавляем компилятор в PATH
set "PATH=%COMPILER_BIN%;%PATH%"

REM Создаём папки build и obj
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo ============================================================
echo   RectangleZoneAnalyzer - Build Script
echo ============================================================
echo.

REM ============================================================================
REM Сбор списка всех .cpp файлов
REM ============================================================================
echo [1/3] Collecting source files...
set "FILE_COUNT=0"
set "OBJ_FILES="

for /f "delims=" %%F in ('dir /b /s "%SRC%\*.cpp" 2^>nul') do (
    set /a FILE_COUNT+=1
)

if "!FILE_COUNT!"=="0" (
    echo [ERROR] No .cpp files found in %SRC%
    pause
    exit /b 1
)

echo Found !FILE_COUNT! source file(s)
echo.

REM ============================================================================
REM Компиляция каждого файла с прогресс-баром
REM ============================================================================
echo [2/3] Compiling RectangleZoneAnalyzer...
echo.

set "CURRENT=0"
set "OBJ_FILES="
set "COMPILE_FAILED=0"

for /f "delims=" %%F in ('dir /b /s "%SRC%\*.cpp" 2^>nul') do (
    set /a CURRENT+=1
    set /a PERCENT=!CURRENT! * 100 / !FILE_COUNT!
    
    REM Извлекаем имя файла без пути и расширения
    set "FILENAME=%%~nF"
    set "FILEPATH=%%F"
    
    REM Формируем имя .o файла (с сохранением структуры папок через хеш)
    set "OBJ_FILE=%OBJ_DIR%\!FILENAME!_!CURRENT!.o"
    set "OBJ_FILES=!OBJ_FILES! "!OBJ_FILE!""
    
    REM Формируем прогресс-бар (40 символов)
    set "BAR="
    set /a FILLED=!PERCENT! * 40 / 100
    for /L %%i in (1,1,!FILLED!) do set "BAR=!BAR!#"
    for /L %%i in (!FILLED!,1,39) do set "BAR=!BAR! "
    
    REM Выводим прогресс (перезаписываем строку)
    <nul set /p "=  [!BAR!] !PERCENT!%% (!CURRENT!/!FILE_COUNT!) !FILENAME!.cpp    "
    <nul set /p "=                                                                             "
    <nul set /p "=  [!BAR!] !PERCENT!%% (!CURRENT!/!FILE_COUNT!) !FILENAME!.cpp"
    echo.
    
    REM Компиляция
    g++ -c "!FILEPATH!" -O3 -march=native -std=c++17 -Wall -Wextra -Wno-unused-parameter ^
        -I"%SRC%" ^
        -o "!OBJ_FILE!"
    
    if errorlevel 1 (
        echo.
        echo [ERROR] Compilation failed: !FILENAME!.cpp
        set "COMPILE_FAILED=1"
        goto :compile_error
    )
)

:compile_error
if "!COMPILE_FAILED!"=="1" (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Linking...

REM Линковка всех .o файлов
g++ %OBJ_FILES% -o "%OUT%" -static -static-libgcc -static-libstdc++

if errorlevel 1 (
    echo.
    echo [ERROR] Linking failed!
    pause
    exit /b 1
)

echo.
echo ============================================================
echo   [OK] Build successful!
echo   Output: %OUT%
echo ============================================================
echo.

REM Запуск
"%OUT%"

endlocal
pause