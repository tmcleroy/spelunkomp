#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# The collection: one launcher for every game (recomp-common's launcher with LAUNCHER_COLLECTION; launcher/src/
# description.cpp), in build/collection/ with each game's program beside it. Each game is built in build/<id>/ with
# its translations (work/<id>/gen, from tools/translate.sh). A game with none would run interpreted, far slower: the
# script stops, naming each and the command that makes its translation (tools/check_gen.sh), unless INTERPRETED=1,
# which builds them so and says so for each. Run from the repository's top.
#   B=folder   where (build/collection)
#   GAMES="a b" those games only (a test; the app has them all)
set -eu
B=${B:-build/collection}
GAMES=${GAMES:-$(for cfg in games/*/game.cfg; do printf '%s ' "$(basename "$(dirname "$cfg")")"; done)}
interpreted=$(tools/check_gen.sh --missing $GAMES)
if [ -n "$interpreted" ]; then
    if [ "${INTERPRETED:-}" != 1 ]; then
        tools/check_gen.sh $GAMES || exit 1
    fi
    echo "warning: INTERPRETED=1: built with no translated code, on the interpreters: $interpreted" >&2
fi
mkdir -p "$B"
for id in $GAMES; do
    cmake -S . -B "build/$id" -G Ninja -DGAME="$id" -DCMAKE_BUILD_TYPE=Release > /dev/null
    ninja -C "build/$id" "$id" > /dev/null
    cp -p "build/$id/$id" "$B/"
    case " $interpreted " in
        *" $id "*) echo "$id: build/$id/$id (interpreted build: no translated code)" ;;
        *) echo "$id: build/$id/$id" ;;
    esac
done
cmake -S . -B "$B/build" -G Ninja -DGAME="$(basename "$(dirname "$(ls games/*/game.cfg | head -1)")")" -DCOLLECTION=ON \
    -DCMAKE_BUILD_TYPE=Release > /dev/null
ninja -C "$B/build" spelunkomp-launcher > /dev/null
cp -p "$B/build/spelunkomp-launcher" "$B/"
# (the launcher's Tools: the scene diorama beside the programs, tools/diorama)
cp tools/diorama/diorama.html "$B/"
echo "built $B/spelunkomp-launcher"
