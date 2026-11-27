#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp.
#
# Builds a macOS app bundle of the launcher and the game: tools/make_app.sh (run from the repository's top, after a
# build in build/<game>; GAME=mushisam by default), or of the collection: tools/make_app.sh --collection (after
# tools/collection.sh). The bundle holds the launcher (what opens), the game program, SDL3, and librashader with
# RetroArch's shader presets (tools/librashader.sh).
# No game data: the player points the launcher at their own ROM set. The game program does contain code translated
# from the game: whether a bundle is given to others is the port owner's decision.
# A game with no translated code (work/<id>/gen) would run interpreted, far slower: the script stops, naming each and
# the command that makes its translation (tools/check_gen.sh). INTERPRETED=1 builds the app anyway, marked: named
# "<name> (interpreted)", with the games in its Info.plist (SpelunkompInterpreted). To install Spelunkomp, use the
# release zip or tools/release/macos.sh. GAMES="a b": the collection with those games only (a test).
set -e
if [ "${1:-}" = "--collection" ]; then
    GAMES=${GAMES:-$(for cfg in games/*/game.cfg; do printf '%s ' "$(basename "$(dirname "$cfg")")"; done)}
else
    GAMES=${GAME:-mushisam}
fi
interpreted=$(tools/check_gen.sh --missing $GAMES)
if [ -n "$interpreted" ]; then
    [ "${INTERPRETED:-}" = 1 ] || { tools/check_gen.sh $GAMES; exit 1; }
    echo "warning: INTERPRETED=1: the app's programs for these games run on the interpreters: $interpreted" >&2
fi
mark=""
[ -z "$interpreted" ] || mark=" (interpreted)"
if [ "${1:-}" = "--collection" ]; then
    # The collection: its launcher and every game's program (tools/collection.sh builds them in build/collection)
    B=${B:-build/collection}
    NAME="Spelunkomp$mark"
    APP_ID=io.github.tmcleroy.spelunkomp
    EXE=spelunkomp-launcher
    APP="$B/$NAME.app"
    rm -rf "$APP"
    mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
    cp "$B/spelunkomp-launcher" "$APP/Contents/MacOS/"
    for id in $GAMES; do
        [ -f "$B/$id" ] && cp "$B/$id" "$APP/Contents/MacOS/"
    done
    # (the launcher's Tools: the scene diorama, tools/diorama)
    cp "$(dirname "$0")/diorama/diorama.html" "$APP/Contents/Resources/"
else
    GAME=${GAME:-mushisam}
    B=${B:-build/$GAME}
    # (one game's own launcher is built only when asked for)
    cmake -S . -B "$B" -G Ninja -DGAME="$GAME" -DLAUNCHER=ON > /dev/null && ninja -C "$B" > /dev/null
    CFG=games/$GAME/game.cfg
    value() { sed -n "s/^$1 = //p" "$CFG" | head -1; }
    NAME="$(value name)$mark"
    APP_ID=$(value app_id)
    EXE=$GAME-launcher
    APP="$B/$NAME.app"
    rm -rf "$APP"
    mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
    cp "$B/$GAME" "$B/$GAME-launcher" "$APP/Contents/MacOS/"
fi

# SDL3 from Homebrew, carried inside the bundle
for exe in "$APP/Contents/MacOS/"*; do
    otool -L "$exe" | awk '/libSDL3/ {print $1}' | while read -r lib; do
        base=$(basename "$lib")
        if [ ! -f "$APP/Contents/Frameworks/$base" ]; then
            cp -L "$lib" "$APP/Contents/Frameworks/$base"
            chmod u+w "$APP/Contents/Frameworks/$base"
            install_name_tool -id "@executable_path/../Frameworks/$base" "$APP/Contents/Frameworks/$base"
        fi
        install_name_tool -change "$lib" "@executable_path/../Frameworks/$base" "$exe"
    done
done

# The Display page's shaders (port/shading.h): librashader in Frameworks and the presets in Resources, made by
# tools/librashader.sh (which needs Rust); without them the games have no shaders, and draw as they always did
L="$APP/Contents/Resources/licenses"
mkdir -p "$L"
if LIBRA=$("$(dirname "$0")/librashader.sh"); then
    cp "$LIBRA/librashader.dylib" "$APP/Contents/Frameworks/"
    cp -R "$LIBRA/shaders" "$APP/Contents/Resources/"
    cp "$LIBRA/librashader-LICENSE.md" "$L/librashader-MPL-2.0.txt"
    cp "$LIBRA/librashader-LICENSE-GPL.md" "$L/librashader-GPL-3.0.txt"
else
    echo "warning: no shaders in $APP (tools/librashader.sh failed)" >&2
fi

# The licences of what the bundle holds (THIRD_PARTY_NOTICES.md)
cp LICENSE "$L/spelunkomp-GPL-3.0.txt"
cp THIRD_PARTY_NOTICES.md "$L/"
cp recomp-common/launcher/flags-LICENSE.txt "$L/flag-icons-MIT.txt"
for dep in imgui inter; do
    lic=$(ls "$B"/_deps/$dep-src/LICENSE* "$B"/*/_deps/$dep-src/LICENSE* build/*/_deps/$dep-src/LICENSE* 2>/dev/null | head -1 || true)
    [ -n "$lic" ] && cp "$lic" "$L/$dep-$(basename "$lic")"
done
sdl=$(ls "$(brew --prefix sdl3 2>/dev/null)"/share/licenses/SDL3/LICENSE.txt "$(brew --prefix sdl3 2>/dev/null)"/LICENSE.txt 2>/dev/null | head -1 || true)
[ -n "$sdl" ] && cp "$sdl" "$L/SDL3-zlib.txt"
VERSION=$(cat VERSION 2>/dev/null || echo 0.1); VERSION=${VERSION%%-*}

# (a build with games that have no translated code says which: tools/check_gen.sh)
plist_mark=""
[ -z "$interpreted" ] || plist_mark="
    <key>SpelunkompInterpreted</key><string>$interpreted</string>"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>$NAME</string>
    <key>CFBundleDisplayName</key><string>$NAME</string>
    <key>CFBundleIdentifier</key><string>$APP_ID</string>
    <key>CFBundleExecutable</key><string>$EXE</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>$plist_mark
</dict>
</plist>
PLIST

codesign --force --deep -s - "$APP" 2>/dev/null
if [ -n "$interpreted" ]; then
    echo "built $APP: INTERPRETED, no translated code for $interpreted"
else
    echo "built $APP"
fi
