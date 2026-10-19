<#
Local convenience for the check push.yaml already makes authoritative in CI
("Check Tag Matches buildspec"): the plugin reports buildspec.json's `version`
+ `prerelease` (see PLUGIN_VERSION_FULL in CMakeLists.txt), the release is named
by the tag, and a release whose tag disagrees is not published. This says so
before a push instead of after one, when a wrong tag is still one command away
from fixed.

Usage:
    pwsh -File scripts/check-tag-version.ps1                 # checks the tag on HEAD
    pwsh -File scripts/check-tag-version.ps1 -Tag 1.0.0      # checks a tag not made yet
#>
param(
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $Tag) {
    try { $Tag = (git -C $repoRoot describe --tags --exact-match 2>$null) } catch { $Tag = $null }
    if (-not $Tag) {
        Write-Host 'HEAD is not exactly at a tag, and none was given with -Tag.' -ForegroundColor Yellow
        Write-Host "Nothing to check yet -- this only matters right before 'git push origin <tag>'."
        exit 0
    }
}

$buildspec = Get-Content (Join-Path $repoRoot 'buildspec.json') -Raw | ConvertFrom-Json
$declared = if ($buildspec.prerelease) { "$($buildspec.version)-$($buildspec.prerelease)" } else { $buildspec.version }

if ($Tag -ne $declared) {
    Write-Host "MISMATCH: tag '$Tag' vs buildspec '$declared'" -ForegroundColor Red
    Write-Host "Update buildspec.json's version/prerelease to match the tag, or retag to match buildspec." -ForegroundColor Red
    exit 1
}

Write-Host "OK: tag and buildspec agree on $declared" -ForegroundColor Green
exit 0
