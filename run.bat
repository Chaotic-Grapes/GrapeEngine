@echo off
setlocal EnableExtensions

set "JOBS=%~1"
if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"

call :check_tool cmake || exit /b 1

echo.
echo ==========================================================
echo          GrapeEngine Build (Release + Config)
echo ----------------------------------------------------------
echo   Jobs: %JOBS%
echo ==========================================================
echo.
call :build_editor || exit /b 1

echo.
echo ==========================================================
echo   Editor Debug and Release builds succeeded
echo ==========================================================
echo.
pause
exit /b 0

:build_editor
echo.
echo [Editor] Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=OFF
if errorlevel 1 (
    echo ERROR: Editor configure failed.
    echo.
    pause
    exit /b 1
)

echo [Editor] Build Debug
cmake --build build --config Debug --target ALL_BUILD --parallel %JOBS%
if errorlevel 1 (
    echo ERROR: Editor Debug build failed.
    echo.
    pause
    exit /b 1
)

echo [Editor] Build Release
cmake --build build --config Release --target ALL_BUILD --parallel %JOBS%
if errorlevel 1 (
    echo ERROR: Editor Release build failed.
    echo.
    pause
    exit /b 1
)
exit /b 0

:check_tool
where %~1 >nul 2>&1
if errorlevel 1 (
    echo ERROR: Required tool "%~1" is not in PATH.
    exit /b 1
)
exit /b 0

:usage
echo Usage: run.bat [jobs]
echo   Builds editor only in Debug and Release
echo   jobs   Optional parallel job count (default: NUMBER_OF_PROCESSORS)
exit /b 0
