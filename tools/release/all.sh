#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# All release builds on a Mac with Docker: macOS here; Linux x86_64, Linux ARM64 and Windows natively in ARM64
# containers on Apple silicon (tools/release/x86_64/Dockerfile and tools/release/Dockerfile; on an x86_64 host,
# Linux x86_64 and Windows in tools/release/Dockerfile). Writes build/release/ with the five downloads,
# SHA256SUMS.txt and update-<system>.txt for macOS, Linux and Windows (what the launcher updates itself from:
# lib.sh). Needs each game's translated code, made from the builder's own ROM sets (lib.sh), and the ROM sets
# themselves (ROMS=folder, ../cv1k-roms by default: macos.sh starts every game's program with its set).
#
#   TARGETS="macos windows" tools/release/all.sh   # some of the targets (SHA256SUMS.txt is left alone)
#   TARGETS="sums" tools/release/all.sh            # SHA256SUMS.txt alone, once every archive is in build/release/
#   PGO=1 PGO_USE=folder tools/release/all.sh      # built with profile-guided optimisation: macOS with a profile
#                                                  #   tools/release/pgo.sh makes first; Linux x86_64 with the
#                                                  #   counts in PGO_USE (run-pgo-linux.sh's, docs/RELEASING.md).
#                                                  #   PGO=1 with no PGO_USE stops; PGO_LINUX=0: Linux without one.
#                                                  #   Windows with the counts in PGO_USE_WINDOWS (run-pgo-windows.ps1's);
#                                                  #   PGO=1 with none stops; PGO_WINDOWS=0: Windows without one
#   PGO_GENERATE=1 TARGETS=linux-x86_64 tools/release/all.sh  # Linux's instrumented build, for run-pgo-linux.sh
#   PGO_GENERATE=1 TARGETS=windows tools/release/all.sh       # Windows', for run-pgo-windows.ps1 on a PC
#   (the Android app is signed with the release key in ~/.config/spelunkomp/android-signing.env, outside the
#    repository; tools/release/android.sh, RELEASE=1, refuses without it: leave android out of TARGETS then)
# Translated code that is a link to another folder (work/<id>/gen -> elsewhere) is followed in the containers too:
# every folder it reaches is mounted at the same path, read only.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

TARGETS="${TARGETS:-macos linux-x86_64 linux-arm64 windows android}"
HOST_ARCH="$(uname -m)"
ALL_START="$(date +%s)"

fmt_dur() {
    s=$1
    if [ "$s" -ge 60 ]; then
        printf '%dm%02ds' $((s / 60)) $((s % 60))
    else
        printf '%ds' "$s"
    fi
}
has_target() {
    case " $TARGETS " in *" $1 "*) return 0 ;; *) return 1 ;; esac
}
step() {
    label=$1; shift
    t0=$(date +%s)
    printf '[%s] %s...\n' "$(date '+%H:%M:%S')" "$label"
    "$@"
    t1=$(date +%s)
    printf '[%s] %s done (%s)\n' "$(date '+%H:%M:%S')" "$label" "$(fmt_dur $((t1 - t0)))"
}

# (PGO=1: Linux x86_64 is built with the counts made on a Steam Machine or Deck, PGO_USE=folder (linux.sh), or said to
# be built without them, PGO_LINUX=0; checked before anything is built. ARM64 has no device the counts are made on:
# it is built without them)
LINUX_PGO_USE=""
if [ "${PGO_LINUX:-}" = 0 ]; then
    ! has_target linux-x86_64 || echo "PGO: Linux ships without a profile (PGO_LINUX=0)"
elif has_target linux-x86_64 && { [ "${PGO:-}" = 1 ] || [ -n "${PGO_USE:-}" ]; }; then
    if [ -z "${PGO_USE:-}" ]; then
        echo "error: PGO=1 with no Linux counts: PGO_USE=<the folder of run-pgo-linux.sh's raw.tar.gz, unpacked> (docs/RELEASING.md), or PGO_LINUX=0 to ship Linux without a profile" >&2
        exit 1
    fi
    [ -d "$PGO_USE" ] || { echo "error: no folder $PGO_USE (PGO_USE)" >&2; exit 1; }
    LINUX_PGO_USE="$(cd "$PGO_USE" && pwd -P)"
    echo "PGO: Linux x86_64 built with the counts in $LINUX_PGO_USE (linux-arm64 without)"
