# SPDX-License-Identifier: AGPL-3.0-or-later
<#
.SYNOPSIS
    Builds OpenPS.exe (the OpenSlicer launcher) and optionally publishes it.

.DESCRIPTION
    Compiles src/openps_launcher with the Visual Studio C++ x64 toolchain (found with vswhere)
    into build/openslicer/OpenPS.exe. PrusaSlicer itself is not rebuilt: the launcher loads the
    official PrusaSlicer.dll at run time.

.PARAMETER Publish
    After a successful build, runs scripts/publish.ps1 to install OpenSlicer in -InstallDir.

.PARAMETER InstallDir
    Install folder used with -Publish (default C:\App\OpenPS).

.PARAMETER PrusaSlicerDir
    PrusaSlicer 2.9.7 install folder used with -Publish (default C:\Program Files\Prusa3D\PrusaSlicer).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build_launcher.ps1 -Publish
#>
param(
    [switch]$Publish,
    [string]$InstallDir = "C:\App\OpenPS",
    [string]$PrusaSlicerDir = "C:\Program Files\Prusa3D\PrusaSlicer"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$SrcDir   = Join-Path $RepoRoot "src\openps_launcher"
$OutDir   = Join-Path $RepoRoot "build\openslicer"
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

# Visual Studio 2019 or newer with the "Desktop development with C++" workload
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found: install Visual Studio 2019 or newer with the C++ workload." }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "No Visual Studio installation with the x64 C++ tools was found." }
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

$exe = Join-Path $OutDir "OpenPS.exe"
if (Test-Path $exe) { Remove-Item $exe -Force }

$libs = "user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib shlwapi.lib msimg32.lib gdiplus.lib winhttp.lib advapi32.lib bcrypt.lib"
$vswhereDir = Split-Path -Parent $vswhere
$cmd = @"
@echo off
set "PATH=$vswhereDir;%PATH%"
call "$vcvars" >nul || exit /b 1
cd /d "$SrcDir" || exit /b 1
rc.exe /nologo /fo "$OutDir\OpenPS.res" OpenPS.rc || exit /b 1
cl.exe /nologo /O2 /MD /EHsc /std:c++17 /W4 /utf-8 /DUNICODE /D_UNICODE /Fo"$OutDir\\" OpenPS_launcher.cpp "$OutDir\OpenPS.res" /Fe"$exe" /link /SUBSYSTEM:WINDOWS /DYNAMICBASE /NXCOMPAT $libs || exit /b 1
"@
$bat = Join-Path $OutDir "build.cmd"
Set-Content -Path $bat -Value $cmd -Encoding ASCII

Write-Host "Building $exe" -ForegroundColor Cyan
& cmd.exe /c $bat
if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) { throw "Build failed." }
Write-Host "Build OK" -ForegroundColor Green

if ($Publish) {
    & (Join-Path $PSScriptRoot "publish.ps1") -ExePath $exe -InstallDir $InstallDir -PrusaSlicerDir $PrusaSlicerDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
