#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's tools/release/android.sh.
#
# Release build for Android (ARM64, Android 9 and later): build/release/Spelunkomp-android.apk, one app with the
# launcher and every game. Each game is a library (lib<id>.so) that the app loads only when that game is played, in
# a process of its own (android/: GameActivity.java); the launcher is recomp-common's, as a library too. Needs the
# translated code of each game, made from its ROM set (lib.sh), CMake and Ninja, a JDK 17 and the Android SDK with
# platform 34, build-tools and NDK 27.2 (ANDROID_HOME; Homebrew's android-commandlinetools is found on its own: brew
# install openjdk@17 android-commandlinetools), and Rust for librashader (tools/librashader.sh: the Graphics page's
# shaders). SDL 3 is built from source (its Java half is compiled into the app).
#
#   tools/release/android.sh                    every game (GAMES="mushisam ddonpach" for those only: a test)
#   ROM_SETS=<folder> tools/release/android.sh
#       a development build with your own ROM sets packed in (the folder's zips of the games built), to try on your
#       own phone: the app copies them into its folder the first time it opens, set up for every game. Never share
#       that APK.
#
# Signing: with KEYSTORE, KEYSTORE_PASSWORD, KEY_ALIAS and KEY_PASSWORD the APK is signed with that key. They are
# also read from the file SIGNING names (default ~/.config/spelunkomp/android-signing.env, outside the repository),
# as lines of NAME="value". Without either it is signed with the Android debug key, which is fine for sideloading
# your own build; RELEASE=1 refuses that, because a published APK has to carry the same key every time or it won't
# install over the one before.
set -eu
. "$(dirname "$0")/lib.sh"
BUILD="$ROOT/build/release-android"
STAGED="$BUILD/staged"
SIGNING="${SIGNING:-$HOME/.config/spelunkomp/android-signing.env}"
if [ -z "${KEYSTORE:-}" ] && [ -f "$SIGNING" ]; then
    . "$SIGNING"
fi
if [ -n "${RELEASE:-}" ] && [ -z "${KEYSTORE:-}" ]; then
    echo "error: a release APK needs the signing key ($SIGNING)" >&2
    exit 1
fi
if [ -n "${RELEASE:-}" ] && [ -n "${ROM_SETS:-}" ]; then
    echo "error: a release APK can't have ROM sets packed in" >&2
    exit 1
fi

if [ -z "${ANDROID_HOME:-}" ]; then
    for candidate in /opt/homebrew/share/android-commandlinetools "$HOME/Library/Android/sdk" "$HOME/Android/Sdk"; do
        [ -d "$candidate/platforms" ] && ANDROID_HOME="$candidate" && break
    done
fi
[ -d "${ANDROID_HOME:-}" ] || { echo "error: no Android SDK found; set ANDROID_HOME" >&2; exit 1; }
export ANDROID_HOME
NDK="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/27.2.12479018}"
[ -f "$NDK/build/cmake/android.toolchain.cmake" ] || { echo "error: no NDK 27.2 in $NDK" >&2; exit 1; }
if [ -z "${JAVA_HOME:-}" ] && [ -d /opt/homebrew/opt/openjdk@17 ]; then
    export JAVA_HOME=/opt/homebrew/opt/openjdk@17
