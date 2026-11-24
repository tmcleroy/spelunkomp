#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# One run of the shadow check (tools/shadow/full.sh starts these, several at once): a CV1000 set played the same way by
# its shadow build (the timing model in use beside its reference, compared at every event: machine/bus_timing_shadow.h)
# and by the reference build (origin/main, or REF), and the two builds' per-frame hash logs compared.
#   run_one.sh OUT SET KIND [ARG]
#     KIND recipe-native, recipe-blocks (the check's recipe with the board's slowdown, translated and interpreted),
#          recipe-off (the slowdown off), boss (to the first stage's boss, then BOSS_FRAMES there), stage ARG (begun at
#          stage ARG, STAGE_FRAMES), auto (made-up play, seed 99, AUTO_FRAMES)
# Writes OUT/results/SET-KIND[-ARG].txt: one line "SET RUN FRAMES pass|FAIL first-difference".
set -u
OUT=$1 SET=$2 KIND=$3 ARG=${4:-}
ROMS=${ROMS:?ROMS: the folder of the ROM sets}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CFG=$ROOT/games/$SET/game.cfg
SHADOW=$OUT/bin/$SET-shadow
REF=$OUT/bin/$SET-ref
NAME=$KIND${ARG:+-$ARG}
LOGS=$OUT/logs/$SET
mkdir -p "$LOGS" "$OUT/results" "$OUT/states"
RESULT=$OUT/results/$SET-$NAME.txt

# The check's way into play (tools/check.sh's), and the shot held on from there for HOLD frames
recipe() {
    case $SET in
    mushisam) echo "--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4700:$1:0x11" ;;
    *) echo "--system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 --press 4700:5:0x10 --press 4800:5:0x10 --press 4900:5:0x10 --press 5000:5:0x10 --press 5200:$1:0x11" ;;
    esac
}
# Both builds the same way; then the shadow's own word and the two hash logs. $1: the frames, then the arguments
both() {
    frames=$1
    shift
    "$SHADOW" --game "$CFG" --roms "$ROMS" --frames "$frames" --hash-log "$LOGS/$NAME.shadow.txt" "$@" > "$LOGS/$NAME.shadow.out" 2>&1
    shadowStatus=$?
    "$REF" --game "$CFG" --roms "$ROMS" --frames "$frames" --hash-log "$LOGS/$NAME.ref.txt" "$@" > "$LOGS/$NAME.ref.out" 2>&1
    refStatus=$?
    ran=$(wc -l < "$LOGS/$NAME.shadow.txt" | tr -d ' ')
    if grep -q '^shadow: MISMATCH' "$LOGS/$NAME.shadow.out"; then
        echo "$SET $NAME $ran FAIL $(grep -m1 '^shadow: MISMATCH' "$LOGS/$NAME.shadow.out" | cut -c9-)" > "$RESULT"
    elif [ $shadowStatus != 0 ] || [ $refStatus != 0 ]; then
        echo "$SET $NAME $ran FAIL the shadow build ended with $shadowStatus, the reference with $refStatus" > "$RESULT"
    elif ! grep -q '^shadow: .* all the same' "$LOGS/$NAME.shadow.out"; then
        echo "$SET $NAME $ran FAIL the shadow build did not say it compared (not built with SHADOW_TIMING?)" > "$RESULT"
    elif ! cmp -s "$LOGS/$NAME.shadow.txt" "$LOGS/$NAME.ref.txt"; then
        line=$(cmp "$LOGS/$NAME.shadow.txt" "$LOGS/$NAME.ref.txt" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
        echo "$SET $NAME $ran FAIL the hash logs differ from frame $(sed -n "${line:-1}p" "$LOGS/$NAME.shadow.txt" | cut -d' ' -f1)" > "$RESULT"
    else
        echo "$SET $NAME $ran pass -" > "$RESULT"
        # (a run that passed keeps its word, not its logs: they are large)
        rm -f "$LOGS/$NAME.shadow.txt" "$LOGS/$NAME.ref.txt"
    fi
}

RECIPE_FRAMES=${RECIPE_FRAMES:-6000}
case $KIND in
recipe-native) both "$RECIPE_FRAMES" --cpu native --timing $(recipe 800) ;;
recipe-blocks) both "$RECIPE_FRAMES" --cpu blocks --timing $(recipe 800) ;;
recipe-off) both "$RECIPE_FRAMES" --cpu native $(recipe 800) ;;
boss)
    # (the state as the game arrives at the first stage's boss, made here from the ROM set: to_boss and out of harm,
    # the trip run unseen as the game program runs it)
    state=$OUT/states/$SET-boss.state
    "$SHADOW" --game "$CFG" --roms "$ROMS" --frames 80000 --timing --features to_boss=1,out_of_harm=1 --save-on-arrival "$state" \
        $(recipe 80000) > "$LOGS/arrival.out" 2>&1
    at=$(sed -n 's/^arrival: frame \([0-9]*\).*/\1/p' "$LOGS/arrival.out")
    if [ -z "$at" ] || [ ! -s "$state" ]; then
        if grep -q '^shadow: MISMATCH' "$LOGS/arrival.out"; then
            echo "$SET boss - FAIL on the way: $(grep -m1 '^shadow: MISMATCH' "$LOGS/arrival.out" | cut -c9-)" > "$RESULT"
        else
            echo "$SET boss - FAIL the trip to the boss did not arrive (logs/$SET/arrival.out)" > "$RESULT"
        fi
        exit 0
    fi
    both $((at + ${BOSS_FRAMES:-3000})) --timing --load-state "$state" --features out_of_harm=1 --press 0:1000000:0x10
    ;;
stage) both "${STAGE_FRAMES:-12000}" --timing --features start_stage="$ARG",out_of_harm=1 $(recipe 1000000) ;;
auto) both "${AUTO_FRAMES:-200000}" --timing --auto 99 ;;
*) echo "$SET $NAME - FAIL no such run" > "$RESULT" ;;
esac
exit 0
