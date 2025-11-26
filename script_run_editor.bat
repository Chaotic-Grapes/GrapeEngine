@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

REM Check if build directory exists
if not exist build (
    echo ERROR: Build directory not found! Build first.
    pause
    exit /b 1
)

REM Check if executable exists
if not exist build\%CONFIG%\GrapeEditor.exe (
    echo ERROR: Executable not found! Build first.
    pause
    exit /b 1
)

echo Running GrapeEditor...
echo.
cd build\%CONFIG%
GrapeEditor.exe
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