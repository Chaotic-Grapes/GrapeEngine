param(
    [string]$Config = "Release",
    [string]$Project = "",
    [string]$BuildDir = "build_game",
    [string]$DistDir = "dist",
    [switch]$IncludeSymbols
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ProjectName {
    param([string]$ExplicitProject)

    if ($ExplicitProject -ne "") {
        return $ExplicitProject
    }

    $projectSettings = Get-ChildItem -Path $PSScriptRoot -Recurse -Filter ProjectSettings.json -File |
        Where-Object {
            $_.FullName -notmatch "\\\\(build|build_game|build_editor|x64|\\.vs)\\\\"
        }

    if ($projectSettings.Count -eq 1) {
        return Split-Path -Leaf $projectSettings[0].DirectoryName
    }

    if ($projectSettings.Count -eq 0) {
        throw "No ProjectSettings.json found. Pass -Project <name> or place a project folder in the repo root."
    }

    $names = $projectSettings | ForEach-Object { Split-Path -Leaf $_.DirectoryName }
    throw ("Multiple projects found: {0}. Pass -Project <name>." -f ($names -join ", "))
}

function Copy-IfExists {
    param(
        [string]$Source,
        [string]$Destination,
        [switch]$Recurse
    )

    if (Test-Path $Source) {
        if ($Recurse) {
            Copy-Item $Source $Destination -Recurse -Force
        } else {
            Copy-Item $Source $Destination -Force
        }
    }
}

$projectName = Resolve-ProjectName -ExplicitProject $Project
$projectDir = Join-Path $PSScriptRoot $projectName
if (!(Test-Path (Join-Path $projectDir "ProjectSettings.json"))) {
    throw "Project '$projectName' not found or missing ProjectSettings.json."
}

$buildDirFull = Join-Path $PSScriptRoot $BuildDir
$distRoot = Join-Path $PSScriptRoot $DistDir
$distOut = Join-Path $distRoot (Join-Path $projectName $Config)

Write-Host "Configuring game build..."
& cmake -S $PSScriptRoot -B $buildDirFull -G "Visual Studio 17 2022" -A x64 -DBUILD_EDITOR=OFF -DBUILD_GAME=ON
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed."
}

Write-Host "Building runtime..."
& cmake --build $buildDirFull --config $Config --target GrapeRuntime
if ($LASTEXITCODE -ne 0) {
    throw "Game build failed."
}

if (Test-Path $distOut) {
    Remove-Item $distOut -Recurse -Force
}
New-Item -ItemType Directory -Path $distOut | Out-Null

$outputDir = Join-Path $buildDirFull $Config
if (!(Test-Path $outputDir)) {
    throw "Build output directory not found: $outputDir"
}

Write-Host "Staging binaries..."
Copy-IfExists -Source (Join-Path $outputDir "*.exe") -Destination $distOut
Copy-IfExists -Source (Join-Path $outputDir "*.dll") -Destination $distOut
Copy-IfExists -Source (Join-Path $outputDir "*.json") -Destination $distOut
if ($IncludeSymbols) {
    Copy-IfExists -Source (Join-Path $outputDir "*.pdb") -Destination $distOut
}

$engineDll = Join-Path $buildDirFull (Join-Path "engine" (Join-Path $Config "GrapeEngineNative.dll"))
Copy-IfExists -Source $engineDll -Destination $distOut

$runtimeConfig = Join-Path $PSScriptRoot "managed\\api\\GrapeEngine.Scripting.runtimeconfig.json"
Copy-IfExists -Source $runtimeConfig -Destination $distOut

$nethost = Join-Path $PSScriptRoot "externals\\lib\\CoreCLR\\nethost.dll"
Copy-IfExists -Source $nethost -Destination $distOut

$fmod = Join-Path $PSScriptRoot "externals\\lib\\Fmod\\fmod.dll"
Copy-IfExists -Source $fmod -Destination $distOut

Write-Host "Staging engine assets..."
Copy-IfExists -Source (Join-Path $PSScriptRoot "assets") -Destination (Join-Path $distOut "assets") -Recurse

Write-Host "Staging project content..."
Copy-IfExists -Source $projectDir -Destination (Join-Path $distOut $projectName) -Recurse

Write-Host "Export complete: $distOut"
