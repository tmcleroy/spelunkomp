#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The gadgets' check (port/gadgets.h): they only read the board. Each game is played (free play, Start, button 1
# through its selects, then fire held with the stick moved) in a wide window, hidden (the software path, whose window picture has the panels), once with the feature "gadgets"
# on and once with it off: the picture of every frame (--picture-log) and the board's state at the end must be the
# same in both. The run with them on leaves window pictures with the panels two thirds of the way and at the end
# (work/<id>/gadgets/on-mid.png, on.png).
#   tools/check_gadgets.sh ROMS [GAME...]   (BUILDS=build: the folder of each game's build, BUILDS/<id>/<id>;
#   FRAMES=8000: how long each run is)
set -e
cd "$(dirname "$0")/.."
ROMS=${1:?usage: tools/check_gadgets.sh ROMS [GAME...]}
shift
GAMES=${*:-$(for cfg in games/*/game.cfg; do basename "$(dirname "$cfg")"; done)}
FRAMES=${FRAMES:-8000}
BUILDS=${BUILDS:-./build}
# (the stick moved left and right, up and down, through the play; fire held)
MOVES=""
f=5000
while [ $f -lt $FRAMES ]; do
    MOVES="$MOVES --press $f:40:0x14 --press $((f + 40)):40:0x19 --press $((f + 80)):40:0x18 --press $((f + 120)):40:0x16"
    f=$((f + 160))
done
PLAY="--system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:100:0x11 $MOVES"
fail=0
for g in $GAMES; do
    [ -x $BUILDS/$g/$g ] || { echo "$g: not built"; continue; }
    out=work/$g/gadgets
    mkdir -p $out
    : > $out/settings.ini
    run() { # name on|off
        cp $out/settings.ini $out/settings-$1.ini
        SPELUNK_HIDDEN_WINDOW=1 SPELUNK_REPORT_STATE=1 $BUILDS/$g/$g --software --roms "$ROMS" --settings $out/settings-$1.ini --mute \
            --window-size 1280x720 --frames $FRAMES --picture-log $out/$1.pictures --window-shot $((FRAMES * 2 / 3)):$out/$1-mid.png --window-shot $FRAMES:$out/$1.png $PLAY \
            --features $2 2>&1 | grep '^state:' > $out/$1.txt || true
    }
    run on free_play,gadgets &
    run off free_play,-gadgets &
    wait
    if [ -s $out/on.txt ] && [ -s $out/on.pictures ] && cmp -s $out/on.txt $out/off.txt && cmp -s $out/on.pictures $out/off.pictures; then
        echo "$g: gadgets on and off: ok ($(wc -l < $out/on.pictures | tr -d ' ') frames the same; $(cut -d' ' -f1-5 $out/on.txt))"
    else
        echo "$g: gadgets on and off: FAIL: $(cat $out/on.txt) / $(cat $out/off.txt)"; fail=1
    fi
done
exit $fail
