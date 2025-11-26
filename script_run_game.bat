@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

echo.
echo ===============================================
echo       Running Standalone Game (%CONFIG%)
echo ===============================================
echo.

set "EXE_DIR=build_game\%CONFIG%"
set "EXE_NAME="

for /f "delims=" %%F in ('dir "%EXE_DIR%\*Game.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"
if not defined EXE_NAME (
    for /f "delims=" %%F in ('dir "%EXE_DIR%\*.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"
)

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
