# DoDonPachi: progress

Set: MAME's `ddonpach` (World, 1997 2/5 Master Ver.), on Cave's 68000 board (docs/CAVE68K.md).

## State

- Plays from boot: the warning, the logos, the title, the demonstration, ship and shot select, the first stage,
  continue. Code translated to C++ (78,210 instructions, none interpreted in play), equal to the interpreter block
  for block over 7,000 frames (tools/check68k.sh).
- Sound: the YMZ280B modelled (machine/ymz280b.cpp); music and effects come out (the samples are not silent and
  not clipped; nobody has listened yet).
- Video: drawn by the GPU (port/gpu68k.cpp) from the frame the board model hands on; the software model
  (machine/cave_video.cpp) stays as the reference, and `--check-video` finds no pixel different over 7,000 frames
  (boot, attract, stage 1) or 8,000 more begun at stage 4 with run-ahead and the readouts on. Looked at, not compared
  with a reference emulator.
- Free play, save states, the shared menu and launcher, remapping, run-ahead, the practice point with replay,
  rewind, fast forward, input display, speed readout (tools/check_tools.sh: the tools put the game back exactly, on
  the GPU path too).
- Training from the game's facts (runtime/shmup.h, games/ddonpach/src): start at stages 2 to 6, straight to the boss,
  out of harm, bombs and ships that never run out. Tried by eye: games begun at stages 3, 4 and 6 (the ship and the
  bombs' row as in play), the trips to stage 1's and stage 3's bosses (about 9,500 and 4,600 frames unseen), bombs
  used every 40 frames and ships lost for 16,000 frames with the game still on.
- Hit boxes (the feature "Hit boxes"): the ship's box, the bullets' points and the enemies' bodies, read as the game's
  hit test (0057EE) reads them; facts in ddonpach.h.
- Dodge training (docs/DODGE.md; games/ddonpach/src/dodge.cpp): an emitter in a boss's arena fires the game's own
  patterns, 439 of them taken from recordings of play (tools/dodge), made by the game's bullet routines.
- M2-style gadgets (2026-11-27, game.cpp): the rank (0 to 63, 101969) as a graph, the chain's timer with hits and the
  best chain, the bees (this stage, sets of 13), the second loop's test as the game makes it (040038 on), and the score
  and stage (the second loop's after the first's) for Score by stage.
- Not yet: the second loop as a start (2-1 to 2-6), a boss health meter, what the rank does to the bullets' speed in
  each enemy (the rank is found and read by the enemies' code at 010A68 on; no single "bullet speed" word exists), the
  Japanese set (`ddonpachj`: the same board, two other program ROMs).

## Facts found

- The interrupt handler (000622) reads 800004 then 800000 and takes the vertical blank's path when bit 0 or bit 2
  of the second is low; otherwise it reads the YMZ280B's status (300002) and clears the busy words at 1013EA on
  for the voices that ended.
- The main loop's tasks: a table of 0x3D records of 0x40 bytes from 100002; 100000 counts through them (000AA2).
- Credits: the word at 1013AA (the byte 1013AB); 1013AC is what it was before the last coin or start. The game
  goes to its "press start" screen for a credit that arrives; one that is simply there from power on leaves it in
  the attract show. So free play gives a credit when Start is pressed with none (games/ddonpach/src/game.cpp).
- 101297 bit 0: two credits to start. 1017A6: the attract show's state.
- The start of a game for player d7: 00456E. All the other facts the training features use are in
  games/ddonpach/src/ddonpach.h with how each was found: the stage (101978), loop (10197A), the boss on (10E87A),
  ships (101964, the one flown among them), bombs and their stock (102CB0, 102CB1), the safe time after a ship
  appears (102CCA), the main loop's tasks (task 0x3A is a stage's end), what the game is doing (1017A6).
- A later stage put in 101978 as a game begins gives that stage's map but neither the ship nor the bombs' row:
  stage 1 brings the ship in from a carrier of its own (10E856 is not 0 until it has launched), and only the stage's
  end sets the rest up. So a game begun at a later stage plays stage 1's launch, then is sent on by the game's own
  stage end (task 0x3A switched on, the boss's state 10E859 given the bits a beaten boss gives) from the stage
  before the one chosen: about 15 seconds, and stage 1's clear bonus is counted.
- The score: 8 BCD digits at 10161E (tens of points); the high score likewise at 101624.

- Enemies and bullets: the stage's objects are tasks 5 to 54 (one routine, 048ADA; the object's kind the byte at +0xC,
  227 kinds in the table at 015A8A), brought in by task 4 (048992) from a table a stage (004895E) by the stage's count
  (10198E). An enemy's bullet is made by 050DE4 (64 directions) or 050F9C (256), in the second loop 054E22 and 054EF4
  (enemies call 050DDC and 050F94, which choose); 40 kinds (05104E). The game's aims are 059A6A (64) and 059C8C (256).
  All in ddonpach.h.

## Recipes (headless)

- games/ddonpach/recipe.txt: a coin at 4000, Start at 4100, button 1 at 4300 and 4400 (ship, shot type), play from
  about 4500.

## Leads (not yet used)

- No decompilation or recompilation of the game was found (searched 2026-11-10). SUDDEN DESU's article on the
  game's leftover debug tools (https://sudden-desu.net/entry/dodonpachi-debug-tools-level-select-and-more/) says the
  DonPachi code base's tools are still in the program: a pause with stage scroll (switched back on by putting two
  NOPs at 000B02; P2 Start pauses, P1 Start goes on, button 3 with up or down moves through the stage), a RAM
  editor (words at 1017A0 and 1017A2 set to 1, a call patched in at 000AF2) and a palette editor, object spawner
  and level select (a byte the article points at 10F001, value 4). Its patches are given for the Japanese set
  (their addresses are not this set's, so the level select was not used); the pause one for both.
