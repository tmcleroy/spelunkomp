# Slowdown

These games slow down on the board, and are played and scored with that slowdown. On the CV1000 two things cause
it, and both are modelled (the 68000 boards: [below](#caves-68000-board-and-igss-pgm)):

- **The blitter's time** (machine/blitter.cpp): how long a draw list keeps the blitter busy and how much its uploads
  take from the CPU, from buffi's CV1000 blitter research as MAME's cv1k_v.cpp has it, but with the VRAM's clock and
  the screen's line fetch as the board has them ([below](#the-blitters-clock-and-line-fetch)). Always on.
- **The CPU waiting on its memory and its pipeline** (machine/bus_timing.h): the SH7709S runs a five-stage pipeline
  out of one 16 KB cache (4 ways of 256 lines of 16 bytes) that holds code and data together. A busy frame touches
  more than that; every line the cache has to fetch stalls the CPU for the SDRAM's cycles, a dirty line pushed out is
  written back first, uncached and write-through writes go out to the bus, and the cache's one port is shared by the
  data accesses and the instruction fetches. It is the **Slowdown as the board** feature (Game page), on unless
  turned off ([the setting](#the-setting)).

## The CPU's model

machine/bus_timing.h was written from the SH7709S hardware manual (Renesas REJ09B0081: the cache, section 5; the
bus state controller, section 10) and the SH-3 software manual (the pipeline, section 10), with the cycles of each case
measured on [HyperSquid3](https://github.com/ika-musume/HS3) ("HS3", ika-musume, BSD-2-Clause), a near
cycle-accurate SH7709S in SystemVerilog whose branch timing its author has matched to a CV1000 board, and then fitted
to a board: buffi's CV1000 benchmark run on an Ibara PCB ([below](#checked-against-the-board)), which showed HS3,
and so the model, too fast on the CPU's work. Where the board is slower than the manual or HS3, the rule says so. It
replaces an earlier model that followed MAME's "SH 7709S experimental cache/memory timing" (sh7709s.cpp); it was
written by someone who had read that, and none of its code is left.

The CPU counts each instruction's plain cycles (one, or the few that take more); the model adds, at each data access
and at each branch taken (or any instruction that sets the PC: an exception, a sleep, an interrupt taken), what the
hardware spends beyond them:

- **The code run since the last such event.** Every branch taken is an event, so that code ran straight on, through
  conditional branches not taken; the path is found by walking on from the last event. Its lines are looked up in the
  cache like data (a line not held is filled: 18 cycles); past the cache (the games' cache flush runs from P2) each
  instruction pair is read on its own.
- **The pipeline**, stepped along that path from where the last event left it: an instruction pair (one longword) is
  fetched three cycles before its first instruction is decoded, through the cache's one port, which data accesses and
  the multiplier's MA stages also use and have first; a branch's target is fetched once the branch is decided, two
  cycles before it is decoded (so a load in the delay slot of a branch taken holds the target's fetch a cycle, as on
  HS3 and the board); a conditional branch taken leaves its bubble (bt/bf 3 cycles, bt/s and bf/s 2); a load whose
  result the next instruction reads holds that one a cycle, and so does STS MACH/MACL (manual 10.4); an instruction
  that reads PR (RTS, STS PR) waits for a PR written just before it: 3 cycles after LDS.L @Rm+,PR (the manual's table
  10.2 says it waits, without the number; HS3 holds it until the load has left WB), 1 as the first instruction of a
  call's target (JSR, BSR, BSRF: HS3's, the manual does not say); the multiplier
  (manual 10.2.4) works on after its instructions' MA stages and holds what reads MACH/MACL (STS: until a cycle after
  it is done; LDS, CLRMAC and another multiply: until it is done). On the board it works a cycle longer than the
  manual says (three cycles for MULS.W/MULU.W, four for MUL.L, DMULS.L, DMULU.L, MAC.W, MAC.L), and the multiply's MA
  stage holds the cache's port until it is done, so no instruction is fetched meanwhile (the benchmark's CPU INTEGER
  and RAMP CPU LOGIC, which the manual's numbers leave 11% and 16% fast).
- **The access.** A hit costs nothing more. A read miss fills the line from the SDRAM (18 cycles until the CPU has its
  word); a write miss allocates (20). A dirty line pushed out goes to the one-line write-back buffer first (4 cycles:
  a longword a cycle), the fill goes before its write-back, and the write-back (18) keeps the SDRAM busy after the fill
  (manual 5.3.4, 5.3.5; HS3 writes the line back first, 36 cycles, which made the benchmark's CPU MEMORY 14% slow). P1
  is write-through on the CV1000's games (CCR.CB clear): its writes are posted (2 cycles) but the next write waits 14
  cycles from the last one's start, an SDRAM read 12. An uncached write to the SDRAM (P2) is not posted: the CPU waits
  for its command to go out (10 cycles), and an uncached access keeps the SDRAM busy 10 cycles longer than HS3 says
  (a read 22 in all, a write 24): both measured on the board (the lists the benchmark writes through P2, and its cache
  flush run from P2); the posted write-through writes are left as HS3 has them, which the benchmark does not use. An
  uncached SDRAM read is 12. The board's devices are ordinary areas: two bus cycles plus WCR2's waits for each beat of
  the area's width (BCR2), two CPU cycles a bus cycle, and three to send it and have the answer (the blitter's
  registers, 10 waits: 27; the NAND, sound chip and EEPROM on 8 bits with 3 waits: 13 a byte; the program flash on 16
  bits with 10 waits: 26 a word, 50 a longword, 196 a line). On-chip registers: 2 (the bus controller's) or 3.
- **CCR.CF** (throwing the cache away): 256 cycles. **A write to the cache's address array** (the games' flush loop)
  that writes back a dirty line: through the write-back buffer, as a miss's. **DMA** (the NAND's pages): the read and
  the write each transfer makes on the bus (20 cycles a byte from the NAND), instead of the fixed 10 a byte used with
  the feature off.
- **The blitter's start:** with the feature on, a list keeps the blitter busy 1.13 µs longer than its operations
  (buffi's time between an upload's 64-byte chunks: the list's first chunk is read before its first operation). The
  benchmark's BLIT LATENCY on the board shows it; with the feature off the blitter's time is MAME's, unchanged.

The costs are fixed for the bus settings every CV1000 game makes at boot (BCR1 C008, BCR2 39F0, WCR1 9551, WCR2
FDD7, MCR 543C: the SDRAM at CAS latency 2, RAS to CAS 2, auto-precharge, refresh on); the ordinary areas' follow
BCR2 and WCR2 as the game sets them.

### How it is fitted in

Every data access of the CPU (machine/sh3.h's Read and Write) asks the model when the feature is on, with the
instruction's address (`c.at`, which translated code sets before any instruction that touches memory and the
interpreter for every instruction), the cycle count, and main RAM to read the code it walks; so does every branch
taken (`TimeBranch`: the interpreter after the branch and its slot have run, translated code at the same point, at
each way out of a block a branch takes), and an interrupt taken. Translated code and the interpreter call it at the
same points with the same counts, so the two stay equal (`--timing` on the headless runner; `TIMING=1
tools/check.sh`). With the feature off nothing is asked and the games run exactly as before. The model's state is
plain data, part of save states (now "spelunkomp state 5"), run-ahead and the training tools' moments. The paths it
has found between two events (`Sh3Paths`: a loop takes the same one every turn, and finding it is most of the
model's work) are kept apart from the state, 8,192 of them, each made ready to run when it is found (the lines it is
fetched from, and the pipeline's steps that depend on the time with the cycles between them added up): a path
depends on the code under it alone, each block of 128 bytes of RAM a kept path read notes the time of its last write
(by the CPU or the DMA), and a state loaded forgets them all, so which paths are kept changes no count.

## Checked against the board

buffi's CV1000 benchmark (homebrew, October 2026: "Benchmarking CV1000: PCB vs MAME and MiSTer"), which buffi ran on
an Ibara PCB, runs on the port's board as an ibara set (`tools/cv1kbench.sh ZIP`; the benchmark is buffi's and not in
this repository): the method, and where each difference came from, are in [plans/BENCHMARK.md](plans/BENCHMARK.md).
The same benchmark on shmupmame 5.4 (its cache model, no per-game settings), MAME 0.275 and current MAME, and under
the CPU settings players post per game, is there too ([Against shmupmame](plans/BENCHMARK.md#against-shmupmame-and-mame-2026-11-23)):
shmupmame's mean error is 27.7% (22-33% with those settings) against this model's 0.7%.
<p align="center"><img src="screenshots/cv1kbench-results.png" width="45%" alt="The results screen of buffi's CV1000 benchmark as the port draws it, Slowdown as the board on: RAMP 8X8 SPR 4850, RAMP 8X8 STILL 9085, RAMP 32X32 SPR 1170, RAMP CPU LOGIC 1512, RAMP GAME 1694, BLIT LATENCY 5 µs, 8X8 SPRITES 603, 32X32 SPRITES 77, SCREEN COPY 90.7, ALPHA 32X32 77, UPLOAD TO VRAM 5653, GAME FRAME 171.4, BLITTER PAIN 20.0, CPU INTEGER 3656, CPU DIVIDE 1190, CPU MEMORY 19.9, CPU CACHE HIT 17.0, CPU FLUSH 885, CPU LIST BUILD 355, CPU QUICKSORT 149"><br>
<em>buffi's CV1000 benchmark running in Spelunkomp with Slowdown as the board on; compare the board column below
(the benchmark and the board's results are buffi's).</em></p>

It times everything with the SH-3's timer, so it measures in the board's own clocks. Rates (higher is faster) and
times; the error is this model's against the board, + where the port is faster:

| test | board | this model | error | HS3's model before | MiSTer (HS3) |
|---|---|---|---|---|---|
| RAMP 8X8 SPR (@60) | 4800 | 4850 | +1.0% | 5700 | 5650 |
| RAMP 8X8 STILL (@60) | 9085 | 9085 | 0 | 9123 | 9085 |
| RAMP 32X32 SPR (@60) | 1170 | 1170 | 0 | 1175 | 1170 |
| RAMP CPU LOGIC (@60) | 1503 | 1512 | +0.6% | 2346 | 1769 |
| RAMP GAME (@60) | 1694 | 1694 | 0 | 1701 | 1715 |
| BLIT LATENCY (µs avg) | 5 | 5 | 0 | 4 | 4 |
| 8X8 SPRITES (kdraw/s) | 603 | 603 | 0 | 605 | 604 |
| 32X32 SPRITES (kdraw/s) | 77 | 77 | 0 | 77 | 77 |
| SCREEN COPY (Mpix/s) | 90.6 | 90.7 | +0.1% | 91.0 | 90.6 |
| ALPHA 32X32 (kdraw/s) | 77 | 77 | 0 | 77 | 77 |
| UPLOAD TO VRAM (kpix/s) | 5632 | 5653 | +0.4% | 7841 | 8198 |
| GAME FRAME (fps) | 171.3 | 171.4 | +0.1% | 171.9 | 171.2 |
| BLITTER PAIN (ms/round) | 19.7 | 20.0 | -1.5% | 19.9 | 19.7 |
| CPU INTEGER (kiter/s) | 3656 | 3656 | 0 | 4653 | 3937 |
| CPU DIVIDE (kdiv/s) | 1176 | 1190 | +1.2% | 1365 | 1256 |
| CPU MEMORY (MB/s) | 20.1 | 19.9 | -1.0% | 17.6 | 19.7 |
| CPU CACHE HIT (Mload/s) | 17.0 | 17.0 | 0 | 20.4 | 20.4 |
| CPU FLUSH (µs/flush) | 886 | 885 | +0.1% | 743 | 734 |
| CPU LIST BUILD (kdraw/s) | 352 | 355 | +0.9% | 433 | 431 |
| CPU QUICKSORT (sorts/s) | 140 | 149 | +6.4% | 150 | 155 |

(The blitter rows since 2026-11-23: [the blitter's clock and line fetch](#the-blitters-clock-and-line-fetch).) All but
one are within 2% of the board. CPU QUICKSORT (branchy code over a 16 KB array, as large as the cache: about
59,000 branches taken and 40,500 not taken a sort, few misses) is about 45,000 cycles a sort (6%) faster than the
board, and no rule the manual or HS3 gives accounts for it. One that fits does not count as one: a conditional
branch not taken costing a cycle more when the instruction just before it set T brings QUICKSORT within 1% and moves
no test that fits, but the manual's tables give a branch not taken one cycle, list no such wait, and HS3 forwards T
without one; it is not in the model. Looked at again with the manuals' pages to hand (the SH-3 software manual,
REJ09B0317-0400, puts conditional branches under "no contention", table 10.1, p.438, and decides them in ID,
§10.4.5, pp.476-478; its only SR wait is for STC SR, STC.L SR and TRAPA, §10.2.3, p.432): nothing documented accounts
for the 6%. Every conditional branch not taken costing a cycle more fits the benchmark as well, against the same
pages; a write hit holding the cache a second cycle moves the list tests 9-11% off. Where the cycles go, the
candidates, what they would change in a game's slowdown, and a test program for a board that would settle it:
[plans/BENCHMARK.md](plans/BENCHMARK.md#cpu-quicksort-looked-at-again-2026-11-21). (Tried before and dropped: a T bit set right before any conditional branch
costing a cycle, which moves CPU CACHE HIT and CPU INTEGER off the board; a branch to an odd halfword costing a cycle
more, which has nothing for it.) CPU DIVIDE (a call per division to GCC's `__udivsi3`: jsr, four bsr, five rts, div1
chains) was 3.6 cycles a division faster than the board until the PR rule above (the routine saves PR first thing
and, on its long path, returns with LDS.L then RTS); with it, 1.2% (about 1 cycle) is left. HS3 is faster than the
board on both too.

### The blitter's clock and line fetch

Until 2026-11-23 every test the blitter alone decides came out 0.4% fast (RAMP 8X8 STILL 9123 against the board's
9085, RAMP 32X32 SPR 1175/1170, RAMP GAME 1701/1694, SCREEN COPY 91.0/90.6, GAME FRAME 171.8/171.3), as in every
MAME-derived emulator (shmupmame, MAME: [plans/BENCHMARK.md](plans/BENCHMARK.md#against-shmupmame-and-mame-2026-11-23)),
while the CV1000 core for MiSTer gives the board's numbers. Two of MAME's constants are rounded, and both round the
same way (ikacore_CV1k's notes on the board, `docs/blitter_detail.md` §1, §9):

- **The VRAM's clock** is 76.8 MHz (the 12.8 MHz crystal times 6): 13.0208 ns, not 13. A draw's clocks are now
  counted and turned into time at that rate (+0.16%).
- **The screen's line fetch** (166 VRAM clocks, 2.161 µs) comes every line of the screen, 63.594 µs (407 dots of
  6.4 MHz: 60.0184 Hz), and also in the time the fetches themselves add. MAME adds 2.16 µs per 63.6 µs of the list's
  own time, which leaves those out (+0.12%).

With both, RAMP 8X8 STILL, RAMP 32X32 SPR, RAMP GAME and 8X8 SPRITES give the board's numbers exactly, SCREEN COPY
90.7 (board 90.6) and GAME FRAME 171.4 (171.3): the MiSTer core's. Either alone leaves RAMP GAME at 1701.
UPLOAD TO VRAM stays 0.4% fast (5653 against 5632): an upload's time is main RAM's (20 ns a bus clock, buffi's
figure), not the VRAM's; not changed. Nothing else in the benchmark moves.

The blitter's busy flag reads the same time with "Slowdown as the board" off, so every CV1000 game's state hashes
move, with the feature on and off, from the first list long enough to differ. Mushihimesama's and Futari Black
Label's scripted half-speed scenes stay as they were (the boss deaths 126 refreshes, 63 late; Black Label's stage 1
midboss and boss: docs/BOARD-VIDEO.md).

### Mushihimesama's stage 3: where a busy frame's time goes (2026-11-23)

The two 60 fps board videos of Mushihimesama's stage 3 were late 356 and 384 frames over the same game frames where
the port was late 303 (docs/BOARD-VIDEO.md). The port's frames there were broken down (a local build that sums each
frame's cycles by where the CPU is and what the model charged; `SPELUNK_BLIT_LOG` for the lists):

- **The frame:** the CPU starts the list in the vertical blank, does the game's work (35-50% of a refresh), waits
  for the blitter, then builds the next list (17-28%), then waits for the next frame. So a frame is late when the
  blitter's time plus the CPU's work after it passes the refresh. In the port's late stretches the blitter is busy
  72-82% of a refresh and the CPU's part after it about 25%.
- **The CPU's work** in the busiest stretch (game frames 9,789-10,197: 66% of a refresh): the instructions' own
  cycles 25%, data cache fills 18% (13,000 a frame), code fetches and the pipeline 11%, branches 4%, uncached
  accesses 8% (6,900 a frame: the list written through P2). No write-through writes. The blitter: 9.9 ms a list,
  12.2 at most.
- **How steep it is** (the same route, late frames over stage 3): as it was 303; blitter time x1.03 545, x1.2 1,402;
  the CPU x1.15 397; the model's memory and pipeline costs x1.15 406. The **route** moves it as much: the ship moved
  left at the stage's start 372, moved up 174, with the same model.

The candidates:

- **IRQ1's 14 ms rule:** it only drops a refresh the game added on purpose. Mushihimesama's wait word (0C1ABD54) is
  1 in all 21,457 frames of the run: nothing to drop. 76 lists over 14 ms only refused object starts. Ruled out.
- **Draws over the edge charged more:** their cost is the background's, the same on every route, and the signed
  rule put it at game frames 6,918-9,300 (+355 late), where both boards are least late (17 and 50 against the port's
  106). Any share of it moves the port away from both boards there. Ruled out.
- **The blitter and the CPU sharing the bus:** the blitter reads its list in 64-byte chunks, about 400 a list at
  0.7 µs (0.3 ms of a 10 ms list), during the CPU's first part, which waits for the blitter anyway in a busy frame.
  At most about 1% of a refresh; not modelled, not the gap.
- **Uncached accesses and cache misses:** the benchmark fits them within 1% (CPU LIST BUILD +0.9%, CPU MEMORY
  -1.0%, CPU CACHE HIT 0). Reaching the boards' count would need them about 7% dearer. Not supported.
- **The route:** the boards are late where the port is not (9,789-10,197: 41 and 67 against 0 on all three port
  routes) and the other way round (7,090-8,587: 3 and 13 against 106; the port's ship leaves a wave of large insects
  alive there that both players kill). The boards' totals lie inside the port's own spread over three routes
  (174-372).

So no missing cost is shown: the 15-20% is within what the route does. The one supported change, the blitter's
clock and line fetch above, makes the three routes 204, 518 and 333 (mean 352, from 283).

### Draws over the edge

A draw's time is worked out from the part of it inside the clip (the screen and 32 pixels round it), as MAME does it
from buffi's measurements. MAME reads the draw's destination as an unsigned 16-bit number for that, while it draws
the pixels with the same number signed: a draw that starts left of the clip or above it (x or y below 0, as
Espgaluda II's and Mushihimesama's background layers are drawn, -1,-1 and 480,-81) is "outside" for the time, so its
whole cost is a list entry's read, while the pixels it paints on the screen are painted. The port does the same.

From 2026-11-22 (#56) to 2026-11-23 the port took the destination signed for the time too, on the reasoning that the
blitter cannot paint those pixels without its VRAM clocks (a 256 x 256 draw at -1,-1 then cost 688 µs instead of
nothing). Two boards filmed at 60 pictures a second say otherwise (docs/BOARD-VIDEO.md, "Mushihimesama's stage 3"):
lined up with the port by the background over the same game frames of Mushihimesama's stage 3, KidpandaAG's board
was late 356 frames and Aaron Kraten's 384, the port 1,228 with the signed time and 303 with MAME's. So the board does
not pay for those draws as the signed rule had it, and the port went back to MAME's reading. Whether it pays
something less is not known: the boards' routes were busier than the port's, and their few dozen frames more fit
either. buffi's benchmark has no draw over the edge; its results are the same either way. The blitter's busy flag reads the same
time with "Slowdown as the board" off, so the games' state hashes with it off move too (back to what they were before
#56).

What the signed rule had done, for the record (late frames, `--features start_stage=N,out_of_harm=1`, the shot held,
the check's coin and selects, frames 5,200 to 18,000; the columns are N, and N=0 is the first stage):

| game | N=1 | 2 | 3 | 4 | 5 | in all |
|---|---|---|---|---|---|---|
| Espgaluda II, unsigned (now) | 498 | 292 | 738 | 119 | 615 | 2,262 |
| Espgaluda II, signed | 617 | 380 | 804 | 218 | 674 | 2,693 (+19%) |
| Mushihimesama, unsigned (now) | 132 | 90 | 78 | 111 | 4 | 415 |
| Mushihimesama, signed | 144 | 104 | 843 | 111 | 6 | 1,208 |
| Futari 1.5, unsigned (now) | 314 | 562 | 9 | 40 | 86 | 1,011 |
| Futari 1.5, signed | 657 | 1,257 | 17 | 78 | 86 | 2,095 |
| Deathsmiles, unsigned (now) | 19 | 775 | 666 | 151 | 45 | 1,656 |
| Deathsmiles, signed | 23 | 830 | 685 | 211 | 57 | 1,806 (+9%) |

(Measured before IRQ1 and on another build's play script; the stage runs of docs/BOARD-VIDEO.md are the newer
numbers.) Futari 1.5's board video (30 pictures a second, a much busier route) was compared with the signed rule and
was late about twice as often as the port; with MAME's reading the port is late less still there. That comparison
cannot tell route from model (docs/BOARD-VIDEO.md); a board run of Futari's stages 1-2 at 60 pictures a second lined up
the same way would. Black Label's 60 fps board (owlnonymous), lined up that way over all five stages after the blitter
change: the board late 4,082 frames to the port's 3,200 where well anchored, 2.5 and 1.9 times the port in stages 1
and 2, 0.8 and 0.9 in stages 3 and 4, on a much busier route (docs/BOARD-VIDEO.md).

### The blitter's interrupt (IRQ1)

When the blitter reaches the end of a list it raises the CPU's IRQ1 (ikacore_CV1k's notes on buffi's research;
MAME does not have it). All fourteen CV1000 games carry the same IRQ1 handler from Cave's library: it acks bit 1 of
the blitter's register 0x24, clears IRQ1 in IRR0 and calls a function the game may register. The port never raised
it until 2026-11-22. Now, with "Slowdown as the board" on, IRQ1 comes at the cycle the list's time runs out
(machine/cv1000.cpp: `SH3::Schedule`, `Bus::Event`). With the feature off nothing changes (Futari 1.5's hashes over
6,000 frames are equal to the earlier build's).

Futari 1.5 uses it, and its slowdown depends on it:

- At each list's start (in the vertical blank) the game notes a 1 ms timer tick count; its IRQ1 function notes the
  count again, so 0C1E1050 holds how many ms the blitter took. Without IRQ1 that word stayed 0.
- The game makes slowdown on purpose: it sets its frame's wait (0C1E102C) to two refreshes instead of one. A boss's or
  midboss's death turns this on for a set number of game frames (the explosion's flag, 0C38A670/2: 82 game frames for
  the stage 1 midboss). Busy play turns it on too, every few frames, when one of two counts the game keeps (0C06D5B0, 0C093980; probably bullets and enemies) passes its limit.
- Either way the game drops the extra refresh when the blitter took more than 14 ms (0C06C4A0, 0C06C53A). The frame
  is already heavy, so it is not slowed more.
- So without IRQ1 the port always slowed down by the full amount; the board does less where its blitter is busy.
  Over Futari 1.5's whole game (the route in docs/BOARD-VIDEO.md) the port is late in 5,324 frames now, 5,573 before
  (−4.5%). 3,654 of the 78,676 lists took more than 14 ms. The two runs part at frame 493, because the IRQ1 handler
  takes some CPU time. The boss deaths and the stage 1 midboss on this route stay the same (109 and 82 late frames):
  none of their lists take 14 ms (11.7 at most at the boss, 13.5 at the midboss).

What the time goes on in those explosions (stage 1 midboss, per list; `SPELUNK_BLIT_LOG=FILE` writes this per list):

- The CPU's work is 55–65% of a refresh. It waits for the blitter 7–24% of a refresh.
- The blitter is busy 9–13.5 ms (55–80% of a refresh). It runs 1,050–1,320 draws a list, two thirds of them blended,
  0.7–1.0 Mpixels.
- Of the blitter's time: the pixels 79%, the 32×32 blocks 16%, the per-draw cost 1–2%, the line fetch 3%. Draws
  64×64 or larger are 20–27%; draws over the clip's left/top edge 3–13% and over its right/bottom edge 5–8%. 18–24%
  of the pixels drawn fall in the clip's 32-pixel margin, off screen.
- None of this is what makes the scene late. Its 82 late frames come from the game's own script: with the blitter's
  time ×0.7 or ×0.9 the count is still 82. With ×1.05 it is 83, with ×1.1 98 (87 with IRQ1).
- So the port cannot be 5–10% too slow there. It cannot make these scenes shorter than the script by being faster;
  only IRQ1's 14 ms rule can. The board's few frames fewer (docs/BOARD-VIDEO.md) mean the board's lists were over
  14 ms in some of those frames. Its route had more on screen (gems, counter). If the board's blitter were slower
  than the port's, that would look the same.

All fourteen games register an IRQ1 function within their first 7,000 frames. Mushihimesama, Espgaluda II and
Futari Black Label keep the same blitter time in ms as Futari 1.5 (Black Label's function is Futari 1.5's,
0C062EF0); DoDonPachi Dai-Fukkatsu's sets a flag instead.

Which games use the blitter's time (2026-11-22, read from each game's code: the IRQ1 function that stores the ms, and
every place that loads that word):

| game | ms kept at | read by |
|---|---|---|
| Futari 1.5 | 0C1E1050 | the frame wait (0C06C4A0, 0C06C53A); two places that start an object from a 128-slot table refuse when over 14 ms (0C09930C, 0C099CDC) |
| Futari Black Label | 0C1DE16C | the same (0C06CD22, 0C06CDCA; 0C09A4BC, 0C09AE8C) |
| Mushihimesama | 0C1ABD74 | the frame wait (0C05FA90, 0C05FB2A: the same 14 ms rule, wait word 0C1ABD54); one place that starts an object refuses when over 14 ms (0C0A8764) |
| Espgaluda II | 0C1A4074 | the frame wait (0C0692D0, 0C06936A); one start refused over 14 ms (0C09AC74) |
| Pink Sweets, Ibara, Ibara Kuro | 0C104D1C, 0C160444, 0C17297C | nothing: the word is written and never read |
| Dai-Fukkatsu (both), Saidaioujou, Deathsmiles (both), Muchi Muchi Pork, Akai Katana | (not this function) | their IRQ1 functions are of another kind (DFK's sets a flag); not read further |

So of the games read, only the four above slow down less (and leave some objects out) when a list runs long; for Pink
Sweets and the Ibaras IRQ1 costs the handler's CPU time and nothing else. With IRQ1 on and off (`--features
start_stage=N,out_of_harm=1` for N = 1 to 5, that is from the second stage on; Mushihimesama's N=5 is past its last stage, so
that run says nothing; the shot held, frames 5,200-18,000; off with a local build; before the edge fix above),
the late frames are Mushihimesama 116/114, 1,090/981, 229/229, 36/35, 0/0 and Pink Sweets 4/9, 46/101, 22/22, 92/92, 52/51.
The runs part where the handler first runs, so these differ by the play as much as by the rule (Mushihimesama's
stage 2 is late more often with IRQ1, which the rule alone cannot do). A second board
agrees on the scripted scenes: owlnonymous's Futari Black Label PCB video (60 pictures a second) has its stage 1
midboss and boss deaths 81 and 108 frames at half speed, against the port's 82 and 109 (docs/BOARD-VIDEO.md).

What the change moves: with the feature on, every CV1000 game's state hashes from the first frame the IRQ1 handler
runs (it takes CPU time; Futari 1.5's late frames part at frame 493 of the run above). With it off, nothing. buffi's
benchmark gives the same results to the digit as the table above. The HS3 check is the CPU's work alone, with
interrupts masked, so this does not touch it.

## Checked against HS3

The harness is kept outside the repository (in `../refs/hs3-bench`, beside the clones of HS3 and ikacore_CV1k): a
CV1000 board around HS3 for Verilator (HS3, Micron's MT48LC2M32B2 SDRAM model with HS3's Verilator patch,
a 16-bit program flash, and the board's devices answered as below), and the same code on the port's SH3 with a stub
bus. A small program in the flash sets the bus as the games' boot does, starts the SDRAM, turns the cache on, loads a
snapshot's registers and returns into the game's code, which is in the SDRAM; both sides then run with interrupts
masked until the game reaches its wait for the next frame, and count the cycles. The snapshots are taken from the
port at the moment the game leaves that wait (main RAM, the registers); what the frame reads outside RAM (the NAND's
bytes, the flash, the ports) is answered in the order the port's run read it, and the blitter is always ready (its
own time is not HS3's to say). Both sides run the same instructions (a hash of the retired PCs is equal on every run
below), so the counts compare the timing alone.

The CPU's work of one frame (cycles from leaving the wait to coming back to it; the plain count is the port with
the feature off, a frame is 1,706,144 cycles):

| frame | instructions | plain | earlier model (MAME's) | fitted to HS3 | HS3 | fitted to the board |
|---|---|---|---|---|---|---|
| Mushihimesama 6000 (stage 1) | 137,727 | 146,184 | 385,712 (+8.2%) | 353,958 (-0.7%) | 356,555 | 372,171 (+4.4%) |
| Mushihimesama 12200 (loading: NAND by DMA) | 101,953 | 406,308 | 1,043,925 (+19.7%) | 866,625 (-0.6%) | 872,265 | 881,260 (+1.0%) |
| Mushihimesama 28200 (busy) | 332,683 | 354,563 | 1,009,028 (+9.2%) | 915,185 (-0.9%) | 923,767 | 953,339 (+3.2%) |
| Mushihimesama 31200 (busy) | 468,707 | 500,846 | 1,332,246 (+9.6%) | 1,205,999 (-0.8%) | 1,215,403 | 1,245,669 (+2.5%) |
| Espgaluda II 7000 | 200,302 | 214,984 | 547,685 (+9.0%) | 505,785 (+0.6%) | 502,609 | 519,101 (+3.3%) |
| Espgaluda II 9000 | 21,942 | 23,011 | 60,462 (+6.4%) | 55,965 (-1.5%) | 56,831 | 60,765 (+6.9%) |
| Futari 1.5 7000 | 154,766 | 164,333 | 414,346 (+8.2%) | 381,079 (-0.4%) | 382,789 | 401,101 (+4.8%) |
| Futari 1.5 9000 | 62,765 | 66,196 | 166,409 (+8.9%) | 152,050 (-0.5%) | 152,845 | 161,864 (+5.9%) |
| Ibara 7000 (loading) | 158,844 | 828,563 | 2,033,607 (+20.0%) | 1,688,480 (-0.4%) | 1,694,561 | 1,745,416 (+3.0%) |
| Ibara 9000 | 346,414 | 357,144 | 820,295 (+11.7%) | 731,333 (-0.4%) | 734,231 | 776,035 (+5.7%) |
| Deathsmiles 7000 | 178,001 | 190,004 | 452,557 (+8.0%) | 417,172 (-0.4%) | 419,039 | 434,527 (+3.7%) |
| Deathsmiles 9000 | 236,240 | 245,996 | 467,126 (+6.0%) | 443,202 (+0.5%) | 440,797 | 462,886 (+5.0%) |

So the earlier model made the CPU's frames 6 to 20% slower than HS3 does (the games' heaviest frames about 9%);
the model fitted to HS3 was within 1.5% on all twelve (0.6% on average). (The last column was taken before the PR
wait above was added; these frames were not measured again with it.) Fitted to the board it is 1 to 7% slower
than HS3 (4% on average): HS3 is faster than the board where these frames spend their time (their draw lists written
through P2, the multiplier, calls and returns), as the benchmark shows. On HS3 none of these frames' CPU work is longer
than a frame (1,706,144 cycles); the earlier model and now the board's made Ibara's loading frame longer (by 2%). Where Mushihimesama's
busy frames run late in the port, the rest of the time is the game waiting for the blitter (23,000 reads of its
status in frame 28200), which is the blitter's model's to say.

Where the earlier model went wrong, for the model fitted to HS3, measured with small loops on both (188 of them: hits, misses, every access kind,
the access patterns against the fetches, the games' own loops; the earlier model is within 5% of HS3 on 31 of them,
this one on 161, and 3.5% off on average):

- **A miss cost too much and the write-back more:** a clean line fill 21 cycles against HS3's 18, a write miss that
  pushes a dirty line out 42 against about 33 (a stream of write misses 23% slow, a copy 21%).
- **P1 was copy-back:** the CV1000 leaves CCR.CB clear, so P1 (where the interrupt handlers run, at VBR 8C000000) is
  write-through. A write there costs a posted bus write (14 cycles apart when they follow each other); the earlier
  model charged a write hit nothing (2.8 times too fast) and a write miss a fill and a write-back (3.5 times too slow).
- **Posted writes did not overlap:** an uncached SDRAM write was 10 cycles whatever followed; HS3 lets the CPU go on
  after 2, so with work between writes they cost almost nothing (a write with 4 instructions between: 1.42 times too
  slow; 8 instructions: 1.49).
- **The board's devices were cheap:** the blitter's registers 24 cycles against 27 (its status polled 10% fast), the
  8-bit area 10 against 13, and an uncached longword from the 16-bit flash was charged as one bus access (24 against
  50).
- **No pipeline:** a data access cost one cycle more every other time, wherever it was; HS3 shows it depends on
  where the access falls against the instruction pairs' fetches (8 accesses in a row: +6; a load between every
  other instruction: +0 aligned, +2 not). A taken conditional branch's second bubble and the load-use cycle were not
  charged at all (a load used at once: 25% fast).
- **Code was looked up only where an access was made:** a line of code with no data access in it was never fetched
  (each costs a fill, 18 cycles); the new model fetches the lines of the path between two accesses. (Code with no
  access at all, a 24 KB straight run of it, is 3.3 times too fast in both.)
- **The cache flush cost 28,700 cycles** (a sum standing for the flush loop's uncached fetches) against HS3's 262
  for CCR.CF; the loop itself is now charged as it runs (its instruction pairs read past the cache).
- **DMA** was charged by the CPU's accesses' model on top of the fixed 10 a byte; HS3's DMA from the NAND is 20
  cycles a byte (hence the loading frames above).
- Kept as they were (the differential test below): the cache's lookup and replacement (true LRU over 4 ways, as the
  manual's LRU bits are), the wait states and the widths read from WCR2 and BCR2, and which addresses are cached.

What the model fitted to HS3 left out, and was off on HS3 by more than a few percent in the loops (not in the
frames): code with no data access at all (a delay loop) got no fetch or branch costs (now every branch taken is an
event of its own, which the board's benchmark needed); back-to-back misses are 2 cycles cheaper on HS3; writes to the board's 8-bit area followed at once by the loop's branch 2 to 3 cycles cheaper; four
uncached reads in a row 10% dearer. Not modelled: the SDRAM's refresh (once every 3,072 cycles, a few cycles when an
access meets it; HS3 does it), the blitter taking the bus to read its draw list (BREQ/BACK), and the blitter's own
time, which HS3 does not cover: that is buffi's model above, unchanged. Nor is this a board: HS3 is matched to a
CV1000 board on its branch timing only, and its DMA and bus timing come from the manual.

### A rule tried and not taken: the fetch after a load-use stall

An outside audit (2026-11-22) proposed that when the first instruction after an access waits for the loaded register
(the load-use cycle), the next instruction pair's fetch should not move with that wait: its lead before its decode
would be 3 cycles plus the wait, so it could no longer meet the load's MA stage at the cache's port. Against the
twelve frames above, fitted to the board, it took 0.2 to 3.5% off each (Deathsmiles 9000 the most), which read as
"closer to HS3". It is not a pipeline rule, and it is not in the model:

- **HS3 does make that fetch wait.** On HS3's own loops a load used at once and followed by a new pair is the model's
  count to the cycle without the rule (`mov.l @r4,r1; add r1,r2` four times a round: 81,966 cycles against HS3's
  81,943; a load, a use, `dt`, `bf`: 32,799 against 32,772; a load, an add, a store, an add twice a round: 65,586
  against 65,559). With the rule those three came out 20%, 12% and 12.5% faster than HS3; the retire trace of the
  second shows HS3 retiring the instruction after the use two cycles after it, as the model does.
- **What it took away in the frames was a cycle the board has.** Of the 188 loops it changed only those above and
  Deathsmiles' copy loop (`mov.l @r6,r4; dt; mov.b @r3+,r1; mov.b r1,@r4; mov.l @r6,r2; add #1,r2; bf/s; mov.l
  r2,@r6`: the model 14 cycles a round, HS3 13). The model's extra cycle there is not the stall's: it is the access in
  the delay slot of a branch taken holding the target's fetch, which is the board's and not HS3's (the benchmark's
  CPU CACHE HIT, [above](#checked-against-the-board): the PCB takes one cycle a round more than HS3 with a load in
  the slot). Any `bf/s` loop with an access in its slot shows it, with no load-use in it at all (an `add` loop
  with `mov.l r1,@r4` or `mov.l @r4,r1` in the slot: the model 8 cycles a round, HS3 7; with an `add` in the slot, 7
  on both). The audit's rule cancelled that cycle where a load-use stall happened to come before it, so the frames
  moved toward HS3 for the wrong reason.
- **The board's benchmark goes the wrong way with it.** RAMP 8X8 SPR from +1.0% to +1.6% fast (4,875 against
  the PCB's 4,800: past the 1.5% the blitter and CPU tests are held to), CPU LIST BUILD from +0.9% to +1.1%; CPU
  QUICKSORT stays 6.4% fast; nothing moves toward the board.

The model fitted to the board is slower than HS3 on purpose where the board is (the rules above say where), so being
closer to HS3 is not by itself a reason for a change.

### Checks

- `GAME=<id> TIMING=1 tools/check.sh ROMS 6500` on all fourteen CV1000 games: translated code equal to the
  interpreter frame by frame with the model on, and two translated runs alike. With it off (`tools/check.sh`) the
  same, and each game's per-frame hashes over that play equal to the earlier build's (`--hash-log`, `--cpu blocks`).
- A differential test of the old model's file (from git, in another namespace) against the new one where they mean
  the same thing: 80 million random accesses through both caches after each set holds four lines (hit or miss, and
  the dirty line pushed out: all equal), WCR2's wait states for every value in areas 0, 4, 5 and 6, BCR2's widths for
  every value, and which of 10 million random addresses are cached: all equal, but for two places where the new one
  follows the manual and HS3: area 2's waits with the field 0 (the old one read 0 as 1, as for the SDRAM's CAS
  latency), and area 1 (the on-chip registers at A4000000, which the old one counted as cached through P0).

## What it does

Mushihimesama, played by a script with the ship out of harm and the shot held (the check's coin and start, then
`--press 4700:40000:0x11 --features out_of_harm=1`), counting the frames of the screen in which the game started no
draw list (it was late), per 2500 frames from frame 5000:

    this model (fitted to the board): 1 0 120 11 0 0 63 28 56 88 147 28 17 93   (652)
    the model fitted to HS3:          1 0  65  1 0 0 65  4  8 44  65  1  0 68   (322)
    without the feature:              0 0  64  0 0 0 64  2  0 10   1  0  0 64   (205)

(The stretches late without the feature too are stage ends and loading. Over the whole 40,000 frames the game is late
in 423 frames without the feature, 824 with the model fitted to HS3 and 1,346 with this one; the earlier model,
MAME's, made it 1,055.) Fitted to the board, the CPU's work of a busy frame is a few percent longer than HS3 gives
(the frames in [Checked against HS3](#checked-against-hs3)), and more of the stages' busy scenes now run past the
frame. Whether the board is late in the same frames has not been compared. It costs processor time:
[below](#what-it-costs).

## The setting

**Slowdown as the board** is on unless turned off, on the CV1000 and PGM games' Game page, in the launcher and the
in-game menu, on every system: these games are played and scored with their slowdown. Off is for a device too slow for
it. In settings.ini it is `slowdown` under `[features]`, a choice: 0 (or nothing) on, 1 off. It was `board_slowdown`,
1 for on, when it was off unless chosen; the launcher writes every feature down once one is changed, so many files
have `board_slowdown = 0` from that time, which only said what the default was. That name is no longer read, so those
files come to the new default too; a player who wants it off turns it off again. Online, the guest takes the host's
setting (docs/ONLINE.md): the host sends `slowdown=1` when it is off, and nothing, read as on, when it is on.

The headless runners (`spelunk-headless`) leave it off unless given `--timing`: they are the checks' runs of the board
alone, and the numbers here and in docs/PGM.md were taken that way. The game program follows the setting
(`--features slowdown=1` turns it off for a run).

## What it costs

The headless runner, Mushihimesama over the check's play (`--press 4700:40000:0x11`), on an Apple M-series MacBook Air
(2026-11-20, with other heavy jobs running beside it the whole time: the rates are low, the three were run side by
side so they share the load), frames a second:

| | frames 0 to 27,000 | 27,000 to 33,000 (stage play, the busiest) |
|---|---|---|
| without | 230 | 179 |
| with the model fitted to HS3 (origin/main, #37) | 95 | 65 |
| with this model | 83 | 58 |

In processor time this model takes 14-15% longer than the one fitted to HS3 over those runs (298 s against 260 s to
frame 27,000; 384 against 337 to 33,000; 543 against 476 over the 40,000 frames above, where the game spends more of
its time working than waiting). Fitting it to the board made it about 30% slower, mostly from making every branch
taken an event of its own; that was won back without changing a count (the per-frame hashes of the checks' play with
the feature on, all fourteen CV1000 games, and the benchmark's results, equal before and after): each path found is
made ready to run once (the lines it is fetched from, and the pipeline's steps that depend on the time with the
cycles between them added up) instead of stepped instruction by instruction every time; 8,192 paths are kept, two
places to a set of the table (1,024 in one place each lost a path a loop takes to another about 3% of the times one
was asked for); a DMA transfer notes the blocks it writes instead of forgetting every kept path; a cache line already
the most recently used of its set is not moved. Most of a frame's events with the feature on are the game's wait
for the next frame, a three-instruction loop polling a timer: a load in a branch's delay slot, then the branch.

On the PGM the board's waits cost nothing measurable (Dai-Ou-Jou's recipe, 9,000 frames: 1,606 frames a second
without, 1,602 with).

Android, in the emulator (arm64, on an Apple M4 Mac mini that had other work running, so the numbers move about):
the headless runner over 27,000 to 30,000 of the same play did 141 to 154 frames a second with the feature on (111 to
122 with the model before), 458 without; the app played Mushihimesama's first stage at full speed with it on, with no
frame of the port slower than 43 ms in the ten-second spans its log gives. While the Mac was busiest it fell to 37 to
47 frames a second for some 40 seconds of the stage's start with the feature on, and not without. Not tried on a
phone: one slower than that emulator may not keep up in the busiest scenes, which is what turning it off is for.

### The poll loop (machine/sh3.cpp's PollTurn)

Most of a busy frame's timing events are not the game's work but its wait: four instructions,

    loop: sub   r1,r0
          cmp/hi r0,r4
          bt/s  loop
          mov.l @r2,r0     ; (the delay slot)

turning until a word (a counter in main RAM that an interrupt moves, or a timer's TCNT) has gone far enough. Every
turn is an access and a branch taken for the model, and on Muchi Muchi Pork!'s stage 1 boss its turns are about
four fifths of the events: 165,000 turns a frame. Once two turns in a row have cost the same cycles, made their read
at the same point, and moved the model's state the same way, the turns after them are worked out instead of run, up to
where the loop ends (the turn whose test fails is run as the board runs it) or the block would stop (an event due, an
interrupt ready). The model's state is checked field by field:
- every field the same each turn, or later by exactly the turn's cycles;
- a time that stays the same must be in the past, so that it is never read;
- the cache's ways and their use order unchanged.

Each skipped turn takes the registers as the four instructions leave them. Its read is main RAM's word, which nothing
writes while the loop turns, or TCNT as it reads at that turn's cycle: the timers are brought up to it on a copy, and
brought up once at the end as the last read left them. Only in native mode: the interpreter's blocks mode, which
tools/check.sh compares the translations with, runs every turn, so check.sh compares the skip with the turns run too.
`SPELUNK_NO_POLL_SKIP=1` runs every turn.

The blitter's wait is the second shape, the same way:

    loop: mov.l @r1,r0     ; the blitter's status (B8000010)
          tst   #0x10,r0
          bt/s  loop
          nop

turning while the blitter's status reads busy (0). The status reads busy until the list's end (m_blitterDone) and
changes only as it is read: the board says, for each turn's read, whether it would read as the last did and change
nothing (Bus::ReadSteady), which it does while the read comes before the list's end. The turn whose read would come
after it is run as the board runs it. With the slowdown on, the list's end is also the CPU's event (IRQ1), which stops
the block anyway. It is the busiest loop of another shape left after the frame's wait: 1.4% to 6.7% of the timing
events, most in Mushihimesama, Matsuri and the dfk/sdoj engine's games.

Where every turn is alike, the turns are worked out all at once rather than one by one: a word of main RAM reads the
same each turn, so once the last turn has read it, each turn after leaves the registers as they are and costs the same
cycles. The number of turns is the number begun under the block's stop, and the cycles and the moving times go on by
that many turns' cycles. The blitter's status is alike up to the last turn whose read comes before the list's end
(found by halving, Bus::ReadSteady being true up to a cycle and at every one before it). A timer's count, which reads
otherwise each turn, is still worked out turn by turn. On Muchi Muchi Pork!'s stage 1 boss that is about 100,000
turns a frame once each: about 7% of the headless runner's time there.

With the slowdown off the timing model is not there to call, and the translated wait turns in its own block until its
stop: on the Steam Deck, with the slowdown off, Muchi Muchi Pork!'s frame spent 18% of its time there. The dispatcher
works those turns out before it runs the block (SH3::WaitAhead, runtime/dispatch.cpp): without the timing model a turn
costs its four instructions' cycles and nothing else, and once the register the loop loads holds the word it reads
(each turn leaves it so), every turn is alike. The turns before the block's last are counted into the cycles, and the
block runs the last itself, so it leaves the registers, T and the cycles as the turns would.

The shadow build (tools/shadow/) runs every skip both ways. A skip worked out all at once is also worked out turn by
turn, from where it began, and the two must give the same turns, cycles, registers, T, timers and model. Then it
puts everything back, runs the
same turns through the interpreter with both models at every event, and compares registers, cycles, T, the timers and
the model's every field. The first difference stops the run; the run goes on from the turns run. Muchi Muchi Pork!'s
boss state, 600 frames: 14,878 skips of 99 million turns, every one the same, and the per-frame hash log identical to
origin/main's.

### The translated sites' plans

Most of the model's time at a busy scene went to finding a path (Sh3Paths: a hash, a set's two places, the stale
check; and with 8,192 places for the 19,000 or so paths a boss streams, finding it again). A translated block knows
its sites, the data accesses and the branches taken in it, so each keeps its own (SitePlans, machine/sh3.h): the first
time a site's event takes the generic path, its plan is made (StreamNew's own way, Sh3Timing::MakePath) and kept in
the block's translation, in the site's slot, found from the instruction's address with no hash; from then on the
event runs it as data. A plan is kept only for a path that is known, whose key is the block's own (its start reached
by a branch, or the last event in it) and that read the block's bytes alone; two keys a site. A site's plan is used
only while its block runs, its code not written meanwhile, and the translation is used only while its bytes are RAM's
(a page of code put back or loaded with other bytes moves its epoch, and SH3::Lookup compares the bytes again), so a
plan can never be another code's; plans hold no state. At the bosses 86-90% of events are a plan's. SaiDaiOuJou's
boss, headless, 1,800 frames on an Apple M4: 6.09 s, 5.57 with the plans (-8.5%); on a Steam Machine see #18.
`SPELUNK_NO_PLANS=1` turns them off (main's generic path, the same frames); `SPELUNK_PLANT_PLAN=1` makes the first
plan served a cycle long, which the shadow check below must catch.

### The shadow check (tools/shadow/)

The model is made faster only where the result stays the same: every cycle it gives and every field of its state, at
every event. A build with `-DSHADOW_TIMING=ON` runs the model in use beside its reference, machine/bus_timing_ref.h.
That is the model as it was before its hot path was first worked on (4d3100e), kept unchanged, which tools/audit.sh
checks by its hash.
- At every data access and branch taken, the shadow build compares the cycles each model gives and the state's fields.
- The cache's 1,024 ways are compared every 256 events and at each frame's end.
- The first difference stops the run with both states and the last 64 events. A difference planted in the model
  (an on-chip register a cycle dearer) is caught at the third event of the run.
- The shadow build is about half as fast as a plain one.

`tools/shadow/full.sh ROMS` builds each CV1000 set's shadow build and a build of origin/main. It plays every set with
both and compares their per-frame hash logs too, and prints a table of the runs. Each set gets:
- the check's recipe with the slowdown, translated and interpreted, and without the slowdown;
- 10,000 frames at the first stage's boss (the state made by the run itself: `--save-on-arrival`, to_boss and out of
  harm; where the trip does not arrive, Akai Katana's, from 12,000 frames into play instead);
- every later stage begun;
- 200,000 frames of made-up play;
- tools/check_tools.sh and online rollback (`--rollback-test`), with the shadow build of the game program.

That is about 400,000 frames a set, a night on 8 cores. `tools/shadow/quick.sh` is the first four kinds of run alone
(3,000 frames at the boss), about half an hour a set.

## Not checked

Against a board, the model is checked only by buffi's benchmark (its CPU and blitter tests, on an Ibara PCB:
[above](#checked-against-the-board)), not by a game. The whole of a game's slowdown (the CPU and the blitter together, frame by frame) has not been compared
with a board, nor with the CV1000 core for MiSTer (Raki's, test builds only).

## Cave's 68000 board and IGS's PGM

On these boards the 68000 does the game's work itself, and the port has always given it the board's clock (16 MHz on
Cave's, 20 MHz on the PGM) with every instruction's own time; what a board can add is waiting: an access the board
acknowledges late, or the bus taken from the CPU. The rules for the boards' waits are read from two FPGA cores'
descriptions of the boards (THIRD_PARTY_NOTICES.md). The 68000's own times, and the time it takes to come to an
interrupt, have since been compared with Cave's core running ([below](#the-68000-against-the-cave-core-running)), and
the PGM's waits with its core running ([below](#the-pgm-against-its-core-running)); neither board has been compared
with a real one.

**Cave's 68000 board: nothing to add.** In Arcade-Cave_MiSTer (rtl/cave/Main.sv) every one of the 68000's chip
selects is acknowledged one of the core's 32 MHz clocks after the address strobe, before the 68000 looks for it: no
wait states at the ROM, work RAM, the sprite and layer memories, the palette, the sound chip or the EEPROM. The
sprite chip and the layer chips read their memories through ports of their own and never take the 68000's bus, and
there is no refresh. (The core's program ROM sits behind a cache in SDRAM; that is the FPGA's, not the board's.)
MAME's cave.cpp models no waits either. So the port's 68000 already runs as the core's does, the busy scenes'
slowdown in the port is the 68000's own work as on the board, and DonPachi, DoDonPachi, ESP Ra.De., Dangun Feveron
and Guwange do not offer the option.

**The PGM: the IGS023.** Read from Arcade-IGSPGM_MiSTer (rtl/PGM.sv, rtl/igs023*.sv) and then measured on that core
running ([below](#the-pgm-against-its-core-running)):

- Work RAM, the program, the IGS027A and the sound side are acknowledged at once. (The core's own SDRAM stalls are
  made up afterwards by running the CPU's clock faster, so they take none of the board's time.)
- A write to the IGS023's registers or the palette waits one cycle, a read none (the core's rule: the board's test
  ROMs do not measure them).
- Its video memory (900000), as a real board has it (measured with the core's test ROMs: docs/PGM.md, "Against a
  board"): every access waits 2 cycles; before each line of the picture the chip has the memory to itself from 6
  cycles before the line to 362 into it (less 2 for each pixel the background is scrolled within its tile), and an
  access begun then waits to its end; from there the chip's 16-cycle microcycle, its second half the CPU's: an access
  begun in the first half waits for the second. Unless the game has set the control register's bit 10 (the CPU
  first): then only the 2 cycles. (Until 2026-11-21 the port had the June 2026 core's rules here, which the board's
  results contradict: reads free, writes 3, the text row's 274 cycles and the background's tiles 9 cycles in 64.)
- At line 261's horizontal sync, three lines before the blank, the sprite DMA copies the sprite list from work RAM
  and holds the CPU off the bus, four of the chip's 50 MHz clocks a word (machine/pgm.cpp, `SpriteDma`, always on).

Not modelled: the IGS027A's time to answer (the core runs a recreation of the chip's ROM for Cave's games on an ARM
behind a cache, where the port answers at once as MAME does; Dai-Ou-Jou waits for the answer either way, and in the
runs compared it took the core two polls of the latch).

**How it is fitted in.** The board says what each data access costs at the time it is made (`M68K::accessWait`,
`Pgm::AccessWait`); the interpreter's Read and Write and translated code's reads and writes outside work RAM and the
program (runtime/native68k.h) both ask it, with the time of the bus cycle: the instruction's start and four cycles for
each extension word and each word read or written before it in the instruction (`M68K::accessInto`; the interpreter
counts it, and the translator, asked with `--access-times`, on by itself for the PGM's games, gives each access its
place as `c.At(n)`), so the two count alike. A read-modify-write instruction's write comes after the prefetch it makes
between its read and its write (`nr np nw`: 4 cycles more); a MOVE writes straight after its read. (The PGM's chip fetches come every 64 cycles, so the instruction's start
was not near enough: a video-memory clear came out 1,300 cycles a frame slower than the core.)
It is the **Slowdown as the board** feature on the PGM games' Game page, on unless turned off ([the
setting](#the-setting); `--timing` on the headless runner). Off, no board is asked and nothing is charged: the games
run exactly as before.

### The 68000 against the Cave core running

Arcade-Cave_MiSTer was run in a Verilator harness of its own (outside this repository: ../refs/arcade-cave, branch
`harness`; FINDINGS.md there): DoDonPachi from power-on, the program answered with no waits as a board's ROM does
(the core's SDRAM cache would add some), the port's play script (coin, start, two and a half minutes of the first stage
with the shot held and the stick moving, two bombs), the 64 KB of work RAM written down at every vertical blank and
compared with the port's (`--ram-log`), the bus cycles of chosen frames logged and laid beside the port's own trace of
the same frames. DoDonPachi makes this a sharp test: its random numbers are its idle loop's count (101272), so one
pass of the idle loop more or less in any frame since power-on changes what the game does later. Its 68000 inside is
fx68k (Jorge Cwik's, cycle-exact to the 68000's microcode); fx68k alone, on memory with no waits, was also run over
every instruction form in the twelve games' code (about 9,100, each with two sets of registers, and 7,500 cases of
branches, jumps, returns, traps and the status register), against the port's interpreter: they now all take the same
time.

What was wrong in the port, and is now as the 68000 does it (machine/m68k.cpp, and in the translator
tools/m68k/m68k.py what it counts itself):

- ADDQ and SUBQ to memory: 8 cycles and the operand's, a long 12 (ADDQ had been 12 for every size).
- A MOVE to -(An): no 2-cycle penalty for the predecrement (that is a source's only).
- MOVEM from memory to registers: 4 fewer for every mode but (An)+.
- BCHG, BCLR and BSET on a data register: 2 fewer for a bit below 16 (the 68000's tables give the most).
- ADD, SUB, AND and OR of a long into a data register, and ADDA and SUBA of a long, from a register or an immediate:
  2 more (8 and the operand's).
- EOR of a data register into another: 4 cycles, a long 8 (as the other ALU operations; it had been 8 and 12). SUBX of
  data registers 4, as ADDX. BTST of a data register's bit number in an immediate: 10.
- DIVU and DIVS: by their operands, a bit at a time, as the 68000 divides (Jorge Cwik's rules: 76 to 140 and 120 to
  158 cycles, 10 or so where the quotient would not fit), where the port had always charged the most. A divide by
  zero 38, TRAP 34, CHK's exception 38.
- The interrupt is not taken at the first instruction's end after the line rises: the 68000 passes the lines
  through two samples, and looks at them once in each instruction, as a rule in its last prefetch, so it finishes
  the instruction whose look begins 4 or more cycles after the rise (a read-modify-write's prefetch comes before its
  write, a long register operation's before its internal cycles: `M68K::InstructionTail`). Where an instruction looks
  was then checked form by form: every instruction form of the twelve games' code (8,176 of them, and 500 cases of
  branches, jumps, returns and traps) run on fx68k alone with the line raised at every half cycle, against the port's
  interpreter doing the same. Those that look elsewhere, now as fx68k: a MOVE to (An)+, and a MOVE.L from a register
  or an immediate to (An), d16(An) or an index, 8 cycles before the end (as the last write begins; a MOVE.L from
  memory or to an absolute address 4); MOVEM from memory 4 (from (An)+, before its reads); DIVU and DIVS at the end
  (a DIVS by a negative number 2 before it); ABCD, SBCD, ADDX and SUBX on -(An) 4 before the write; CMPM before its
  second operand's last word; MOVEP to memory 4, from memory 8; LEA with an index 4, PEA with one 14 and of an
  absolute address 4; MOVE, ORI, ANDI and EORI to CCR and SR 4 (the MOVEs from a register or memory
  found later, by Ketsui's bot run: docs/CAVE68K.md); UNLK 8; TAS on memory and CHK 4. Every form now looks where
  fx68k does. Where that comes inside translated code, the run's last stretch before the interrupt (2,048 cycles) is
  done an instruction at a time, by the interpreter in every mode, so that translated code and the interpreter still
  go alike (the PGM's 8,192: with its waits a block can take longer; docs/CAVE68K.md, "Long bot runs").
- An autovectored interrupt takes 36 cycles and its acknowledge, which waits for the 68000's E clock (its clock
  divided by ten, running on from power on): 13 to 22 cycles by where the E clock is when it begins
  (`M68K::InterruptTime`; it had been 44 flat). The E clock's place against the CPU's first instruction is the core's.

With these, the whole of DoDonPachi's work RAM was equal to the core's at every blank through power-on, the boot,
the title and the attract demo, to frame 3,127 (the game starts at 3,100), where it had been 141 (equal to frame 129,
then the idle count apart from frame 130 on), and frame by frame the idle loop starts within half a cycle of the
core's. From there the differences began where the YMZ280B's interrupts come (a voice of the sound chip has ended;
DoDonPachi's handler takes one that comes inside the blank for the blank's, bit 2 of 800000 being low, and counts it
in 101270). Four more things were wrong in the port, and are now as the board does them:

- A voice's end raises the interrupt at the chip's own tick (88,200 a second, from its 16.9344 MHz crystal), and the
  68000 is run exactly to it (`Ymz280b::NextIrq`, `Cave68k::RunFrame`); it had been raised at the next sixteenth of a
  frame, up to a millisecond late.
- The chip takes a write when the 68000's bus makes it: a MOVE's write comes just before its last prefetch, 8 cycles
  before its end (a long's high word 12), not at the instruction's start (`M68K::writeTime`, counted the same by the
  interpreter and translated code). DoDonPachi starts its sounds with `move.w Dn,$300002`, 8 cycles in: a key-on
  that came within those cycles of one of the chip's ticks had started its voice a tick early, and its end and
  interrupt came a tick (11 µs) early.
- A MOVE from memory reads after the words of its source's address (`move.w $800000,d0`: 8 cycles in, `M68K::readTime`),
  and that is when the board reads the blank's bit. A sound interrupt taken again just before the blank's end
  (DoDonPachi leaves the chip's status unread in the blank, so the line stays up) had been counted by the port as a
  blank and by the core as the sound chip's.
- A key-on starts a voice from its first sample at its first tick, whatever it played before: the port had kept the
  old sound's place between two samples when a voice was started again while it still sounded (as DoDonPachi does
  with its tunes), and so ended it up to 512 / (pitch + 1) ticks off. The core starts it afresh (`lerpIndex` to 0, a sample fetched at once: ChannelController.sv); MAME does
  not (its key-on leaves `output_pos` as it was), and the datasheet does not say: this is the core's rule, not checked
  against a chip, and it is in the shipped timing.

The board's chip runs from its own crystal, and the port keeps it so. The core runs the chip from the CPU's clock
instead (a tick every 181.5 cycles, 88,154 a second, an FPGA convenience), so its ticks drift from the board's by one
every 1,900 or so, and a run with the board's timing parts from the core's at the first sound interrupt that the drift
moves across an instruction. To compare the rest, the headless runner can time the chip as the core does
(measurement only, `--ymz-ticks 181.5:-0.5:158:0:2:2.5:5.5:7:157`): ticks of 181.5 cycles; the core's scheduler
comes to each voice in turn, 157 cycles into the tick plus 2.5 for each idle voice before it, 5.5 for each playing
one and 7 for each fetching a sample (the core's state machine: 5, 11 and 14 of its 32 MHz clocks; Arcade-Cave's
ChannelController.sv and AudioPipeline.sv), and takes a key-on written before that in this tick, after it in the next
(and, as that visit can run past the tick's end, a key-on written before the last tick's late visit with that tick);
a voice ends a byte past MAME's rule (when its address reaches the end address), its interrupt 158 cycles after the
tick it ends in, when the scheduler comes to it next, behind the voices before it as they stand in that tick. First
fitted on the core's interrupt line in its bus log (every voice end from 3,127 to 3,384 within -1.5 to +0.5 cycles,
with a fetching voice charged as a playing one and the slots of the tick before); the fetch's own cost, the next
tick's slots and the late visit were found from the core's bus log at frames 8,908 and 9,442 (below). The core used for this reads the sound ROM at zero wait as a board's chip does (its SDRAM path adds ±20
cycles to every voice's end, an FPGA artefact): FINDINGS.md in the harness, `CAVE_SIM_SNDROM`.

Why the port's own timing parts from the core at 3,127, measured: it is not the clock's rate alone. The voice that
ends in frame 3,127 ends 3,505 cycles sooner on the port's chip than on the core's: 1,339 of them are the rate (the
core's slower tick, 0.05% over the voice's life: the port's own rules at 181.5 cycles a tick end it at 27,503 in the
frame, against 26,164), and 2,166 the core's own rules (its voice plays a byte past the end address, about 2,000
cycles at that voice's pitch; its interrupt comes 158 cycles after the tick; 11 more for its scheduler's slots).
Either alone is enough: the core's rules at the crystal's rate part at 3,127 too, and so does the crystal's rule at
the core's rate. (The lone frame at 2,977 is the same voice end on the port's timing landing a pass of the idle loop
sooner.) Which end rule the real chip has, the core's or MAME's, is not known: the core's YMZ280B is its author's
own, not taken from a chip, so the port keeps MAME's. With the board's crystal, then, the port matches the core up to
the first sound interrupt that either difference moves across an instruction, and no further check of the rest is
possible that way; everything past it is compared with the chip timed as the core's.

So timed, DoDonPachi's whole work RAM was equal to the core's at every blank for its first 5,269 frames (power-on,
the boot, the title, the attract demo and the first 38 seconds of stage play; the game starts at 3,100), sound
interrupts and all. At 5,270 the blank counter (101270) parted: the key-on above (a tune started again over itself
had ended two ticks late, so one more pass of the handler fell inside the blank on the core). Then at 6,294 the idle
counter, at the blank only: a sound interrupt that the core took one instruction later than the port, after a
`move.l d0,4(a0)` whose last prefetch began 6 cycles after the rise. That led to the form-by-form check of where each
instruction looks at the lines (above), which found the MOVE.L rule and the rest: equal to 8,961. There the core took a
sound interrupt (frame 8,908) one instruction later than the port, its line rising 3.5 cycles later: seven voices
before it playing, one fetching a sample in the tick that raised it, which the model had charged as the tick before
had them. And at 9,443, a lone frame: a voice keyed on 4 cycles into a tick, before the scheduler's visit of the tick
before (which, with seven voices before it, came 190 cycles into that tick, past its end), started with that tick on
the core and a tick later on the port. With these three in the measurement model (above), the whole work RAM is equal
at every blank for **all 10,400 frames** of the play script (3 minutes 1 second: power-on to 2 minutes 13 seconds of
stage play), in translated code and the interpreter alike.

Slowdown, the same run: the frames on which the game is late (its idle loop not run between two blanks: none of its
fetches in the core's count, at most one pass in the port's, the one the blank cut) are the same on both sides: 139 in
the boot (its ROM check and screens) and, from frame 3,000, these 44, in the stage's start and its busy scenes: 3,009-3,011, 3,498-3,501, 5,058, 5,441, 6,976-6,989 (6 of them),
7,621, 7,959-8,047 (7), 8,698-8,699, 8,844-8,884 (14: from 8,861 to 8,884 the game runs at half speed, every other
frame late) and 9,439, 9,502, 9,505. Thirteen more frames (ten from 3,000) show one idle pass on the port and two fetches of it on the
core: the pass the blank cut, counted on both sides of it; the work RAM is equal on all of them. With the shipped
timing the same script slows down the same way where it follows the same play (the same seven frames to 3,501), and
past 3,128 by its own course: 54 frames from 3,000 with at most one pass, 18 of them in its half-speed stretch, 8,887-8,930. Earlier notes named
8,730-8,750, 8,880-8,930 and 9,345-9,560 as busy: those were the late frames of earlier, differently timed runs.

Slowdown with the shipped timing, as players get it (measured 2026-11-22; no default changed):

- Run as shipped from power-on, the same script takes another course from frame 2,785, the attract demo, because the
  random index (101272) is the idle loop's count and the reset's phase moves it (below). From about 3,750 the
  game draws other random values (101Bxx), and its stage runs some 46 frames behind the core's by 5,441. Its busy scenes are
  the core's, later: the late frames come in the same clusters (8 against 9 at 6,976 / 7,021, 7 against 9 at 7,959 /
  8,005, the half-speed stretch 14 frames over 24 at 8,861 against 18 over 44 at 8,887), so compared frame by frame
  they differ, by the play's course, not by how the shipped timing slows the same scene.
- The same scene, both timings: run as the core's up to a frame and with the shipped timing from there
  (`--ymz-ticks-until FRAME`), so that both start from the same state. From 6,900 and 7,900 the work RAM stays equal
  for the 250 frames after (no sound interrupt moved far enough to matter); from 8,600 and 8,800 it parts at 8,823, and
  the late frames of the half-speed stretch are still the same 14; from 9,400 it parts at 9,402, and two frames are
  late that were not on the core (9,507 and 9,509) because the game's state differs from there. From 9,480 they are
  the same again, and from 9,420 or 9,450 one frame differs (9,508, where the core had 54 passes of the idle loop to
  spare). So at the same moment the shipped timing slows down where the core does, and frames differ once the
  random numbers part, which happens on a board too, power-on to power-on.
- The parting at 131 (the idle counter) is the reset's phase alone: the core's 68000 starts 15.5 cycles into its
  first frame, the port's at once; with `--start-cycle 15` the shipped timing is equal to 3,127. On a board the phase
  is whatever its reset gives, so each power-on of a board draws other random numbers in the same way. It is not
  something a player can feel as speed: the game's own work and its slowdown are the same; only which random
  choices it makes differ.

What ties the core's slowdown to a board, besides fx68k being cycle-exact and the frame's rate probed on a board: a
52-minute side-by-side of a DoDonPachi board and the MiSTer core in play ("[PCB vs MiSTer FPGA] Dodonpachi", znpfg.,
YouTube I6vFuRVPgHE; credited to the players SON and Akatronics), whose uploader concludes that "the slowdown in much
of bullet situation for MiSTer cave core was same as original PCB". It is a composite capture conformed to 60 frames
a second, of play rather than a fixed script, so it supports the core's slowdown but cannot be compared frame by
frame; it has not been analysed here.

For the comparison the port runs as the core does: its frame is 272 lines of 1,024 cycles, 57.44 Hz, with a blank of
32 lines, as the core's (the rate nullobject probed on a Dangun Feveron board: docs/CAVE68K.md, "The frame"; the
port's default since 2026-11, where it had been MAME's unmeasured 271.5 lines, 57.55 Hz, and a 2 ms blank: with those
the same run parts from the core at frame 17, lastingly at 129, the boot's end; `--frame-lines 271.5 --blank-lines
31.25` runs them still), and `--start-cycle 15` (a measurement switch of the headless runner: the core's
68000 begins its first instruction 15.5 cycles into its first frame, the port's at once; on a board that depends on
when the reset is let go). The work RAM is taken as the bus had it at the blank: an instruction the blank falls in
has not yet made its last writes (`M68K::RamAt`).

Which timing each count holds for (DoDonPachi's play script, the whole work RAM at every blank against the core's z2
run; "the idle counter" is 101272-101273, the random index the idle loop advances):

| The port run with | Frames equal from power-on | Where it parts |
|---|---|---|
| the chip timed as the core's (`--ymz-ticks 181.5:-0.5:158:0:2:2.5:5.5:7:157`) and the core's reset phase (`--start-cycle 15`): measurement | all 10,400 | |
| the shipped timing (the chip on its crystal, 272 lines) and the core's reset phase (`--start-cycle 15`) | 3,127 (and frame 2,977 the idle counter only) | 3,128: a sound interrupt moved by the chip's clock and end rule (above) |
| the shipped timing exactly as shipped (the 68000 let go at once) | 130; leaving the idle counter aside, 2,784 | 131: the idle counter (the reset's phase); 2,785: the attract demo's random effects |
| the old default (271.5 lines, a 2 ms blank) | 17; lastingly 129 | 18: an EEPROM read's bit; 130: the boot's end |

The reset's phase is the board's to choose (when its reset is let go against the blank), so the second line is the one
that says how far the shipped timing follows the core; the third is the port as a player runs it, started the moment
it is.

What it costs: nothing to speak of; about 140 instructions a frame are interpreted for the exact stretch before the
frame's interrupt, and translated code notes the time of each MOVE's memory access (a store). The game's state hashes change (the timing is different), as a deliberate fix.

The other Cave 68000 games against the core (2026-11-22). The core runs five of the port's seven: DoDonPachi, ESP
Ra.De. (`--game 3`), Guwange (5), Dangun Feveron (0) and DonPachi (2); Mazinger Z and Air Gallet are not in it. Each
was run from power-on with its MRA's image unpatched (the harness's mkrom_mra.py), its program and sound ROM at zero
wait, and the play script of its recipe with continues (coin and start every 300 frames from 3,300) to keep the ship in
play. Two faults were found in the port, both now fixed:

- A read of memory by any instruction comes at its bus cycle, not only a MOVE's (`M68K::readTime`, interpreter and
  translator). ESP Ra.De. and Guwange wait for the blank with `btst #2,$800001` (`$300001` on Guwange), whose read
  comes 12 cycles in. And a read that falls past the frame's end, in an instruction the frame's end cut, sees the next
  frame's blank. ESP Ra.De. had parted from the core at frame 133, the boot's end.
- A YMZ280B voice that ends in the 2 ms before the picture (ESP Ra.De. and Guwange make the picture 2 ms after the
  blank's interrupt; DoDonPachi 90 µs) raised its interrupt only at the picture: `Cave68k::RunFrame` ran the CPU there
  before it looked for the chip's next interrupt. ESP Ra.De. had parted at frame 3,760.

With both, and the chip timed as the core's: ESP Ra.De.'s work RAM is equal at 5,997 of the 6,000 frames of its
script (boot, attract, 58 seconds of stage 1 with continues); the three others (4,470, 4,472 and 4,476) are the
idle counter alone at the blank, equal again after. Its late frames are the same: all 268 frames late on the core are
late on the port, 68 of them from the coin at 2,500, among them a stretch at a third of the speed (5,056 to
5,189, two frames late in three). The port shows 25 more frames with one idle pass, where the core counts two fetches
of it: the pass the blank cut, as on DoDonPachi. Guwange is equal at all 1,600 frames run (boot and attract; the old
port parted at 1,357). DoDonPachi's state hashes are unchanged by both fixes. Not run yet: Dangun Feveron and
DonPachi (their images are made), and Guwange in play. The state hashes of ESP Ra.De. and Guwange change: deliberate
accuracy fixes.

In play (2026-11-23, on main after #77). The core was resumed from its saves; the port ran from power-on with the
chip timed as the core's. "Dropped" means a frame in which the game's own count of frames done does not move:

| Game, run | Frames compared | Work RAM equal | Dropped: core / port |
|---|---|---|---|
| Guwange, play from 1,501 to 9,000 (its busiest stretch 6,588-7,366 in it) | 7,500 | all 7,500 | 376 / 376, the same frames |
| Dangun Feveron, power-on to 5,000 | 5,000 | all 5,000 (the stack's page aside) | none in play on either |
| ESP Ra.De., stage 6 from 3,001 to 6,000 | 3,000 | all 3,000 | 672 / 672, the same frames |
| DonPachi, power-on to 7,389 | 7,389 | to 2,506 (3,127 with `--oki-slots`) | the same to 5,937, none in play; after 2,507 the two games differ |

ESP Ra.De. had one frame apart (5,161, two bytes). That was the port's RAM log, not the game: a blank fell inside a
`movem.w d2-d3,$2a(a6)`, and the log put that MOVEM's writes in the wrong order. On fx68k, a MOVEM to memory writes
ascending, a long's high word first; to -(An), the low word first. Fixed (`M68K::RamAt`, measurement only).

DonPachi parts at 2,507 in its idle count (the game's random numbers) and stays apart after that. The cause is the
OKI chips' status bit. The game stops a voice and reads the status until the voice's bit falls. The port's chip, like
MAME's, drops the bit at once. The core's chip (jt6295) serves its four voices in turn, a quarter of a sample each,
and the bit falls only at the voice's turn: 6 reads on the core where the port made 1. `--oki-slots ORIGIN[:DELAY]`
times the status as jt6295 does, for this comparison only (off by default; nothing shipped changes). With origin 50
to 54 the run is equal to frame 3,127; at 3,128 the core repeats a start one more time, which no start delay fits
(left open: it moves only the random numbers).
What it moves is a few hundred cycles in a frame with such a stop, and the random numbers after it: not something a
player can feel. The real chip's timing is not known.

### The PGM against its core running

Arcade-IGSPGM_MiSTer (wickerwaka's) was run in a Verilator harness of its own (outside this repository:
../refs/Arcade-IGSPGM_MiSTer, branch `harness`, sim/harness): DoDonPachi Dai-Ou-Jou from power-on, its ROMs put in as
the MiSTer downloads them (the MRA), its NVRAM file in work RAM, the core held in reset until a blank begins (frame 0
starts there, as the port's), the port's play scripts, the 128 KB of work RAM written down at every blank and
compared with the port's (`--ram-log`, as the bus had it at the blank), and the bus cycles of chosen frames logged and
laid beside the port's own trace of the same frames (`SPELUNK_CYCLE_TRACE`). Save states (Verilator's) let a stretch
be run again from where it was; a resumed run's log is the same, byte for byte, as the run's own. The core's 68000 is
fx68k, so the 68000's own times were already the core's (above); what was found is the board around it.

What was wrong in the port, and is now as the core runs it (machine/pgm.cpp):

- The IGS023's waits ([above](#caves-68000-board-and-igss-pgm)): the port had charged a cycle on every access to the
  video memory and nothing on the palette and registers; writes there wait (one cycle, three in the video memory),
  reads do not, and the background layer's tile fetches hold the memory too, not only the text layer's. Boot frame 9
  had been some 2,000 cycles apart from this alone (Dai-Ou-Jou clears its video memory and palette at boot).
- The sprite DMA at line 261's horizontal sync (it had been 384 cycles sooner).
- The 68000 reads the Z80's RAM and the sound latches as the Z80 has them at the time of the read: the Z80 is run up
  to it first. It had been run at sixteenths of a frame, so a read saw it as it was up to 21,000 cycles before;
  Dai-Ou-Jou's boot waits in a loop for the sound driver to answer (18B0BE).
- The sound side's clock: 33.8656 MHz (the core's, "measured on a board": its Z80 is a quarter of it, the ICS2115 is
  run on it), where it had been MAME's 33.8688. The Z80 now runs 3,075 cycles for the 68000's 7,264.
- The level-4 interrupt comes at a line's horizontal sync, 126 cycles into the line: 62 lines and that much after the
  reset for the first (it had been at the line's start). Its phase on a board depends on when the reset is let go; the
  harness sets the core's counter at the release to match.
- The switches' word (C08006) reads FF in its high byte, as the core has the lines; MAME reads them 0, and Dai-Ou-Jou
  keeps the word's complement at 803764. (Not a timing matter; which a board does is not known.)

The harness sets what the port fixes and a board leaves to chance: the V3021 clock's time (2003-01-01 00:00:00, the
port's), the level-4 counter's phase, the switches (all off).

With these, Dai-Ou-Jou's work RAM (less the stack's page, 81FC00-81FFFF, which holds only what calls left behind)
is equal to the core's at every blank but a handful, over its attract show, a coin, the start, the ship and style
chosen and the first stage played with the stick moving (frames 1 to 2,648: 2,631 equal), and over a stage-5 game played to its boss
with the shot held and the ship kept out of harm (a poke of its frames out of harm, 810398, the same in both)
(frames 1,251 to 9,500, the core's run resumed from its save at 1,250: 8,205 of 8,250 equal, through the stretch
before the boss where it slows down). The frames that differ are each alone, and the next is equal again, so on
both the game drops the same frames: a palette fade the game runs
across a blank (319 to 341, 586 to 590), where the port finishes a few hundred cycles sooner or later and one more or
one less colour step falls before the blank; frame 16 and 1,310 alike; and in the stage, every one of the 45 is a
frame whose work runs up to the blank (the port's idle loop goes round at most twice in it: 49 such frames), where the
blank finds the sprite list half made on one side. Those few hundred cycles are the FPGA's own:
the core's background fetches move with its SDRAM, and a read of the program now and then waits a cycle on its ROM
cache while its clock catches up; the port does not model either (a board's would not be the FPGA's).

Not modelled: the IGS027A's time to answer (above). Not compared: the picture, the sound.

**Which core.** The runs above were made on wickerwaka's repository as of June 2026 (e898860). In July its IGS023's
video memory arbitration was rebuilt from logic-analyser captures of a real board, and its program fetch from SDRAM
was made to go first (MiSTer-devel/Arcade-IGSPGM_MiSTer 7ad8046): the port's video-memory rules above were fitted to
the older core, which the board's own test-ROM results contradict (docs/PGM.md, "Against a board"), and the port now
follows the board. The harness was then built again on the current core (MiSTer-devel main, outside this
repository; the older binary and its saves kept), and the play scripts run from power-on (a coin, start, the selects
with the shot, then the shot held and the stick moved; the ROMs and NVRAM as the core's MRAs give them):

| Game | frames | equal, the board's rules (now) | the June core's rules (0.1.0-beta.1) |
|---|---|---|---|
| Dai-Ou-Jou | 2,700 | 2,697 | 591 (parts at frame 16, then a frame or two off) |
| Ketsui | 3,400 | 3,396 | |
| Espgaluda | 3,400 | 3,396 | |
| Black Label | 5,100 | 5,099 | |
| DoDonPachi II | 762 (then the core fails its ARM check) | 762 | |

The frames that differ are lone ones, each followed by an equal frame: the attract show's palette fade across a
blank (Dai-Ou-Jou 321, 323, 341; Ketsui 310-311; Espgaluda 315-316), and two in play (Ketsui 1,403 and 1,405 as the
game begins; Espgaluda 1,682 and 1,859). (Against the June core with its own rules, for the record: Ketsui 3,381 and
Espgaluda 3,372 of 3,400.)

Black Label's one is frame 1,725. **DoDonPachi II** is equal through all 762 frames of its boot, and there the core
stops: the game asks the IGS027A whether it started well (145A12 reads D10001), the port's ARM answers 81 (yes) and
the core's 86, and the core shows "ASIC27 ERROR 86 / FALSE CODE". It does so in every run of the simulation, with the
core's own MRA and the same ROMs as the port's, so nothing after it can be compared until the core's ARM works there
(whether the simulation or the core is at fault is not known). Before that, the simulation's clock chip ran a second
ahead of the port's (its first second counted at once), which parted the two at frame 583: the harness was fixed.

**Where they slow down.** The games' busiest stretches, from a save of the play run, the shot held and the ship out
of harm, sent to the stage named (a poke of the stage number):

| Game | stage | frames | equal | frames with no new picture |
|---|---|---|---|---|
| Ketsui | 5 | 1,251-5,300 | 4,047 of 4,050 | 156 on both (152 of them 1,251-1,405, the stage's start) |
| Espgaluda | 1 | 1,251-5,500 | 4,228 of 4,250 | 48 on both (35 of them 5,232-5,324) |
| Black Label | 5 | 2,751-6,300 | 3,541 of 3,550 | 847 on both (351 to 3,104, the stage's start; then 4,043-4,231, 4,283-4,891, 5,204-6,298) |
| Dai-Ou-Jou | 5 | 1,251-9,500 | 7,571 of 8,250 | port 1,401, core 1,403 (151 at the stage's start, then most of 2,000-5,000 and 6,500-9,500, up to one in four) |

In Ketsui, Espgaluda and Black Label the port's frames and the core's line up at the same index from first to last
(no frame early or late on either side), so the two drop the same frames. Dai-Ou-Jou does the same up to frame 7,359.
There the core drops a frame the port does not, and again at 9,140, so from each of those frames the port is one frame
further ahead. Between them, three of every four frames are still equal at the new offset. The fourth differs almost always in one byte
only, at 80F9F9. Why they part: the game's own slowdown
(13C21E-13C302: when the objects on screen pass a threshold it waits a second blank) is decided the same on both;
what differs is whether its work beats the blank. At frame 7,357 the work runs up to the blank on both (the port's idle
loop goes round 8 times, the core's 2 fetches of it), and the core's 68000 is 413 cycles behind the port's by then
(it is so in every frame of the stretch, 300 to 400 cycles by the work's end, about 0.15%). The core reaches the wait
just after the blank and so waits one more. Its extra cycles are waits of 1 to 3 clocks, not whole bus cycles (most in
the loop at 180E08-180E0E): the FPGA's ROM cache catching up, as in the play run above, which a board's 68000 would not
have. So the port is likely the closer to a board there; nothing was changed. Elsewhere the frames that
differ are again single ones, each followed by an equal one: a buffer the CPU is writing when the blank comes (a
palette fade's colours, a 30-byte piece of the sprite list, the stack), caught at a slightly different moment on each side.

**The picture.** Ketsui's background scrolls each line by its own amount in play (some 200 of the 224 lines, a pixel
or two either way), and the IGS023 reads that scroll by the line on the screen, not by the tilemap's row: the core
(rtl/igs023_bg.sv reads 907000 + 2 x the screen line before each line), MAME and the port alike. The core's picture
at frames 2,500 and 3,300 of the play script against the port's (`--raw-shots`): the background is the same to the
pixel, row scroll and all; what differs (1% and 4% of the pixels) is a sprite's colours in a palette the game is
changing at the time (the hit counter's "x21"), which the core shows as the palette is at its lines and the port as
it is at the blank. The background's scale register (904000) is written 0610 then 0210 by Dai-Ou-Jou, Black Label and
DoDonPachi II (the BIOS) and never by Ketsui or Espgaluda: its low bits, 10, are the chip's normal rate, and neither
the core nor MAME draws it.

### Checks

The headless runners count a game's idle loop (`--spin ADDRESS` with `--cpu blocks`) and write a line a frame
(`--frame-log FILE`: the frame, whether the game had a new picture, the spins, the board's waits). The idle loops:
Dangun Feveron 0772F2 (it also counts in 1015F3), DoDonPachi 0571AC (counting in 101273), Dai-Ou-Jou 13C30A
(`tst.b $803932; bne`, 26 cycles a spin).

Dai-Ou-Jou, started at stage 5 and sent to its boss with the ship out of harm and the shot held (frames 2000 to
9000 of a run; `--features start_stage=5,to_boss=1,out_of_harm=1`):

|  | spins a frame (mean) | fewest in play | waits a frame (mean) | frames with no new picture |
|---|---|---|---|---|
| without | 5,588 | 0 | 0 | 796 |
| with | 5,573 | 0 | 390 cycles | 794 |

The frames with no new picture are the same stretches in both (the stage's start and a pause at 5,560): the game
touches its video memory little while the picture is drawn, so the waits come to about a tenth of a percent of a
frame in play (15 spins, 400 of 338,000 cycles), and they fall mostly where it writes much of that memory at once
(a screen being cleared: up to 64,000 cycles in such a frame). By this model the PGM's own waits change Dai-Ou-Jou's
slowdown very little: where it slows in the port, it is the 68000's own work (and the sprite DMA, modelled before).
The run of the checks' play (`games/ddpdoj/recipe.txt`): 7,641 spins a frame without, 7,626 with, 803 cycles of
waits a frame.

On Cave's board there is nothing to compare. For the record, the port's spare time there in the checks' play
(`--cpu blocks`, from the first stage's start to frame 7000): Dangun Feveron 1,912 spins a frame at the median and
1,313 at the 5th percentile (2 frames of 3,900 with no new picture); DoDonPachi 2,815 and 1,467 (none).

The other PGM games over the checks' play (7,000 frames from power-on, attract mode and the first stage's start),
the board's waits a frame on average, and the frames with no new picture without and with them: Dai-Ou-Jou 754
(2,413 / 2,524: the difference is in screens cleared between scenes), its Black Label 383 (3,605 / 3,607), Ketsui 687
(2,277 / 2,278), Espgaluda 577 (1,796 / 1,796).

DoDonPachi II: Bee Storm (added 2026-11-15; its wait for the frame is the routine at 149DC0, counted at 149DCE) over
its recipe (`games/ddp2/recipe.txt`): 65 cycles of the board's waits a frame on average (11 in play), 2,491 frames
with no new picture without and with them (all before the game begins: none in play), 752 rounds of the wait a
frame in play either way (562 at the fewest without, 563 with). Its time is not the video memory's but the ARM's:
what a command to the IGS027A costs the 68000 is what the ARM's interrupt handler takes, and that is run, not
estimated (docs/PGM.md, "The ARM beside the 68000"), with the feature on or off. Checked with `--timing` as the
others (docs/PGM.md).

Checked: `GAME=<id> tools/check68k.sh rom 7000 $(grep -v '^#' games/<id>/recipe.txt) --timing` on the four PGM
games: translated code equal to the interpreter block by block (some 74 million blocks for Dai-Ou-Jou), and two
translated runs and the interpreter alike frame by frame. With the feature off, `tools/check68k.sh rom 7000` passes on
all nine 68000 and PGM games, and each one's per-frame hashes over that play are the same as origin/main's build's.

Not checked: a board. The PGM's core running: Dai-Ou-Jou only ([above](#the-pgm-against-its-core-running)); the other PGM games are next.