fi
# (the same for Windows, with the counts made on a Windows PC: PGO_USE_WINDOWS=folder, or PGO_WINDOWS=0)
WINDOWS_PGO_USE=""
if [ "${PGO_WINDOWS:-}" = 0 ]; then
    ! has_target windows || echo "PGO: Windows ships without a profile (PGO_WINDOWS=0)"
elif has_target windows && { [ "${PGO:-}" = 1 ] || [ -n "${PGO_USE_WINDOWS:-}" ]; }; then
    if [ -z "${PGO_USE_WINDOWS:-}" ]; then
        echo "error: PGO=1 with no Windows counts: PGO_USE_WINDOWS=<the folder of run-pgo-windows.ps1's raw.zip, unpacked> (docs/RELEASING.md), or PGO_WINDOWS=0 to ship Windows without a profile" >&2
        exit 1
    fi
    [ -d "$PGO_USE_WINDOWS" ] || { echo "error: no folder $PGO_USE_WINDOWS (PGO_USE_WINDOWS)" >&2; exit 1; }
    WINDOWS_PGO_USE="$(cd "$PGO_USE_WINDOWS" && pwd -P)"
    echo "PGO: Windows built with the counts in $WINDOWS_PGO_USE"
fi
# (the source's commit, for what a container cannot read itself: a worktree's .git is outside it)
SOURCE_COMMIT="$(git rev-parse HEAD 2>/dev/null || echo unknown)$(git diff --quiet HEAD -- . ':!work' ':!build' 2>/dev/null || echo '+changes')"

[ "${NO_AUDIT:-}" = 1 ] || [ ! -x tools/audit.sh ] || step "audit" tools/audit.sh

# (CCACHE_BASE, if set: the folder each container keeps its per-target caches in, mounted at /ccache)
if [ -n "${CCACHE_BASE:-}" ]; then
    mkdir -p "$CCACHE_BASE"
    CCACHE_BASE="$(cd "$CCACHE_BASE" && pwd)"
