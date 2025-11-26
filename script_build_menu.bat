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
echo 1. Clean Build Folders
echo 2. Editor
echo 3. Game
echo 4. Build All (Debug)
echo 5. Build All (Release)
echo 6. Exit
echo.
set /p choice=Enter your choice (1-6): 

if "%choice%"=="1" (
    call script_clean.bat pause
    goto MENU
)

if "%choice%"=="2" goto EDITOR_MENU
if "%choice%"=="3" goto GAME_MENU
if "%choice%"=="4" (
    call script_build_all.bat Debug
    pause
    goto MENU
)
if "%choice%"=="5" (
    call script_build_all.bat Release
    pause
    goto MENU
)
if "%choice%"=="6" exit /b
goto MENU

:: Editor submenu
:EDITOR_MENU
cls
echo ===============================================
echo              Editor - Build / Run
echo ===============================================
echo.
echo 1. Build Editor (Debug)
echo 2. Build Editor (Release)
echo 3. Run Editor (Debug)
echo 4. Run Editor (Release)
echo 5. Clean, Build ^& Run (Debug)
echo 6. Clean, Build ^& Run (Release)
echo 7. Back
echo.
set /p echoice=Enter your choice (1-7): 

if "%echoice%"=="1" (
    call script_build_editor.bat Debug
    pause
    goto EDITOR_MENU
)
if "%echoice%"=="2" (
    call script_build_editor.bat Release
    pause
    goto EDITOR_MENU
)
if "%echoice%"=="3" (
    call script_run_editor.bat Debug
    pause
    goto EDITOR_MENU
)
if "%echoice%"=="4" (
    call script_run_editor.bat Release
    pause
    goto EDITOR_MENU
)
if "%echoice%"=="5" (
    call script_clean.bat
    call script_build_editor.bat Debug
    call script_run_editor.bat Debug
    goto EDITOR_MENU
)
if "%echoice%"=="6" (
    call script_clean.bat
    call script_build_editor.bat Release
    call script_run_editor.bat Release
    goto EDITOR_MENU
)
if "%echoice%"=="7" goto MENU

goto EDITOR_MENU

:: Game submenu
:GAME_MENU
cls
echo ===============================================
echo               Game - Build / Run
echo ===============================================
echo.
echo 1. Build Game (Debug)
echo 2. Build Game (Release)
echo 3. Run Game (Debug)
echo 4. Run Game (Release)
echo 5. Clean, Build ^& Run (Debug)
echo 6. Clean, Build ^& Run (Release)
echo 7. Back
echo.
set /p gchoice=Enter your choice (1-7): 

if "%gchoice%"=="1" (
    call script_build_game.bat Debug
    pause
    goto GAME_MENU
)
if "%gchoice%"=="2" (
    call script_build_game.bat Release
    pause
    goto GAME_MENU
)
if "%gchoice%"=="3" (
    call script_run_game.bat Debug
    pause
    goto GAME_MENU
)
if "%gchoice%"=="4" (
    call script_run_game.bat Release
    pause
    goto GAME_MENU
)
if "%gchoice%"=="5" (
    call script_clean.bat
    call script_build_game.bat Debug
    call script_run_game.bat Debug
    goto GAME_MENU
)
if "%gchoice%"=="6" (
    call script_clean.bat
    call script_build_game.bat Release
    call script_run_game.bat Release
    goto GAME_MENU
)
if "%gchoice%"=="7" goto MENU

goto GAME_MENU
