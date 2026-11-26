# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Shared by the release scripts (sourced, not run). A release is the collection: its launcher and every game's
# program (games/*), as one download for each system. A game's program holds the game's code translated to C++, as
# a build from source does: work/<id>/gen, made by tools/translate.sh (CV1000), tools/translate68k.sh (the 68000
# boards) and tools/translatez80.sh (a PGM game's sound driver) from the builder's own ROM sets. So releases are
# built on the machine of someone who has every game, never by CI. With NO_GAME_CODE=1 the programs are built with
# no translated code instead (they run a ROM set on the interpreters): that is what CI builds, to prove everything
# compiles and starts on each system, and it is never what a release holds. GAMES="a b" builds those games only (a
# test; a release has them all). HEADLESS=1 builds each game's spelunk-headless too (the platform checks' hash
# logs, docs/BUILDING.md; never in an archive).
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAMES="${GAMES:-$(cd "$ROOT/games" && for cfg in */game.cfg; do printf '%s ' "${cfg%/game.cfg}"; done)}"
VERSION="${VERSION:-$(cat "$ROOT/VERSION")}"
OUT="$ROOT/build/release"
DEPS="$ROOT/build/deps"
NAME=Spelunkomp           # the collection: its launcher's window, app and folder
EXE=spelunkomp            # its launcher: spelunkomp-launcher
ARCHIVE=Spelunkomp        # the downloads: Spelunkomp-macOS.zip and the rest
APP_ID=io.github.tmcleroy.spelunkomp
SDL_VERSION=3.4.16        # (the same as recomp-common/launcher/launcher.cmake fetches)
SDL_SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
SDL_MINGW_SHA256=c7ef65bd72eabac6e5b535411dbd8d5824d0aab24fd62ff8812666b336f18a9c
# llvm-mingw (clang, lld, libc++ and the MinGW-w64 runtime, for UCRT: Windows 10 on), the Windows build's compiler:
# https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104, its Ubuntu 22.04 builds for each kind of Linux host,
# checked against the release's own sha256 (windows.sh)
LLVM_MINGW_VERSION=20261104
LLVM_MINGW_SHA256_aarch64=595e95bdffabfc62559fe26e30bdfbccf277cbaed4902793555015ee47306b94
LLVM_MINGW_SHA256_x86_64=919380403de87dca3c581c2053230cd298af5e891286628ba0613b7fed370926
sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
mkdir -p "$OUT" "$DEPS"

