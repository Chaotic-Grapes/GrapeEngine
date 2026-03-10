@echo off
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=RELEASE

REM Check if build directory exists
if not exist build (
    echo ERROR: Build directory not found! Build first.
    pause
    exit /b 1
)

REM Check if executable exists
if not exist build\editor\%CONFIG%\GrapeEngine.exe (
    echo ERROR: Editor executable not found! Build first.
    echo Missing: build\editor\%CONFIG%\GrapeEngine.exe
    pause
    exit /b 1
)

REM Check if engine DLL exists in editor directory, if not copy it
if not exist build\editor\%CONFIG%\GrapeEngineNative.dll (
    echo Engine DLL not found in editor directory, checking source...
    if exist build\engine\%CONFIG%\GrapeEngineNative.dll (
        echo Copying GrapeEngineNative.dll to editor directory...
        copy build\engine\%CONFIG%\GrapeEngineNative.dll build\editor\%CONFIG%\ >nul
        if %errorlevel% neq 0 (
            echo ERROR: Failed to copy engine DLL!
            pause
            exit /b 1
        )
        echo DLL copied successfully!
    ) else (
        echo ERROR: Engine DLL not found! Build first.
        echo Missing: build\engine\%CONFIG%\GrapeEngineNative.dll
        pause
        exit /b 1
    )
)

echo Running GrapeEngine Editor...
echo.
cd build\editor\%CONFIG%
GrapeEngine.exe
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