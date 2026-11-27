# Building

Spelunkomp builds from this repository alone with CMake, Ninja, a C++20 compiler, zlib and SDL 3. What it cannot
hold is the games: each game's program is built with that game's code translated to C++, which is made from your
own ROM set. Without it a game's program still builds and plays, slower, in the board's interpreters (what CI
builds).

## What you need

| System | Packages |
| --- | --- |
| macOS 13 or later | Xcode's command line tools; `brew install cmake ninja sdl3` |
| Linux | `cmake ninja-build g++ zlib1g-dev` and SDL 3 (built from its source where the system has none: the commands are in `.github/workflows/ci.yml`; the X11, Wayland and sound headers listed there too) |
| Windows | MSYS2's MINGW64: `pacman -S make mingw-w64-x86_64-{gcc,cmake,ninja,python,sdl3,zlib}` |
| Android (built on a Mac) | `brew install cmake ninja openjdk@17 android-commandlinetools rustup`, then `sdkmanager "platforms;android-34" "build-tools;34.0.0" "ndk;27.2.12479018"` |

Python 3 runs the translators. The Graphics page's shaders need librashader (`tools/librashader.sh`, which builds it
with Rust's cargo; without it the game runs without shaders).

## Without a game (what CI builds)

```sh
make games       # every game's program, build/<id>/<id> (or: make GAME=mushisam)
make launcher    # the collection's launcher, build/collection/build/spelunkomp-launcher
make check       # the audit, the copy of recomp-common, and both started with no game and no window
```

## With your ROM sets

Each game's set is a zip named by the game's id (`mushisam.zip`, `ddonpach.zip`; the launcher's page for a game
names the set and its version: only that version works). Translate a game's code once into `work/<id>/gen` (kept out of git), then build it as above:

```sh
GAME=mushisam STATIC=4 FOUND=1 tools/translate.sh path/to/mushisam.zip   # a CV1000 game
GAME=ddonpach tools/translate68k.sh path/to/ddonpach.zip                 # a 68000 board or PGM game
GAME=ddpdoj tools/translatez80.sh path/to/ddpdoj.zip                     # a PGM game's Z80 sound driver too
make GAME=mushisam
```

`tools/explore.sh` finds code a CV1000 game runs that the first translation missed. The checks that prove a
translation (CONTRIBUTING.md) take the same set.

The collection (every game and the launcher in `build/collection/`) is `make collection`; the macOS app made from it
is `make app` (`tools/make_app.sh --collection`). Both need every game's translation: they stop, naming each game
without one and the command that makes it (`tools/check_gen.sh`). `INTERPRETED=1` builds them anyway, marked: the app
is `Spelunkomp (interpreted).app`, and such a program logs `interpreted build: no translated code` and has a note on
its launcher row. To install locally, use the release zip or `tools/release/macos.sh`.

## Release builds

A release is built on the machine of someone with every game's set, never by CI (docs/RELEASING.md). The scripts:

- `tools/release/macos.sh`: `Spelunkomp-macOS.zip`, one app for Apple silicon and Intel, SDL 3 built in, signed ad
  hoc (Gatekeeper warns the first time it is opened).
- `tools/release/linux.sh`: `Spelunkomp-linux-x86_64.tar.gz` or `-arm64`, in the container of `tools/release/Dockerfile`.
- `tools/release/windows.sh`: `Spelunkomp-windows-x64.zip`, cross-compiled with llvm-mingw's clang (pinned in `tools/release/lib.sh`; `WINDOWS_CC=gcc`: MinGW-w64 gcc) in the same container.
- `tools/release/android.sh` (`make android`): `Spelunkomp-android.apk`, one app for 64-bit ARM (Android 9 and
  later). Each game is its own library (`lib<id>.so`), loaded only when that game is played, in a process of its own;
  the launcher is a library too, and SDL 3 and librashader are built from source for it. Signed with the Android debug
  key unless a key is given (the script's first lines; `RELEASE=1` refuses the debug key). `GAMES="mushisam ddonpach"`
  builds some of the games; `ROM_SETS=<folder>` packs your own sets in, for your own phone or the emulator only (never
  share that APK).
- `tools/release/all.sh`: all of them on a Mac with Docker, and `SHA256SUMS.txt`. The Android app needs the release
  signing key (`tools/release/android.sh`'s first lines); without it, leave `android` out of `TARGETS`.

Each script's first lines say what it needs. `NO_GAME_CODE=1` builds any of them without translated code.
