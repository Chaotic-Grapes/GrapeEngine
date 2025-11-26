@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ===============================================
echo   Building Both Editor and Game (%CONFIG%)
echo ===============================================

REM Create build folder if it doesn't exist
if not exist build_both mkdir build_both
cd build_both

REM Configure with CMake for BOTH editor and game
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=ON
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
echo Editor location: build_both\%CONFIG%\GrapeEngine.exe
echo Game location: build_both\%CONFIG%\GrapeGame.exe
pause