fi
STRIP=$(ls -d "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-strip | head -1)
# Profile-guided optimisation: PGO_USE=folder builds the libraries with the counts made on Linux
# (tools/release/run-pgo-linux.sh, docs/RELEASING.md). Clang's counts are of the source, not of the processor, so the
# Linux x86_64 ones fit ARM64: SaiDaiOuJou's boss 8% faster, Muchi Muchi Pork's 7% (Linux ARM64 on a MacBook Air,
# M4, 2026-11-27, the same state hashes). There is no instrumented Android build (PGO_GENERATE): no Android device
# plays the games for counts.
if [ "${PGO_GENERATE:-}" = 1 ]; then
    echo "error: no instrumented Android build: the counts are made on Linux (run-pgo-linux.sh)" >&2
    exit 1
fi
NDK_BIN=$(dirname "$STRIP")
PATH_BEFORE=$PATH
PATH="$NDK_BIN:$PATH" # (the counts merged by the NDK's llvm-profdata, for its clang)
pgo_prepare "$NDK_BIN/clang" run-pgo-linux.sh
PATH=$PATH_BEFORE
ANDROID="-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_static -Wno-deprecated"

# SDL 3 as a shared library (SDL's Java half loads libSDL3.so by name)
sdl_source
if [ ! -f "$DEPS/sdl-android/lib/libSDL3.so" ]; then
    setup_ccache release-android
    cmake -S "$DEPS/SDL3-$SDL_VERSION" -B "$DEPS/sdl-android-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DCMAKE_INSTALL_PREFIX="$DEPS/sdl-android" $CCACHE_ARGS $ANDROID >/dev/null 2>&1
    cmake --build "$DEPS/sdl-android-build" --target install >/dev/null
fi
# (the NDK's toolchain file warns of CMake versions to come, at length: that is left out of what is shown)
# (the flags given every time, empty without PGO: a build folder's cache would keep the last ones)
if ! build_programs "$BUILD" $ANDROID -DSDL3_DIR="$DEPS/sdl-android/lib/cmake/SDL3" -DCMAKE_C_FLAGS="$PGO_FLAGS" \
    -DCMAKE_CXX_FLAGS="$PGO_FLAGS" -DCMAKE_SHARED_LINKER_FLAGS="$PGO_FLAGS" > "$BUILD.log" 2>&1; then
    grep -v "^CMake Warning\|^  \|^$\|^This warning\|^Call Stack\|Wprofile-instr-unprofiled" "$BUILD.log" | tail -40 >&2
    echo "error: the build failed (all of it in $BUILD.log)" >&2
    exit 1
fi
grep ": built$" "$BUILD.log" || true
[ -z "${PGO_USE:-}" ] || pgo_report "$BUILD.log"

# The libraries: SDL's, the launcher's and each game's (stripped), and librashader's
LIBS="$STAGED/jniLibs/arm64-v8a"
rm -rf "$STAGED" && mkdir -p "$LIBS" "$STAGED/assets"
cp "$DEPS/sdl-android/lib/libSDL3.so" "$BUILD/launcher/lib$EXE-launcher.so" "$LIBS/"
for game in $GAMES; do
    cp "$BUILD/game-$game/lib$game.so" "$LIBS/"
done
"$STRIP" --strip-unneeded "$LIBS"/*.so
add_notices "$STAGED/assets" "$BUILD/launcher"
# (the Graphics page's shaders, port/shading.h: librashader, with OpenGL ES, and the presets, which GameActivity.java
# unpacks into the app's folder)
add_librashader "$LIBS" librashader.so "$STAGED/assets" "$STAGED/assets/licenses" --android "$NDK"
APK="$ARCHIVE-android.apk"
if [ -n "${ROM_SETS:-}" ]; then
    # (LauncherActivity.java unpacks assets/roms/ and sets up the games roms/games.txt names)
    mkdir -p "$STAGED/assets/roms"
    for game in $GAMES; do
        set=$(sed -n 's/^rom_set = //p' "$ROOT/games/$game/game.cfg" | head -1)
        [ -f "$ROM_SETS/$set.zip" ] || { echo "error: no $ROM_SETS/$set.zip for $game" >&2; exit 1; }
        cp "$ROM_SETS/$set.zip" "$STAGED/assets/roms/"
    done
    printf '%s\n' $GAMES > "$STAGED/assets/roms/games.txt"
    APK="$ARCHIVE-android-with-roms-DO-NOT-SHARE.apk"
    echo "packing the ROM sets in $ROM_SETS: a development build, not to be shared"
fi

set -- -PstagedDir="$STAGED" -PsdlDir="$DEPS/SDL3-$SDL_VERSION"
if [ -n "${KEYSTORE:-}" ]; then
    set -- "$@" -PkeyStore="$KEYSTORE" -PkeyStorePassword="$KEYSTORE_PASSWORD" -PkeyAlias="$KEY_ALIAS" -PkeyPassword="$KEY_PASSWORD"
fi
(cd "$ROOT/android" && ./gradlew --console=plain -q assembleRelease "$@")
cp "$ROOT/android/app/build/outputs/apk/release/app-release.apk" "$OUT/$APK"
[ -n "${ROM_SETS:-}" ] || check_archive "$OUT/$APK"
echo "wrote $OUT/$APK ($(du -h "$OUT/$APK" | cut -f1))"
