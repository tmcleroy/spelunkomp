#!/bin/zsh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The recordings Dodge training's patterns are taken from (tools/dodge/extract.py): DoDonPachi played through both of
# its loops and the hidden last boss four times, out of harm, the ship moved about at random (so that a bullet aimed at
# it can be told from one that is not), twice with the laser held and twice with the wide shot (the bosses last
# longer). A line for every bullet made, at the four places the game's bullet routines write a bullet's record.
#   tools/dodge/record.sh ROMS OUT-FOLDER      (about ten minutes; needs build/ddonpach/spelunk-headless)
#   tools/dodge/extract.py work/ddonpach/program.bin work/ddonpach/dodge_patterns.inc OUT-FOLDER/r?.txt
#   LOOKS=1 tools/dodge/record.sh ROMS OUT-FOLDER, and extract.py again
set -e
cd "$(dirname "$0")/../.."
ROMS=${1:?usage: tools/dodge/record.sh ROMS OUT-FOLDER}
OUT=${2:?usage: tools/dodge/record.sh ROMS OUT-FOLDER}
mkdir -p $OUT
# A second step (LOOKS=1, after extract.py has been run once: it writes OUT-FOLDER/r?.txt.frames, the frames its
# patterns' bursts begin in): the same four runs again, which play out the same, with what each of the game's tasks
# put on the sprite list in those frames (the main loop's place after a task, 000ACC), each sprite with its colours
# as the palette had them, to OUT-FOLDER/r?.txt.looks.
# extract.py run again then gives each pattern the look of what fires it.
for run in 1 2 3 4; do
    fire=$([ $((run % 2)) = 1 ] && echo 0x10 || echo 0x40)
    # (left and right mostly, each for 15 to 80 frames; bits as machine/inputs.h)
    moves=($(python3 -c "
import random
r = random.Random($run)
f = 4700
while f < 260000:
    n = r.randint(15, 80)
    b = r.choice([4, 8, 4, 8, 4, 8, 4, 8, 1, 2, 2, 0])
    if b:
        print('--press %d:%d:%d' % (f, n, b))
    f += n
"))
    play=(--roms "$ROMS" --cpu blocks --frames 240000 --system 4000:6:4 --system 4100:6:0x10 --press 4300:6:0x10 --press 4400:6:0x10
          --press 4500:300000:$fire --poke 4600:0x102CCA:0xEF:1 $moves)
    if [ -n "${LOOKS:-}" ]; then
        SPELUNK_TRACE_AT=ACC SPELUNK_TRACE_FRAMES=$OUT/r$run.txt.frames SPELUNK_TRACE_SPRITES=101216 SPELUNK_TRACE_PALETTE=C00000:512 \
            ./build/ddonpach/spelunk-headless $play > $OUT/r$run.txt.looks &
    else
        # (the registers at each place, then: the object's task's longs at +4 and +0xC, its body's at 0 and +6, the
        # ship's place, the stage and loop, the stage's count, the boss's word)
        SPELUNK_TRACE_OBJECT=1 SPELUNK_TRACE_AT=50E62,5101A,54EA2,54F74,2C84 SPELUNK_TRACE_MEM=102C92,101978,10197A,10198E,10E87A \
            ./build/ddonpach/spelunk-headless $play > $OUT/r$run.txt &
    fi
done
wait
