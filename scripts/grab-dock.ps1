<#
.SYNOPSIS
Renders the real ScoreBoard dock to PNG files from a portable OBS (development tool).

.DESCRIPTION
Starts a portable OBS with the plugin and OBS_SCOREBOARD_GRAB set; the plugin renders the dock for
several sports, the dark and a light theme and two densities with QWidget::grab() (no screen
capture, so nothing else on the desktop can end up in the pictures), writes `grab.done` and the
script closes OBS.

    pwsh -File scripts/grab-dock.ps1 -OutDir C:\temp\dock -ObsVersion 32.2.2

.NOTES
SPDX-License-Identifier: GPL-2.0-or-later
#>
param(
    [Parameter(Mandatory)][string]$OutDir,
    [string]$ObsVersion = '32.2.2',
    [string]$Dll,
    [string]$Language = 'en-US',
    [ValidateSet('dark', 'light')][string]$Theme = 'dark',
    [double]$Scale = 1
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'ObsPortable.psm1') -Force
if (-not $Dll) {
    $Dll = (Get-ChildItem -Path (Join-Path $repo 'build_x64') -Recurse -Filter obs-scoreboard.dll |
        Where-Object { $_.FullName -match 'RelWithDebInfo' } | Select-Object -First 1).FullName
}
if (-not $Dll) { throw 'obs-scoreboard.dll not found: build the plugin first' }

$cache = Join-Path $env:TEMP 'obs-scoreboard-selftest'
$zip = Get-ObsZip -Version $ObsVersion -CacheDir $cache
$root = Reset-Obs -Zip $zip -Work (Join-Path $env:TEMP 'obs-scoreboard-grab')
foreach ($f in 'global.ini', 'user.ini') {
    Add-Content -LiteralPath (Join-Path $root "config\obs-studio\$f") -Value "Language=$Language`r`n"
}
if ($Theme -eq 'light') {
    foreach ($f in 'global.ini', 'user.ini') {
        Add-Content -LiteralPath (Join-Path $root "config\obs-studio\$f") -Value "[Appearance]`r`nTheme=com.obsproject.Yami.Light`r`n"
    }
}
Install-Plugin -Root $root -Layout legacy -Dll $Dll -LocaleDir (Join-Path $repo 'data\locale') -FontsDir (Join-Path $repo 'data\fonts')

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Remove-Item -Path (Join-Path $OutDir "*-$Theme*.png"), (Join-Path $OutDir 'grab.done') -Force -ErrorAction SilentlyContinue
$env:OBS_SCOREBOARD_NO_PICKER = '1'
if ($Scale -ne 1) { $env:QT_SCALE_FACTOR = [string]$Scale } else { Remove-Item Env:QT_SCALE_FACTOR -ErrorAction SilentlyContinue }
$env:OBS_SCOREBOARD_GRAB = (Resolve-Path $OutDir).Path
$env:OBS_SCOREBOARD_GRAB_THEME = $Theme
$p = Start-Process -FilePath (Join-Path $root 'bin\64bit\obs64.exe') -WorkingDirectory (Join-Path $root 'bin\64bit') `
    -ArgumentList '--portable', '--disable-shutdown-check' -PassThru
$deadline = (Get-Date).AddSeconds(120)
while (-not (Test-Path (Join-Path $OutDir 'grab.done')) -and (Get-Date) -lt $deadline -and -not $p.HasExited) {
    Start-Sleep -Seconds 2
}
$ok = Test-Path (Join-Path $OutDir 'grab.done')
$null = Close-ObsMainWindow -ProcessId $p.Id
Start-Sleep -Seconds 6
if (-not $p.HasExited) { $p.Kill() }
if (-not $ok) { throw 'the plugin did not finish the screenshots (crash? see the OBS log and crashes folder)' }
Get-ChildItem $OutDir -Filter *.png | ForEach-Object { Write-Host $_.FullName }