fi
# (the folders work/<id>/gen links reach, to mount in the containers at the same paths)
LINKED=""
for gen in work/*/gen; do
    [ -L "$gen" ] || continue
    target="$(cd "$gen" && pwd -P)"
    LINKED="$LINKED $(dirname "$target")"
done
# a release script in a build image: $1 the image, $2 the script, the rest more arguments for docker run (IMAGE_PGO_USE:
# the folder of the system's counts, given as PGO_USE: inside the worktree at its path there, or mounted read only)
in_image() {
    image=$1 script=$2; shift 2
    set -- docker run --rm "$@" -e GAMES="${GAMES:-}" -e NO_GAME_CODE="${NO_GAME_CODE:-}" -e NO_CCACHE="${NO_CCACHE:-}" \
        -e HEADLESS="${HEADLESS:-}" -e TRANSLATED="${TRANSLATED:-}" -e SOURCE_COMMIT="$SOURCE_COMMIT" \
        -e PGO_GENERATE="${PGO_GENERATE:-}"
    if [ -n "${IMAGE_PGO_USE:-}" ]; then
        case "$IMAGE_PGO_USE/" in
            "$(pwd -P)"/*) set -- "$@" -e PGO_USE="/src/${IMAGE_PGO_USE#"$(pwd -P)"/}" ;;
            *) set -- "$@" -v "$IMAGE_PGO_USE:$IMAGE_PGO_USE:ro" -e PGO_USE="$IMAGE_PGO_USE" ;;
        esac
    fi
    [ -z "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ] || set -- "$@" -e CMAKE_BUILD_PARALLEL_LEVEL="$CMAKE_BUILD_PARALLEL_LEVEL"
    [ -z "${CCACHE_BASE:-}" ] || set -- "$@" -v "$CCACHE_BASE":/ccache -e CCACHE_BASE=/ccache
    for folder in $LINKED; do
        set -- "$@" -v "$folder:$folder:ro"
    done
    "$@" -v "$ROOT":/src -w /src "$image" "$script"
}

# Only the Docker images the targets need
need_x86=0 need_arm=0 need_amd64=0
if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
    has_target linux-x86_64 && need_x86=1
    { has_target linux-arm64 || has_target windows; } && need_arm=1
else
    { has_target linux-x86_64 || has_target windows; } && need_amd64=1
    has_target linux-arm64 && need_arm=1
fi
[ "$need_x86" = 1 ] && step "docker build (linux-x86_64 cross)" docker build -q -t spelunkomp-build-x86_64 tools/release/x86_64
[ "$need_arm" = 1 ] && step "docker build (arm64)" docker build --platform linux/arm64 -q -t spelunkomp-build-arm64 tools/release
[ "$need_amd64" = 1 ] && step "docker build (amd64)" docker build --platform linux/amd64 -q -t spelunkomp-build tools/release

# (the 68000 games' code translated again from their sources, once for all the systems: lib.sh)
if [ "${NO_GAME_CODE:-}" != 1 ]; then
    step "translate" sh -c '. "$(dirname "$0")/lib.sh" && translate_games' "$ROOT/tools/release/all.sh"
    TRANSLATED=1
    export TRANSLATED
fi

# (each system's manifest is written again by its script, beside its archive)
for target in macos linux-x86_64 linux-arm64 windows; do
    system=$target
    [ "$target" != windows ] || system=windows-x64
    ! has_target "$target" || rm -f "build/release/update-$system.txt"
done
if has_target macos; then
    # (PGO=1: the macOS programs built with a profile made here first, tools/release/pgo.sh: every game played, about
    # three minutes a game more)
    [ "${PGO:-}" != 1 ] || step "macos profile (PGO)" tools/release/pgo.sh
    step "macos" tools/release/macos.sh
fi
if has_target linux-x86_64; then
    IMAGE_PGO_USE="$LINUX_PGO_USE"
    if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
        step "linux-x86_64 (native cross)" in_image spelunkomp-build-x86_64 tools/release/linux.sh
    else
        step "linux-x86_64" in_image spelunkomp-build tools/release/linux.sh --platform linux/amd64
    fi
    IMAGE_PGO_USE=""
fi
if has_target linux-arm64; then
    step "linux-arm64" in_image spelunkomp-build-arm64 tools/release/linux.sh --platform linux/arm64
fi
if has_target android; then
    # the Android app, signed with the release key (~/.config/spelunkomp/android-signing.env; the script refuses without it)
    step "android" env RELEASE=1 tools/release/android.sh
fi
if has_target windows; then
    IMAGE_PGO_USE="$WINDOWS_PGO_USE"
    if [ "$HOST_ARCH" = "arm64" ] || [ "$HOST_ARCH" = "aarch64" ]; then
        step "windows-x64 (native cross)" in_image spelunkomp-build-arm64 tools/release/windows.sh --platform linux/arm64
    else
        step "windows-x64" in_image spelunkomp-build tools/release/windows.sh --platform linux/amd64
    fi
    IMAGE_PGO_USE=""
fi

# SHA256SUMS.txt: on a run of the four archives (and the app, when android is a target), or TARGETS=sums once every
# archive is in build/release/. (A run of some of them leaves it alone: build/release may still hold another
# version's files.) It lists the downloads only, not the update-<system>.txt manifests, which carry their archives'
# SHA-256 themselves; each is checked here to name its archive as it is and this version.
ARCHIVES="Spelunkomp-macOS.zip Spelunkomp-linux-x86_64.tar.gz Spelunkomp-linux-arm64.tar.gz Spelunkomp-windows-x64.zip"
# (TARGETS=sums: the release's five, the APK among them, as the Android app's update checks it against this file)
! has_target android && ! has_target sums || ARCHIVES="$ARCHIVES Spelunkomp-android.apk"
if has_target sums || { has_target macos && has_target linux-x86_64 && has_target linux-arm64 && has_target windows; }; then
    cd build/release
    for f in $ARCHIVES; do
        [ -f "$f" ] || { echo "error: no build/release/$f" >&2; exit 1; }
    done
    shasum -a 256 $ARCHIVES > SHA256SUMS.txt
    cat SHA256SUMS.txt
    for system in macos linux-x86_64 linux-arm64 windows-x64; do
        manifest="update-$system.txt"
        [ -f "$manifest" ] || { echo "error: no build/release/$manifest" >&2; exit 1; }
        set -- $(sed -n 's/^archive //p' "$manifest")
        if [ "$#" -ne 3 ] || [ ! -f "$3" ] || [ "$(shasum -a 256 "$3" | cut -d' ' -f1)" != "$1" ] || \
            [ "$(sed -n 's/^version //p' "$manifest")" != "$(cat "$ROOT/VERSION")" ]; then
            echo "error: $manifest does not name ${3:-its archive} as it is, or another version" >&2
            exit 1
        fi
    done
    echo "update manifests: $(ls update-*.txt | tr '\n' ' ')"
    cd "$ROOT"
fi

ALL_END="$(date +%s)"
printf '[%s] version %s: release files in %s/build/release (total %s)\n' \
    "$(date '+%H:%M:%S')" "$(cat "$ROOT/VERSION")" "$ROOT" "$(fmt_dur $((ALL_END - ALL_START)))"
