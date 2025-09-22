@echo off
REM ===============================================
REM Windows Clean Script
REM ===============================================

echo.
echo ===============================================
echo   Cleaning Build Directory - GrapeEngine
echo ===============================================
echo.

REM Clean build directory
if exist build (
    echo Removing build directory...
    rmdir /s /q build
    echo Build directory removed successfully!
) else (
    echo No build directory found - nothing to clean.
)

echo.
echo Clean completed!
pause