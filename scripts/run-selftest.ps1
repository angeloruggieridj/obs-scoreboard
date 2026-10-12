<#
.SYNOPSIS
Runs the in-OBS fps selftest of the plugin in a PORTABLE OBS at a given frame rate.

.DESCRIPTION
Unpacks the OBS release under %TEMP%, installs the built plugin, creates a
profile at the requested frame rate, starts OBS with OBS_SCOREBOARD_SELFTEST*
set, waits for the JSON report, then closes OBS through its main window while
the capture is still running (the plugin must stop it cleanly on exit).

With -Reopen, after a first start that passed, OBS is started a second time on
the SAME portable config with OBS_SCOREBOARD_SELFTEST_PASS=reopen: it commands
nothing, it only checks that the source links came back and that the saved match
was rotated without being rewritten. The script then checks that
previous-match.json is byte-identical to the first start's last-match.json and
that last-match.json no longer exists.

Exit 0 = PASS (report says pass and the shutdown was clean; with -Reopen, both
reports say pass and both file checks hold), 1 = FAIL, 2 = no report (OBS did
not start, timeout, crash) or setup problem.

.EXAMPLE
pwsh -File scripts/run-selftest.ps1 -Fps 30

.EXAMPLE
pwsh -File scripts/run-selftest.ps1 -Fps 30 -Reopen

.NOTES
SPDX-License-Identifier: GPL-2.0-or-later
#>
param(
    [Parameter(Mandatory)][ValidateSet(25, 30, 50, 60)][int]$Fps,
    [int]$Seconds = 20,
    [string]$ObsVersion = '32.2.2',
    [string]$Dll,
    [string]$ArtifactDir,
    [ValidatePattern('^\d+x\d+$')][string]$Canvas = '1280x720',
    [switch]$Reopen
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $Dll) { $Dll = Join-Path $repo 'build_x64\RelWithDebInfo\obs-scoreboard.dll' }
if (-not $ArtifactDir) { $ArtifactDir = Join-Path $env:TEMP 'obs-scoreboard-selftest\artifacts' }
$localeSrc = Join-Path $repo 'data\locale'
$fontsSrc = Join-Path $repo 'data\fonts'

foreach ($need in @($Dll, $localeSrc, $fontsSrc)) {
    if (-not (Test-Path -LiteralPath $need)) { Write-Host "missing: $need (build first)"; exit 2 }
}

