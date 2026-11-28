# Changelog

All notable changes are written here, in the form of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- **Gadgets in twelve more games**: ESP Ra.De. (score items and how far the counter goes, the power shot's multiplier
  and its time, the barrier), Espgaluda (Kakusei's multiplier, Overmode's level, gold and its step), Espgaluda II
  (Overmode's level, gold), every set of Mushihimesama Futari (the rank as a graph and what it is made of), Dangun
  Feveron (the rank as a graph, the cyborgs on the way to the extend) both sets of DonPachi (the rank as a graph) and
  Deathsmiles and its MegaBlack Label (the level chosen for each stage), each with the score by stage. Read from the
  game's memory, never written: tools/check_gadgets.sh plays a game with them on and off and finds every frame the
  same.
- **SPEED**, a gadget every game has: the game's speed as a graph, so the slowdown shows where it comes.

## 0.6.0-beta.1

Gadgets beside the picture in eight games, Dai-Ou-Jou Black Label's scoring readouts and hidden bees, and faster
again: the slowdown's timing model up to 12% faster, the Android and Linux ARM64 downloads built with a profile,
and run-ahead's frames run again early when the controls change.

- **Gadgets**, panels in the space beside the picture in a wide window, as in M2's ports (Training mode > Gadgets,
  off by default): the rank as a graph, the chain and hyper gauges, chips, lard, zan, the next drops and medals, boss
  life, the second loop's terms and the score each stage made, read from the game's own memory, never written. In
  DoDonPachi, Dai-Ou-Jou and Black Label, Ketsui, Muchi Muchi Pork, Pink Sweets, Ibara and Mushihimesama; every game
  gets the controls held. None when the picture fills the window's width.
- **Gamepad buttons named as the pad prints them** on the Controllers page: on a Nintendo-layout pad A, B, X and Y
  are where its own letters are, not an Xbox pad's.
- **Show scenes folder** on Game > Scene export: opens exports/scenes in the file manager (not on Android).
- **Android and Linux ARM64 built with a profile**, as Linux x86_64 and Windows are: the same counts as Linux
  x86_64's, since clang counts the source, not the processor. About 8% faster at SaiDaiOuJou's boss and 7% at Muchi
  Muchi Pork's (Linux ARM64 on a MacBook Air), the same frames (the state hash each frame), and a smaller app.
- **The slowdown's timing model finds its paths faster**: the paths it keeps are one cache line each, and four
  times as many fit. The same counts, so the same slowdown, frames and picture. Headless from the boss on a Steam
  Machine: SaiDaiOuJou 11.8% faster, Muchi Muchi Pork 8.9%, Akai Katana (stage 1) 8.0%; on a Steam Deck 6.4%, 0.8%
  and 6.1%.
- **Run-ahead runs its frames again early** (run-ahead 1 and 2, offline): when the stick or a button changes, the
  frames ahead are run again in the wait before the next frame, not all at once at its start, so the change costs that
  frame one frame's work instead of three. The same frames, picture and sound. SaiDaiOuJou's boss on a Steam Machine
  at run-ahead 2 with the stick moving: from 60 to 70 frames late a minute to 2 or 3, the slowest 5% of frames from
  15.5 to 5.9 ms; Muchi Muchi Pork's from 154 to 53. SPELUNK_NO_AHEAD_EARLY=1 turns it off.
- **DoDonPachi Dai-Ou-Jou Black Label's scoring readouts**, what the game keeps and does not show: the most hits in a
  chain, the hyper gauge as a number (or, while a hyper is on, its level and the time it has left) with the hyper items
  held, the ranks as the game works them out (the bullets' speed, the game rank that sets their interval, the hyper
  rank), the bees taken in the stage, and the second loop's terms as the game tests them at the first
  loop's end (ships lost, bombs used, stages with all ten bees, no continue). Read from the game's memory while a stage
  is played, on the Training mode page.
- **Scoring readouts on the left or the right**: the switch is a choice of side (Off, Left, Right), so a game's own
  counters on one side stay clear. A settings file from before means the same (on is Left).
- **Hidden items, a kind of hit box**: where something a game hides is before it is found, as faint lines across the
  picture meeting there. Dai-Ou-Jou Black Label's hidden bees, read from its enemies' records as the stage puts them
  there: every stage and loop. On the Hit boxes page, its own colour or hidden, like the other kinds.

