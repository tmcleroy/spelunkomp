# Mushihimesama: progress

Set: MAME's `mushisam` (2004/10/12.MASTER VER.), CV1000-B.

## State

- Plays in a window from the collection app (/Applications/Spelunkomp.app): boot, attract, mode and shot select,
  stages, continue. Code translated to C++ (99.3% of blocks; the rest interpreted), checked against the block
  interpreter frame for frame (tools/check.sh).
- Sound: the YMZ770 modelled (machine/ymz770.cpp); music and effects come out, at the chip's 16 kHz.
- Video is native (docs/NATIVE.md): the game's draw lists run on the GPU, pixel-exact against the software blitter
  (tools/check_video.sh). Run-ahead and the lag measurement: docs/LAG.md.
- Free play (credits byte 0x0C056BE7 kept at 1), save states, the shared menu and launcher, remapping.
- Not yet: slowdown as the board has it (the CPU's cache and bus timing are not modelled, so heavy scenes run faster
  than a board), native sound and loading (docs/NATIVE.md). Save states are 75 MB (all of VRAM, not packed).

## Facts found

- The boot code in U4 copies the program into RAM and runs it from 0x0C000000 up. The main loop's wait for the frame
  is around 0x0C04A2AA (the same address MAME's driver uses as its idle skip), reading a flag at RAM offset 0x24D8.
- Graphics come from the NAND a page (2112 bytes) at a time through DMA channel 0 (source 0xB0000000 fixed, byte
  transfers, auto request), polled on CHCR0's end flag.
- Inputs: ports C (coins, starts), D (player 1), L (player 2); the vertical sync is IRQ2.
- A blank EEPROM is initialised by the game on first boot ("INITIALIZE 00C0").

## Recipes (headless)

- A coin at frame 4000 (`--system 4000:5:4`), Start at 4100 (`--system 4100:5:0x10`), button 1 at 4300, 4400 and
  4500 through the mode and shot selects; play begins about frame 4650.

## Not checked by a person

Everything by hand: nobody has played it or listened to it yet. The sound was only looked at as a spectrum and by its levels.

## Training

Start at stage (1-5), out of harm, endless bombs and ships (docs/TRAINING.md); the practice point, rewind and fast forward every game has. The facts are in src/mushisam.h.

## Gadgets (panels beside the picture, 2026-11-27)

Score and stage (for score by stage), the Maniac/Ultra counter (the "+N" under the ships: a graph and the number), the
score items taken (100, 200 and 500 points) and the score the next extra ship comes at, all read from RAM; addresses and
how each was confirmed are in src/mushisam.h. Not found: a rank (nothing in memory that grows with play, and sets bullet
speed or count, was found; the words at 0x0C1ED07C-0x0C1ED084 grow with time only and were not tied to bullets), the
shot power level and the power items. Ultra's mode word (2) and the counter's top were not checked.