# Profile-guided optimisation (clang's), in two builds of the same folder ($BUILD): PGO_GENERATE=1 builds the
# programs instrumented, each run writing its counts as a .profraw (the system's trainer, on the device: $2 below),
# with pgo-made.txt beside them: the commit, the games and the compiler they were built from. PGO_USE=folder builds
# them with the counts brought back there, merged by this clang's llvm-profdata (the first on PATH); it says what
# they were made from, and warns where that is not this commit or this compiler. A function the counts know from
# other source is said (-Wprofile-instr-out-of-date); a file with no counts at all is counted for each game and
# summed up at the end (a game whose every file has none: counts not made from it, or the wrong folder)
# (the source's commit: git's, or SOURCE_COMMIT, which all.sh passes: a worktree's .git is not in the container)
source_commit() {
    if [ -n "${SOURCE_COMMIT:-}" ]; then
        echo "$SOURCE_COMMIT"
    elif c=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null); then
        git -C "$ROOT" diff --quiet HEAD -- . ':!work' ':!build' 2>/dev/null || c="$c+changes"
        echo "$c"
    else
        echo unknown
    fi
}
# pgo_prepare CLANG TRAINER: PGO_FLAGS (empty without PGO), and the counts merged for PGO_USE (CLANG: the compiler's
# command, for its version; TRAINER: the script that makes the counts, for the messages)
pgo_prepare() {
    PGO_FLAGS=""
    compiler=""
    if [ "${PGO_GENERATE:-}" = 1 ] || [ -n "${PGO_USE:-}" ]; then
        compiler=$("$1" --version | head -1)
    fi
    if [ "${PGO_GENERATE:-}" = 1 ]; then
        PGO_FLAGS="-fprofile-instr-generate"
        echo "PGO: building the programs instrumented, from $(source_commit)"
        [ "$(source_commit)" != unknown ] ||
            echo "warning: the source's commit is unknown here (a worktree's .git is not in the container): build this with all.sh, or SOURCE_COMMIT=<commit>" >&2
    elif [ -n "${PGO_USE:-}" ]; then
        [ -d "$PGO_USE" ] || { echo "error: no folder $PGO_USE of the programs' counts (.profraw)" >&2; exit 1; }
        runs=$(find "$PGO_USE" -name '*.profraw' | wc -l | tr -d ' ')
        [ "$runs" -gt 0 ] || { echo "error: no counts (.profraw) in $PGO_USE: what $2 gives back, unpacked there" >&2; exit 1; }
        echo "PGO: building with the counts in $PGO_USE ($runs runs)"
        made=$(find "$PGO_USE" -name pgo-made.txt | head -1)
        if [ -z "$made" ]; then
            echo "warning: ********************************************************************************" >&2
            echo "warning: no pgo-made.txt in $PGO_USE: nothing says what these counts were made from" >&2
            echo "warning: ********************************************************************************" >&2
        else
            # (written on Windows it has CRLF line ends: read without the CR)
            tr -d '\r' < "$made" | sed 's/^/PGO:   /'
            here=$(source_commit)
            was=$(tr -d '\r' < "$made" | sed -n 's/^commit //p')
            was_cc=$(tr -d '\r' < "$made" | sed -n 's/^compiler //p')
            if [ "${was%+changes}" != "${here%+changes}" ] || [ "$was_cc" != "$compiler" ]; then
                echo "warning: ********************************************************************************" >&2
                [ "${was%+changes}" = "${here%+changes}" ] ||
                    echo "warning: the counts were made from $was, not this source ($here): the functions changed since build without them (clang says which files, profile-instr-out-of-date)" >&2
                [ "$was_cc" = "$compiler" ] ||
                    echo "warning: the counts were made by $was_cc, not this compiler ($compiler)" >&2
                echo "warning: ********************************************************************************" >&2
            fi
        fi
        mkdir -p "$BUILD"
        rm -f "$BUILD/pgo.profdata"
        find "$PGO_USE" -name '*.profraw' -print0 | xargs -0 llvm-profdata merge -o "$BUILD/pgo.profdata"
        echo "PGO: $(llvm-profdata show "$BUILD/pgo.profdata" | grep 'Total functions')"
        # (-Wprofile-instr-unprofiled, a file with no counts, on: its lines are taken out of the build's and counted)
        PGO_FLAGS="-fprofile-instr-use=$BUILD/pgo.profdata"
        NO_CCACHE=1 # (a cached object would not know the counts had changed)
    fi
}
# pgo_build BUILD: runs the function that builds; with PGO_USE, its log kept and the files with no counts summed up
pgo_build() {
    if [ -n "${PGO_USE:-}" ] && [ "${PGO_GENERATE:-}" != 1 ]; then
        log="$BUILD/pgo-build.log"
        rm -f "$BUILD/pgo-built"
        { "$1"; : > "$BUILD/pgo-built"; } 2>&1 | tee "$log" | grep -v -e 'Wprofile-instr-unprofiled' -e '^[0-9]* warnings\{0,1\} generated\.$' || true
        [ -f "$BUILD/pgo-built" ] || { echo "error: the build failed ($log)" >&2; exit 1; }
        # (each game's files built, and those of them with no counts: its lines end with "<game>: built"; the launcher last)
        awk '/Building C(XX)? object/ { files++ } /Wprofile-instr-unprofiled/ { none++ }
            /^[a-z0-9_]+: built$/ { name = $1; sub(/:$/, "", name); report(name) }
            END { report("launcher"); if (bad) print "warning: " bad " program(s) built with no counts in any file: the counts are not theirs (PGO_USE the wrong folder, or made from other source)" > "/dev/stderr" }
            function report(name) {
                printf "PGO: %s: %d of %d files built with no counts\n", name, none, files
                if (files > 0 && none == files) bad++
                files = 0; none = 0
            }' "$log"
        echo "PGO: $(grep -c 'Wprofile-instr-out-of-date' "$log" || true) files with functions whose counts are from other source (-Wprofile-instr-out-of-date, $log)"
    else
        "$1"
    fi
}
# pgo_made FILE SYSTEM: what a training build's counts are made from (PGO_USE says so, and warns where the build is
# not that)
pgo_made() {
    { echo "commit $(source_commit)"; echo "games $(echo $GAMES)"; echo "compiler $compiler"; echo "system $2"
      echo "built $(date -u +%Y-%m-%dT%H:%MZ)"; } > "$1"
}