## 0.5.0-beta.1

Faster again, above all with run-ahead: the processor kept at speed between frames, run-ahead keeping only the
memory a frame changed, and the Windows and Linux downloads built with clang and a profile; and scene export, the
picture taken apart into its layers for cps2recomp's diorama.

- **Scene export** (Game > Scene export, Shift+F12; Ctrl+F12 a clip of 3, 5 or 10 seconds): the picture taken apart,
  each of the board's layers by itself as a see-through PNG (Cave's 68000 board: its tile layers and sprites by
  priority; the PGM: the sprites behind, the background, the sprites, the text; the CV1000: the frame's draws in the
  order the game made them), each sprite by itself, and the hit boxes and readouts as layers of their own, in
  cps2recomp's files: the launcher's Open diorama shows them in 3D (docs/FEATURES.md, Scene export).
- **Processor between frames**, on the Game page: the processor kept from turning its clock down while the program
  waits for the next frame, which on a Steam Machine let it fall from 4.8 to 3.7 GHz in play. Light (the default)
  spends the last 3 ms of each wait busy, Full all of it (more power and battery); with run-ahead at 2 frames or more,
  Light is Full. SaiDaiOuJou's boss on a Steam Machine: 7.8 ms a frame with it off, 6.0 Light, 5.45 Full; with
  run-ahead 2 and the stick moving, half as many frames late. The game plays exactly the same with any.
- **The Linux download is built with clang**, and can be built with a profile made on a Steam Machine or Deck
  (tools/release/linux.sh, run-pgo-linux.sh, docs/RELEASING.md): the same frames and hashes as gcc's build (three
  games of the three boards compared frame by frame), SaiDaiOuJou's busiest scene 4.4 ms a frame with gcc, 4.2 with
  clang, 3.7 with clang and a profile on a Steam Machine. gcc's own profiles, which this replaces, made it slower.
- **Run-ahead keeps only what a frame changed** (run-ahead 1 and 2, offline): a kept frame copied the board's whole
  16 MB of memory, now only the pages written since the last. The same frames; on a Steam Machine at SaiDaiOuJou's
  boss, run-ahead 1 with the stick moving went from 301 frames late a minute in 0.4.0 to 36 (with the clang build),
  and with it still costs no more than run-ahead off.
- **The Windows download is built with clang and a profile** made on a Windows PC (llvm-mingw, tools/release/
  windows.sh, run-pgo-windows.ps1): the same frames and hashes as gcc's build, SaiDaiOuJou's boss about 16% faster;
  run-ahead 1 with the stick moving from about 230 frames late a minute to about 45. It needs Windows 10 or later.

## 0.4.0-beta.1

Run-ahead that costs about one frame instead of two or three, so slowdown as the board and run-ahead fit on a Steam
Deck together; a macOS build laid out by profile, faster again; each game's board settings and autofire per button;
hidden content unlocked by a switch; a choice of which controller is which player; the menu turned with the picture;
and Espgaluda II's gems and Akai Katana's energy kept full in training.

- **The macOS app is built with profile-guided optimisation** (`PGO=1`, tools/release/pgo.sh): every game is played
  first with no window (stage 1 with either renderer and run-ahead, the boss with the slowdown on and off) and the
  code laid out by where their time goes. The same code, the same frames and hashes; SaiDaiOuJou's busiest scene about
  11-16% faster on an Apple M-series. Linux, Windows and Android are built as before.
- **Unlock hidden content**, a switch on the Game page: what the arcade boards' secret codes unlock can be chosen
  without entering them. Deathsmiles' Follett, Rosa and the canyon's extra stage; DoDonPachi Dai-Fukkatsu Ver 1.5's
  Strong Style, and Ver 1.0's Type-B ship and Strong Style; Mushihimesama Futari Ver 1.0's Ultra mode; Futari Black
  Label's Spiritual Larsa after God mode's last boss; Muchi Muchi Pork's Manpuku and Harahara modes. Off as the board,
  where the codes still work (the other versions have all of it from the start); Futari 1.0, as after its code, keeps
  Ultra unlocked in its EEPROM once a game is played with it. Online, the host's.
