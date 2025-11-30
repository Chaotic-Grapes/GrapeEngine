@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

TITLE Build Both (%CONFIG%)

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ===============================================
echo            Building Both (%CONFIG%)
echo ===============================================

REM Build Editor in 'build' directory
echo.
echo --- Building Editor ---
if not exist build mkdir build
pushd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=OFF
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration for Editor failed
    popd
    pause
    exit /b 1
)

REM Build the C# ScriptAPI first
echo.
echo --- Building BuildScriptAPI (C# ScriptAPI) ---
cmake --build . --config %CONFIG% --target BuildScriptAPI
if %errorlevel% neq 0 (
    echo ERROR: Building BuildScriptAPI target failed
    popd
    pause
    exit /b 1
)

REM Now build the full Editor
cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Editor build failed
    popd
    pause
    exit /b 1
)
popd

REM Build Game in 'build_game' directory
echo.
echo --- Building Game ---
if not exist build_game mkdir build_game
pushd build_game
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration for Game failed
    popd
    pause
    exit /b 1
)

cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Game build failed
    popd
    pause
    exit /b 1
)
popd

echo.
echo Build %CONFIG% completed successfully!
echo Editor location: build\%CONFIG%\GrapeEngine.exe

REM Detect game executable name
set "EXE_DIR=build_game\%CONFIG%"
set "EXE_NAME="

for /f "delims=" %%F in ('dir "%EXE_DIR%\*Game.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"
if not defined EXE_NAME (
    for /f "delims=" %%F in ('dir "%EXE_DIR%\*.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"
)

if defined EXE_NAME (
    echo Game location: %EXE_DIR%\%EXE_NAME%
) else (
    echo Game location: %EXE_DIR%\^<not found^>
)

echo.
pause