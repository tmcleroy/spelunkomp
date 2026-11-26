#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; after cps2recomp's.
#
# Release build for Windows x64, cross-compiled on Linux: build/release/Spelunkomp-windows-x64.zip (the launcher,
# every game's program, SDL3.dll, librashader.dll and the shader presets, licences) and update-windows-x64.txt, its
# manifest for the launcher's updates (lib.sh). Needs the packages of tools/release/Dockerfile, each game's
# translated code (lib.sh), and Rust with its x86_64-pc-windows-gnu target for librashader (tools/librashader.sh
# --windows). zlib and the C++ and MinGW-w64 runtimes are linked in.
#   docker build --platform linux/arm64 -t spelunkomp-build-arm64 tools/release
#   docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src spelunkomp-build-arm64 tools/release/windows.sh
# The compiler: llvm-mingw's clang (lib.sh pins the release; fetched into build/deps), for UCRT; WINDOWS_CC=gcc:
# Ubuntu's MinGW-w64 gcc, for MSVCRT, as the releases up to 0.4. The same frames and hashes either way (the
# headless runners' hash logs). Profile-guided optimisation as on Linux (lib.sh's pgo_prepare): PGO_GENERATE=1
# builds Spelunkomp-windows-x64-pgo-train.zip, the programs instrumented with tools/release/run-pgo-windows.ps1,
# which plays them on the PC; PGO_USE=folder builds with the counts it gives back.
set -eu
. "$(dirname "$0")/lib.sh"
CC_NAME="${WINDOWS_CC:-clang}"
BUILD="$ROOT/build/release-windows-$CC_NAME" # (a folder for each compiler: a build folder keeps the one it began with)
if [ "$CC_NAME" = clang ]; then
    case "$(uname -m)" in
        x86_64) host=x86_64; want=$LLVM_MINGW_SHA256_x86_64 ;;
        aarch64|arm64) host=aarch64; want=$LLVM_MINGW_SHA256_aarch64 ;;
        *) echo "error: no llvm-mingw for a $(uname -m) host" >&2; exit 1 ;;
    esac
    export LLVM_MINGW="$DEPS/llvm-mingw-$LLVM_MINGW_VERSION-ucrt-$host"
    if [ ! -x "$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" ]; then
        name="llvm-mingw-$LLVM_MINGW_VERSION-ucrt-ubuntu-22.04-$host"
        fetch=$(mktemp -d) # (a fresh, empty folder for the download)
        curl -fsSL -o "$fetch/$name.tar.xz" "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VERSION/$name.tar.xz"
        echo "$want  $fetch/$name.tar.xz" | sha256 -c - >/dev/null || { echo "error: $name.tar.xz is not the one pinned (lib.sh)" >&2; exit 1; }
        rm -rf "$LLVM_MINGW" "$DEPS/$name"
        python3 -c 'import sys, tarfile; tarfile.open(sys.argv[1]).extractall(sys.argv[2], filter="data")' "$fetch/$name.tar.xz" "$DEPS"
        mv "$DEPS/$name" "$LLVM_MINGW"
        rm -rf "$fetch"
    fi
    TOOLCHAIN="$ROOT/tools/release/mingw-w64-clang.cmake"
    echo "compiler: $("$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" --version | head -1)"
    # (zlib: Ubuntu's MinGW-w64 build of it, its two headers and the library alone, so that the toolchain's own headers
    # are the only C runtime's in the build; the programs use its deflate, inflate and crc32, which call nothing of
    # the runtime but memory's)
    ZLIB="$DEPS/zlib-mingw"
    mkdir -p "$ZLIB/include" "$ZLIB/lib"
    cp /usr/x86_64-w64-mingw32/include/zlib.h /usr/x86_64-w64-mingw32/include/zconf.h "$ZLIB/include/"
    cp /usr/x86_64-w64-mingw32/lib/libz.a "$ZLIB/lib/"
    ZLIB_ARGS="-DZLIB_INCLUDE_DIR=$ZLIB/include -DZLIB_LIBRARY=$ZLIB/lib/libz.a"
elif [ "$CC_NAME" = gcc ]; then
    TOOLCHAIN="$ROOT/tools/release/mingw-w64.cmake"
    ZLIB_ARGS="-DZLIB_USE_STATIC_LIBS=ON"
    if [ "${PGO_GENERATE:-}" = 1 ] || [ -n "${PGO_USE:-}" ]; then
        echo "error: PGO_GENERATE and PGO_USE are clang's profiles (WINDOWS_CC=gcc has none: leave WINDOWS_CC out)" >&2
        exit 1
    fi
else
    echo "error: WINDOWS_CC is clang or gcc" >&2
    exit 1
fi
# (llvm-mingw's tools first while the programs are built, its llvm-profdata among them; not for librashader, whose
# Rust target links with Ubuntu's MinGW-w64 gcc)
KEEP_PATH="$PATH"
[ "$CC_NAME" != clang ] || export PATH="$LLVM_MINGW/bin:$PATH"
pgo_prepare x86_64-w64-mingw32-clang run-pgo-windows.ps1
SDL="$DEPS/SDL3-mingw-$SDL_VERSION"
if [ ! -d "$SDL/x86_64-w64-mingw32" ]; then
    curl -fsSL -o "$DEPS/sdl3-mingw.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-devel-$SDL_VERSION-mingw.tar.gz"
    echo "$SDL_MINGW_SHA256  $DEPS/sdl3-mingw.tar.gz" | sha256 -c - >/dev/null
    mkdir -p "$SDL" && tar -xzf "$DEPS/sdl3-mingw.tar.gz" -C "$SDL" --strip-components=1
    rm -f "$DEPS/sdl3-mingw.tar.gz"
