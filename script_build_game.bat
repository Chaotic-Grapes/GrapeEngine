@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

TITLE Build Game (%CONFIG%)

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ===============================================
echo     Building Standalone Game (%CONFIG%)
echo ===============================================
echo   Engine: GrapeEngineLib (DLL)
echo   Runtime: GrapeRuntime -^> Game executable
echo ===============================================

REM Create build folder if it doesn't exist
if not exist build_game mkdir build_game
cd build_game

REM Configure with CMake for GAME ONLY
REM Engine is built as a library (GrapeEngineNative.dll)
REM Runtime links against it and outputs game executable
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project (builds both engine library and runtime executable)
cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ===============================================
echo Standalone Game build %CONFIG% completed successfully!
echo ===============================================
REM Try to detect the produced executable name
set "EXE_DIR=build_game\%CONFIG%"
set "EXE_NAME="

for /f "delims=" %%F in ('dir "%EXE_DIR%\*.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"

if defined EXE_NAME (
    echo Executable location: %EXE_DIR%\%EXE_NAME%
) else (
    echo Executable location: %EXE_DIR%\^<not found^>
)
pause
