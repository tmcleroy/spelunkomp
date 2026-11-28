# Brief: how DoDonPachi's bosses build their bullet patterns

Goal: describe every boss attack in DoDonPachi as the *rules* its code follows (not recorded bullets), in one shared
vocabulary, so that a web editor can show, change and make patterns, and Dodge training can fire them.

## Where things are (worktree ~/code/recomp/spelunkomp-dodgeboss)

- Disassembly of the whole program: `work/ddonpach/program.lst` (lines look like ` 0326E6  jsr $50F94`; `>` marks a
  branch target). The program bytes: `work/ddonpach/program.bin` (68000, big-endian; read tables with python struct).
- Facts already known: `games/ddonpach/src/ddonpach.h`, `docs/DODGE.md`, `tools/dodge/extract.py` (the aim routines are
  re-implemented there in class Aim).
- Volley dumps from recordings (one line per volley = what one task fired in one frame; `atk` = byte +0x26 of the
  task record; `rel` = each bullet's angle less the first's, of 256; `aimC/aimF` = first angle less the game's coarse/fine
  aim at the ship): make one with
  `tools/dodge/volleys.py work/ddonpach/program.bin OUT-FOLDER/r1.txt L-S` (OUT-FOLDER: tools/dodge/record.sh's)
  (L loop 0/1, S stage 0..5, 6 = hidden last boss; r1..r4 are four runs).
  Use them to check what you read in the code, not instead of reading it.

## What is known about the calling convention

- Bullet makers (what objects call): `jsr $50DDC` (angle of 64 in d1) and `jsr $50F94` (angle of 256 in d1). In the
  second loop (word 10197A != 0) they become 2 bullets 8/256 apart (`50D68`/`50F24`) or 3 depending on object flags
  (`+0xD(a5)` bits 0x81, `(+6(a5))` bit 1); other entries: 50DA0/50F58 (loop 2: speed +4 only), 50DBE/50F76,
  50DCE/50F86. Plain single-bullet: 50DE4 / 50F9C.
- Registers: d0 = speed in the high word (signed) | bullet kind in the low byte (0..63, table 5104E); d1 = angle;
  d2 = place (x high word, y low word, 64ths of a pixel); d3 = parent object (0 none) with d4 offset from it.
- Aims at the ship: 59A36 / 59A58 / 59A5E / 59A64 (coarse, 64 directions) and 59C58 / 59C66 (fine, 256), result in
  d1; carry set = no ship (skip). From a point in d0/d1 or the object's place (+6(a6)).
- Random: `jsr $57C84` returns a random number in d0 (check).
- Each boss part is a task record at a5 (0x40 bytes, from 100002) with its body at a6 (0x20-byte records from 104AF6,
  parts at a6+0x20, +0x40...). Seen in stage 1's boss (0326xx): `+0x26(a5)` the attack chosen, `+0x10(a6)` countdown to
  the next volley, `+0x16(a6)` volley counter, `+0x1A/+0x1C(a6)` turns that change each volley, table 32CCA a wobble,
  word 10E73E a flag that makes it harder.

## What to write

For your boss, `docs/plans/dodge-boss/<name>.md`:

1. **Phases**: what chooses the attack (which byte, set where; by time, by the boss's life (where is life kept?), by
   parts destroyed), the order, and how long each lasts. Name each attack by what it looks like (e.g. "aimed 5-way
   fans", "rotating 13-bullet spiral").
2. **Each attack in the vocabulary below**, with the addresses that prove each number. Every number in the attack
   (counts, steps, speeds, kinds, intervals, random ranges, tables) from the code; say what differs in loop 2 and with
   the harder flag(s).
3. Anything that does not fit the vocabulary: say exactly what it is (that is the most useful finding).

Vocabulary (a proposal; extend it where the code needs more):

```
attack:   emitters (each: offset from the boss's place, or a part that moves: say how), timeline
timeline: volleys every N frames (or a table of gaps), K times, then a pause; may run several emitters together
volley:   base angle = fixed A | aim (coarse|fine, from where, now or as the attack began) | previous base + step
          (sweep: step, and whether it bounces between limits) ; + random turn (range, granularity) ; + table wobble
          shape = list of bullets (angle offset, speed, kind, place offset), or generators:
                  fan(n, spacing) | ring(n) | line(n, speed from..to) | ...
bullet:   kind (0..63), speed, angle of 64 or of 256, maker entry (so loop 2's extras follow)
```

Do not edit anything outside `docs/plans/dodge-boss/`. Do not run the game program with a window; headless runs
(build/ddonpach/spelunk-headless) only if needed, and only one at a time. Report: the file you wrote and a one-paragraph
summary of what fits and what does not.
