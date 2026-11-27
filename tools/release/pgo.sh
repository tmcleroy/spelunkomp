#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#
# Profile-guided optimisation for the macOS release (clang's): build/pgo/spelunkomp.profdata, which macos.sh builds
# with (PGO=1, or PGO_PROFILE=file). Each game's program is built instrumented (Apple silicon only: the profile is the
# source's, not the processor's, and the release's Intel half is built with it too), then played with no window and
# no sound, as fast as it goes, the ways a player's time goes:
#   - begun at stage 1 and played there, the slowdown on, with the GPU renderer and with the software one;
#   - the same with run-ahead (1 frame);
#   - taken straight to stage 1's boss and played there (where the game has that), the slowdown on and off.
# Then the profiles are merged. A game not played has its own code built without a profile; the code the games share
# (the boards, the renderers, the program) is profiled by every game played. Needs each game's translated code and its
# ROM set (lib.sh; ROMS=folder, ../cv1k-roms by default).
#   tools/release/pgo.sh               # every game (GAMES="a b": those), about a minute a game
#   env: PGO_FRAMES=3600 (the frames of each run after the game begins)
# Each game's profile is kept (build/pgo/games/<id>.profdata) with what it was made from: the game's own code (its
# translated code, followed through a link, and games/<id>), the compiler, the code the games share (the boards, the
# renderers, the program, recomp-common, third_party/, CMakeLists.txt and this script, the training's recipe) and the
# commit. A game is played again when any of those changed, or with PGO_FULL=1; then every game's kept profile is
# merged. PGO_KEEP_SHARED=1: a game whose own code is as it was keeps its profile though the shared code changed
# (said, and macos.sh warns: the shared code's hottest functions that changed build without a profile). PGO_PLAN=1:
# say which games would be played, and stop. PGO_KEPT=folder: where the kept profiles are (build/pgo/games unless
# said: a folder outside the worktree, for a release made in a worktree made anew each time, docs/RELEASING.md)
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/pgo-train"
PROF="$ROOT/build/pgo"
ROMS="${ROMS:-$ROOT/../cv1k-roms}"
FRAMES="${PGO_FRAMES:-3600}"
ARCH="-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES=arm64"
KEPT="${PGO_KEPT:-$PROF/games}"
mkdir -p "$KEPT"
compiler=$(xcrun clang --version | head -1)
# (a game's own code, as it is now: its translated code (work/<id>/gen is often a link to another tree: -L follows
# it), its folder; and the code the games share, from git)
own_code() {
    { find -L "$ROOT/work/$1/gen" "$ROOT/games/$1" -type f -print0 2>/dev/null | sort -z | xargs -0 shasum -a 256 | sed "s#$ROOT/##"
      echo "$compiler"; } | shasum -a 256 | cut -c1-16
}
SHARED="machine runtime port recomp-common third_party CMakeLists.txt tools/release/pgo.sh"
shared_code() {
    { for part in $SHARED; do git -C "$ROOT" rev-parse "HEAD:$part" 2>/dev/null || echo "$part none"; done
      git -C "$ROOT" diff HEAD -- $SHARED; } | shasum -a 256 | cut -c1-16
}
shared=$(shared_code)
commit="$(git -C "$ROOT" rev-parse HEAD)$(git -C "$ROOT" diff --quiet HEAD -- . ':!work' ':!build' || echo '+changes')"
ALL_GAMES="$GAMES"
PLAY_GAMES=""
for game in $ALL_GAMES; do
    made="$KEPT/$game.made"
    if [ "${PGO_FULL:-}" = 1 ] || [ ! -f "$KEPT/$game.profdata" ] || [ ! -f "$made" ] || [ "$(sed -n 1p "$made")" != "$(own_code "$game")" ]; then
        PLAY_GAMES="$PLAY_GAMES $game"
    elif [ "$(sed -n 2p "$made")" != "$shared" ]; then
        if [ "${PGO_KEEP_SHARED:-}" = 1 ]; then
            echo "warning: $game's profile kept though the shared code changed since it was made (PGO_KEEP_SHARED=1): what changed builds without a profile" >&2
        else
            PLAY_GAMES="$PLAY_GAMES $game"
        fi
    fi
