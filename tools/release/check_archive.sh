#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Fails if a release archive holds anything it must not (ROM files or other game data, a builder's home path) or
# lacks what it must carry (every game's program, the launcher, the licences). A release's game program holds the
# translated code; an archive made with NO_GAME_CODE=1 (CI's) does not, carries NO-GAME-CODE.txt to say so, and
# --no-game-code checks that kind instead, so that neither is taken for the other. Every game (games/*, or
# GAMES="a b") must have its program in it (an Android app's: its library, lib<id>.so).
#   check_archive.sh [--no-game-code] ARCHIVE...
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAMES="${GAMES:-$(cd "$ROOT/games" && for cfg in */game.cfg; do printf '%s ' "${cfg%/game.cfg}"; done)}"
status=0
kind=release
if [ "${1:-}" = "--no-game-code" ]; then kind=nocode; shift; fi
bad() { echo "ARCHIVE FAIL ($archive): $1"; status=1; }
for archive in "$@"; do
    dir="$(mktemp -d)"
    case "$archive" in
        *.zip|*.apk) unzip -q "$archive" -d "$dir" ;;
        *) tar -xzf "$archive" -C "$dir" ;;
    esac
    names="$(cd "$dir" && find . -type f | sort)"
    # (ROM sets and their files: zips, MAME's names for the boards' chips (u4, u23, ...rom, ...bin), saves, NAND and
    # EEPROM images)
    found="$(echo "$names" | grep -iE '\.(zip|7z|rar|bin|rom|u[0-9]+[a-z]?|nv|nand|eep|eeprom|state|sav|chd)$|/u[0-9]+$' || true)"
    [ -z "$found" ] || bad "game or data files: $found"
    for need in LICENSE.txt THIRD_PARTY_NOTICES.md README.md SOURCE.txt licenses/SDL3.txt licenses/DearImGui.txt \
        licenses/Inter-OFL.txt licenses/flag-icons-MIT.txt licenses/librashader-MPL-2.0.txt; do
        echo "$names" | grep -q "/$need\$" || bad "missing $need"
    done
    if echo "$names" | grep -q '/NO-GAME-CODE.txt$'; then
        [ "$kind" = nocode ] || bad "built with no translated code (NO-GAME-CODE.txt: a CI build, not a release)"
    else
        [ "$kind" = release ] || bad "no NO-GAME-CODE.txt: a release's programs, with translated code"
    fi
    for game in $GAMES; do
        find "$dir" -type f \( -name "$game" -o -name "$game.exe" -o -name "lib$game.so" \) | grep -q . || bad "no program for $game"
    done
    find "$dir" -type f \( -name 'spelunkomp-launcher' -o -name 'spelunkomp-launcher.exe' -o -name 'libspelunkomp-launcher.so' \) | grep -q . || bad "no launcher"
    find "$dir" -type f -name 'spelunk-headless*' | grep -q . && bad "a check's program (spelunk-headless) inside"
    # (in the programs and libraries: the texts beside them may name example folders. llvm-mingw's libc++abi names
    # its source files where its release was built, GitHub's runner, which is no one's home)
    paths="$(cd "$dir" && for f in $(grep -rlaE --exclude='*.md' --exclude='*.txt' '/Users/[a-z]|/home/[a-z]+/|C:\\Users\\' . || true); do
        grep -aoE '/Users/[a-z][^[:cntrl:]]*|/home/[a-z]+/[^[:cntrl:]]*|C:\\Users\\[^[:cntrl:]]*' "$f" |
            grep -qv '^/home/runner/work/llvm-mingw/' && echo "$f"
    done || true)"
    [ -z "$paths" ] || bad "a builder's home path inside: $paths"
    rm -rf "$dir"
    [ $status -ne 0 ] || echo "ok: $archive"
done
exit $status
