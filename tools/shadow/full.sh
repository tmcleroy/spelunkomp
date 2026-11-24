#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The shadow check of the CV1000's timing model (docs/SLOWDOWN.md, "What it costs"): every CV1000 set played many
# ways by two builds of it.
#   - The shadow build (-DSHADOW_TIMING=ON) runs the model in use and its reference (machine/bus_timing_ref.h, the
#     model as it was before its hot path changed) side by side. At every event it compares the cycles each gives
#     and the model's state; the first difference stops the run with both states and the last 64 events.
#   - A reference build of REF (origin/main) plays the same, and the two builds' per-frame hash logs must be
#     byte-identical.
# It builds what it needs, runs JOBS at a time, prints a table (set, run, frames, pass or FAIL, the first difference)
# and exits non-zero on any difference.
#
#   tools/shadow/full.sh ROMS [OUT]   (ROMS: the folder of the ROM sets, <id>.zip; OUT: build/shadow)
#   env: JOBS=8 (runs at once), TIER=full or quick (quick: the check's recipe with and without the slowdown, and the
#        boss; full adds every stage begun, made-up play, the training tools and online rollback),
#        SETS="mmpork ddpsdoj" (default: every CV1000 set), WORK_DIR=work (each set's translated code, <id>/gen),
#        REF=origin/main, AUTO_FRAMES=200000, STAGE_FRAMES=12000, BOSS_FRAMES (quick 3000, full 10000),
#        NICE=10, KEEP_BUILDS=1 (keep each build folder: about 150 MB each)
# About 400,000 frames a set in the full tier: one night on 8 cores for all 18 sets. tools/shadow/quick.sh is the
# quick tier.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
ROMS=${1:?usage: tools/shadow/full.sh ROMS [OUT]}
ROMS=$(cd "$ROMS" && pwd)
OUT=${2:-$ROOT/build/shadow}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
JOBS=${JOBS:-8}
TIER=${TIER:-full}
NICE=${NICE:-10}
REF=${REF:-origin/main}
WORK_DIR=${WORK_DIR:-$ROOT/work}
WORK_DIR=$(cd "$WORK_DIR" 2>/dev/null && pwd || echo "$WORK_DIR")
case $TIER in full) BOSS_FRAMES=${BOSS_FRAMES:-10000} ;; quick) BOSS_FRAMES=${BOSS_FRAMES:-3000} ;; *) echo "TIER is full or quick"; exit 2 ;; esac
export ROMS BOSS_FRAMES
[ -n "${SETS:-}" ] || SETS=$(cd "$ROOT/games" && for cfg in */game.cfg; do grep -q '^hardware' "$cfg" || printf '%s ' "${cfg%/game.cfg}"; done)
rm -rf "$OUT/results"
mkdir -p "$OUT/bin" "$OUT/results" "$OUT/logs"
echo "shadow check ($TIER tier): $SETS"
echo "against $REF; $JOBS at a time; out: $OUT"

# ---- The builds: each set's shadow build of this tree, and its build of REF (a worktree of it, made here) ----
REFSRC=$OUT/ref-src
if [ ! -d "$REFSRC" ]; then
    git -C "$ROOT" worktree add --detach "$REFSRC" "$REF" > /dev/null 2>&1 || { echo "error: cannot check out $REF into $REFSRC"; exit 2; }
