@echo off
REM Check if we should pause
set PAUSEFLAG=%1

echo.
echo ===============================================
echo            Cleaning Build Folder
echo ===============================================
if exist build (
    echo Removing build directory...
    rmdir /s /q build
    echo Build directory removed successfully!
) else (
    echo No build directory found - nothing to clean.
)

REM Only pause if PAUSEFLAG is "pause"
if /i "%PAUSEFLAG%"=="pause" pause