- **Run-ahead costs about one frame a frame** (run-ahead 1 and 2, offline). Its frames ahead were all run again at
  every frame; now they are run as the game's own and kept, and taken as the game's next frames while the controls
  stay as they were, so a frame shown costs one frame (and a kept moment) instead of two or three. On a Steam Deck in
  SaiDaiOuJou's first boss with the slowdown on and the controls changing every few frames: run-ahead 1 from 24.1 ms
  a frame to 13.6 (12.3 without run-ahead), run-ahead 2 from 35.0 to 13.6. The frames, the picture and the sound are
  the same as before (docs/LAG.md). `SPELUNK_NO_AHEAD_KEEP=1` turns it off.
- **Online rollback's sound is exact on the CV1000**: the sound decoders' history is kept with the rollback's moments,
  so frames run again make the same sound as frames run once (it was a little off after each rollback).
- **Which controller is which player** (recomp-common#99): the launcher's Controllers page gives each connected
  gamepad Automatic, Player 1, Player 2 or Not used (kept in settings.ini's `[controllers]`); the rest go to the
  players without one in the order they were connected, as before. A Steam Deck's own controls can be Not used, so
  two pads plugged into it play each other. Every connected gamepad is now opened, not only the first two.
- **The board's settings and autofire** (#56). A **Board** page (under Game, in the menu and the launcher) has each
  game's own test-menu settings: difficulty, lives, extends, continues, the board's own full-auto button and the like,
  put into the board's EEPROM (the PGM's battery RAM) as the board starts, the game's checksum made again where it has
  one. **As the board is set** (every one, to begin with) changes nothing, and the board's own are kept in the saves
  folder. In 22 of the 31 sets: every one of Cave's 68000 board and the PGM, and the CV1000 games that keep their settings in the EEPROM (the later ones keep them in the NAND flash, not yet) (docs/FEATURES.md). Online, the host's. **Autofire** for each button on the Controls page: 30, 20
  or 15 presses a second while held, by the board's frame number, so rollback, replays, rewind and run-ahead have the
  same; online each player's own, told to the other side as the match begins (online protocol 2: both players need
  this version).
