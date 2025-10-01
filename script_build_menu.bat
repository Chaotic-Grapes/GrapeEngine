@echo off
title GrapeEngine Build Menu
call :CHECK_CMAKE
call :CHECK_CL
goto MENU

REM Function to check CMake
:CHECK_CMAKE
cmake --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: CMake is not installed or not in PATH.
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)
goto :eof

REM Function to check MSVC compiler
:CHECK_CL
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo WARNING: Visual Studio Build Tools not found in PATH
    echo Make sure you have Visual Studio or Build Tools installed
    echo.
)
goto :eof

:MENU
cls
echo ===============================================
echo          GrapeEngine - Build Menu
echo ===============================================
echo.
echo 1. Clean Build Folder
echo 2. Build Debug
echo 3. Build Release
echo 4. Run Debug
echo 5. Run Release
echo 6. Clean, Build ^& Run Debug
echo 7. Clean, Build ^& Run Release
echo 8. Exit
echo.
set /p choice=Enter your choice (1-8): 

if "%choice%"=="1" call script_clean.bat pause
if "%choice%"=="2" call script_build.bat Debug
if "%choice%"=="3" call script_build.bat Release
if "%choice%"=="4" script_run.bat Debug
if "%choice%"=="5" script_run.bat Release
if "%choice%"=="6" (
    call script_clean.bat
    call script_build.bat Debug
    call script_run.bat Debug
)
if "%choice%"=="7" (
    call script_clean.bat
    call script_build.bat Release
    call script_run.bat Release
)
if "%choice%"=="8" exit /b
goto MENU
