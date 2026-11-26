# Releasing

Versions follow `MAJOR.MINOR.PATCH`, with `-beta.N` or `-rc.N` while a version is not final, one for the whole
repository: the toolkit, the launcher and every game. A tag is `v` and the version (`v0.1.0-beta.1`). A release
with a `-` in its version is marked as a pre-release on GitHub. Versions stay beta until every platform's download
has been played by hand.

## What a release is

| File | For |
| --- | --- |
| `Spelunkomp-macOS.zip` | macOS 13 or later, Apple silicon and Intel (one app; signed ad hoc, not notarized) |
| `Spelunkomp-linux-x86_64.tar.gz` | Linux x86_64, Steam Deck, Steam Machine |
| `Spelunkomp-linux-arm64.tar.gz` | Linux ARM64 (aarch64) |
| `Spelunkomp-windows-x64.zip` | Windows 10 and 11, 64-bit |
| `Spelunkomp-android.apk` | Android 9 or later, 64-bit ARM (signed with the project's release key, kept outside the repository: every release must carry the same key) |
| `SHA256SUMS.txt` | The five downloads' checksums (not the manifests', which carry their archives' own) |
| `update-macos.txt`, `update-linux-x86_64.txt`, `update-linux-arm64.txt`, `update-windows-x64.txt` | The launcher's updates: each system's manifest |

Each holds the launcher, every game's program and the licences. No ROM, graphics or sound is in them: the player
supplies their own ROM sets. **Each game's program does hold that game's code translated to C++ and compiled**, as a
build from source does. That means a download can only be built by someone with every game's ROM set, on their own
machine: CI never builds or holds one. `tools/release/check_archive.sh` checks each archive before it is packed (no
game files, no emulator's name and no home path in any program, the licences there).

To install locally, use the release zip or `tools/release/macos.sh`; `make app` needs translations (it stops, naming
each game without one and its translate command; `INTERPRETED=1` builds an app marked `(interpreted)`).

**The launcher updates itself from these** (`recomp-common/launcher/src/update.h`), from 0.1.0-beta.1 on: when it
opens it reads the repository's releases, takes the newest with a manifest for its system (a beta only while it is
a beta itself), and compares the files the manifest lists with its own. **Update**, on the update card at the top of
the Games page, downloads that system's archive (the same file as a player's download), checks it against the
manifest's SHA-256, unpacks it, checks each file that changed, and only then puts them in place. Only a copy whose
launcher is where the manifest has it updates itself (the app's `Contents/MacOS/spelunkomp-launcher`, or
`spelunkomp-launcher` at the unpacked folder's top); a build from source says to download the release instead. Each
release script writes its system's manifest after its archive (`tools/release/lib.sh`, `update_files`), and
`tools/release/all.sh` checks that each names its archive as it is and this version. A release without them is not
offered to the launchers, and a release whose `version` line is not its tag is refused by them. The Android app has
no manifest: it takes the newest release's APK, checks it against `SHA256SUMS.txt` and hands it to Android's
installer, which asks the player and installs it over the app (the same release key: settings, saves and ROM sets
stay). So `SHA256SUMS.txt` must list the APK.

The release's text is `tools/release/notes.sh`'s: the paragraph the version's section of `CHANGELOG.md` begins
with (the script stops if there is none), the downloads, how to install on each system, and the section's list.

## Steps

1. **In a pull request:** set `VERSION`, move what is under "Unreleased" in `CHANGELOG.md` to a new `## <version>`
   section that begins with a paragraph saying what is in the release. `make check` passes, and on a machine with
   the ROM sets the checks of CONTRIBUTING.md pass for the games the release changes (`tools/gate.sh ROMS` for all
   of them). Merge.
2. **Tag** the merged commit on `main` and push the tag:

   ```sh
   git switch main && git pull
   git tag -a "v$(cat VERSION)" -m "Spelunkomp $(cat VERSION)"
   git push origin "v$(cat VERSION)"
   ```

   The `Release` workflow (`.github/workflows/release.yml`) checks that the tag is `VERSION`, runs the audit, and
   opens a **draft** release whose notes are the changelog's section. It fails if the changelog has no such
   section. It builds nothing.
3. **Build the downloads** from the tagged commit, on a Mac with Docker, every game's translated code in
   `work/<id>/gen` and the ROM sets (`ROMS=folder`):

   ```sh
   git status --short            # must print nothing: build exactly what was tagged
   PGO=1 PGO_KEPT=../spelunkomp-pgo/games tools/release/all.sh
                                 # macOS, Linux x86_64 and ARM64, Windows, Android, then SHA256SUMS.txt and the
                                 # update-<system>.txt manifests, in build/release/
   ```

   **Linux x86_64** is built with clang (`LINUX_CC=gcc`: gcc, as up to 0.4): the same frames and hashes, about 5%
   faster, and 15-18% with a profile made on a Steam Machine or Deck before the command above.
   `PGO_GENERATE=1 TARGETS=linux-x86_64 tools/release/all.sh` writes the instrumented
   `Spelunkomp-linux-x86_64-pgo-train.tar.gz`; unpacked on the device, in its game session,
   `Spelunkomp/run-pgo-linux.sh ROMS` plays every game (about two minutes a CV1000 game) and writes
   `~/spelunkomp-pgo/raw.tar.gz`: the counts, with `pgo-made.txt` (the commit, games and clang they were built from).
   Unpacked into a folder, `PGO_USE=<that folder>` added to the command above builds Linux x86_64 with them (a folder
   outside the worktree is mounted read only). The build prints `pgo-made.txt`, warns where its commit or clang is
   not the build's, and counts each game's files that had no counts. `PGO=1` with no `PGO_USE` stops before building
   anything; `PGO_LINUX=0` ships Linux without a profile, and says so. Linux ARM64 is built without one.

   `PGO=1` builds the macOS programs with profile-guided optimisation: `tools/release/pgo.sh` first builds every
   game's program instrumented and plays each (stage 1 with either renderer and with run-ahead, and stage 1's boss
   with the slowdown on and off: about three minutes a game, an hour and a half for them all on an M4), then
   `macos.sh` builds with the profile (`build/pgo/spelunkomp.profdata`, and what it was made from beside it). The
   games play exactly as without it (the same hash logs and pictures); only the code is laid out for where the time
   goes. Windows, Android and Linux ARM64 are built as before; without `PGO=1` macOS is too.

   Each game's profile is kept (in `PGO_KEPT`, a folder outside the worktree, so the next release's new worktree
   finds them) with what it was made from, and a game is played again only when its own code (its translated code,
   `games/<id>`), the compiler or the code the games share (the boards, the renderers, the program, recomp-common,
   third_party/, CMakeLists.txt, pgo.sh) changed: a release that changes a few games and nothing shared plays those,
   minutes instead of an hour and a half. `PGO_FULL=1` plays every game again; `PGO_KEEP_SHARED=1` keeps the games'
   profiles though the shared code changed (said, and macos.sh warns for each game whose profile is from another
   commit: what changed builds without a profile); `PGO_PLAN=1` says which games would be played, and stops.

