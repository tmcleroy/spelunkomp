# Changelog

All notable changes are written here, in the form of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versions follow [Semantic Versioning](https://semver.org/).

## Unreleased

- **No more stalls and memory growth on the way to a stage or boss** (#18): Begin at the stage and Straight to the
  boss run the game unseen and show one picture in 20, and with the GPU renderer each of those pictures waited for the
  20 frames of drawing queued before it (a quarter of a second on an Apple M4, far longer on small graphics), and
  the game's memory reached 1.3 GB there (mmpork). The unseen frames are now held to a few frames in flight: no
  stall, 0.7 GB, and the trip as fast as before.
- **A Renderer setting** on the Display page, in the launcher and the in-game menu (#18): GPU, as before, or Software,
  the processor drawing the same picture, for small integrated graphics where the GPU's drawing is the slow part. Used
  from the next start. The game's log suggests it when the GPU is Intel's integrated graphics or a low-end Mali or
  Adreno.

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
