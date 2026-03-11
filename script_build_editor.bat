@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=DEBUG

TITLE Build Editor (%CONFIG%)

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ==========================================================
echo              Building Editor (%CONFIG%)
echo ----------------------------------------------------------
echo   Engine: GrapeEngineLib (DLL)
echo   Editor: GrapeEditor -^> GrapeEngine.exe
echo ==========================================================

REM Create build folder if it doesn't exist
if not exist build mkdir build
cd build

REM Configure with CMake for EDITOR
REM Engine is now built as a library (GrapeEngineNative.dll)
REM Editor links against it and outputs as GrapeEngine.exe
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=OFF
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project (builds both engine library and editor executable)
cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ==========================================================
echo      Editor build %CONFIG% completed successfully!
echo ----------------------------------------------------------
echo  Engine DLL: build\engine\%CONFIG%\GrapeEngineNative.dll
echo  Editor EXE: build\editor\%CONFIG%\GrapeEngine.exe
echo ==========================================================
pause
