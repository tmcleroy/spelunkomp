# Plan: the next speed work on the CV1000 (a second core; the timing model less often)

A design note, 2026-11-28, on main bebdc7b. Nothing here is built. It weighs two ideas for issue #18 (slow on
low-power CPUs): work moved to a second core, and the timing model run less often. For each: what it would take, and
what it can save at best and in practice, bounded by the measured profiles. Frames must stay exact (the same hashes);
an idea that cannot keep that is out.

## What is measured

The device numbers are the deck-perf round's (../spelunkomp-data/research/deck-perf/SHELVED.md and its printed files):
headless, 1,800 frames from each boss (Akai Katana: stage 1, frame 12,000), `--timing --no-raster`, clang 18.1.3,
no PGO. Machine: Steam Machine, AMD Custom CPU 1772 (Zen, ~5 GHz). Deck: Steam Deck, AMD Custom APU 0932 (Zen 2,
~3.5 GHz). The new numbers for this note are from a MacBook Air (Apple M4), main bebdc7b, `nice -n 10`, `sample` at
1 ms. They are marked "Air".

Time a frame, headless, the boss (main bebdc7b, with c1; the Air: 3 rounds, median):

| | ddpsdoj | mmpork | akatana | deathsml |
|---|---|---|---|---|
| Deck | 7.9 ms | 5.0 | 12.6 | 6.4 |
| Machine | 3.7 | 2.4 | 5.8 | 3.0 |
| Air | 2.24 | | | |

Where the time goes (self shares, perf record, main 7fed65e, before c1; Air: `sample`, bebdc7b):

| part | Deck | Machine | Air |
|---|---|---|---|
| the timing model, all of it (TimeAccess, Find, TimeBranch and the rest of bus_timing.h) | 75% | 75% | 64% (timing off: 4.03 s to 1.47 s) |
| of which Find / Walk / Run / Access (split build) | 20 / 14 / 12 / 12% | 15 / 17 / 15 / 15% | 14 / 21 / 9 / 16% |
| SH3::Lookup | 7.3% | 6.0% | 3.1% |
| the rest: translated code, RunBlocks, stores, dispatch | ~18% | ~19% | ~33% |
| the sound chip (Ymz770 and AmmDecoder) | under 0.3% (ddpsdoj); 0.4-0.65% (mmpork, deathsml) | same | 0.5% |
| the blitter's list (DrawOp: decode and its time; no pixels) | 0.3-0.9% | same | 0.35% |

The Deck and Machine rows are ddpsdoj's. The Machine's sound and blitter figures are the only ones in its top lists.
On the Air, timing off plays other frames (no slowdown), so 64% is a rough bound. Summing `sample`'s model symbols
gives 82%, but that count includes the SH-3's own access path inlined into TimeAccess.

The game program, windowed (Air, Metal, 640 x 480, real time, ddpsdoj from the same boss state, run-ahead off,
`SPELUNK_AUDIO_LOG=1`, 30 s, 20 s of it sampled):

- The port's time a frame (host time): 3.2 ms median, 4.6 p95 with Processor between frames on Light. With Full:
  2.5 ms, 3.6 p95. Headless: 2.24 ms.
- The main thread's busy samples (Full), 2,340: Cv1000::RunFrame 95.9%, Hw::AfterFrame (Gpu::Run: encoding the frame's
  draw lists) 2.6%, Gpu::PresentScreen 0.6%, SDL's events 0.7%. The sound chip sits inside RunFrame: 0.4%. Metal's own
  queue threads did 78 samples over 20 s.
- The main thread idles 85% of the time (Full) to 78% (Light).
- Run-ahead 2 with the stick held: one board frame a shown frame. Its frames ahead are kept while the controls stay the
  same, so frames run again only when the controls change.

So **rendering is already split**: the pixels run on the GPU (the software blitter draws nothing,
`rasterize = false`), and Metal's driver runs on its own threads. Only the draw lists' encoding (Gpu::Run) and the
present are on the emulation's thread. They come after RunFrame, in Hw::AfterFrame and presentPicture.

### What real time means for a saving

