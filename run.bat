@echo off
setlocal EnableExtensions

set "JOBS=%~1"
if "%JOBS%"=="" set "JOBS=%NUMBER_OF_PROCESSORS%"

set "ENABLE_CUDA=ON"
echo.
set /p "CUDA_CHOICE=Enable CUDA build? [Y/n]: "
if /I "%CUDA_CHOICE%"=="n" set "ENABLE_CUDA=OFF"
if /I "%CUDA_CHOICE%"=="no" set "ENABLE_CUDA=OFF"

call :check_tool cmake || exit /b 1

echo.
echo ==========================================================
echo           GrapeEngine Build (Release + Debug)
echo ----------------------------------------------------------
echo   Jobs: %JOBS%
echo   CUDA: %ENABLE_CUDA%
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
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=OFF -DENABLE_CUDA=%ENABLE_CUDA%
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
