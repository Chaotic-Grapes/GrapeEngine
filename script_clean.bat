@echo off
REM Check if we should pause
set PAUSEFLAG=%1

echo.
echo ===============================================
echo            Cleaning Build Folders
echo ===============================================
if exist build (
    echo Removing build directory...
    rmdir /s /q build
    echo Build directory removed successfully!
) else (
    echo No build directory found at "build" - nothing to clean.
)

if exist build_game (
    echo Removing build_game directory...
    rmdir /s /q build_game
    echo build_game directory removed successfully!
) else (
    echo No build directory found at "build_game" - nothing to clean.
)

REM Only pause if PAUSEFLAG is "pause"
if /i "%PAUSEFLAG%"=="pause" pause