@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ===============================================
echo           Building Project (%CONFIG%)
echo ===============================================

REM Create build folder if it doesn't exist
if not exist build mkdir build
cd build

REM Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project
cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo Build %CONFIG% completed successfully!
pause