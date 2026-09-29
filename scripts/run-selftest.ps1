<#
.SYNOPSIS
Runs the in-OBS fps selftest of the plugin in a PORTABLE OBS at a given frame rate.

.DESCRIPTION
Unpacks the OBS release under %TEMP%, installs the built plugin, creates a
profile at the requested frame rate, starts OBS with OBS_SCOREBOARD_SELFTEST*
set, waits for the JSON report, then closes OBS through its main window while
the capture is still running (the plugin must stop it cleanly on exit).

Exit 0 = PASS (report says pass and the shutdown was clean), 1 = FAIL,
2 = no report (OBS did not start, timeout, crash) or setup problem.

.EXAMPLE
pwsh -File scripts/run-selftest.ps1 -Fps 30

.NOTES
SPDX-License-Identifier: GPL-2.0-or-later
#>
param(
    [Parameter(Mandatory)][ValidateSet(25, 30, 50, 60)][int]$Fps,
    [int]$Seconds = 20,
    [string]$ObsVersion = '32.2.2',
    [string]$Dll,
    [string]$ArtifactDir
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Dll) { $Dll = Join-Path $repo 'build_x64\RelWithDebInfo\obs-scoreboard.dll' }
if (-not $ArtifactDir) { $ArtifactDir = Join-Path $env:TEMP 'obs-scoreboard-selftest\artifacts' }
$localeSrc = Join-Path $repo 'data\locale'

foreach ($need in @($Dll, $localeSrc)) {
    if (-not (Test-Path -LiteralPath $need)) { Write-Host "missing: $need (build first)"; exit 2 }
}
# Any failure before a report can exist (unpack, install, launch) is a setup problem: exit 2.
try {
    Import-Module (Join-Path $PSScriptRoot 'ObsPortable.psm1') -Force

    $base = Join-Path $env:TEMP 'obs-scoreboard-selftest'
    $zip = Get-ObsZip -Version $ObsVersion -CacheDir $base
    $root = Reset-Obs -Zip $zip -Work (Join-Path $base $ObsVersion)
    Install-Plugin -Root $root -Layout legacy -Dll $Dll -LocaleDir $localeSrc

    $profileDir = Join-Path $root 'config\obs-studio\basic\profiles\SBSelftest'
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    $ini = "[General]`r`nName=SBSelftest`r`n`r`n[Video]`r`nBaseCX=1280`r`nBaseCY=720`r`nOutputCX=1280`r`nOutputCY=720`r`nFPSType=1`r`nFPSInt=$Fps`r`n"
    [System.IO.File]::WriteAllText((Join-Path $profileDir 'basic.ini'), $ini, (New-Object System.Text.UTF8Encoding($false)))

    New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
    $report = Join-Path $ArtifactDir "selftest-${Fps}fps.json"
    $logCopy = Join-Path $ArtifactDir "obs-${Fps}fps.log"
    foreach ($old in @($report, "$report.tmp", $logCopy)) { Remove-Item -LiteralPath $old -Force -ErrorAction SilentlyContinue }

    $env:OBS_SCOREBOARD_SELFTEST = '1'
    $env:OBS_SCOREBOARD_SELFTEST_FPS = "$Fps"
    $env:OBS_SCOREBOARD_SELFTEST_SECS = "$Seconds"
    $env:OBS_SCOREBOARD_SELFTEST_OUT = $report

    $started = Get-Date
    $bin = Join-Path $root 'bin\64bit'
    $proc = Start-Process -FilePath (Join-Path $bin 'obs64.exe') -WorkingDirectory $bin -PassThru `
        -ArgumentList '--portable', '--multi', '--profile', 'SBSelftest', '--disable-shutdown-check', '--disable-missing-files-check'
} catch {
    Write-Host "setup failed: $($_.Exception.Message)"
    exit 2
}

$deadline = $started.AddSeconds($Seconds + 90)
while ((Get-Date) -lt $deadline -and -not (Test-Path -LiteralPath $report) -and -not $proc.HasExited) {
    Start-Sleep -Milliseconds 500
}

# Closing with the capture still running is intended: the plugin must stop it on exit.
$closed = $false
if (-not $proc.HasExited) {
    if (-not (Close-ObsMainWindow -ProcessId $proc.Id)) { $null = $proc.CloseMainWindow() }
    if ($proc.WaitForExit(20000)) { $closed = $true }
    else { Stop-Process -Id $proc.Id -Force; $proc.WaitForExit() }
}

$logDir = Join-Path $root 'config\obs-studio\logs'
$log = Get-ChildItem -LiteralPath $logDir -Filter '*.txt' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime | Select-Object -Last 1
$text = ''
if ($log) { Copy-Item -LiteralPath $log.FullName -Destination $logCopy -Force; $text = Get-Content -LiteralPath $log.FullName -Raw }

$crashDir = Join-Path $root 'config\obs-studio\crashes'
$crashes = @(Get-ChildItem -LiteralPath $crashDir -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $started })
$stopped = $text -match [regex]::Escape('[obs-scoreboard] selftest: capture stopped on exit')
$unloaded = $text -match [regex]::Escape('[obs-scoreboard] unloaded')
$clean = $closed -and $stopped -and $unloaded -and ($crashes.Count -eq 0)

if (-not (Test-Path -LiteralPath $report)) {
    Write-Host "selftest ${Fps} fps: NO REPORT (log: $logCopy)"
    exit 2
}
$r = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
Write-Host "selftest ${Fps} fps: report $report"
foreach ($c in $r.criteria.PSObject.Properties) { Write-Host ("  criterion {0,-10} {1}" -f $c.Name, $c.Value) }
foreach ($c in $r.survival.PSObject.Properties) { Write-Host ("  survival  {0,-10} {1}" -f $c.Name, $c.Value) }
Write-Host ("  maxLatencyMs {0} (budgetMs {1})" -f $r.maxLatencyMs, $r.budgetMs)
Write-Host ("  frames captured={0} badTimestamps={1} lagged={2} skipped={3}" -f $r.frames.captured, $r.frames.badTimestamps, $r.frames.lagged, $r.frames.skipped)
foreach ($p in $r.problems) { Write-Host "  problem: $p" }
Write-Host ("  clean shutdown: closed={0} captureStopped={1} unloaded={2} crashes={3}" -f $closed, $stopped, $unloaded, $crashes.Count)
if ($r.pass -and $clean) { Write-Host 'SELFTEST PASS'; exit 0 }
Write-Host 'SELFTEST FAIL'
exit 1
