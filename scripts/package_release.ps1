# SPDX-License-Identifier: AGPL-3.0-or-later
<#
.SYNOPSIS
    Builds the portable OpenSlicer release zip (build\release\OpenSlicer-<Version>-win64.zip).

.DESCRIPTION
    1. Builds OpenPS.exe (build_launcher.ps1).
    2. Installs it with publish.ps1 into a fresh staging folder, which copies the PrusaSlicer runtime from
       -PrusaSlicerDir (must be the official PrusaSlicer 2.9.7 Windows x64 release) and the OpenSlicer resources.
    3. Adds LICENSE and README.txt, then zips the folder and prints its SHA-256.

    The zip runs without installing PrusaSlicer. It redistributes PrusaSlicer binaries under the AGPL-3.0;
    the corresponding sources are this repository at the release tag.
#>
param(
    [string]$Version = "0.2",
    [string]$PrusaSlicerDir = "C:\Program Files\Prusa3D\PrusaSlicer"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Name = "OpenSlicer-$Version-win64"
$OutDir = Join-Path $RepoRoot "build\release"
$Stage = Join-Path $OutDir $Name
$AppDir = Join-Path $Stage "OpenSlicer"
$Zip = Join-Path $OutDir "$Name.zip"

& (Join-Path $PSScriptRoot "build_launcher.ps1")
if ($LASTEXITCODE) { throw "Build failed." }

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
if (Test-Path $Zip) { Remove-Item $Zip -Force }
New-Item -ItemType Directory -Path $AppDir -Force | Out-Null

& (Join-Path $PSScriptRoot "publish.ps1") -InstallDir $AppDir -PrusaSlicerDir $PrusaSlicerDir 3>&1 | Tee-Object -Variable publishOutput
if ($publishOutput | Where-Object { $_ -is [System.Management.Automation.WarningRecord] }) {
    throw "The PrusaSlicer runtime in $PrusaSlicerDir is not the supported build: release not packaged."
}

Copy-Item (Join-Path $RepoRoot "LICENSE") (Join-Path $AppDir "LICENSE.txt")
$commit = (git -C $RepoRoot rev-parse --short HEAD).Trim()
@"
OpenSlicer $Version for Windows x64 (portable)
=================================================

EXPERIMENTAL TEST VERSION. Please report everything that does not work:
https://github.com/technapa/OpenPS/issues

Start: OpenPS.exe. No installation needed; keep the whole folder together.
Settings, profiles and the Prusa Account login are shared with PrusaSlicer
(%APPDATA%\PrusaSlicer). An installed PrusaSlicer keeps working next to it.
For the Sync tab, log in once in the Prusa Connect tab.

Contents: OpenPS.exe (OpenSlicer), the official PrusaSlicer 2.9.7 runtime
(PrusaSlicer.dll, its libraries and resources) and the OpenSlicer resources.

OpenSlicer is a community project, not affiliated with or endorsed by
Prusa Research. Prusa, PrusaSlicer and Prusa Connect are trademarks of
Prusa Research a.s.

License: GNU AGPL v3 (LICENSE.txt). Source code, including the PrusaSlicer
sources this build is based on: https://github.com/technapa/OpenPS
(tag v$Version, commit $commit).

-----------------------------------------------------------------------

OpenSlicer $Version pour Windows x64 (portable)

VERSION DE TEST EXPÉRIMENTALE. Merci de signaler tout ce qui ne va pas :
https://github.com/technapa/OpenPS/issues

Lancement : OpenPS.exe. Aucune installation ; gardez tout le dossier ensemble.
Réglages, profils et connexion au compte Prusa sont partagés avec PrusaSlicer
(%APPDATA%\PrusaSlicer). Un PrusaSlicer installé continue de fonctionner à côté.
Pour l'onglet Sync, connectez-vous une fois dans l'onglet Prusa Connect.

Licence : GNU AGPL v3 (LICENSE.txt). Code source : https://github.com/technapa/OpenPS
"@ | Set-Content -Path (Join-Path $AppDir "README.txt") -Encoding UTF8

# Windows' own tar (bsdtar) writes zip files; call it by path so that a Git/MSYS tar on PATH is not used
& (Join-Path $env:SystemRoot "System32\tar.exe") -a -c -f $Zip -C $Stage OpenSlicer
if ($LASTEXITCODE) { throw "Zip failed." }

$hash = (Get-FileHash $Zip -Algorithm SHA256).Hash
$size = [math]::Round((Get-Item $Zip).Length / 1MB, 1)
Write-Host "Release: $Zip ($size MB)" -ForegroundColor Green
Write-Host "SHA-256: $hash"