# SDL 3's source, checked against its published checksum: $DEPS/SDL3-$SDL_VERSION
sdl_source() {
    if [ ! -d "$DEPS/SDL3-$SDL_VERSION/src" ]; then
        [ -f "$DEPS/SDL3-$SDL_VERSION.tar.gz" ] || curl -fsSL -o "$DEPS/SDL3-$SDL_VERSION.tar.gz" \
            "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz"
        echo "$SDL_SHA256  $DEPS/SDL3-$SDL_VERSION.tar.gz" | sha256 -c - >/dev/null
        tar -xzf "$DEPS/SDL3-$SDL_VERSION.tar.gz" -C "$DEPS"
    fi
}
# ccache across releases, per target (CCACHE_BASE defaults to $ROOT/build/ccache)
setup_ccache() {
    CCACHE_ARGS=""
    if command -v ccache >/dev/null 2>&1 && [ "${NO_CCACHE:-}" != 1 ]; then
        export CCACHE_DIR="${CCACHE_BASE:-$ROOT/build/ccache}/$1"
        export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$ROOT}"
        export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-4G}"
        mkdir -p "$CCACHE_DIR"
        CCACHE_ARGS="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
    fi
}
# SDL 3 as a static library in $DEPS/sdl-$1 (the rest: more arguments for CMake)
sdl_static() {
    name=$1; shift
    if [ ! -f "$DEPS/sdl-$name/lib/cmake/SDL3/SDL3Config.cmake" ]; then
        sdl_source
        setup_ccache "$name"
        cmake -S "$DEPS/SDL3-$SDL_VERSION" -B "$DEPS/sdl-$name-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF \
            -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DCMAKE_INSTALL_PREFIX="$DEPS/sdl-$name" $CCACHE_ARGS "$@" >/dev/null
        cmake --build "$DEPS/sdl-$name-build" --target install >/dev/null
        rm -rf "$DEPS/sdl-$name-build"
    fi
}

# The 68000 games' code translated again from the game's sources as they are now: the translator builds the game's
# hooks (games/<id>/src) into the code it writes, and a program whose code was translated before a hook was added
# stops as it starts ("the hook ... was added after the game's code was translated"). From the program and the
# entries kept in work/<id>/ (program.bin, entries.txt: tools/translate68k.sh), with no ROM set and no run; a CV1000
# game's translation (tools/translate.py) holds no hooks and is taken as it is. all.sh does this once, before the
# systems' builds, and passes TRANSLATED=1 on. A work/<id>/gen that is a link to another tree is never written
# through: translate it there.
translate_games() {
    if [ "${NO_GAME_CODE:-}" = 1 ] || [ "${TRANSLATED:-}" = 1 ]; then
        return 0
    fi
    for game in $GAMES; do
        grep -qE '^hardware = (cave68k|pgm)' "$ROOT/games/$game/game.cfg" || continue
        w="$ROOT/work/$game"
        if [ ! -f "$w/program.bin" ] || [ ! -f "$w/entries.txt" ]; then
            echo "error: no work/$game/program.bin and entries.txt to translate $game from: tools/translate68k.sh first" >&2
            exit 1
        fi
        if [ -L "$w/gen" ]; then
            echo "error: work/$game/gen is a link to another tree: translate $game there (or copy its work/$game here)" >&2
            exit 1
        fi
        echo "translating $game"
        (cd "$ROOT" && python3 tools/m68k/recomp.py --program "$w/program.bin" --entries "$w/entries.txt" --out "$w/gen" \
            --guess "$w/guesses.txt" --hook-sources "games/$game/src" --list "$w/program.lst") >/dev/null || {
            echo "error: translating $game failed" >&2
            exit 1
        }
    done
    TRANSLATED=1
    export TRANSLATED
}

