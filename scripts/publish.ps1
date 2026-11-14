# SPDX-License-Identifier: AGPL-3.0-or-later
<#
.SYNOPSIS
    Installs OpenSlicer (OpenPS.exe + PrusaSlicer 2.9.7 runtime + OpenSlicer resources) in a folder.

.DESCRIPTION
    1. Closes OpenSlicer if it runs from -InstallDir (like the user would; never forced: if a
       "save changes?" prompt keeps it open, the publication is cancelled).
       Other programs, including an official PrusaSlicer, are never touched.
    2. Copies the PrusaSlicer runtime (PrusaSlicer.dll, dependencies, resources) from
       -PrusaSlicerDir when it is not installed yet.
    3. Overlays the OpenSlicer resources of this repository (icons, splash screen, French catalog).
    4. Copies OpenPS.exe.

.PARAMETER ExePath
    OpenPS.exe to install (default build/openslicer/OpenPS.exe, see build_launcher.ps1).
#>
param(
    [string]$ExePath = "",
    [string]$InstallDir = "C:\App\OpenPS",
    [string]$PrusaSlicerDir = "C:\Program Files\Prusa3D\PrusaSlicer"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ExePath) { $ExePath = Join-Path $RepoRoot "build\openslicer\OpenPS.exe" }
if (-not (Test-Path $ExePath)) { throw "OpenPS.exe not found: $ExePath (run scripts\build_launcher.ps1 first)." }

# PE identity of the PrusaSlicer.dll build the launcher was written for (see PrusaSlicerDll.hpp)
$SupportedDll = @{ TimeStamp = 0x6AE86654; SizeOfImage = 0x05C9F000; Version = "2.9.7" }

function Get-PeIdentity([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($bytes, 0x3C)
    return @{ TimeStamp = [BitConverter]::ToUInt32($bytes, $pe + 8); SizeOfImage = [BitConverter]::ToUInt32($bytes, $pe + 24 + 56) }
}

function Close-PublishedInstances([string]$Folder) {
    $root = [System.IO.Path]::GetFullPath($Folder).TrimEnd('\') + '\'
    $procs = @(Get-Process -Name "OpenPS", "prusa-slicer", "prusa-gcodeviewer" -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase) })
    $stillRunning = @()
    foreach ($p in $procs) {
        Write-Host ("Closing {0} ({1})" -f $p.ProcessName, $p.Id)
        if ($p.CloseMainWindow()) { $null = $p.WaitForExit(10000) }
        if (-not $p.HasExited) { $stillRunning += $p }
    }
    if ($stillRunning.Count -gt 0) {
        $names = ($stillRunning | ForEach-Object { "{0} ({1})" -f $_.ProcessName, $_.Id }) -join ", "
        Write-Error "Publication cancelled: $names is still open (unsaved project?). Close it and publish again."
        exit 1
    }
    if ($procs.Count -gt 0) { Start-Sleep -Milliseconds 300 }
}

Close-PublishedInstances $InstallDir
New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

# PrusaSlicer runtime, copied once
$runtime = @(
    "PrusaSlicer.dll", "OCCTWrapper.dll", "WebView2Loader.dll", "libgmp-10.dll", "libmpfr-4.dll",
    "msvcp140.dll", "msvcp140_codecvt_ids.dll", "vcruntime140.dll", "vcruntime140_1.dll",
    "api-ms-win-crt-runtime-l1-1-0.dll"
)
foreach ($file in $runtime) {
    $src = Join-Path $PrusaSlicerDir $file
    $dst = Join-Path $InstallDir $file
    if ((Test-Path $src) -and -not (Test-Path $dst)) { Copy-Item $src $dst -Force }
}
foreach ($dir in "mesa", "resources") {
    $src = Join-Path $PrusaSlicerDir $dir
    if ((Test-Path $src) -and -not (Test-Path (Join-Path $InstallDir $dir))) { Copy-Item $src $InstallDir -Recurse -Force }
}

$dll = Join-Path $InstallDir "PrusaSlicer.dll"
if (-not (Test-Path $dll)) { throw "PrusaSlicer.dll is missing: install PrusaSlicer $($SupportedDll.Version) or pass -PrusaSlicerDir." }
$id = Get-PeIdentity $dll
if ($id.TimeStamp -ne $SupportedDll.TimeStamp -or $id.SizeOfImage -ne $SupportedDll.SizeOfImage) {
    Write-Warning "PrusaSlicer.dll is not the PrusaSlicer $($SupportedDll.Version) Windows x64 release: the ViewCube and 'Set origin' will be disabled."
}

# OpenSlicer resources
$res = Join-Path $InstallDir "resources"
New-Item -ItemType Directory -Path (Join-Path $res "icons") -Force | Out-Null
Copy-Item -Path (Join-Path $RepoRoot "resources\icons\*") -Destination (Join-Path $res "icons") -Recurse -Force
New-Item -ItemType Directory -Path (Join-Path $res "localization\fr") -Force | Out-Null
Copy-Item -Path (Join-Path $RepoRoot "resources\localization\fr\PrusaSlicer.mo") -Destination (Join-Path $res "localization\fr") -Force

Copy-Item -Path $ExePath -Destination (Join-Path $InstallDir "OpenPS.exe") -Force
Write-Host "OpenSlicer installed in $InstallDir" -ForegroundColor Green
