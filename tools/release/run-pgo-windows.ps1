# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#
# The Windows release's profile-guided optimisation, its training (tools/release/windows.sh, PGO_GENERATE=1, builds
# the programs instrumented, into Spelunkomp-windows-x64-pgo-train.zip with this script): run on a Windows PC from
# the unpacked Spelunkomp folder, it plays each game the ways a player's time goes, as fast as it goes, with no
# sound, as run-pgo-linux.sh does on Linux:
#   - begun at stage 1 and played there, the slowdown on, with the GPU renderer and with the software one;
#   - the same with run-ahead (1 frame);
#   - taken straight to stage 1's boss and played there (where the game has that), the slowdown on and off.
# The GPU renderer needs the desktop: run it in the signed-in player's session (a scheduled task, not over ssh).
# Every run's counts (.profraw, clang's) are written under OUT\raw, and at the end put in OUT\raw.zip with the
# training build's pgo-made.txt (what they were made from), for PGO_USE=<that folder, unpacked> tools/release/
# windows.sh. Nothing is written anywhere but OUT and the unpacked folder (portable.txt, made here, keeps the
# programs' own things in its user\ folder); the ROM folder is only read.
#   powershell -NoProfile -ExecutionPolicy Bypass -File run-pgo-windows.ps1 -Roms ROMS [-Out OUT] [-Frames N] [GAME...]
#   (OUT: %USERPROFILE%\spelunkomp-pgo; Frames 3600, each run's frames after the game's title; every game in
#   pgo-games.txt by default)
param(
    [Parameter(Mandatory = $true)][string]$Roms,
    [string]$Out = (Join-Path $env:USERPROFILE 'spelunkomp-pgo'),
    [int]$Frames = 3600,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Games = @()
)
$ErrorActionPreference = 'Stop'
$Here = $PSScriptRoot
$list = Join-Path $Here 'pgo-games.txt'
if (-not (Test-Path $list)) { Write-Error "no $list`: run this from the unpacked training build"; exit 1 }
foreach ($d in 'raw', 'saves', 'logs') { Remove-Item -Recurse -Force (Join-Path $Out $d) -ErrorAction SilentlyContinue }
Remove-Item -Force (Join-Path $Out 'raw.zip') -ErrorAction SilentlyContinue
foreach ($d in 'raw', 'saves', 'logs') { New-Item -ItemType Directory -Force (Join-Path $Out $d) | Out-Null }
New-Item -ItemType File -Force (Join-Path $Here 'portable.txt') | Out-Null
$raw = Join-Path $Out 'raw'
$start = Get-Date
$played = 0
$report = Join-Path $Out 'played.txt'
Set-Content $report "training from $Here, ROMs $Roms, $Frames frames after the title"
foreach ($line in Get-Content $list) {
    $game, $set, $title = $line -split ' '
    if ($Games.Count -gt 0 -and $Games -notcontains $game) { continue }
    if (-not (Test-Path (Join-Path $Roms "$set.zip"))) { Add-Content $report "${game}: no $set.zip in $Roms, not played"; continue }
    $t0 = Get-Date
    $exe = Join-Path $Here "$game.exe"
    $env:LLVM_PROFILE_FILE = 'NUL'
    $ErrorActionPreference = 'Continue' # (Windows PowerShell 5 makes a native program's stderr an error)
    $features = & $exe --list-features 2>$null
    $ErrorActionPreference = 'Stop'
    $titleFrames = if ($title) { [int]$title } else { 3000 }
    $total = $titleFrames + $Frames
    $bad = @()
    # (each run's counts in a file of its own: the program and its process)
    $env:LLVM_PROFILE_FILE = Join-Path $raw '%m-%p.profraw'
    $runs = @(
        @('gpu', @('--features', 'out_of_harm=1')),
        @('software', @('--software', '--features', 'out_of_harm=1')),
        @('ahead', @('--features', 'out_of_harm=1,run_ahead=1'))
    )
    if ($features | Where-Object { $_ -match '^to_boss' }) {
        $runs += , @('boss', @('--features', 'to_boss=1,out_of_harm=1'))
        if ($features | Where-Object { $_ -match '^slowdown' }) {
            $runs += , @('boss-fast', @('--features', 'to_boss=1,out_of_harm=1,slowdown=1'))
        }
    }
    foreach ($run in $runs) {
        $name = $run[0]
        $saves = Join-Path $Out "saves\$game-$name"
        New-Item -ItemType Directory -Force $saves | Out-Null
        $log = Join-Path $Out "logs\$game-$name.log"
        $arguments = @('--roms', $Roms, '--saves', $saves, '--settings', (Join-Path $saves 'settings.ini'), '--mute',
                       '--frames', "$total", '--begin-now', '5', '--press', '0:1000000:0x10') + $run[1]
        $p = Start-Process -FilePath $exe -ArgumentList ($arguments | ForEach-Object { '"' + $_ + '"' }) -WorkingDirectory $Here `
             -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$log.out" -RedirectStandardError $log
        if ($p.ExitCode -ne 0) { $bad += $name }
    }
    $played++
    $note = if ($bad.Count) { " (ended badly: $($bad -join ' '): $Out\logs)" } else { '' }
    Add-Content $report "${game}: played in $([int]((Get-Date) - $t0).TotalSeconds) s$note"
}
Remove-Item Env:LLVM_PROFILE_FILE
# (what the counts were made from, which windows.sh's PGO_USE reads: the training build's pgo-made.txt, and this run)
$made = Join-Path $Here 'pgo-made.txt'
$lines = if (Test-Path $made) { Get-Content $made } else { @("commit unknown (no pgo-made.txt in $Here)") }
$lines += "played $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mmZ')): $played games"
Set-Content -Path (Join-Path $raw 'pgo-made.txt') -Value $lines
Compress-Archive -Path (Join-Path $raw '*') -DestinationPath (Join-Path $Out 'raw.zip') -Force
$count = (Get-ChildItem $raw -Filter '*.profraw').Count
Add-Content $report "$played games in $([int]((Get-Date) - $start).TotalSeconds) s; $count count files in $Out\raw.zip"
Get-Content $report
