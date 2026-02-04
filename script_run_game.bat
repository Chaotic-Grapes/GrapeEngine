@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

echo.
echo ==========================================================
echo            Running Standalone Game (%CONFIG%)
echo ==========================================================
echo.

set "EXE_DIR=build_game\export\EchoesBelow\%CONFIG%"
set "EXE_NAME="

REM Check if engine DLL exists
if not exist "%EXE_DIR%\GrapeEngineNative.dll" (
    echo ERROR: Engine DLL not found!
    echo Missing: %EXE_DIR%\GrapeEngineNative.dll
    echo Please build the game first using script_build_game.bat
    pause
    exit /b 1
)

REM Find the game executable
for /f "delims=" %%F in ('dir "%EXE_DIR%\*.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"

if not defined EXE_NAME (
    echo ERROR: Game executable not found!
    echo Please build the game first using script_build_game.bat
    pause
    exit /b 1
)

cd "%EXE_DIR%"
start "" "%EXE_NAME%"
cd ..\..

echo Game launched: %EXE_DIR%\%EXE_NAME%
