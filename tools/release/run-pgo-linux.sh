#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#
# The Linux release's profile-guided optimisation, its training (tools/release/linux.sh, PGO_GENERATE=1, builds the
# programs instrumented, into Spelunkomp-linux-<processor>-pgo-train.tar.gz with this script): run on the machine the
# release is for (a Steam Deck or Steam Machine for x86_64), from the unpacked Spelunkomp folder, it plays each game
# the ways a player's time goes, as fast as it goes, with no sound:
#   - begun at stage 1 and played there, the slowdown on, with the GPU renderer and with the software one;
#   - the same with run-ahead (1 frame);
#   - taken straight to stage 1's boss and played there (where the game has that), the slowdown on and off.
# Every run's counts (.profraw, clang's) are written under OUT/raw, and at the end put in OUT/raw.tar.gz with the
# training build's pgo-made.txt (what they were made from), for
# PGO_USE=<that folder, unpacked> tools/release/linux.sh. Nothing is written anywhere but OUT: its own HOME, saves and
# settings; the ROM folder is only read.
#   run-pgo-linux.sh ROMS [GAME...]   (OUT=$HOME/spelunkomp-pgo; PGO_FRAMES=3600, each run's frames after
#                                      the game's title; every game in pgo-games.txt by default)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROMS=${1:?usage: run-pgo-linux.sh ROMS [GAME...]}
shift
OUT=${OUT:-$HOME/spelunkomp-pgo}
FRAMES=${PGO_FRAMES:-3600}
WANT=" $* "
[ -f "$HERE/pgo-games.txt" ] || { echo "no $HERE/pgo-games.txt: run this from the unpacked training build" >&2; exit 1; }
rm -rf "$OUT/raw" "$OUT/home" "$OUT/saves" "$OUT/logs" "$OUT/raw.tar.gz"
mkdir -p "$OUT/raw" "$OUT/home" "$OUT/saves" "$OUT/logs"
# (the game session's display, sound and bus, as Steam has them: the GPU renderer needs the display)
if pid=$(pgrep -x steam | head -1) && [ -n "$pid" ]; then
    for v in DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR DBUS_SESSION_BUS_ADDRESS XDG_SESSION_TYPE PULSE_SERVER; do
        val=$(tr '\0' '\n' < /proc/$pid/environ | grep "^$v=" | cut -d= -f2-)
        [ -n "$val" ] && export "$v=$val"
    done
fi
unset SteamGameId SteamAppId
# (each run's counts in a file of its own: the program and its process)
export LLVM_PROFILE_FILE="$OUT/raw/%m-%p.profraw"
start=$(date +%s)
played=0
while read -r game set title; do
    [ "$WANT" = "  " ] || [[ "$WANT" == *" $game "* ]] || continue
    if [ ! -e "$ROMS/$set.zip" ]; then
        echo "$game: no $set.zip in $ROMS, not played"
        continue
    fi
    t0=$(date +%s)
    features=$(LLVM_PROFILE_FILE=/dev/null "$HERE/$game" --list-features 2>/dev/null)
    frames=$(( ${title:-3000} + FRAMES ))
    bad=""
    play() { # name, then the program's arguments
        name=$1
        shift
        mkdir -p "$OUT/saves/$game-$name"
        (cd "$HERE" && HOME="$OUT/home" "./$game" --roms "$ROMS" --saves "$OUT/saves/$game-$name" \
            --settings "$OUT/saves/$game-$name/settings.ini" --mute --frames "$frames" --begin-now 5 --press 0:1000000:0x10 \
            "$@" > "$OUT/logs/$game-$name.log" 2>&1) || bad="$bad $name"
    }
    play gpu --features out_of_harm=1
    play software --software --features out_of_harm=1
    play ahead --features out_of_harm=1,run_ahead=1
    if printf '%s\n' "$features" | grep -q '^to_boss'; then
        play boss --features to_boss=1,out_of_harm=1
        printf '%s\n' "$features" | grep -q '^slowdown' && play boss-fast --features to_boss=1,out_of_harm=1,slowdown=1
    fi
    played=$((played + 1))
    echo "$game: played in $(( $(date +%s) - t0 )) s${bad:+ (ended badly:$bad: $OUT/logs)}"
done < "$HERE/pgo-games.txt"
# (what the counts were made from, which linux.sh's PGO_USE reads: the training build's pgo-made.txt, and this run)
{ cat "$HERE/pgo-made.txt" 2>/dev/null || echo "commit unknown (no pgo-made.txt in $HERE)"
  echo "played $(date -u +%Y-%m-%dT%H:%MZ): $played games"; } > "$OUT/raw/pgo-made.txt"
tar -czf "$OUT/raw.tar.gz" -C "$OUT/raw" .
echo "$played games in $(( $(date +%s) - start )) s; $(find "$OUT/raw" -name '*.profraw' | wc -l) count files in $OUT/raw.tar.gz"
