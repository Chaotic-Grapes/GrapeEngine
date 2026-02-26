@echo off
setlocal EnableExtensions

set "CONFIG=%1"
if "%CONFIG%"=="" set CONFIG=Release

set "PROJECT=%2"
if "%PROJECT%"=="" call :resolve_project_from_settings
if "%PROJECT%"=="" set "PROJECT=EchoesBelow"

echo.
echo ==========================================================
echo            Running Standalone Game (%CONFIG%)
echo ==========================================================
echo.

set "EXE_DIR=build_game\export\%PROJECT%\%CONFIG%"
set "EXE_NAME="

REM Check if engine DLL exists
if not exist "%EXE_DIR%\GrapeEngineNative.dll" (
    echo ERROR: Engine DLL not found!
    echo Missing: %EXE_DIR%\GrapeEngineNative.dll
    echo Please build the game first using script_build_game.bat
    pause
    exit /b 1
)

REM Find the game executable
for /f "delims=" %%F in ('dir "%EXE_DIR%\*.exe" /b 2^>nul') do if not defined EXE_NAME set "EXE_NAME=%%F"

if not defined EXE_NAME (
    echo ERROR: Game executable not found!
    echo Please build the game first using script_build_game.bat
    pause
    exit /b 1
)

cd "%EXE_DIR%"
start "" "%EXE_NAME%"
cd ..\..

echo Game launched: %EXE_DIR%\%EXE_NAME%
exit /b 0

:resolve_project_from_settings
set "DOCS_ROOT="
set "SETTINGS_PATH="
set "PROJECT_INFO="
set "CHOICE_FILE=%TEMP%\grape_run_choice.txt"
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
  "  $projects = $resolved | Sort-Object -Unique;" ^
  "  if ($projects.Count -eq 0) { Write-Host 'No projects defined. Run the editor and add a project first before exporting.'; exit 1 }" ^
  "  if ($projects.Count -gt 1) {" ^
  "    Write-Host 'Choose project to run:';" ^
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
  "  Set-Content -Path $out -Value $name -Encoding ASCII" ^
  "}"

if not exist "%CHOICE_FILE%" goto :eof
set /p PROJECT=<"%CHOICE_FILE%"
del /f /q "%CHOICE_FILE%" >nul 2>&1
goto :eof
