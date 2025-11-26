@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

TITLE Build Both Editor and Game (%CONFIG%)

REM Optional second arg: number of parallel jobs (defaults to number of processors)
set JOBS=%2
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

echo.
echo ===============================================
echo   Building Both Editor and Game (%CONFIG%)
echo ===============================================
REM Build Editor (into `build`)
echo.
echo --- Configuring and building Editor ---
if not exist build mkdir build
pushd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=ON -DBUILD_GAME=OFF
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration for Editor failed
    popd
    pause
    exit /b 1
)

cmake --build . --config %CONFIG% --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Editor build failed
    popd
    pause
    exit /b 1
)
popd

REM Build Game (into `build_game`)
echo.
echo --- Configuring and building Game ---
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
echo Game location: build_game\%CONFIG%\GrapeGame.exe
echo.
echo Note: the game executable name may be overridden by CMake variable GAME_OUTPUT_NAME
pause