4. **Try each download** before anyone else does: unpack it somewhere else, start the launcher, choose the ROM
   folder, play a game of each board with a gamepad and with the keyboard (the picture, the sound, a save state, the
   training tools). macOS on a Mac, the Linux one on a Steam Deck or Steam Machine, the Windows one on a Windows PC,
   the Android app on a phone or tablet.
5. **Run the memory check** against the new downloads on Windows, Linux and macOS: `tools/release/memcheck.sh`, from
   this Mac. Each system plays mmpork (CV1000-B), ddpsdoj (CV1000-D), ddonpach (Cave 68000) and ket (PGM) for 2
   minutes each, and ddpsdoj again with rewind on, all three systems at once (about 10 minutes). A run fails if its
   program goes over 1 GB (its VRAM, where it can be read: over 1.5 GB; the rewind run: the plain run's peak and
   450 MB), keeps climbing after its first 30 s (a steady 150 MB a minute or more; steps to plateaus are not), or does
   not end by itself; the script prints a table and exits non-zero on any failure. A run going wrong is stopped early
   (asked to quit first); a program held in its GPU driver ends that machine's runs, and the machine may need
   restarting before the next. Unpack each download on its machine first, with the ROM sets there; the
   Windows user logged in at the desktop (the games are started by a scheduled task in that session), the Linux
   machine in its desktop session (gamescope's on a Steam Machine or Deck). Say where each is:

   ```sh
   MEMCHECK_WIN=user@windows-pc MEMCHECK_WIN_DIR='C:\Games\Spelunkomp' MEMCHECK_WIN_ROMS='C:\Games\roms' \
   MEMCHECK_LINUX=user@steam-machine MEMCHECK_LINUX_DIR=Games/Spelunkomp MEMCHECK_LINUX_ROMS=Games/roms \
   MEMCHECK_MAC_APP=/path/to/Spelunkomp.app MEMCHECK_MAC_ROMS=../cv1k-roms \
       tools/release/memcheck.sh            # --dry-run: the commands, run on no machine; win, linux, mac: those only
   ```

   ssh must reach the two without a password (a key). The logs are in `build/memcheck/`; the script's first lines say
   the rest.
6. **Upload and publish:**

   ```sh
   cd build/release
   gh release upload "v$(cat ../../VERSION)" Spelunkomp-macOS.zip Spelunkomp-linux-x86_64.tar.gz \
       Spelunkomp-linux-arm64.tar.gz Spelunkomp-windows-x64.zip Spelunkomp-android.apk SHA256SUMS.txt \
       update-macos.txt update-linux-x86_64.txt update-linux-arm64.txt update-windows-x64.txt
   gh release edit "v$(cat ../../VERSION)" --draft=false
   ```

7. Add an empty "Unreleased" section to `CHANGELOG.md` for what comes next.

Keep `build/release-windows/symbols/` from the Windows build: its programs unstripped. A Windows crash writes the
calls that led to it into the game's `logs/game.log` as program+offset (`ddpdoj.exe+0x40ba8`), and
`tools/release/crash_where.sh build/release-windows/symbols/ddpdoj.exe 0x40ba8 ...` names their functions. Only the
same build's programs match.

To check the report on a new build, start a game with `SPELUNKOMP_TEST_CRASH_AT_FRAME=600` in its environment (on
Windows, `set SPELUNKOMP_TEST_CRASH_AT_FRAME=600` in a console, then start the game's program from it): it writes
through a null pointer after frame 600, and `logs/game.log` (or the console) should end with the exception, the
address it wrote (0) and the calls, which `crash_where.sh` names. Nothing happens unless the variable is set.

## If something is wrong after publishing

Don't replace the files of a published release. Fix it, raise the version (`-beta.N+1`, or the patch number) and
release again; say in the changelog what was wrong.

## Source for the binaries

The GPL-3.0 asks that the source of a binary be offered with it. The tag is that source: GitHub attaches it to
every release. Build only from a tagged, clean commit so that this stays true. The translated code is not part of
that source: it is made from the player's own ROM sets by the tools that are.