# One OBS start with the selftest environment: waits for the report, closes OBS
# through its main window while the capture is still running, then examines the
# log and the crash dir. Used for the first start and, with -Reopen, for the
# second one (-Pass reopen), which reuses the very same portable config.
function Invoke-ObsRun {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Report,
        [Parameter(Mandatory)][string]$LogCopy,
        [string]$Pass = ''
    )
    foreach ($old in @($Report, "$Report.tmp", $LogCopy)) {
        Remove-Item -LiteralPath $old -Force -ErrorAction SilentlyContinue
    }

    $env:OBS_SCOREBOARD_SELFTEST = '1'
    $env:OBS_SCOREBOARD_SELFTEST_FPS = "$Fps"
    $env:OBS_SCOREBOARD_SELFTEST_SECS = "$Seconds"
    $env:OBS_SCOREBOARD_SELFTEST_OUT = $Report
    # The live vendor check runs through obs-websocket's call_request: require it, so a portable
    # build that shipped without the bundle fails the selftest instead of silently passing (finding 12).
    $env:OBS_SCOREBOARD_SELFTEST_REQUIRE_VENDOR = '1'
    if ($Pass) { $env:OBS_SCOREBOARD_SELFTEST_PASS = $Pass }
    else { Remove-Item Env:OBS_SCOREBOARD_SELFTEST_PASS -ErrorAction SilentlyContinue }

    $started = Get-Date
    $bin = Join-Path $Root 'bin\64bit'
    $proc = Start-Process -FilePath (Join-Path $bin 'obs64.exe') -WorkingDirectory $bin -PassThru `
        -ArgumentList '--portable', '--multi', '--profile', 'SBSelftest', '--disable-shutdown-check', '--disable-missing-files-check'

    $deadline = $started.AddSeconds($Seconds + 90)
    while ((Get-Date) -lt $deadline -and -not (Test-Path -LiteralPath $Report) -and -not $proc.HasExited) {
        Start-Sleep -Milliseconds 500
    }

    # Closing with the capture still running is intended: the plugin must stop it on exit.
    $closed = $false
    if (-not $proc.HasExited) {
        if (-not (Close-ObsMainWindow -ProcessId $proc.Id)) { $null = $proc.CloseMainWindow() }
        if ($proc.WaitForExit(20000)) { $closed = $true }
        else { Stop-Process -Id $proc.Id -Force; $proc.WaitForExit() }
    }

    $logDir = Join-Path $Root 'config\obs-studio\logs'
    $log = Get-ChildItem -LiteralPath $logDir -Filter '*.txt' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    $text = ''
    if ($log) { Copy-Item -LiteralPath $log.FullName -Destination $LogCopy -Force; $text = Get-Content -LiteralPath $log.FullName -Raw }

    $crashDir = Join-Path $Root 'config\obs-studio\crashes'
    $crashes = @(Get-ChildItem -LiteralPath $crashDir -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $started })
    [pscustomobject]@{
        Report = $Report
        LogCopy = $LogCopy
        HasReport = (Test-Path -LiteralPath $Report)
        Closed = $closed
        CaptureStopped = $text -match [regex]::Escape('[obs-scoreboard] selftest: capture stopped on exit')
        Unloaded = $text -match [regex]::Escape('[obs-scoreboard] unloaded')
        Crashes = $crashes.Count
    }
}

# Any failure before a report can exist (unpack, install, launch) is a setup problem: exit 2.
try {
    Import-Module (Join-Path $PSScriptRoot 'ObsPortable.psm1') -Force

    $base = Join-Path $env:TEMP 'obs-scoreboard-selftest'
    $zip = Get-ObsZip -Version $ObsVersion -CacheDir $base
    $root = Reset-Obs -Zip $zip -Work (Join-Path $base $ObsVersion)
    # The selftest exercises the live vendor through obs-websocket's call_request; the bundle must be
    # in this portable OBS, otherwise -REQUIRE_VENDOR would fail the run (finding 12). The install
    # folder moved across OBS versions, so look for the library anywhere in the tree.
    if (-not (Get-ChildItem -LiteralPath $root -Recurse -Filter 'obs-websocket.dll' -File -ErrorAction SilentlyContinue)) {
        throw "obs-websocket.dll is missing from the portable OBS $ObsVersion (no live vendor to test)"
    }
    Install-Plugin -Root $root -Layout legacy -Dll $Dll -LocaleDir $localeSrc -FontsDir $fontsSrc

    $cw, $ch = $Canvas -split 'x'
    $profileDir = Join-Path $root 'config\obs-studio\basic\profiles\SBSelftest'
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    $ini = "[General]`r`nName=SBSelftest`r`n`r`n[Video]`r`nBaseCX=$cw`r`nBaseCY=$ch`r`nOutputCX=$cw`r`nOutputCY=$ch`r`nFPSType=1`r`nFPSInt=$Fps`r`n"
    [System.IO.File]::WriteAllText((Join-Path $profileDir 'basic.ini'), $ini, (New-Object System.Text.UTF8Encoding($false)))

    New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
    $report = Join-Path $ArtifactDir "selftest-${Fps}fps.json"
    $logCopy = Join-Path $ArtifactDir "obs-${Fps}fps.log"
    $first = Invoke-ObsRun -Root $root -Report $report -LogCopy $logCopy
} catch {
    Write-Host "setup failed: $($_.Exception.Message)"
    exit 2
}

if (-not $first.HasReport) {
    Write-Host "selftest ${Fps} fps: NO REPORT (log: $($first.LogCopy))"
    exit 2
}
$r = Get-Content -LiteralPath $first.Report -Raw | ConvertFrom-Json
Write-Host "selftest ${Fps} fps: report $($first.Report)"
foreach ($c in $r.criteria.PSObject.Properties) { Write-Host ("  criterion {0,-10} {1}" -f $c.Name, $c.Value) }
foreach ($c in $r.survival.PSObject.Properties) { Write-Host ("  survival  {0,-10} {1}" -f $c.Name, $c.Value) }
Write-Host ("  maxLatencyMs {0} (budgetMs {1})" -f $r.maxLatencyMs, $r.budgetMs)
Write-Host ("  frames captured={0} badTimestamps={1} lagged={2} skipped={3}" -f $r.frames.captured, $r.frames.badTimestamps, $r.frames.lagged, $r.frames.skipped)
foreach ($p in $r.problems) { Write-Host "  problem: $p" }
$clean = $first.Closed -and $first.CaptureStopped -and $first.Unloaded -and ($first.Crashes -eq 0)
Write-Host ("  clean shutdown: closed={0} captureStopped={1} unloaded={2} crashes={3}" -f $first.Closed, $first.CaptureStopped, $first.Unloaded, $first.Crashes)

if (-not $Reopen) {
    if ($r.pass -and $clean) { Write-Host 'SELFTEST PASS'; exit 0 }
    Write-Host 'SELFTEST FAIL'
    exit 1
}

# The restart check only makes sense after a first start that passed.
if (-not ($r.pass -and $clean)) {
    Write-Host 'SELFTEST FAIL (first start failed; not restarting)'
    exit 1
}

# The plugin rotates last-match.json to previous-match.json at the next module
# start, before the saver can rewrite it. The first start must have written it.
$pluginConfig = Join-Path $root 'config\obs-studio\plugin_config\obs-scoreboard'
$last = Join-Path $pluginConfig 'last-match.json'
$previous = Join-Path $pluginConfig 'previous-match.json'
if (-not (Test-Path -LiteralPath $last)) {
    Write-Host "selftest ${Fps} fps reopen: FAIL (the first start wrote no last-match.json: $last)"
    Write-Host 'SELFTEST FAIL'
    exit 1
}
$lastHash = (Get-FileHash -LiteralPath $last -Algorithm SHA256).Hash

$reopenReport = Join-Path $ArtifactDir "selftest-${Fps}fps-reopen.json"
$reopenLog = Join-Path $ArtifactDir "obs-${Fps}fps-reopen.log"
try {
    $second = Invoke-ObsRun -Root $root -Report $reopenReport -LogCopy $reopenLog -Pass 'reopen'
} catch {
    Write-Host "reopen setup failed: $($_.Exception.Message)"
    exit 2
}

if (-not $second.HasReport) {
    Write-Host "selftest ${Fps} fps reopen: NO REPORT (log: $($second.LogCopy))"
    exit 2
}
$rr = Get-Content -LiteralPath $reopenReport -Raw | ConvertFrom-Json
Write-Host "selftest ${Fps} fps reopen: report $reopenReport"
foreach ($c in $rr.reopen.PSObject.Properties) {
    if ($c.Name -eq 'problems') { continue }
    Write-Host ("  reopen    {0,-15} {1}" -f $c.Name, $c.Value)
}
foreach ($p in $rr.reopen.problems) { Write-Host "  problem: $p" }

$sameHash = $false
if (Test-Path -LiteralPath $previous) {
    $sameHash = ((Get-FileHash -LiteralPath $previous -Algorithm SHA256).Hash -eq $lastHash)
}
$lastGone = -not (Test-Path -LiteralPath $last)
Write-Host ("  previous-match.json same SHA-256 as last-match.json: {0}" -f $sameHash)
Write-Host ("  last-match.json removed: {0}" -f $lastGone)
Write-Host ("  clean shutdown: closed={0} unloaded={1} crashes={2}" -f $second.Closed, $second.Unloaded, $second.Crashes)
$reopenClean = $second.Closed -and $second.Unloaded -and ($second.Crashes -eq 0)
if ($rr.pass -and $sameHash -and $lastGone -and $reopenClean) { Write-Host 'SELFTEST PASS'; exit 0 }
Write-Host 'SELFTEST FAIL'
exit 1