- **Gems and energy kept full** (#55): two Training mode switches. Espgaluda II's "Gems kept full" holds the green
  gems at 500, the most the game allows, so Kakusei lasts as long as you want. Akai Katana's "Energy kept full" holds
  the energy gauge that Phantom form spends at the most the life bar leaves it. Akai Katana's Controllers page now
  names its buttons 3 and 4 as the game uses them: button 3 is full auto and button 4 is Phantom.
- **The menu turns with the picture** (#43). With a vertical game shown As the cabinet, for a monitor turned on its
  side, the in-game menu, the notes (SCREENSHOT SAVED, STOPPED, the online notes) and the readouts (input display,
  speed, FPS, input lag, the scoring readouts) were still drawn for an ordinary monitor and read sideways. A new
  **Menu orientation** setting on the Display page (the launcher's and the menu's) turns them with the picture: Auto,
  the default, follows the picture; Upright is as before; Top to the right, Upside down and Top to the left turn them
  for a monitor turned some other way. The menu is laid out for the turned screen's shape, up on the stick is up the
  list as you see it, and the mouse and touch find the row under them. The hit boxes stay on the picture. The game
  and its picture are the same frame for frame.
- **The test menu opens in Guwange.** F2 and the menu's Test menu command held the board's test switch for a moment
  only (the menu's, 10 frames), and Guwange looks for it held about half a second, so its test menu never opened. A
  press is now held a second at least. (On a Mac keyboard F2 is the brightness key unless fn is held: fn+F2, or the
  menu's command.)
- **Rewind works on the PGM games** (#48). Holding rewind in Dai-Ou-Jou Black Label, Ketsui, Espgaluda or DoDonPachi
  II only paused the game or went back a few frames: the board's saved state changes size with the number of sprites
  on screen, and each change threw rewind's history away. Rewind now keeps a state of any size. Save states are
  unchanged.
- **`make app` needs every game's translation**: it and `make collection` stop, naming each game without one and
  the command that makes it, instead of quietly building programs that run on the interpreters. `INTERPRETED=1` builds
  them anyway, marked: the app is `Spelunkomp (interpreted).app`, and such a program logs `interpreted build: no
  translated code` and has a note on its launcher row.
- **Run-ahead on the CV1000 costs a little less** (#46). Run-ahead runs the whole board again for each frame it looks
  ahead, so each frame of it costs about as much as the game itself; with Slowdown as the board on, that is too much
  for run-ahead 2 on many machines. The game's put-back no longer forgets the slowdown model's worked-out paths nor
  copies the 16 MB of main RAM back (SaiDaiOuJou), and run-ahead's frames no longer wait for the graphics chip from
  the fourth on. The game and the picture are the same frame for frame.

## 0.3.0-beta.1

A much lighter CV1000 (slower machines now hold 60), touch controls that follow the finger on Android, 99 save slots,
Spelunkomp's own online service, scoring readouts, a Renderer setting and the host's frame time, MAME's split sets,
and fixes for the lag readout's stutter and the trip to a stage or boss.

- **The CV1000 games run much lighter** (#18): the time the game spends waiting for its next frame is fast-forwarded
  exactly instead of run instruction by instruction, cutting 46-70% of the board-timing work in every CV1000 game.
  On a slower processor (an Apple M4's efficiency cores, about an Intel N150 or a Steam Deck) a busy frame went from
  21-27 ms to 10-17 ms, inside the 16.7 ms a frame needs; on a fast one it saves about a quarter. Slowdown is
  unchanged: every game was checked against the previous model on every frame and every skip, 193 runs over all 18
  CV1000 sets.
- **An FPS counter** (Display page, off by default): the frames this machine hands to the screen each second. Lower
  than the board's rate means the machine isn't keeping up; the game's own slowdown is the Speed readout's.
- **MAME's split and merged sets** (#29). A clone's split zip (`ddpdfk10.zip`, holding only the files that differ)
  is read with its parent's beside it (`ddpdfk.zip`; Dai-Ou-Jou's `ddp3.zip`, Bee Storm's `pgm.zip`), and a merged
  parent zip holding a clone's files is found for the clone. A split set missing its parent says which zip it needs.
- **The in-game menu answers a finger** on Android, whether the touch controls are on or off: tap a row to choose
  it, slide up or down to move through the rows. With the touch controls turned off and no gamepad, there was no
  way back to them; now a tap on Touch controls turns them on again. The touch controls' own buttons still work
  as before while the menu is open.
- **99 save-state slots** (were 9): the menu's Slot row and F6/F7 go from 1 to 99, around both ways. States already
  saved in slots 1-9 load as before.
- **Online play has its own service**, spelunkomp-online (`online/`, a copy of cps2recomp's), no longer cps2recomp's:
  the lobby, the waiting room and the codes are Spelunkomp's alone. Play online with this version on both sides.
- **Follow the finger** on Android, the touch controls' new way to move: put a finger down anywhere away from the
  buttons and the ship moves as it moves, the same distance on the screen, from where the ship is (not to under
  the finger, which would hide it), as some of Cave's own phone ports played. The ship's place is read from the
  game's own memory and the stick pushed toward where the finger says each frame, let go within half the ship's
  step. On by default; the on-screen stick is still there (Controls page: Touch movement, Stick).
- **Scoring readouts** (Training mode): a game's hidden counters written down the left of the picture, read from its
  memory. The first: Muchi Muchi Pork!'s lard meter as a number. More games' to come (docs/TRAINING.md).
- **Lard meter kept full** (Muchi Muchi Pork!) and **Gems kept full** (Espgaluda: 500, Kakusei for as long as you want
  it), on the Training mode page.
- **Scanline direction has Auto**, now the default (#28): with the picture stood up on a screen wider than tall, a
  shader's lines run across it; on a screen taller than wide, or As the cabinet, they run as on the cabinet. A Horizontal
  chosen before is kept; the old default, Vertical, becomes Auto.
- **CRT Newpixie sits with the other CRT shaders** in the Shader list (after CRT Light), no longer at the end after
  Your own. The Shader choice and the in-game menu's shorter list of shaders are now kept by name in settings.ini
  (`shader_id = crt-newpixie`), so a later change of order changes nothing. A settings.ini from an earlier release is
  read as it was and rewritten in the new form once, and the old number is still written beside the name, so an
  earlier release opening the file picks the same shader.
- **The input lag readout no longer stutters.** It found the game's lag by running twenty unseen frames inside one
  frame every few seconds, which made that frame late on a slower machine. It now runs one of them in each frame
  shown, and measures less often once the number is steady. The numbers it shows are the same.
- **No more stalls and memory growth on the way to a stage or boss** (#18): Begin at the stage and Straight to the
  boss run the game unseen and show one picture in 20, and with the GPU renderer each of those pictures waited for the
  20 frames of drawing queued before it (a quarter of a second on an Apple M4, far longer on small graphics), and
  the game's memory reached 1.3 GB there (mmpork). The unseen frames are now held to a few frames in flight: no
  stall, 0.7 GB, and the trip as fast as before.
- **A Renderer setting** on the Display page, in the launcher and the in-game menu (#18): GPU, as before, or Software,
  the processor drawing the same picture, for small integrated graphics where the GPU's drawing is the slow part. Used
  from the next start. The game's log suggests it when the GPU is Intel's integrated graphics or a low-end Mali or
  Adreno.
- **The host's own time a frame** (#18): the game's log (logs/game.log) says once a minute how long this machine took
  to make each frame (median, p95, the most, against the board's 16.7 ms), and the Speed readout has a new choice, **On,
  with the host's time**, that shows it beside the game's own speed: the board's slowdown told from the machine's.
- **Rewind's memory as it says**: rewind is held to 384 MB, but it took about 450 MB on a Mac (the GPU's 64 MB buffer
  it read VRAM back through, uncounted, and the gaps its moments left in the allocator). It now counts the buffer,
  keeps it only as large as a moment's reads, and keeps its moments in one store of its own: about 370 MB in all at
  the limit.

## 0.2.0-beta.1

Mushihimesama Matsuri Ver 1.5 joins the collection (26 games, 31 versions), and your own shaders get one folder for every
game, chosen by name.

- **Mushihimesama Matsuri Ver 1.5**, the version Cave made for its 2011 Cave Matsuri festival (FinalBurn Neo's
  `matsuri15` set), as its own entry beside Mushihimesama, with every training feature and the hit boxes.
- **Your own shaders, one folder for every game** (#16): the Shader setting's **Your own** now draws with a preset you
  choose by name, under **Your own preset**, from a `shaders` folder all the games share, beside the launcher's
  settings (a RetroArch shader pack can go in whole, its passes beside it). The launcher makes the folder, shows where it
  is and has an **Open shaders folder** button; updates leave it alone. A game's own `shaders` folder still works and
  comes first. A preset that fails to load says why (librashader's reason), over the picture and in the menu, instead
  of the picture quietly going without it.

## 0.1.0-beta.2

A fix for the CV1000 games' memory on Windows and some Linux desktops, the GPU named in the log, crash logs kept, and a
new CRT shader.

- **The CV1000 games' memory growth fixed.** With the GPU renderer, on Windows (and on some Linux desktops) a CV1000
  game's memory could grow by gigabytes in seconds, the game turning choppy, until it used all of the RAM and was
  stopped. It now stays flat. No input lag is added.
- **The GPU and driver named at startup**, in the game's log (logs/game.log), for reports.
- **Rewind's memory cap counts what it really holds**: up to 384 MB means 384 MB.
- **A crash leaves its log.** A game's output reached `logs/game.log` only in blocks, so a crash lost it and the
  launcher's report pointed at an empty log. It is now written as it goes, and on Windows a crash adds where it
  happened and the calls that led there (the build's unstripped programs and `tools/release/crash_where.sh` name them).
- **The left stick's click no longer opens the menu** unless you ask for it: a gamepad's Guide, or Start and Select
  together, open it now. The launcher's Controllers page has a **Menu button** setting to choose: Guide, left-stick
  click or Start+Select; Guide or Start+Select; or Start+Select only. Start and Select together always open it (Steam
  often takes Guide on a Steam Deck), and in an online match they ask Leave the match?. Esc and F1 are unchanged.
- **CRT Newpixie** on the Shader setting: RetroArch's newpixie-crt, a light CRT look with chromatic aberration,
  ghosting, a little curvature and scanlines.

## 0.1.0-beta.1

The first public version: 26 arcade shooting games (30 versions), Cave's and three made for its boards or under its licence, on
three boards, in one collection with one launcher. Each needs your own ROM set.

- **The games.** On the CV1000: Mushihimesama, Ibara, Ibara Kuro Black Label, Espgaluda II, Pink Sweets,
  Mushihimesama Futari (Ver 1.0, Ver 1.5, Black Label and its 2009 Another Ver), Muchi Muchi Pork!, Deathsmiles
  and MegaBlack Label, DoDonPachi Dai-Fukkatsu (Ver 1.0, Ver 1.5 and Black Label), Akai Katana and DoDonPachi SaiDaiOuJou. On Cave's 68000 board: DonPachi
  (Japan Ver 1.01 and US Ver 1.12), DoDonPachi, Dangun Feveron, ESP Ra.De. and Guwange, with Mazinger Z and Air
  Gallet. On IGS's PGM: DoDonPachi Dai-Ou-Jou and Black Label, Ketsui, Espgaluda, and DoDonPachi II: Bee Storm. All
  play with sound.
- **DonPachi in its Japanese version** (Ver 1.01, the one high-score players use) as the collection's DonPachi, with
  every training feature the US Ver 1.12 has; the US version is still there as "DonPachi (US 1.12)".
- **Futari Black Label in its Japanese version** (2007/12/11, the one score records use) as the collection's
  Mushihimesama Futari Black Label; the 2009 international re-release is still there as "Black Label Another Ver
  (2009)". **Futari Ver 1.0 and Dai-Fukkatsu Ver 1.0** beside their Ver 1.5s. All three with every training feature
  and the hit boxes of the versions they came from.
- **Native code.** Each game's code translated to C++ ahead of time (the SH-3, the 68000 and the Z80 sound
  drivers), checked against the interpreters; the boards' hardware written fresh. Drawn by the GPU, exact to the
  pixel against the software model: the CV1000 games from their own draw lists, the others from their sprite lists,
  tile layers and palettes.
- **Training mode** in every game: a practice point with a replay of the last attempt, rewind through the last 30
  seconds or so, fast forward, an input display and a speed readout. From each game's own memory: start at any
  stage or straight at its boss, out of harm, bombs and ships that never run out, and the hit boxes as the game's
  own hit test reads them (ship, bullets, enemies, ground enemies, shots), each kind in a colour of its own.
- **Online play** with rollback: a game's two-player mode with someone on another machine, by a code, the lobby or
  an invitation from the waiting room; the host's input delay; country flags in the waiting room.
- **No added lag**: the controls are read when the game reads them, and run-ahead takes out the frames the games
  themselves take to answer a press. An input lag readout shows both.
- **The boards' slowdown, on by default**: the CV1000's CPU cache, bus and pipeline and its blitter (draws that hang
  over the screen's left or top edge cost only their list entry, as in MAME: two boards filmed at 60 pictures a second
  agree), and the PGM's
  video memory waits, so the games slow down where a board does, as they are played and scored (it can be turned
  off on the Game page for a slow device).
- **The picture**: stood up for an ordinary screen or as the cabinet has it for one on its side; whole-pixel or
  fitted scaling; the cabinet's 4:3 shape or square pixels; RetroArch's CRT and scanline shaders, with the
  scanlines down the picture or across it.
- Free play, straight to the title, save states, pause and frame advance, half and double speed, the board's test
  and service switches; keyboard and two gamepads, remapped in the launcher.
- **The launcher**: finds each game's set in a folder, lists the games by name, release date or board, and has
  the Features, Controllers and Online pages; each game's page shows its own files.
- **The launcher updates itself** from this version on: it checks the releases when it opens and offers a new one on
  the Games page, where **Update** downloads it, checks it and puts it in place (on Android it hands the new APK to
  Android's installer). A beta is offered newer betas too; a final version only final ones.
- Builds for macOS (Apple silicon and Intel), Linux x86_64 and ARM64 (the Steam Deck and Steam Machine included),
  Windows x64, and Android 9 or later on 64-bit ARM (one app for the whole collection, with touch controls).
- **The 68000 games run at the 68000's own speed, to the cycle.** Checked against Cave's MiSTer core running
  (DoDonPachi from power-on, its work RAM at every frame) and against its 68000 alone over every instruction the
  games use: a dozen instructions had the wrong time (among them ADDQ to memory, a MOVE to -(An), MOVEM, long
  arithmetic from a register, EOR, and the divides, which now take as long as their operands make them), and an
  interrupt is now taken where the 68000 takes it, its acknowledge waiting for the E clock. Zoomed sprites are placed to the
  256th of a pixel and overlap as the core's sprite chip draws them (MAME's rounding left many a pixel off): ESP
  Ra.De.'s pictures equal to the core's in every frame compared. DoDonPachi's boot, title
  and attract demo now go as the core's, where its random choices had parted at the end of the boot (docs/SLOWDOWN.md).
- **The YMZ280B's interrupts on time.** On Cave's 68000 board, a sound that ends interrupts the 68000 at the sound
  chip's own tick, as the board does, where it had waited for the next sixteenth of a frame; and the chip takes a
  write, and the board its blank bit, when the 68000's bus makes it, not at the instruction's start. Against Cave's
  MiSTer core, DoDonPachi's work RAM is equal at every frame for its first 5,269 frames, 38 seconds into stage play,
  with the chip timed as the core times it (it had been 3,127). The chip keeps its board's crystal (the core runs it
  from the CPU's clock, which drifts from the crystal, so as played the two part at frame 3,128).
- **The PGM checked against its MiSTer core running**: DoDonPachi Dai-Ou-Jou's work RAM compared at every frame.
  The IGS023's video memory waits (and its own fetches holding the CPU), the time of each wait within the
  instruction, the sprite copy's start, the Z80's memory and sound latches as they are at the 68000's read, the
  sound clock and the level-4 interrupt are now the board's: 2,631 of 2,648 frames identical in attract and play, and
  8,205 of 8,250 through stage 5's slowdown, each difference a single frame (docs/SLOWDOWN.md, docs/PGM.md).
- **The CV1000's slowdown fitted to a real board**: the CPU's model (Slowdown as the board) is now measured against
  buffi's CV1000 benchmark run on an Ibara PCB, and every test is within 1.5% of the board but one (CPU QUICKSORT,
  6% fast); before, the CPU tests were up to 56% fast. The busy scenes slow down more than before, as the board's CPU
  is slower than the HS3 core the model followed (docs/SLOWDOWN.md, docs/plans/BENCHMARK.md). It costs about 15% more
  processor time than the model before.
- **Slow motion and frame advance** for study: the Speed row is at the top of the Training mode page too (3/4 down to
  1/8, or faster), with the sound now played slower or faster by as much (it was silent away from full speed), as
  cps2recomp does; the page's **Frame advance** row stops the game and shows the frame's number, and then O, Tab or a
  gamepad's right trigger goes on a frame at a time (held: frame after frame). The game computes exactly the same
  frames. Stopped, slowed or faster, the frame shown is the game's own (not run-ahead's).
- **Attract mode** in every game (Alternative modes): the board started again and left to run its own show over and
  over, with no coin in and no controls, as cps2recomp's; Start or a coin ends it.
- **Less sound lag**: the sound reaches the system about 15 ms after the game makes it, down from about 43 ms (95 ms
  on the PGM; docs/LAG.md's Sound): the sound device asked for small buffers, as cps2recomp does, and the sound waiting
  kept at 10 ms by playing it a touch faster or slower, where before it was whatever it had grown to (up to 200 ms).
  `tools/audio_lag.sh` measures it.
- **An arcade monitor** (CRT Emudriver, a custom resolution): a new **Arcade monitor** page on the launcher's
  Features page (and the in-game menu's Display page, which can only turn it back off), as cps2recomp has it, every
  game on all three boards. *Full screen resolution*: full screen in the board's own resolution, pixel for pixel
  (CV1000 and Cave's 68000 board 320 x 240, Mazinger Z 384 x 240, the PGM 448 x 224), or a super resolution of its
  lines and 1920 columns or more (the PGM's 224 in a mode of 240 where the card has none of 224), at the refresh
  nearest the board's (60.02, 57.44 or 59.19 Hz); a game made for a monitor on its side shown as the board sends it,
  for a monitor turned as the cabinet's was. *Screen*: which screen it opens and goes full screen on. *Make the mode
  at the board's rate* and *Monitor* (Windows): Switchres makes that mode at exactly the board's rate within the
  monitor's ranges, the desktop's mode back at the end. *Keep the board's pace* (off by default): on an arcade mode
  the frames are synced to the screen's refreshes, with a frame delay (Windows) that reads each frame's controls as
  late before its refresh as its work allows; on, the board's clock, as in a window; online always the board's.
  *Compact menu*: the in-game menu's small letters, on by itself on an arcade mode, drawn on the board's own lines.
  Not on Android (recomp-common's `arcade_screen.h`; Switchres downloaded when a Windows program is built).
- **CV1000 draws over the screen's left or top edge timed as MAME times them again** (a list entry's read), with
  "Slowdown as the board" on. Timing them by the pixels they paint (#56) made Mushihimesama's stage 3 late about
  three times as often as two boards filmed at 60 pictures a second (port 1,228 late frames, the boards 356 and 384
  over the same game frames; now 303). Espgaluda II, Futari and Deathsmiles slow down less in their busy stages too.
  docs/BOARD-VIDEO.md, docs/SLOWDOWN.md.
- **The CV1000 blitter's interrupt (IRQ1)**, with "Slowdown as the board" on: the board raises it when a draw list is
  done. Every CV1000 game registers a function for it. Futari 1.5 and Black Label time the blitter with it and drop
  their programmed slowdown in a frame whose list took over 14 ms. Without it the port always slowed by the full
  amount (Futari 1.5 over a whole game: 5,573 late frames before, 5,324 now). Off: unchanged. docs/SLOWDOWN.md.
- **The Discord link at the top of the launcher's Online page** ("Join us on Discord"), as cps2recomp has it.
- **Sound filter**: the in-game menu's **Game > Sound** page and the launcher's Features page, every game on all
  three boards. What the sound chips hand over can be harsh in the treble, a hiss and grit that headphones and good
  speakers bring out; **Soft** takes the treble down from about 8 kHz, **Softer** from about 5 kHz (a second-order
  low-pass on what is played only, at the rate the board's sound is made: the game, recordings and online play are as
  before, and it is each player's own in an online match). Off by default (`port/sound_filter.h`, from cps2recomp).
- **The 68000 takes an interrupt where a real one does, for every instruction the games use.** Each instruction
  form in the twelve 68000 and PGM games (8,176, and 500 branches, jumps, returns and traps) was run on fx68k, the
  cycle-exact 68000 in the MiSTer cores, with the interrupt raised at every half cycle; the instructions that look at
  the interrupt earlier or later than their last prefetch (MOVE.L, MOVE to (An)+, MOVEM, DIVU/DIVS and a dozen more)
  now do it where fx68k does.
- **The sound chip restarts a voice cleanly**: a voice started again while it still sounded (DoDonPachi's tunes) had
  kept its old place between two samples, and ended up to a few of the chip's ticks off.
- **Cave's 68000 boards run at 57.44 Hz, a board's measured rate** (272 lines at 15.625 kHz, as nullobject probed on
  a Dangun Feveron board for the Cave MiSTer core), not MAME's unmeasured 57.55 Hz (271.5 lines); the blank is the
  32 lines not shown. The games run 0.2% slower, as the board does.
- DoDonPachi matches the Cave MiSTer core byte for byte for all 10,400 frames of its play script (three minutes, its
  slowdown included: the same frames late on both), with the sound chip timed as the core times it (a measurement
  option). With the port's shipped timing (the board's own sound crystal) and the 68000 started at the core's moment,
  for the first 3,127 frames; exactly as shipped, 130 (a board's reset phase is its own). docs/SLOWDOWN.md has the
  table and why.
- **The PGM's slowdown measured against a real board.** The PGM core's own test ROMs, whose results on a board its
  author published, run on the port: the 68000 and the sprite DMA were already the board's (within 0.005%), the video
  memory's waits were not (up to 50% fast where the chip draws). They now follow the board's (231 of 268 results
  within 0.5%, every one within 2% but four one-line windows), with a read-modify-write's write after its prefetch.
  The games' waits drop 2-12%; the frames they drop stay as they were (docs/PGM.md).
