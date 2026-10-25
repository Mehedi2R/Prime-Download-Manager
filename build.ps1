<#
.SYNOPSIS
    Modern CMake & CI/CD build runner for Prime Download Manager.
.DESCRIPTION
    Orchestrates automated versioning, CMake configuration, Release compilation,
    standalone runtime deployment (windeployqt + MSVC CRT DLLs), and Inno Setup packaging.
.PARAMETER Target
    Build target: 'app' or 'installer' (default).
.PARAMETER Config
    Build configuration: 'Release' (default) or 'Debug'.
.PARAMETER AppVersion
    Explicitly set new version (e.g. -AppVersion 3.0.48). Updates all project files.
.PARAMETER BumpPatch
    Automatically increment patch version (e.g. 3.0.47 -> 3.0.48).
#>
[CmdletBinding()]
param(
    [ValidateSet('app', 'installer', 'clean')]
    [string]$Target = 'installer',

    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',

    [string]$QtDir = '',

    [string]$AppVersion = '',

    [switch]$BumpPatch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = $PSScriptRoot
Set-Location $ProjectRoot

# Read current version from VERSION file
$VersionFile = Join-Path $ProjectRoot "VERSION"
$CurrentVersion = "3.0.47"
if (Test-Path $VersionFile) {
    $CurrentVersion = (Get-Content $VersionFile -Raw).Trim()
}

# Auto-increment patch version if requested
if ($BumpPatch) {
    $parts = $CurrentVersion.Split('.')
    if ($parts.Length -ge 3) {
        $patch = [int]$parts[2] + 1
        $AppVersion = "$($parts[0]).$($parts[1]).$patch"
    }
}

# Synchronize new version across all project files if changed
if ($AppVersion -and ($AppVersion -ne $CurrentVersion)) {
    Write-Host "[VERSION] Upgrading project version: $CurrentVersion -> $AppVersion" -ForegroundColor Magenta
    $CurrentVersion = $AppVersion
    Set-Content -Path $VersionFile -Value $CurrentVersion -NoNewline

    # 1. Update Version.h
    $verH = @"
#pragma once

#ifndef APP_VERSION
#define APP_VERSION "$CurrentVersion"
#endif

#define APP_NAME "Prime Download Manager"
#define APP_TITLE APP_NAME " " APP_VERSION
"@
    Set-Content -Path (Join-Path $ProjectRoot "src-qt\src\Version.h") -Value $verH -NoNewline

    # 2. Update version.rc
    $vParts = $CurrentVersion.Split('.')
    $maj = $vParts[0]; $min = $vParts[1]; $pat = $vParts[2]
    $rcFile = Join-Path $ProjectRoot "src-qt\resources\version.rc"
    if (Test-Path $rcFile) {
        $rcContent = Get-Content $rcFile -Raw
        $rcContent = $rcContent -replace 'FILEVERSION\s+[\d,]+', "FILEVERSION     $maj,$min,$pat,0"
        $rcContent = $rcContent -replace 'PRODUCTVERSION\s+[\d,]+', "PRODUCTVERSION  $maj,$min,$pat,0"
        $rcContent = $rcContent -replace '"FileVersion",\s+"[^"]+"', "`"FileVersion`",      `"$CurrentVersion.0\0`""
        $rcContent = $rcContent -replace '"ProductVersion",\s+"[^"]+"', "`"ProductVersion`",   `"$CurrentVersion\0`""
        Set-Content -Path $rcFile -Value $rcContent -NoNewline
    }

    # 3. Update PDM Extension/manifest.json
    $manifestFile = Join-Path $ProjectRoot "PDM Extension\manifest.json"
    if (Test-Path $manifestFile) {
        $mfContent = Get-Content $manifestFile -Raw
        $mfContent = $mfContent -replace '"version":\s*"[^"]+"', "`"version`": `"$CurrentVersion`""
        Set-Content -Path $manifestFile -Value $mfContent -NoNewline
    }

    # 4. Update README.md
    $readmeFile = Join-Path $ProjectRoot "README.md"
    if (Test-Path $readmeFile) {
        $rmContent = Get-Content $readmeFile -Raw
        $rmContent = $rmContent -replace '# Prime Download Manager [\d\.]+ \(Qt 6 C\+\+ Edition\)', "# Prime Download Manager $CurrentVersion (Qt 6 C++ Edition)"
        Set-Content -Path $readmeFile -Value $rmContent -NoNewline
    }
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host " Prime Download Manager - Modern CMake Build Runner" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "Project Root:    $ProjectRoot"
Write-Host "Current Version: $CurrentVersion"
Write-Host "Target:          $Target"
Write-Host "Config:          $Config"

# 1. Terminate running instance if any to prevent binary file locking
Stop-Process -Name "PrimeDownloadManager" -Force -ErrorAction SilentlyContinue

# 2. Locate CMake
$CmakeCmd = 'cmake'
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $PotentialCmake = @(
        "$env:ProgramFiles\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
    )
    foreach ($c in $PotentialCmake) {
        if (Test-Path $c) {
            $CmakeCmd = $c
            break
        }
    }
    if ($CmakeCmd -eq 'cmake') {
        # Discover CMake bundled with Visual Studio
        $VsCmake = Get-ChildItem -Path "$env:ProgramFiles\Microsoft Visual Studio" -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
        if ($VsCmake -and (Test-Path $VsCmake)) {
            $CmakeCmd = $VsCmake
            $env:PATH = "$(Split-Path $VsCmake);$env:PATH"
        }
    }
}
Write-Host "[INFO] Using CMake from: $CmakeCmd" -ForegroundColor Green

# 3. Locate Qt 6
if (-not $QtDir) {
    $PotentialQt = @(
        "C:\Qt\6.8.0\msvc2022_64",
        "C:\Qt\6.7.2\msvc2022_64",
        "C:\Qt\6.6.3\msvc2022_64",
        "$env:CMAKE_PREFIX_PATH"
    )
    foreach ($p in $PotentialQt) {
        if ($p -and (Test-Path "$p\bin\qmake.exe")) {
            $QtDir = $p
            break
        }
    }
}

if ($QtDir) {
    Write-Host "[INFO] Using Qt 6 from: $QtDir" -ForegroundColor Green
} else {
    Write-Host "[WARN] Qt 6 path not auto-detected; relying on system CMake search paths." -ForegroundColor Yellow
}

# 4. Handle 'clean'
if ($Target -eq 'clean') {
    $BuildDir = Join-Path $ProjectRoot "build"
    if (Test-Path $BuildDir) {
        Write-Host "[INFO] Cleaning build directory: $BuildDir" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
    Write-Host "[SUCCESS] Clean completed." -ForegroundColor Green
    exit 0
}

# 5. CMake Configure
Write-Host "`n[INFO] Running CMake configure..." -ForegroundColor Cyan
$CmakeArgs = @(
    "-B", "build",
    "-A", "x64",
    "-DPDM_VERSION=$CurrentVersion"
)
if ($QtDir) {
    $CmakeArgs += "-DCMAKE_PREFIX_PATH=$QtDir"
}

& $CmakeCmd @CmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configure failed with exit code $LASTEXITCODE."
}

# 6. CMake Build
Write-Host "`n[INFO] Compiling Project ($Config)..." -ForegroundColor Cyan
$BuildArgs = @(
    "--build", "build",
    "--config", $Config
)

if ($Target -eq 'installer') {
    $BuildArgs += @("--target", "installer")
}

& $CmakeCmd @BuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake build failed with exit code $LASTEXITCODE."
}

Write-Host "`n========================================================" -ForegroundColor Green
Write-Host " BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host " Standalone Binary: build\src-qt\$Config\PrimeDownloadManager.exe"
if ($Target -eq 'installer') {
    Write-Host " Setup Installer:   dist\PrimeDownloadManager_v$CurrentVersion.exe"
}
Write-Host "========================================================" -ForegroundColor Green
