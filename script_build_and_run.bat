@echo off
REM ===============================================
REM Windows Build and Run Script
REM ===============================================

echo.
echo ===============================================
echo   GrapeEngine - Windows Build and Run
echo ===============================================
echo.

REM Check if CMake is available
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

REM Check if Visual Studio Build Tools are available
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: Visual Studio Build Tools not found in PATH
    echo Make sure you have Visual Studio or Build Tools installed
    echo.
)

REM Clean previous build
echo [1/4] Cleaning previous build...
if exist build (
    rmdir /s /q build
    echo    Previous build directory removed
) else (
    echo    No previous build found
)

REM Create build directory
echo [2/4] Creating build directory...
mkdir build
cd build

REM Configure with CMake
echo [3/4] Configuring project with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo [4/4] Building project...
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo.
echo ===============================================
echo   Build completed successfully!
echo ===============================================
echo.

REM Run the application
echo Running GrapeEngine...
echo.
cd Debug
GrapeEngine.exe
if %errorlevel% neq 0 (
    echo.
    echo Application exited with error code: %errorlevel%
) else (
    echo.
    echo Application completed successfully!
)

echo.
echo Press any key to exit...
pause >nul