A frame's work counts only when it is over the board's 16.7 ms frame. Otherwise the program waits for the next frame
anyway: on the Machine it idles about 40% of each frame at the boss (#82). Run-ahead off, it takes 6.0 ms a frame
(median, Light), 5.45 (Full).

- **Air:** 2.5 to 3.2 ms of 16.7. No saving shows; it only spends less battery.
- **Machine:** nothing to gain with run-ahead off. Run-ahead 2 with the stick moving is where it shows: a 3-frame
  put-back takes 15.4 to 16.2 ms, near the 16.7, and 260 to 330 frames ran over in each run (#82). A 15% cut would
  bring the put-back to about 13-14 ms. Akai Katana at run-ahead 1 is late often (the Run-ahead row's own text).
- **Deck:** the one near the limit. Akai Katana's stage 1 takes 12.6 ms a frame headless. Real time adds to that: on
  the Machine, real time was 1.4 to 1.6 times headless (6.0 or 5.45 ms against 3.9). If the Deck's ratio is alike,
  akatana runs over at run-ahead off, and the others have no room for run-ahead 1. That is a guess.
  **To settle it:** a real-time profile on the Deck (below, "What would sharpen it").

## 1. A second core

What could run beside the SH-3 on the CV1000, and what each one would need to stay exact. The rule for every case:
nothing that feeds back into the emulated state may depend on when a thread ran. The emulation thread may only see
another thread's result at a fixed point of the emulation (a frame's end, a given cycle), never "when it is ready".

### The draw lists' encoding (Gpu::Run) on a render thread

- **Feedback: none.** The CPU cannot read VRAM (Cv1000::Read has no path to it). The blitter's busy time and IRQ1
  come from the list's contents, worked out on the emulation thread when the list starts (Blitter::Write). The GPU
  only draws. So the state hash cannot change. The pictures can, if the order of GPU work changes.
- **Ordering:** every GPU user goes through one queue, in the order the emulation thread issues them now: the frame's
  lists; run-ahead's journal (BeginAhead, AheadFrame, Rollback); online play's NetKeep and NetBack; the input-lag
  readout's looks (TradeVram); the training tools' VRAM reads and writes; window shots; scene export; check-video;
  GetVram. A read (ReadScreen, ReadAreas, GetVram) waits for the queue to empty. NativeList is double-buffered.
  point.Note, timeline.Note and KeepDraws keep reading the main thread's copy.
- **The present stays on the main thread.** SDL's swapchain acquire belongs to the window's thread. The present of
  frame N must follow the render thread's submit of N. If the main thread waited for that submit, the work would not
  overlap. If it presented later, that adds a frame of lag, and docs/LAG.md's whole point is not to. The way out: hand
  the steps of the frame's list to the render thread as the blitter starts them, during RunFrame. The encoding then
  overlaps the rest of the frame's emulation, and the present waits only for the last list.
- **Checks:** `--picture-log` hashes equal to main's; check_video; the state and VRAM hashes at the end of a run
  (`SPELUNK_REPORT_STATE`) at run-ahead 0 and 2 with the stick moving; tools/check_tools.sh; tools/check_online.sh. A
  planted fault: one queue item run out of order must change the picture log.
- **Effort:** 2 to 3 days. The risky part is the tools' VRAM users. Each is a path where a read or a journal could
  pass a draw still queued. They give wrong pictures, not wrong state, so the picture hashes catch them.
- **Payoff:** at best the encoding's share. Air: 2.6% of the main thread's work, measured. Deck (Vulkan through
  SDL_GPU, RADV): 3 to 6%, a guess. **To settle it:** the Deck profile below.

### The sound chip (YMZ770) on its own thread

- **Feedback: none.** The CPU only writes to it (no read at 0x10400000) and it raises no interrupt. A queue of
  (cycle, register, value) writes and the frame's end cycle, run by another thread, makes the same samples.
- **Sync points:** the queue drained before Serialize, KeepNow and PutBack (rollback, run-ahead's exact moments, the
  timeline), Trade and SetMoment, and before the frame's samples go to SDL. `held` (run-ahead's frames) is a queue
  item too.
- **Checks:** `--sound-log` byte for byte equal to main's; the state hashes; rollback-test.
- **Effort:** 1 day. **Payoff:** at most 0.5% (Air 0.5%; Machine 0.4-0.65% where it shows at all). **Not worth it.**

### The blitter's list (decode and time)

The time a list takes is needed at its start: it sets m_blitterDone, IRQ1 and the cycles its uploads take from the CPU.
To defer it, the CPU would have to stall at its next poll of the status, and it polls at once. It is 0.35 to 0.9% of a
frame. **Not worth it.**

### The timing model on a second core

The model is three quarters of a Deck frame, so this is the only part of idea 1 with a large ceiling.

- **How it would work:** thread A runs the translated code with the plain cycle count and puts each event (the
  instruction, the address, the size, read or write, the slot flag) in a ring buffer. Thread B runs the model on them
  and keeps the exact count.
- **Feedback:** A must stop and wait for B wherever the exact cycle decides anything. That is a block's stop
  (`until`, `eventAt`), an interrupt due, and a device read (TCNT, the blitter's status, the NAND, the EEPROM's line).
  It is also a device write: the blitter's start, a sound write's RunTo, DMA. And the poll-loop skip and WaitAhead,
  which read the model's state. A can run ahead between those points only while an upper bound on the pending extra
  cycles keeps it short of the next one. There is a second ordering problem: the model reads code from main RAM where
  a path is new (StreamNew). A writes ahead of B, so B would read tomorrow's RAM. Writes into watched blocks would
  have to be logged or make A wait.
- **Determinism:** exact by construction if A never reads an unconfirmed count. A missed point is the worst kind of
  bug: a result that depends on thread timing, which shows up as online desyncs. The guard would be the exact count
  as a separate type, with stall-only access. The shadow and hash gates would also run with random delays put into B.
- **Ceiling:** the frame becomes the model's part alone. Deck and Machine: -25%. Air: -36%.
- **Realistic:** the critical path is B, and the queue adds to it. About 140,000 events a frame (248 M over 1,800
  ddpsdoj frames) at a few ns each to read is ~0.4 ms of a 7.9 ms Deck frame. A stall at every device access gives
  up the overlap there. A guess: -5 to -15% on the Deck, possibly a loss.
  **To settle it:** a count of the decision points a frame (any machine: the counts are the emulation's), and a
  two-thread handoff benchmark on the Deck.
- **Effort:** 2 to 3 weeks. The cycle accounting of the SH-3, the translated code, the poll skip, DMA, the timers
  and the devices all assume the count is exact as they run. High risk. **Not worth it before idea 2**: if idea 2
  shrinks the model, this ceiling shrinks with it.

### Cave's 68000 board and the PGM

- **Feedback everywhere.** On the Cave 68000 board, the YMZ280B's voice ends interrupt the 68000 at the chip's own
  tick (Ymz280b::NextIrq; the CPU is run exactly to it). On Mazinger Z and Air Gallet the 68000 reads the Z80 side's
  answers. On the PGM, the 68000 reads the Z80's latches, and on DoDonPachi II it reads the ARM7's latch and shared
  RAM. Each is brought up to the 68000's time at fixed steps and at each write, in lockstep. Running one on another
  core would need the same stalls at every step and every read.
- **Little to gain.** These boards run far faster than real time already: the PGM headless at ~1,600 frames a second
  (docs/SLOWDOWN.md), and the Run-ahead row says they take any run-ahead.
- **Not worth it.**

### Idea 1 in short

| part | effort | best case (Deck / Machine) | realistic (Deck / Machine) | verdict |
|---|---|---|---|---|
| draw lists' encoding on a render thread | 2-3 days | Air 2.6% measured; Deck/Machine 3-6% guess | 2-4% guess; nothing at run-ahead 0 on the Machine | only if the Deck profile shows over 5% |
| sound chip | 1 day | 0.5% / 0.5% | same | no |
| blitter's decode and time | - | 0.9% / 0.9% | ~0 (it feeds back at once) | no |
| timing model pipelined | 2-3 weeks | -25% / -25% | -5 to -15% guess, or a loss | no, not first |
| 68000 / PGM sound and ARM | - | small | - | no |

## 2. The timing model less often

Every event (a data access, a branch taken) costs the model ~70 Air cycles. Most of a busy frame repeats the same
blocks with the cache warm, and the model works out the same answer each time. The question is whether that answer
can be kept and reused, exactly.

### What one event's cost depends on

The answer of Sh3Timing::Access or Taken depends on:

- the path since the last event: Sh3Paths::Find, keyed by (`from`, the instruction, `lastLoad`);
- each code line's place in the emulated cache (hit or fill, the way and the use order);
- the port's recent cycles (`portMask` against now);
- `nextDecode`, `mmEnd`, `sdramFree`, `postedDone` and `otherFree`, each against now;
- the data address's line (hit, fill, write-back, posted or uncached);
- for a device, BCR2 and WCR2.

Everything but the data address is either fixed by the code or a time relative to now.

At a translated block's entry there is little state: the block begins at a taken branch's target, and Taken has just
set `lastInsn = 0`, `nextDecode` to the branch's end and `lastLoad` to -1 or PR. So the state at entry is small.

### A whole-block summary

- **What is kept:** a summary for each translated block and entry state, kept the first time the block runs that
  way, with the cache warm. It holds each event's extra cycles in order, the model's state after each one (as offsets
  from the block's start) and the order of the cache lines touched. The key:
  - the block's address;
  - `lastLoad`;
  - `nextDecode - now`;
  - `mmEnd - now`, `sdramFree - now`, `postedDone - now` and `otherFree - now`, each clamped at 0, since a time in the
    past is not read;
  - the port's last few cycles.
- **When it can be used:** every code line of the block is held (checked once, at entry), and every data access in it
  hits a line that is held, cached and not write-through. Then the block's timeline is a pure function of the key.
  Each event checks that its instruction is the one expected, looks up its data line (the check the model makes
  anyway), notes a write (`dirty`, `Sh3Paths::Written`), stores the state fields from the summary, and returns the
  summary's extra. Find, Walk, Run, FetchLine and the port's and multiplier's steps are not run.
- **Bailing out:** a miss, an uncached or device access, an event not the expected one, or the block stopping early
  (an exception, DMA) makes the model take over at that event. It first applies the use-order updates still pending,
  in order, so the cache is as the model would have it.
- **When it goes stale:** the summary depends on the code, as a kept path does. It is dropped by the same means: the
  128-byte block marks of Sh3Paths, Forget on a state load, PutBack's clock and generation. The cache contents and
  the times are in the key and the check, not in the summary.
- **Where it applies:** native mode only, like the poll-loop skip. The interpreter's blocks mode always runs the
  model, so `TIMING=1 tools/check.sh` compares the two for free.
- **Skipping the model where it cannot change the result:** this summary is that idea. No event can be skipped
  outright, since every extra cycle moves the frame's end. The poll loops are already worked out (PollTurn). Batching
  a block's events into one call saves only the call, and the translated code would have to flush before every
  device access. That is worth doing only as part of the summary, if at all.

### How it differs from Option A and Option B

- **Option B** (plans per translated site, shelved 2026-11-27) put a check and an out-of-line call at each site of the
  translated code, planless blocks included. On the Machine (mmpork, run-ahead 2 walk): instructions +9.4%, cycles
  +1.3%, L1d misses -36%. It removed the lookup and added more instructions than it saved. c1 (#97) has since taken
  much of the lookup's latency away, so a per-site plan would now save even less.
- **Option A** (plans compiled into the translated code, A-design.md) removes the lookup and the plan's
  interpretation, but costs code size: 60-150 bytes a site, thousands of sites, a build tool and regenerated
  translations.
- **The summary** changes no translated code. Everything stays inside TimeAccess and TimeBranch: no site checks, no
  code growth, no build step. It costs one lookup a block (about 4 events a block: 60.8 M blocks and 248 M events over
  1,800 ddpsdoj frames) instead of one an event. On a hit it skips more than A would: A leaves FetchLine and the port's
  steps dynamic, and the summary removes them too. Its own cost per served event is an instruction compare, the data
  line's lookup and touch, and a few stores.
- **Whether it avoids B's problem:** it avoids B's cause (code at every site, paid in blocks that gain nothing). It
  can still fall into the same trap if its served path is not much cheaper than the Walk + Run it replaces, or if the
  summaries' table adds misses on the Deck. The Deck is memory-bound: IPC 2.3 against the Machine's 3.3, and 1.5
  times the Machine's L2 misses. To keep failed lookups cheap, a block that bails often should stop being tried (a bit
  a block). The first prototype must be timed on the Deck (instructions, L2 misses), under the same 5% stop rule as
  the shelved candidates.

### Payoff

The parts a served event skips (split builds, ddpsdoj, before c1): Find 20 + Walk 14 + Run 12 + FetchLine 3.7 +
Stream 2.6 + Taken 2.2 = 55% of a Deck frame. On the Machine: 15 + 17 + 15 + 3.9 + 2.8 + 2.3 = 56%. Split builds
overstate these parts (their calls are out of line), and c1 has since taken about 5-7 points of Find.

- **Ceiling:** about 45% of a Deck or Machine frame. That would need every event served at no cost, which cannot
  happen.
- **Realistic:** h x s x 48% - g, where h is the share of events in served blocks, s the share of a served event's
  model cost saved, and g the cost of the table and of failed tries:
  - h: 0.6-0.8, a **guess**;
  - s: 0.5-0.6, a **guess** (the cursor, the touches and the stores stay);
  - g: 2-4%.

  That gives **-10 to -20% of a Deck frame, and the same on the Machine**, around -15%.
  - **To settle h:** a counting build. For each block run in 1,800 boss frames, record whether all its events hit,
    its entry key, and the keys per block (the table's size). Headless, on any machine: the counts are the
    emulation's, not the host's. Half a day.
  - **To settle s and g:** only a prototype, timed on the Deck.
- **In real time:** at -15%, akatana's stage 1 on the Deck goes from 12.6 to ~10.7 ms headless. The Machine's
  run-ahead 2 put-back goes from 15.4-16.2 to ~13-14 ms. Neither shows on the Air.

### Proving it exact

- The shadow build (tools/shadow/) compares every event's cycles and every state field with the reference model,
  summaries included. Quick tier on all CV1000 sets; full tier's two halves before merge.
- Hash logs per frame equal to main's: the boss (3,000 frames) and power-on (8,000), `--timing`, ddpsdoj, mmpork,
  akatana and deathsml.
- `TIMING=1 tools/check.sh` (translated with summaries against interpreted without).
- tools/check_tools.sh (rewind, practice point, run-ahead end in the same state). tools/check_online.sh and
  `--rollback-test` (PutBack's clock and generation drop summaries as they drop paths).
- A line `summaries: N events served, M by the model, B bail-outs`, so that a pass is not vacuous.
- `SPELUNK_NO_SUMMARY=1`: the model alone in the same binary, for A/B runs and equal hashes.
- A planted fault: `SPELUNK_PLANT_SUMMARY=1` adds a cycle to one summary's extra. The shadow must stop at its first
  use, and the hash logs must differ.
- Akai Katana's stage 1: its word among its instructions (0C1ED388, written 910 times a frame) must leave block
  0C1ED370's summary in use, as it leaves its paths.

### Effort

5 to 7 days:
- the counting build, as a go or no-go on h: 0.5 day;
- the summary's keep and served path: 2 to 3 days;
- bailing out with the use order exact, and the shadow's hooks: 1 to 1.5 days;
- the gates: 1 day;
- Deck and Machine A/B rounds: 0.5 to 1 day.

The risky parts:
- a key that misses an input the model reads (a wrong cycle; the shadow finds it at once);
- the pending use-order updates at a bail-out;
- the poll-loop skip and c1's successor, which both read the state a summary writes;
- the table's footprint on the Deck's 512 KB L2.

## Recommendation

**Build idea 2 first, in two stages:**

1. The counting build. If h comes out under ~0.5, stop: the summary cannot clear the stop rule.
2. Otherwise, the prototype, timed on the Deck before it is finished.

It is the only idea with a likely payoff of over 10% on the Deck that changes no translated code. Its exactness
rests on the shadow, which already compares every event.

**Leave idea 1 alone for now.** The parts that are easy to make exact (sound, the render thread) are worth 0.5% and
2.6% (Air) of a frame. Rendering's pixels are already on the GPU. The part with a real ceiling (the model on a second
core, -25%) is weeks of work, at a risk to online play's determinism. It may lose after the queue's cost, and its
ceiling falls if idea 2 lands. Move the draw lists' encoding to a render thread only if the Deck profile below shows
it over 5% of a frame.

## What would sharpen it

- **A Deck profile of the game program in real time** (not headless): the game program from bebdc7b built in the
  spelunkomp-build-x86_64 image with symbols. 60 s at ddpsdoj's and akatana's boss, run-ahead 0 and 1, Light and Full,
  under `perf record -g`, with the host-time lines. It settles the encoding's share on Vulkan and the Deck's ratio of
  real time to headless (so whether akatana is over 16.7 ms there), and with it the real-time figures above. The
  package would be like profile-split's awake-*.tar.gz and run-awake.sh, with the game program in place of the
  headless runner.
- **h, the decision points a frame, the keys per block:** counting builds, headless, on any machine. No device needed.
- **The Machine** adds nothing the Deck would not, for either idea.
