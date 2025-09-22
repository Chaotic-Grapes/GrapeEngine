@echo off
REM ===============================================
REM Windows Run Script
REM ===============================================

echo.
echo ===============================================
echo   Running GrapeEngine - Windows
echo ===============================================
echo.

REM Check if build directory exists
if not exist build (
    echo ERROR: Build directory not found!
    echo Please run script_build_and_run.bat first to build the project.
    pause
    exit /b 1
)

REM Check if executable exists
if not exist build\Debug\GrapeEngine.exe (
    echo ERROR: Executable not found!
    echo Please run script_build_and_run.bat first to build the project.
    pause
    exit /b 1
)

echo Running GrapeEngine...
echo.
cd build\Debug
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