# Every game's program, in $1/game-<id>/, and the collection's launcher, in $1/launcher/ (the rest: more arguments
# for CMake). The launcher is built with no translated code (WORK_DIR an empty folder): it only reads each game's
# game.cfg, and would otherwise carry the code of the game it is built beside.
build_programs() {
    build=$1; shift
    setup_ccache "$(basename "$build")"
    translate_games
    empty="$build/no-game-code"
    mkdir -p "$empty"
    work="$ROOT/work"
    [ "${NO_GAME_CODE:-}" != 1 ] || work="$empty"
    for game in $GAMES; do
        [ -f "$ROOT/games/$game/game.cfg" ] || { echo "error: no game $game (games/$game/game.cfg)" >&2; exit 1; }
    done
    # (every game's translated code there before any is built: the games without it, and the commands that make it)
    if [ "${NO_GAME_CODE:-}" != 1 ]; then
        WORK="$work" OVERRIDE=NO_GAME_CODE=1 "$ROOT/tools/check_gen.sh" $GAMES || exit 1
    fi
    for game in $GAMES; do
        cmake -S "$ROOT" -B "$build/game-$game" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME="$game" -DWORK_DIR="$work" \
            $CCACHE_ARGS "$@" >/dev/null
        targets=game
        [ "${HEADLESS:-}" != 1 ] || targets="$targets spelunk-headless"
        cmake --build "$build/game-$game" --target $targets
        echo "$game: built"
    done
    first=$(echo $GAMES | cut -d' ' -f1)
    cmake -S "$ROOT" -B "$build/launcher" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME="$first" -DWORK_DIR="$empty" \
        -DCOLLECTION=ON -DCOLLECTION_VERSION="$VERSION" $CCACHE_ARGS "$@" >/dev/null
    cmake --build "$build/launcher" --target launcher
}
# Every game's program and the launcher, from $1 (build_programs') into $2 (the end of a program's name: $3)
copy_programs() {
    cp "$1/launcher/$EXE-launcher$3" "$2/"
    for game in $GAMES; do
        cp "$1/game-$game/$game$3" "$2/"
    done
}
check_archive() { # the archive
    if [ "${NO_GAME_CODE:-}" = 1 ]; then
        GAMES="$GAMES" "$ROOT/tools/release/check_archive.sh" --no-game-code "$1"
    else
        GAMES="$GAMES" "$ROOT/tools/release/check_archive.sh" "$1"
    fi
}
# Every game's program in $1 started with its ROM set ($ROMS/<set>.zip; ROMS: the folder of the builder's sets, by
# default ../cv1k-roms) and run 300 frames with no window and no sound: no other check of a release runs the
# programs it ships. Only where they run (the Mac's own build); ROMS=none skips it, saying so.
check_programs_start() {
    roms="${ROMS:-$ROOT/../cv1k-roms}"
    if [ "$roms" = none ]; then
        echo "warning: the programs were not started (ROMS=none)" >&2
        return 0
    fi
    home=$(mktemp -d)
    for game in $GAMES; do
        set=$(sed -n 's/^rom_set = //p' "$ROOT/games/$game/game.cfg" | head -1)
        [ -e "$roms/$set.zip" ] || { echo "error: no $roms/$set.zip to start $game's program with (ROMS=folder)" >&2; exit 1; }
        if ! env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$1/$game" --roms "$roms/$set.zip" --settings "$home/settings.ini" \
            --saves "$home/saves" --software --mute --frames 300 > "$home/out.txt" 2>&1; then
            tail -3 "$home/out.txt" >&2
            echo "error: $game's program stopped (above)" >&2
            exit 1
        fi
    done
    rm -rf "$home"
    echo "every game's program starts and runs ($(echo $GAMES | wc -w | tr -d ' ') games)"
}
# What every archive carries beside the programs: $1 the folder, $2 the launcher's build folder
add_notices() {
    mkdir -p "$1/licenses"
    cp "$ROOT/LICENSE" "$1/LICENSE.txt"
    cp "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/README.md" "$1/"
    [ ! -f "$ROOT/CHANGELOG.md" ] || cp "$ROOT/CHANGELOG.md" "$1/"
    cp "$DEPS/SDL3-$SDL_VERSION/LICENSE.txt" "$1/licenses/SDL3.txt"
    cp "$2/_deps/imgui-src/LICENSE.txt" "$1/licenses/DearImGui.txt"
    cp "$(find "$2/_deps/inter-src" -iname 'LICENSE*' | head -1)" "$1/licenses/Inter-OFL.txt"
    cp "$ROOT/recomp-common/launcher/flags-LICENSE.txt" "$1/licenses/flag-icons-MIT.txt"
    if [ "${NO_GAME_CODE:-}" = 1 ]; then
        # (what tells a CI build from a release: check_archive.sh)
        echo "Built with no translated code: the games run on the interpreters. Not a release." > "$1/NO-GAME-CODE.txt"
    fi
    printf 'Source for this version (GPL-3.0): https://github.com/tmcleroy/spelunkomp/tree/v%s\n' "$VERSION" > "$1/SOURCE.txt"
}
# What the launcher updates itself from (recomp-common/launcher/src/update.h): $OUT/update-$1.txt, the manifest of
# the archive $3 (its SHA-256 and size) and of every file in $2 (the app, or the folder at the archive's top) with
# its SHA-256 and size. Uploaded to the release beside the archives (docs/RELEASING.md); the launcher downloads the
# archive itself. (Android has none: its launcher takes the release's APK, checked against SHA256SUMS.txt.)
update_files() {
    platform=$1 top=$2 archive=$3
    manifest="$OUT/update-$platform.txt"
    printf 'recomp-update 2\nversion %s\n' "$VERSION" > "$manifest"
    printf 'archive %s %s %s\n' "$(sha256 "$archive" | cut -d' ' -f1)" "$(wc -c < "$archive" | tr -d ' ')" "$(basename "$archive")" >> "$manifest"
    printf 'top %s\n' "$(basename "$top")" >> "$manifest"
    (cd "$top" && find . -type f | sed 's|^\./||' | LC_ALL=C sort) | while IFS= read -r path; do
        mode=-
        [ -x "$top/$path" ] && mode=x
        printf 'file %s %s %s %s\n' "$(sha256 "$top/$path" | cut -d' ' -f1)" "$(wc -c < "$top/$path" | tr -d ' ')" "$mode" "$path" >> "$manifest"
    done
    # (the launcher updates only a copy whose launcher is where the manifest has it)
    case "$platform" in
        macos) self="Contents/MacOS/$EXE-launcher" ;;
        windows-x64) self="$EXE-launcher.exe" ;;
        *) self="$EXE-launcher" ;;
    esac
    grep -q " $self\$" "$manifest" || { echo "error: $manifest has no $self" >&2; exit 1; }
    echo "wrote $manifest ($(grep -c '^file ' "$manifest") files)"
}
# The Graphics page's shaders (port/shading.h): librashader ($2: its file in tools/librashader.sh's folder) into $1,
# the presets into $3 and their licences into $4. The rest: tools/librashader.sh's arguments (--windows).
add_librashader() {
    to=$1 lib=$2 presets=$3 licences=$4
    shift 4
    LIBRA=$("$ROOT/tools/librashader.sh" "$@")
    mkdir -p "$to" "$licences"
    cp "$LIBRA/$lib" "$to/"
    cp -R "$LIBRA/shaders" "$presets/"
    cp "$LIBRA/librashader-LICENSE.md" "$licences/librashader-MPL-2.0.txt"
    cp "$LIBRA/librashader-LICENSE-GPL.md" "$licences/librashader-GPL-3.0.txt"
}