done
echo "pgo: to play:${PLAY_GAMES:- none}"
[ "${PGO_PLAN:-}" != 1 ] || exit 0
GAMES="$PLAY_GAMES"
INSTR=-fprofile-instr-generate
[ -z "$GAMES" ] || sdl_static macos-arm64 $ARCH
[ -z "$GAMES" ] || build_programs "$BUILD" $ARCH -DSDL3_DIR="$DEPS/sdl-macos-arm64/lib/cmake/SDL3" -DCMAKE_C_FLAGS="$INSTR" \
    -DCMAKE_CXX_FLAGS="$INSTR" -DCMAKE_EXE_LINKER_FLAGS="$INSTR"

rm -rf "$PROF/raw"
mkdir -p "$PROF/raw"
home=$(mktemp -d)
for game in $GAMES; do
    set=$(sed -n 's/^rom_set = //p' "$ROOT/games/$game/game.cfg" | head -1)
    [ -e "$ROMS/$set.zip" ] || { echo "error: no $ROMS/$set.zip to play $game with (ROMS=folder)" >&2; exit 1; }
    title=$(sed -n 's/^title_frame = //p' "$ROOT/games/$game/game.cfg" | head -1)
    frames=$(( ${title:-3000} + FRAMES ))
    program="$BUILD/game-$game/$game"
    features=$(LLVM_PROFILE_FILE=/dev/null "$program" --list-features 2>/dev/null || true)
    play() { # name, then the program's arguments
        name=$1; shift
        mkdir -p "$home/$game-$name"
        if ! HOME="$home" SPELUNK_HIDDEN_WINDOW=1 LLVM_PROFILE_FILE="$PROF/raw/$game-$name-%p.profraw" "$program" \
            --roms "$ROMS" --saves "$home/$game-$name" --settings "$home/$game-$name/settings.ini" --mute \
            --frames "$frames" --begin-now 5 --press 0:1000000:0x10 "$@" > "$home/$game-$name.log" 2>&1; then
            echo "error: $game's $name run ended badly ($home/$game-$name.log)" >&2
            exit 1
        fi
    }
    play gpu --features out_of_harm=1
    play software --software --features out_of_harm=1
    play ahead --features out_of_harm=1,run_ahead=1
    if echo "$features" | grep -q '^to_boss'; then
        play boss --features to_boss=1,out_of_harm=1
        echo "$features" | grep -q '^slowdown' && play boss-fast --features to_boss=1,out_of_harm=1,slowdown=1
    fi
    xcrun llvm-profdata merge -o "$KEPT/$game.profdata" "$PROF/raw/$game"-*.profraw
    { own_code "$game"; echo "$shared"; date -u +%Y-%m-%dT%H:%MZ; echo "$commit"; } > "$KEPT/$game.made"
    echo "$game: played"
done
rm -rf "$home"
GAMES="$ALL_GAMES"
profiles=""
kept="" # (each game not played now, with the commit its profile was made from: macos.sh warns where it is not HEAD)
for game in $GAMES; do
    profiles="$profiles $KEPT/$game.profdata"
    case " $PLAY_GAMES " in
        *" $game "*) ;;
        *) kept="$kept $game@$(sed -n 4p "$KEPT/$game.made" | grep . || echo unknown)" ;;
    esac
done
xcrun llvm-profdata merge -o "$PROF/spelunkomp.profdata" $profiles
# (what the profile was made from: macos.sh says so when it builds, and warns when the source is not that any more)
{
    echo "commit $(git -C "$ROOT" rev-parse HEAD)$(git -C "$ROOT" diff --quiet HEAD -- . ':!work' ':!build' || echo ' (with changes not committed)')"
    echo "games $GAMES"
    echo "played now:${PLAY_GAMES:- none}"
    [ -z "$kept" ] || echo "kept$kept"
    echo "made $(date -u +%Y-%m-%dT%H:%MZ) by $(xcrun clang --version | head -1)"
} > "$PROF/spelunkomp.profdata.txt"
echo "wrote $PROF/spelunkomp.profdata ($(echo $GAMES | wc -w | tr -d ' ') games, $(echo $PLAY_GAMES | wc -w | tr -d ' ') played now)"
