@echo off
setlocal EnableExtensions

set "CONFIG=%1"
if "%CONFIG%"=="" set CONFIG=Release

set "PROJECT=%2"
set "PROJECT_PATH="

if "%PROJECT%"=="" call :resolve_project_from_settings
if "%PROJECT%"=="" set "PROJECT=EchoesBelow"

REM Optional third arg: number of parallel jobs (defaults to number of processors)
set "JOBS=%3"
if "%JOBS%"=="" set JOBS=%NUMBER_OF_PROCESSORS%

TITLE Export Game (%CONFIG%)

echo.
echo ==========================================================
echo           Exporting Standalone Game (%CONFIG%)
echo ----------------------------------------------------------
echo   Engine: GrapeEngineLib (DLL)
echo   Runtime: GrapeRuntime -^> Game executable
echo   Output: build_game\export\%PROJECT%\%CONFIG%
echo ==========================================================

REM Create build folder if it doesn't exist
if not exist build_game mkdir build_game
cd build_game

REM Configure with CMake for GAME ONLY
REM Engine is built as a library (GrapeEngineNative.dll)
REM Runtime links against it and outputs game executable
set "EXTRA_ARGS="
if not "%PROJECT_PATH%"=="" (
    set "EXTRA_ARGS=-DEXPORT_PROJECT_DIR=""%PROJECT_PATH%"""
) else if not "%PROJECT%"=="" (
    set "EXTRA_ARGS=-DEXPORT_PROJECT_NAME=%PROJECT%"
)
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON %EXTRA_ARGS%
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build and export the game
cmake --build . --config %CONFIG% --target ExportGame --parallel %JOBS%
if %errorlevel% neq 0 (
    echo ERROR: Export failed
    cd ..
    pause
    exit /b 1
)

echo.
echo ==========================================================
echo           Compiling C# Scripts (%CONFIG%)
echo ----------------------------------------------------------
echo   Project: %PROJECT%
echo   Output: export\%PROJECT%\%CONFIG%\GameScripts.dll
echo ==========================================================

set "PROJECT_PATH_ARG=..\%PROJECT%"
if not "%PROJECT_PATH%"=="" set "PROJECT_PATH_ARG=%PROJECT_PATH%"
dotnet run --project ..\managed\tools\ScriptCompiler\ScriptCompiler.csproj --configuration %CONFIG% -- "%PROJECT_PATH_ARG%" "export\%PROJECT%\%CONFIG%\GameScripts.dll"
if %errorlevel% neq 0 (
    echo ERROR: C# script compilation failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo.
echo ==========================================================
echo  Standalone Game export %CONFIG% completed successfully!
echo ==========================================================
echo Export location: build_game\export\%PROJECT%\%CONFIG%
echo.
pause
exit /b 0

:resolve_project_from_settings
set "DOCS_ROOT="
set "SETTINGS_PATH="
set "PROJECT_INFO="
set "CHOICE_FILE=%TEMP%\grape_export_choice.txt"
if exist "%CHOICE_FILE%" del /f /q "%CHOICE_FILE%" >nul 2>&1

for /f "usebackq delims=" %%D in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('MyDocuments')"`) do set "DOCS_ROOT=%%D"
if "%DOCS_ROOT%"=="" goto :eof
set "SETTINGS_PATH=%DOCS_ROOT%\Grape Engine\EditorSettings.json"
if not exist "%SETTINGS_PATH%" goto :eof

powershell -NoProfile -Command ^
  "$settings = '%SETTINGS_PATH%'; $out = '%CHOICE_FILE%';" ^
  "if (Test-Path $settings) {" ^
  "  $json = Get-Content $settings -Raw | ConvertFrom-Json;" ^
  "  $projects = @();" ^
  "  if ($json.RecentProjects) { $projects += $json.RecentProjects }" ^
  "  if ($json.LastProject) { if (-not ($projects -contains $json.LastProject)) { $projects = @($json.LastProject) + $projects } }" ^
  "  $projects = $projects | Where-Object { $_ -and (Test-Path $_) };" ^
  "  $resolved = @();" ^
  "  foreach ($p in $projects) { try { $resolved += (Resolve-Path $p).Path } catch {} }" ^
  "  $projects = @($resolved | Sort-Object -Unique);" ^
  "  if ($projects.Count -eq 0) { Write-Host 'No projects defined. Run the editor and add a project first before exporting.'; exit 1 }" ^
  "  if ($projects.Count -gt 1) {" ^
  "    Write-Host 'Choose project to export:';" ^
  "    for ($i=0; $i -lt $projects.Count; $i++) {" ^
  "      $path = $projects[$i];" ^
  "      $name = Split-Path $path -Leaf;" ^
  "      Write-Host (($i+1).ToString() + '. ' + $name + ' (' + $path + ')');" ^
  "    }" ^
  "    $choice = Read-Host;" ^
  "    [int]$idx = $choice - 1;" ^
  "    if ($idx -lt 0 -or $idx -ge $projects.Count) { exit 0 }" ^
  "    $selected = $projects[$idx]" ^
  "  } else { $selected = $projects[0] }" ^
  "  $selectedPath = (Resolve-Path $selected).Path;" ^
  "  $name = Split-Path $selectedPath -Leaf;" ^
  "  Set-Content -Path $out -Value ($name + '|' + $selectedPath) -Encoding ASCII" ^
  "}"

if not exist "%CHOICE_FILE%" goto :eof
set /p PROJECT_INFO=<"%CHOICE_FILE%"
del /f /q "%CHOICE_FILE%" >nul 2>&1

if "%PROJECT_INFO%"=="" goto :eof
for /f "tokens=1,2 delims=|" %%A in ("%PROJECT_INFO%") do (
    set "PROJECT=%%A"
    set "PROJECT_PATH=%%B"
)
goto :eof
