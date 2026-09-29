<#
.SYNOPSIS
Helpers to run a PORTABLE copy of an OBS Studio release on Windows, shared by
check-obs-load.ps1 and run-selftest.ps1.

.DESCRIPTION
Get-ObsZip           downloads (or finds in the cache) the Windows x64 zip of a release
Reset-Obs            unpacks it fresh, in portable mode, with a quiet config
Install-Plugin       copies the plugin in the "new" and/or "legacy" folder layout
Close-ObsMainWindow  posts WM_CLOSE to the OBS main window of a process

.NOTES
SPDX-License-Identifier: GPL-2.0-or-later
#>

# CloseMainWindow() lets Windows pick the "main" window, and on a fresh config
# that is our dock: it starts floating, and WM_CLOSE on a floating dock hides it
# and nothing else -- OBS runs on, gets killed, and never writes its shutdown.
# So the window is picked by title ("OBS <version> ..."), among this process's.
if (-not ('LoadCheck.Win' -as [type])) {
    Add-Type -Namespace LoadCheck -Name Win -MemberDefinition @'
public delegate bool EnumProc(System.IntPtr hwnd, System.IntPtr lp);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, System.IntPtr lp);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(System.IntPtr hwnd, out uint pid);
[System.Runtime.InteropServices.DllImport("user32.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)] public static extern int GetWindowText(System.IntPtr hwnd, System.Text.StringBuilder s, int n);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern bool PostMessage(System.IntPtr hwnd, uint msg, System.IntPtr w, System.IntPtr l);
'@
}

function Get-ObsZip {
    param([Parameter(Mandatory)][string]$Version, [Parameter(Mandatory)][string]$CacheDir)
    New-Item -ItemType Directory -Force -Path $CacheDir | Out-Null
    $assets = (gh release view $Version --repo obsproject/obs-studio --json assets | ConvertFrom-Json).assets.name
    if (-not $assets) { throw "could not list the assets of OBS $Version (is gh authenticated?)" }
    # OBS 30.0.0 ships its zip as "OBS-Studio-30.0.zip" (no patch number), so
    # "X.Y.0" is also tried as "X.Y".
    $names = @($Version)
    if ($Version -match '^(\d+\.\d+)\.0$') { $names += $Matches[1] }
    $name = $null
    foreach ($n in $names) {
        $v = [regex]::Escape($n)
        $name = $assets | Where-Object { $_ -match "^OBS-Studio-$v(-Windows)?(-x64)?(-Full)?\.zip$" } | Select-Object -First 1
        if ($name) { break }
    }
    if (-not $name) {
        $name = $assets | Where-Object { $_ -like "OBS-Studio-$Version*.zip" -and $_ -notmatch 'PDB|arm64|ARM64|Installer|Debug|Symbols' } |
            Select-Object -First 1
    }
    if (-not $name) { throw "no Windows x64 zip among the OBS $Version assets: $($assets -join ', ')" }
    $zip = Join-Path $CacheDir $name
    if (-not (Test-Path -LiteralPath $zip)) {
        gh release download $Version --repo obsproject/obs-studio --pattern $name --dir $CacheDir
        if ($LASTEXITCODE -ne 0) { throw "could not download $name" }
    }
    return $zip
}

function Reset-Obs {
    param([Parameter(Mandatory)][string]$Zip, [Parameter(Mandatory)][string]$Work)
    if (Test-Path -LiteralPath $Work) { Remove-Item -LiteralPath $Work -Recurse -Force }
    Expand-Archive -LiteralPath $Zip -DestinationPath $Work -Force
    # A zip that wraps everything in one folder is unwrapped, so $Work is the
    # OBS root (the folder holding bin\64bit\obs64.exe) either way.
    $exe = Get-ChildItem -LiteralPath $Work -Recurse -Filter 'obs64.exe' | Select-Object -First 1
    if (-not $exe) { throw "no obs64.exe in $Zip" }
    $root = $exe.Directory.Parent.Parent.FullName
    New-Item -ItemType File -Force -Path (Join-Path $root 'portable_mode.txt') | Out-Null

    # A config of its own: no first-run wizard and no update check (either
    # opens a modal -- a beta offers the newest stable -- and a modal disables
    # the main window, so the close request is refused and OBS gets killed).
    # FirstRun=true means "the first run has HAPPENED" (OBSBasic.cpp opens the
    # wizard when it is false). Which file holds the keys depends on the OBS
    # version: OBS 30 reads them from global.ini, OBS 31+ splits the settings
    # and reads them from user.ini. Both keys go into both files, which is
    # harmless: each version ignores what it does not read. NOT LastVersion: a
    # made-up one makes OBS try to migrate the "old" global config and stop on
    # "Unable to migrate global configuration".
    $cfg = Join-Path $root 'config\obs-studio'
    New-Item -ItemType Directory -Force -Path $cfg | Out-Null
    foreach ($file in @('global.ini', 'user.ini')) {
        Set-Content -LiteralPath (Join-Path $cfg $file) -Value "[General]`r`nFirstRun=true`r`nEnableAutoUpdates=false`r`n" -Encoding UTF8
    }
    return $root
}

function Install-Plugin {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][ValidateSet('new', 'legacy', 'both')][string]$Layout,
        [Parameter(Mandatory)][string]$Dll,
        [Parameter(Mandatory)][string]$LocaleDir
    )
    if ($Layout -in @('new', 'both')) {
        $dir = Join-Path $Root 'plugins\obs-scoreboard'
        New-Item -ItemType Directory -Force -Path (Join-Path $dir 'data') | Out-Null
        Copy-Item -LiteralPath $Dll -Destination $dir -Force
        Copy-Item -LiteralPath $LocaleDir -Destination (Join-Path $dir 'data') -Recurse -Force
    }
    if ($Layout -in @('legacy', 'both')) {
        $bin = Join-Path $Root 'obs-plugins\64bit'
        $data = Join-Path $Root 'data\obs-plugins\obs-scoreboard'
        New-Item -ItemType Directory -Force -Path $bin, $data | Out-Null
        Copy-Item -LiteralPath $Dll -Destination $bin -Force
        Copy-Item -LiteralPath $LocaleDir -Destination $data -Recurse -Force
    }
}

function Close-ObsMainWindow {
    param([Parameter(Mandatory)][int]$ProcessId)
    $cb = [LoadCheck.Win+EnumProc] {
        param($hwnd, $lp)
        [uint32]$owner = 0
        $null = [LoadCheck.Win]::GetWindowThreadProcessId($hwnd, [ref]$owner)
        if ($owner -eq $ProcessId) {
            $sb = New-Object System.Text.StringBuilder 512
            $null = [LoadCheck.Win]::GetWindowText($hwnd, $sb, 512)
            if ($sb.ToString() -like 'OBS *') { $script:found = $hwnd; return $false }
        }
        return $true
    }
    $script:found = [IntPtr]::Zero
    $null = [LoadCheck.Win]::EnumWindows($cb, [IntPtr]::Zero)
    if ($script:found -eq [IntPtr]::Zero) { return $false }
    return [LoadCheck.Win]::PostMessage($script:found, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) # WM_CLOSE
}

Export-ModuleMember -Function Get-ObsZip, Reset-Obs, Install-Plugin, Close-ObsMainWindow
