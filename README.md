# Spelunkomp

Unofficial native ports of 26 of Cave's arcade shooting games (31 versions), from three of the boards they ran on (the CV1000, Cave's
68000 board and IGS's PGM), in one collection with one launcher, for macOS, Linux (the Steam Deck included), Windows
and Android. Each game's code is translated to C++ ahead of time and runs on the board's hardware written fresh,
with its board's speed and slowdown, and training tools on top. You supply the games.

**[Join me on Discord](https://discord.gg/FSC9GZ6nm)**

<p align="center"><img src="docs/screenshots/titles.jpg" width="100%" alt="The title screens of all 26 games, as the port draws them (the other five versions share their titles): Mazinger Z, DonPachi, Air Gallet, DoDonPachi, Dangun Feveron, ESP Ra.De. and Guwange; DoDonPachi II, Dai-Ou-Jou and Black Label, Ketsui, Espgaluda, Mushihimesama and Ibara; Espgaluda II, Pink Sweets, Ibara Kuro Black Label, Futari 1.5, Muchi Muchi Pork! and Deathsmiles; Futari Black Label, Deathsmiles MegaBlack Label, Dai-Fukkatsu 1.5 and Black Label, Akai Katana and SaiDaiOuJou"></p>

> **Beta.** Every game plays with sound and every feature below, with its board's speed and slowdown on by default.
> The games are close to the boards, and where they have been measured the numbers are under [Accuracy](#accuracy);
> we're working on the rest. Not every game has been played through by hand, and the Android app has run only in
> Android's emulator, not yet on a phone. Expect rough edges, and please [tell us what you find](#tell-us-what-is-different).

## Accuracy at a glance

buffi's [CV1000 benchmark](https://buffis.com/arcade/benchmarking-cv1000-pcb-vs-mame-and-mister/), run on a real
board (an Ibara PCB) and in the port with its board's slowdown on, as it ships. The PCB, MiSTer and MAME columns are
buffi's own results (MAME is master built from source, as the newest CV1000 code is in no release yet; today's
master gives the same numbers); thanks to buffi for the benchmark and the board's numbers.

| test | PCB | Spelunkomp | MiSTer | MAME master | Spelunkomp vs PCB (+ faster) |
|---|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 4850 | 5650 | 4875 | +1.0% |
| RAMP 8X8 STILL (@60) | 9085 | 9085 | 9085 | 9123 | 0 |
| RAMP 32X32 SPR (@60) | 1170 | 1170 | 1170 | 1175 | 0 |
| RAMP CPU LOGIC (@60) | 1503 | 1512 | 1769 | 2135 | +0.6% |
| RAMP GAME (@60) | 1694 | 1694 | 1715 | 1701 | 0 |
| BLIT LATENCY (µs avg) | 5 | 5 | 4 | 5 | 0 |
| 8X8 SPRITES (kdraw/s) | 603 | 603 | 604 | 604 | 0 |
| 32X32 SPRITES (kdraw/s) | 77 | 77 | 77 | 77 | 0 |
| SCREEN COPY (Mpix/s) | 90.6 | 90.7 | 90.6 | 91.0 | +0.1% |
| ALPHA 32X32 (kdraw/s) | 77 | 77 | 77 | 77 | 0 |
| UPLOAD TO VRAM (kpix/s) | 5632 | 5653 | 8198 | 5665 | +0.4% |
| GAME FRAME (fps) | 171.3 | 171.4 | 171.2 | 171.8 | +0.1% |
| BLITTER PAIN (ms/round) | 19.7 | 20.0 | 19.7 | 19.9 | -1.5% |
| CPU INTEGER (kiter/s) | 3656 | 3656 | 3937 | 4653 | 0 |
| CPU DIVIDE (kdiv/s) | 1176 | 1190 | 1256 | 1304 | +1.2% |
| CPU MEMORY (MB/s) | 20.1 | 19.9 | 19.7 | 14.7 | -1.0% |
| CPU CACHE HIT (Mload/s) | 17.0 | 17.0 | 20.4 | 17.0 | 0 |
| CPU FLUSH (µs/flush) | 886 | 885 | 734 | 604 | +0.1% |
| CPU LIST BUILD (kdraw/s) | 352 | 355 | 431 | 368 | +0.9% |
| CPU QUICKSORT (sorts/s) | 140 | 149 | 155 | 142 | +6.4% |

Within 1.5% of the PCB on 19 of the 20 tests (BLITTER PAIN at 1.5% slower), the same as the PCB on 9; CPU QUICKSORT, at
+6.4%, is not explained yet. The SH-3's caches, bus and pipeline and the blitter are timed in the port, and fitted to
buffi's results on the board. More, for all three boards, under [Accuracy](#accuracy); how shmupmame 5.4 does in the
same board scenes: [docs/BOARD-VIDEO.md](docs/BOARD-VIDEO.md#shmupmame-in-the-same-scenes-2026-11-23).

**No game files are included.** You need your own copy of each game: its arcade ROM set.

This project is not affiliated with, endorsed by, or sponsored by Cave, IGS or any of the games' rights holders.
The games' names are their owners' trademarks.

## The games

| Board | Games |
| --- | --- |
| CV1000 | Mushihimesama (and its 2011 Cave Matsuri Ver 1.5), Ibara, Ibara Kuro Black Label, Espgaluda II, Pink Sweets, Mushihimesama Futari (Ver 1.0, Ver 1.5, Black Label and its 2009 Another Ver), Muchi Muchi Pork!, Deathsmiles and MegaBlack Label, DoDonPachi Dai-Fukkatsu (Ver 1.0, Ver 1.5 and Black Label), Akai Katana, DoDonPachi SaiDaiOuJou |
| Cave's 68000 board | DonPachi (Japan Ver 1.01 and US Ver 1.12), DoDonPachi, Dangun Feveron, ESP Ra.De., Guwange; Mazinger Z and Air Gallet, which other companies made for it |
| IGS's PGM | DoDonPachi Dai-Ou-Jou and Black Label, Ketsui, Espgaluda; DoDonPachi II: Bee Storm, which IGS made under licence from Cave |

<p align="center"><img src="docs/screenshots/bosses.jpg" width="100%" alt="Eight boss fights from the collection, as the port draws them, the screen full of bullets"></p>

<p align="center"><img src="docs/screenshots/ddp-attract-hitboxes.gif" width="49%" alt="DoDonPachi's two-player attract demo in motion with the hit boxes shown: both ships, their shots, the enemies' boxes and a dot in each bullet"><br><em>DoDonPachi's attract demo, with the hit boxes on</em></p>

## What you need

- macOS 13 or later (Apple silicon or Intel), Linux x86_64 or ARM64 (the Steam Deck and Steam Machine included),
  Windows 10 or 11, 64-bit, or Android 9 or later on a 64-bit ARM phone or tablet with Vulkan.
- Each game's arcade ROM set, as a zip named for the version (`mushisam.zip`, `futari15.zip`): each entry in the
  launcher takes its own version's set, which its page names. A game with several versions has an entry for each.
  MAME's split and merged sets work too: a clone's zip with its parent's (`ddpdfk.zip` for `ddpdfk10.zip`) beside it.
- A keyboard, or one or two gamepads; on Android the screen's own controls until a gamepad is used: the game's
  buttons, and the ship moving as your finger moves anywhere else on the screen, from where the ship is (so it stays
  in sight), as some of Cave's own phone ports played. An on-screen stick instead is a setting away.

## Playing

1. **Get the release** for your system from the [Releases page](https://github.com/tmcleroy/spelunkomp/releases)
   and unpack it (the release's page says how to open it the first time: the programs are not signed by Apple or
   Microsoft).
2. **Supply your own games**: on the launcher's **Games** page, press **Choose folder…** and pick the folder with
   your ROM sets (or drop a set on the window). On Android (install `Spelunkomp-android.apk`), **Add ROM sets…**
   picks the zips, which are copied into the app.
3. **Press Play** beside a game.

## Features

- **Training mode in every game**: a practice point (the game kept as it is and brought back at once, as often as you
  like) with a replay of your last attempt, rewind through the last half minute, fast forward, an input display.
  From each game's own memory: a start at any stage or straight at its boss, a ship nothing can hit, bombs and ships
  that never run out, and the hit boxes as the game itself tests them. All off unless you turn them on.

  <p align="center"><img src="docs/screenshots/rewind-ddp.gif" width="49%" alt="DoDonPachi's stage 5 boss: the ship is hit and explodes, the rewind runs the scene backwards about three seconds (a REWIND note at the top, the explosion undone and the score counting down), then play goes on and the ship gets through"><br><em>Rewind: hold R or the left trigger and the game runs backwards; let go and play on</em></p>

- **Online play** with rollback: a game's two-player mode with someone on another machine, by a code or from the
  lobby. Nothing to set up on either router ([more](docs/ONLINE.md)).
- **No added lag**: the controls are read the moment the game reads them, and run-ahead takes out the frames of lag the
  games themselves have ([more](docs/LAG.md)).
- **Drawn by the GPU, exact to the pixel**, with integer scaling; the picture stood up for an ordinary screen, or as
  the cabinet has it for a screen on its side; RetroArch's CRT and scanline shaders.
- **An arcade monitor**: full screen at the board's own resolution (or a super resolution), synced to the screen
  with a frame delay; on Windows the mode made at the board's exact rate with Switchres, as GroovyMAME does.
- **The board's slowdown**, on unless you turn it off: the CPU's time to the cycle, the CV1000's cache, bus and
  blitter, and the PGM's video memory, so the busy scenes slow down where a board does ([Accuracy](#accuracy)).
- **Slow motion and frame advance** for study: down to 1/8 speed, or a frame at a time, the game's frames unchanged.
- **Attract mode**: the game's own show on a loop, as a cabinet nobody is playing.
- **The board's settings and autofire**: a game's own test-menu settings (difficulty, lives, extends) on a Board page,
  without the test menu, and autofire for each button at 30, 20 or 15 shots a second, online too.
- **Scene export**: any moment of any game saved as its layers, each a see-through PNG (the tile layers and the sprites
  by priority; the CV1000's draws in the order the game made them), or a few seconds of them, to open in 3D in the scene
  diorama ([more](docs/FEATURES.md#scene-export)).
- Free play, straight to the title, save states, faster play (up to 8x).

<p align="center">
  <img src="docs/screenshots/ddp-boss-hitboxes.gif" width="49%" alt="DoDonPachi's stage 5 boss in motion with the hit boxes shown: the ship's small yellow core and its laser's box weaving through about two hundred bullets, each with a white dot, under the boxes of the boss's parts">
  <img src="docs/screenshots/training-mode.jpg" width="49%" alt="The in-game menu's Training mode page">
</p>

<p align="center"><img src="docs/screenshots/crt-shader.jpg" width="80%" alt="DoDonPachi as the board draws it, and through the CRT shader"></p>

Every feature and which games have it: [docs/FEATURES.md](docs/FEATURES.md).

## Accuracy

Accuracy to the arcade board comes first, and it is on by default: each game runs at its board's speed, with its
board's slowdown. Faster or smoother play is a choice in the menu. Where each board stands, measured against real
boards and the FPGA cores made from them:

- **The CV1000**: buffi's benchmark, on a real board, is under [Accuracy at a glance](#accuracy-at-a-glance): a mean
  error of 0.7% against the board (current MAME's 7.5%, shmupmame 5.4's 27.7%). In play, against two Mushihimesama boards filmed at 60 pictures a second,
  stage 3 is late 303 frames on the port and 356 and 384 on the boards. A Futari Black Label board on video is late
  about 1.16 times as often as the port running the same 2007 Black Label program, on a busier route than the port's
  (1.17 times with the 2009 program: the version is not the difference).
- **Cave's 68000 board**: the 68000 runs to the cycle as fx68k, the cycle-exact 68000 in Cave's MiSTer core, and the
  frame is the 57.44 Hz measured on a board. Run beside that core, simulated, DoDonPachi, Guwange, ESP Ra.De. and
  Dangun Feveron keep every byte of the game's memory the same at every frame compared, play included, and drop the
  same frames (Guwange 376 in 7,500 on both, ESP Ra.De.'s stage 6 672 in 3,000 on both). That is with the sound chip
  timed as the core times it, from the CPU's clock; as shipped the port keeps the board's own sound crystal, and
  DoDonPachi's run then parts from the core's at frame 3,128. DonPachi parts at 2,507, in its random numbers only.
  Mazinger Z and Air Gallet are not in the core.
- **IGS's PGM**: measured against a real board with the PGM MiSTer core's test ROMs, whose results on a board its
  author published: the 68000 and the sprite copies within 0.005% of the board, the video memory's waits within 0.5%
  on 231 of 268 results. Run beside that core, simulated, through each game's heaviest slowdown, Ketsui, Espgaluda and
  Dai-Ou-Jou Black Label drop the same frames as the core; Dai-Ou-Jou's stage 5 is two frames apart in 8,250.

Not compared yet: the sound against a real board, and the input lag against a real board. The details, and how each
number was measured, are in [docs/SLOWDOWN.md](docs/SLOWDOWN.md), [docs/BOARD-VIDEO.md](docs/BOARD-VIDEO.md) and
[docs/PGM.md](docs/PGM.md).

## Tell us what is different

If a game feels or looks different from the board, we want to hear it, above all from strong players and from
people who own the boards. [Open an issue](https://github.com/tmcleroy/spelunkomp/issues), or tell us on
[Discord](https://discord.gg/FSC9GZ6nm), with:

- the game, and the release version you played;
- where: the stage, and the moment (a boss, a pattern, a stretch of slowdown);
- what differs: the slowdown, the timing of a pattern, the controls' response, the picture, the sound;
- a video if you can (a link to it): the port's, and the board's of the same place if you have one.

Please don't attach ROM sets, save states or anything else from the game.

## Controls

| | Keyboard | Gamepad |
| --- | --- | --- |
| Move | arrows | d-pad |
| Buttons 1 to 4 | Z X C V | A B X Y (south, east, west, north) |
| Start, coin | Enter, 5 | Start, Select |
| In-game menu | Esc or F1 | Guide, or Start and Select together |
| Save, load a state | F5, F9 | (the menu) |
| Pause, a frame on | P, O | (stopped) right trigger |
| Set the practice point, back to it, replay | `\`, Backspace, `]` | right shoulder, left shoulder, right stick click |
| Rewind (hold), fast forward (hold) | R, Tab | left trigger, right trigger |
| Full screen | F11 | |
| Screenshot, save scene, record a scene clip | F12, Shift+F12, Ctrl+F12 | (the menu) |

The first two gamepads are players 1 and 2. Everything can be remapped on the launcher's Controllers page.

## Building

CMake, Ninja, a C++20 compiler, zlib and SDL 3. `make games` and `make launcher` build every game's program and the
launcher; with your own ROM sets, the translators make each game's native code first. See
[docs/BUILDING.md](docs/BUILDING.md). To install locally, use the release zip or `tools/release/macos.sh`; `make app`
needs translations.

## How it works

A static recompilation: the games' SH-3 and 68000 code (and their Z80 sound drivers) is translated to C++ ahead of
time and checked block by block against an interpreter, on models of the CV1000, Cave's 68000 board and IGS's PGM
written for this project. See [docs/TOOLKIT.md](docs/TOOLKIT.md).

## Licence and credits

GPL-3.0-only ([LICENSE](LICENSE)). The boards' hardware was written with MAME's drivers as the reference (BSD-3-Clause),
and checked against Cave's and IGS's MiSTer cores (fx68k among them), ika-musume's HyperSquid3 SH-3 and buffi's
CV1000 research and benchmark; the FM chips are Aaron Giles's ymfm; the shaders are
RetroArch's, through librashader; the Windows programs make arcade monitor modes with Switchres; the launcher uses
SDL 3 and Dear ImGui. Everything taken from another project,
and its licence, is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Contributions are welcome: [CONTRIBUTING.md](CONTRIBUTING.md).
