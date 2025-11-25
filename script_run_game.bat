@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

echo.
echo ===============================================
echo       Running Standalone Game (%CONFIG%)
echo ===============================================
echo.

if not exist "build_game\%CONFIG%\GrapeGame.exe" (
    echo ERROR: Game executable not found!
    echo Please build the game first using script_build_game.bat
    pause
    exit /b 1
)

cd build_game\%CONFIG%
start GrapeGame.exe
cd ..\..

echo Game launched!