fi
sdl_source # (for its licence text)
# (the flags given every time, empty without PGO: a build folder's cache would keep the last ones)
build() {
    build_programs "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_PREFIX_PATH="$SDL/x86_64-w64-mingw32" $ZLIB_ARGS \
        -DCMAKE_C_FLAGS="$PGO_FLAGS" -DCMAKE_CXX_FLAGS="$PGO_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="-static $PGO_FLAGS"
}
pgo_build build
export PATH="$KEEP_PATH"

STAGE="$BUILD/stage/$ARCHIVE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
copy_programs "$BUILD" "$STAGE" .exe
cp "$SDL/x86_64-w64-mingw32/bin/SDL3.dll" "$STAGE/"
# (the programs unstripped, kept beside the archive, not in it: a crash log's program+offset into a function name,
# tools/release/crash_where.sh)
rm -rf "$BUILD/symbols" && mkdir -p "$BUILD/symbols" && cp "$STAGE"/*.exe "$BUILD/symbols/"
if [ "${PGO_GENERATE:-}" != 1 ]; then
    x86_64-w64-mingw32-strip "$STAGE"/*.exe
fi
# (linked statically but for SDL3.dll and Windows' own (WinHTTP: the launcher's requests, recomp-common); anything
# else here is a mistake)
if x86_64-w64-mingw32-objdump -p "$STAGE"/*.exe | grep -i 'DLL Name' | grep -viE 'SDL3|KERNEL32|msvcrt|USER32|SHELL32|ADVAPI32|ole32|api-ms-|GDI32|IMM32|WINMM|VERSION|SETUPAPI|OLEAUT32|COMDLG32|WS2_32|IPHLPAPI|WINHTTP'; then
    echo "error: a program needs a DLL that is not in the archive" >&2
    exit 1
fi
add_notices "$STAGE" "$BUILD/launcher"
add_librashader "$STAGE" librashader.dll "$STAGE" "$STAGE/licenses" --windows
if x86_64-w64-mingw32-objdump -p "$STAGE/librashader.dll" | grep -i 'DLL Name' | grep -viE 'KERNEL32|ntdll|msvcrt|USER32|ADVAPI32|api-ms-|bcrypt|WS2_32|userenv|OPENGL32|GDI32|ole32|OLEAUT32|SHELL32'; then
    echo "error: librashader.dll needs a DLL that is not in the archive" >&2
    exit 1
fi
# (zlib and the compiler's runtimes are linked into the programs: MinGW-w64's, and gcc's or LLVM's)
cp "$(ls /usr/share/doc/libz-mingw-w64*/copyright | head -1)" "$STAGE/licenses/zlib.txt"
if [ "$CC_NAME" = clang ]; then
    cp "$LLVM_MINGW/x86_64-w64-mingw32/share/mingw32/COPYING.MinGW-w64-runtime.txt" "$STAGE/licenses/MinGW-w64-runtime.txt"
    cp "$LLVM_MINGW/x86_64-w64-mingw32/share/mingw32/COPYING.winpthreads.txt" "$STAGE/licenses/winpthreads.txt"
    cp "$LLVM_MINGW/LICENSE.TXT" "$STAGE/licenses/LLVM-runtime.txt" # (libc++, libunwind, compiler-rt)
else
    cp /usr/share/doc/mingw-w64-common/copyright "$STAGE/licenses/MinGW-w64-runtime.txt"
    cp /usr/share/doc/gcc-mingw-w64-base/copyright "$STAGE/licenses/GCC-runtime.txt"
fi
if [ "${PGO_GENERATE:-}" = 1 ]; then
    # (the instrumented programs are for training only, never a release: with the script that plays them on the PC
    # and each game's ROM set and title frame, into their own archive; no manifest)
    cp "$ROOT/tools/release/run-pgo-windows.ps1" "$STAGE/"
    for game in $GAMES; do
        printf '%s %s %s\n' "$game" "$(sed -n 's/^rom_set = //p' "$ROOT/games/$game/game.cfg" | head -1)" \
            "$(sed -n 's/^title_frame = //p' "$ROOT/games/$game/game.cfg" | head -1)"
    done > "$STAGE/pgo-games.txt"
    pgo_made "$STAGE/pgo-made.txt" windows-x64
    rm -f "$OUT/$ARCHIVE-windows-x64-pgo-train.zip"
    (cd "$BUILD/stage" && zip -qr "$OUT/$ARCHIVE-windows-x64-pgo-train.zip" "$ARCHIVE")
    echo "wrote $OUT/$ARCHIVE-windows-x64-pgo-train.zip (instrumented: for tools/release/run-pgo-windows.ps1, not a release)"
    exit 0
fi
rm -f "$OUT/$ARCHIVE-windows-x64.zip"
(cd "$BUILD/stage" && zip -qr "$OUT/$ARCHIVE-windows-x64.zip" "$ARCHIVE")
check_archive "$OUT/$ARCHIVE-windows-x64.zip"
update_files windows-x64 "$STAGE" "$OUT/$ARCHIVE-windows-x64.zip"
echo "wrote $OUT/$ARCHIVE-windows-x64.zip"
