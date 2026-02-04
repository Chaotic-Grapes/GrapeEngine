@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

set PROJECT=%2
if "%PROJECT%"=="" set PROJECT=EchoesBelow

REM Optional third arg: number of parallel jobs (defaults to number of processors)
set JOBS=%3
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

TITLE Export Game (%CONFIG%)

echo.
echo ==========================================================
echo           Exporting Standalone Game (%CONFIG%)
echo ----------------------------------------------------------
echo   Engine: GrapeEngineLib (DLL)
echo   Runtime: GrapeRuntime -^> Game executable
echo   Output: build_game\export\%PROJECT%\%CONFIG%
echo ==========================================================

REM Create build folder if it doesn't exist
if not exist build_game mkdir build_game
cd build_game

REM Configure with CMake for GAME ONLY
REM Engine is built as a library (GrapeEngineNative.dll)
REM Runtime links against it and outputs game executable
set EXTRA_ARGS=
if not "%PROJECT%"=="" set EXTRA_ARGS=-DEXPORT_PROJECT_NAME=%PROJECT%
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON %EXTRA_ARGS%
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build and export the game
cmake --build . --config %CONFIG% --target ExportGame --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Export failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ==========================================================
echo  Standalone Game export %CONFIG% completed successfully!
echo ==========================================================
echo Export location: build_game\export\%PROJECT%\%CONFIG%
echo.
pause