fi
git -C "$REFSRC" checkout --detach -q "$REF" || { echo "error: $REF"; exit 2; }
build() { # SOURCE DIR TARGETS... (the rest: CMake's arguments after --)
    src=$1 dir=$2
    shift 2
    targets=""
    while [ $# -gt 0 ] && [ "$1" != -- ]; do targets="$targets $1"; shift; done
    [ $# -gt 0 ] && shift
    nice -n "$NICE" cmake -S "$src" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWORK_DIR="$WORK_DIR" "$@" > "$dir.cmake.log" 2>&1 &&
        nice -n "$NICE" cmake --build "$dir" --target $targets > "$dir.build.log" 2>&1
}
for set in $SETS; do
    if [ ! -x "$OUT/bin/$set-shadow" ] || [ ! -x "$OUT/bin/$set-ref" ] || { [ "$TIER" = full ] && [ ! -x "$OUT/bin/$set-game" ]; }; then
        echo "building $set"
        mkdir -p "$OUT/build"
        targets=spelunk-headless
        [ "$TIER" = full ] && targets="spelunk-headless game"
        build "$ROOT" "$OUT/build/$set-shadow" $targets -- -DGAME="$set" -DSHADOW_TIMING=ON || { echo "error: the shadow build of $set (build/$set-shadow.build.log)"; exit 2; }
        build "$REFSRC" "$OUT/build/$set-ref" spelunk-headless -- -DGAME="$set" || { echo "error: the reference build of $set"; exit 2; }
        cp "$OUT/build/$set-shadow/spelunk-headless" "$OUT/bin/$set-shadow"
        cp "$OUT/build/$set-ref/spelunk-headless" "$OUT/bin/$set-ref"
        if [ "$TIER" = full ]; then
            mkdir -p "$OUT/games/$set"
            cp "$OUT/build/$set-shadow/$set" "$OUT/games/$set/$set"
            cp "$OUT/build/$set-shadow/$set" "$OUT/bin/$set-game"
        fi
        [ "${KEEP_BUILDS:-}" = 1 ] || rm -rf "$OUT/build/$set-shadow" "$OUT/build/$set-ref"
    fi
done

# ---- The runs ----
JOBLIST=$OUT/jobs.txt
: > "$JOBLIST"
for set in $SETS; do
    for kind in recipe-native recipe-blocks recipe-off boss; do echo "$set $kind" >> "$JOBLIST"; done
    if [ "$TIER" = full ]; then
        stages=$("$OUT/bin/$set-shadow" --game "$ROOT/games/$set/game.cfg" --roms "$ROMS" --features list 2>/dev/null | sed -n 's/^start_stage \([0-9]*\)$/\1/p')
        s=2
        while [ -n "$stages" ] && [ "$s" -lt "$stages" ]; do echo "$set stage $s" >> "$JOBLIST"; s=$((s + 1)); done
        echo "$set auto" >> "$JOBLIST"
    fi
done
echo "$(wc -l < "$JOBLIST" | tr -d ' ') runs"
# (each line "SET KIND [ARG]" as run_one.sh's arguments, JOBS at a time)
tr '\n' '\0' < "$JOBLIST" | xargs -0 -P "$JOBS" -I{} sh -c 'nice -n "$1" sh "$2" "$3" $4' sh "$NICE" "$ROOT/tools/shadow/run_one.sh" "$OUT" {}

# ---- The full tier's runs of the game program (its shadow build): the training tools, and online rollback ----
if [ "$TIER" = full ]; then
    for set in $SETS; do
        out=$(cd "$ROOT" && BUILDS="$OUT/games" tools/check_tools.sh "$ROMS" "$set" 2>&1)
        if echo "$out" | grep -q FAIL; then
            echo "$set tools - FAIL $(echo "$out" | grep -m1 FAIL)" > "$OUT/results/$set-tools.txt"
        else
            echo "$set tools - pass -" > "$OUT/results/$set-tools.txt"
        fi
        log=$OUT/logs/$set/rollback.out
        SPELUNK_HIDDEN_WINDOW=1 "$OUT/bin/$set-game" --roms "$ROMS" --mute --frames 6000 --rollback-test --features slowdown=0 \
            --system 4000:5:4 --system 4100:5:0x10 --press 4300:5:0x10 --press 4400:5:0x10 --press 4500:5:0x10 --press 4600:5:0x10 \
            --press 5200:800:0x11 > "$log" 2>&1
        status=$?
        if grep -q '^shadow: MISMATCH' "$log" || [ $status != 0 ]; then
            echo "$set rollback 6000 FAIL $(grep -m1 '^shadow: MISMATCH' "$log" | cut -c9-) (status $status)" > "$OUT/results/$set-rollback.txt"
        else
            echo "$set rollback 6000 pass -" > "$OUT/results/$set-rollback.txt"
        fi
    done
fi

# ---- The table ----
echo
printf '%-10s %-14s %8s  %-5s %s\n' set run frames result "first difference"
fail=0
for f in "$OUT"/results/*.txt; do
    read -r set run frames result rest < "$f"
    printf '%-10s %-14s %8s  %-5s %s\n' "$set" "$run" "$frames" "$result" "$rest"
    [ "$result" = pass ] || fail=1
done
expected=$(wc -l < "$JOBLIST" | tr -d ' ')
got=$(ls "$OUT"/results/*.txt 2>/dev/null | wc -l | tr -d ' ')
[ "$TIER" = full ] && expected=$((expected + 2 * $(echo $SETS | wc -w)))
if [ "$got" -lt "$expected" ]; then
    echo "only $got of $expected runs gave a result"
    fail=1
fi
echo
if [ $fail = 0 ]; then echo "shadow check passed: every run the same as the reference"; else echo "SHADOW CHECK FAILED"; fi
exit $fail
