# DoDonPachi stage 2 boss: the rules behind its bullets

Read from `work/ddonpach/program.lst` (the boss is the code at 033F46 init, 0342A8-035718 per frame, helpers 03571E-035EEC),
and checked against the volley dumps of loop-stage `0-1` and `1-1` (runs r1..r4; `volleys.py`). Every address below is in
that listing. Task kind 127 is the boss (task 100182 in the recordings), kind 139 is its "ball" (a child task, 035A36 init /
035AAE per frame). In the recordings the boss body array starts at a4 = 0x1053F6 (part n is at a4 + 0x20 n); the task is a5.

Coordinates: a place is (x = word +6, y = word +8) in 64ths of a pixel, as in the brief (x is the long axis, 0..0x5000; y the
short one, 0..0x3C00). Angles are all of 256 (every shot here goes through `50F94`/`50EB4`/`50F58`, the fine makers).

## 0. Things that apply to every attack

- **Speed**: the d0 high word is *added to the kind's own speed* (`50E80`-`50E84`: `move.w (a1)+,d7; add.w (a7),d7`). Own speed
  (word at +0xE of the kind record, table 05104E, loop 2's copy 054FA8 is identical): 22 for kinds 4, 7, 17, 27, 33, 38; 18 for
  kind 39. So the "speeds" below (-13, -4, +8 ...) are offsets from 22 (or 18); more negative = slower. (An aimed -13 shot
  moves at 9, a +8 shot at 30.)
- **Kinds seen**: 4, 7, 17, 27, 33, 38, 39 (graphics/size; kind 33/39 alternate in the layered fans).
- **Random numbers** are three tables read through one shared counter byte `$101273` (every call, by any object, advances it):
  `57C84` -> signed byte -7..7 (table 057CA8; 0 twice as likely: 32/256, the rest 16/256), `57DA8` -> 0..255 (table 057DBA, a
  permutation), `57FA2` -> 0, 1 or 2 (table 057FB6). Model them as uniform; the real sequence depends on how many calls every
  other object made.
- **Pause**: nothing fires while `$101764` or `$1017BE` is non-zero (bomb out / stage over); every site below tests them.
- **`atk` (task +0x26) is NOT this boss's attack selector.** The dumps show atk 0 / 255; that word is the sign of the hull's
  sideways move (`03485C`, `03488E`). The selectors are `0x32(a5)` (the state, below), `0x19C(a4)` (which big volley of four)
  and `0x1F0(a4)` (build-up on/off).
- **Loop 2** (word `$10197A` != 0) does not change any code of this boss; it changes what the makers do:
  - `50F94` (the "fine, 256" maker, used by every shot except the head's) fires **2 bullets at the same angle, speeds v and v+6**
    (`50EE8`-`50EF4`; 3 bullets v, v+5, v+10 if bit 1 of the body flag byte is set, `50F00`; and angle -8/+8 pair instead if
    `0xD(a5)&0x81` is 0 and body bit 1 set, `50F40` - none of these seen). In the dumps this is every "n" doubling:
    22 -> 44 (speeds 8 and 14), 16 -> 32 (27: -9/-3 ...).
  - `50EB4` (head bullet 1): if DIP word `$10176C` (= `$1017A8` & 0x30, set at 000B86; non-zero in all recordings) is non-zero the
    pair (v, v+6) as above, else one bullet at v+4 (`50EC6`). `50F58` (head bullets 2, 3): one bullet at **v+4** (`50F64`).
    Head volley in loop 2 = speeds -13, -7 (same angle), -4, 0: **4 bullets**, as dumped.
- **Harder flags** (all inside this boss):
  1. **Late** = timer `0x1A(a5)` < 0x189C. The timer starts at 0x2A30 (`033F66`; 10800 frames) and counts down each unpaused frame
     (`034536`), so late = after 4500 frames. It is also forced to 0x189B when the boss's life drops below 0x4AAA
     (`034762`-`03476A`). Effects: head shots +1 speed (`034B96`-`034B9E`, d4 = 0x10000: -13 -> -12 in the dumps); head
     sweep period `0xDE(a4)` forced to 1 (`03454E`, i.e. a volley every 2 frames; the dumps show gaps 2-4, the longer ones are
     volleys the game could not make for lack of free bullet records); and, with the next condition, flag 2.
  2. **Hard flag** = word `$10E738` = 12 (`03455C`), set once when late **and** the word at `0xA(a4)` >= 0x102 (0xA = number of
     side pods 10/11 destroyed, high byte; 0xB = number of big turrets 24/25 destroyed, low byte; so: both pods, or one pod and
     both turrets). `$10E738` is cleared at init (`033F7E`). Effects: layered fans A, B, C, D below get **+12 speed**
     (`add.l $10E738,d0` at 035048, 035096, 0350DC, 035140, 0351B6 and the `tst.l d7` ones at 035122, 035164, 03519A, 0351D2) and
     pattern D gains 4 extra bullets (`035114`-`03514E`).
  3. **Life below 0xE000 (half of 0x1C000)**: `0x1F0(a4)` is cleared (`034770`) -> only the full layered fan from then on, and any
     part-death timers `0xB2..0xBA(a4)` are set to 1. **Below 0x4AAA**: also the timer clamp above (`03475C` sets `0x3C(a6)=1` so
     this runs once).
- **Life** is `0x16(a5)` (long), start 0x1C000 (`033F5E`). Each frame the damage is `0x7FFF - min(hp(part 0), hp(part 12))`
  (`0x18(a6)`, `0x198(a6)`; both are reset to 0x7FFF, `0345B2`-`0345CE`). The boss ends when it goes negative (`0345DA`) or the timer
  reaches 0 (`03453A`).

## 1. Phases: what chooses the attack

One word, `0x32(a5)` (init 0, `033FDA`; `0x34(a5)`/`0x35(a5)` init 0xC0 beside it), is the state. The cycle:

```
state 0  "turret phase"  (only while a big turret, part 16 or 17, is alive)
   |  every alive turret finished its ball round (mask test 035638-03567A), or the second big turret dies in state 0 (0357E4)
   v
state 1  "head phase"    (head, part 6, comes out and fires; 034B1A-034C3E)
   |  head has gone back in (0xD2(a4) < 0, 034C52)
   v
state 2  "big volleys"   (4 volleys; 034E80-0351E4; also the anti-camper fan)
   |  after the 4th: if part 16 or 17 is alive -> state 0, else (both gone) -> state 1   (035202-035238)
   v  back to 0 or 1
```

Hull Y wanders on its own all the time (observed y of the volley places 0x0AB0..0x2E20); the hull's drift is done by code I did not
decode (`0347B0` calls `014E3C` with the life; positions of the sub-parts are re-derived from the hull each frame at 03482C-034934).

### State 0 - turret phase (ends in `state 1`)

Entered at the end of a state-2 cycle (`035238`, d1 = 0) with these timers set (`0351E6`-`035238`): `0x34(a5)` = `0x35(a5)` = 0xC0
(`0351EC`), `0x4(a4)` = number of big turrets alive, turret A (code 035272-03546C, part 24, a6 = a4+0x300) `0x1B0=0x120, 0x1B2=0x40`
if part 16 is alive, turret B (03547x-035638, part 25) `0x1B4=0xC0, 0x1B6=0x40` if part 17 is. Initially (`record 13` of the
table at 033FE4): `0x1B0=0x120, 0x1B2=0x3E, 0x1B4=0xC0, 0x1B6=0x3E`.
What happens here (all while `0x4(a4)` != 0 for the first):
- **7-fan**, "random fan of 7": first when `0x34(a5)` borrows (193 frames after entering: 0xC0+1), then every 65 frames
  (`03588E` reloads 0x40). Also at `0x34(a5)` = 0x50 the helper `035E76` spawns 0x8F child tasks (no bullets).
- Each turret counts its own `0x1B0`/`0x1B2` down (1 per frame while `0x1B0` > 0, then `0x1B0` = 1 so 2 frames per `0x1B2` step of
  2). When `0x1B2` = 0x1E the turret does a **ball round** (below) and its aimed shots. A: first round ~0x121 + 2*17 = ~323 frames
  after entering; B: ~0xC1 + 34 = ~227 frames. After the round, `0x1B0` = 0x20, `0x1B2` = 0x1C and it counts out to -2, then sets its
  done byte (`0x1BC` / `0x1BD`) (`03545A`, `035626`). The state moves on to 1 when all alive turrets are done (`035670`, which also
  reloads `0x34(a5)` = 0xC0).
- **In the dumps**: r1 state 0 after the 1st big cycle: 7-fans at 19702, 19767, 19832 (65 apart, the first 193 after the last big
  volley at 19509), balls bursting 19856-19904, head phase from 20038. At the start of the fight, balls burst 18030-18174 (26 = both
  turrets).

### State 1 - head phase (ends in `state 2`)

Head = part 6 (a6 = a4 + 0xC0, record 6 of 033FE4). Entering (`034B2E`): `0xC1(a4)` bit 0 set, `0xD0(a4)` = 0x60, `0xD2(a4)` = 0x46.
The head slides in along table `0359D8` (word per step; `0xD2` is the byte offset; added to the head's +6 each frame, `034C60`-`034C6C`)
stepping `0xD2` -= 2 every `0xD1+1` = 2 frames (record: `0xD1` = 1) from 0x46 to **0x22** and holds there (x offset 960/64 = 15 px).
At 0x22 the head fires; its pacing counter is `0xD4(a4)` (init 0x20, `0xD5` also 0x20).
Per volley (the counter borrow, `034B56`-`034B5E`): `0xD4` = 0x18 (so 25 frames to the next), `5(a5)` bit 0 flips (sprite only).
Then, with `0xD6(a4)` = number of straight volleys left (init 10 = `0xDC`), `0xD7` = sweep cycles left (init 1 = `0xDD`),
`0xD8` = sweep step (starts 0x12), `0xDE(a4)` = sweep period (init 8):
1. while `0xD6` > 0: a **straight** volley, `0xD6`-- (`034B8A`-`034B94`), next one in 25 frames. 10 straight volleys = 250 frames.
2. then **sweep** volleys: `0xD4` = `0xDE(a4)` (period `0xDE`+1 = 9 frames at the start), `0xD8` counts 18..0 (19 volleys); when
   it wraps `0xD7`-- and when `0xD7` reaches 0 (`034BCC`-`034BE2`): `0xD0` = 0x20, `0xD2` -= 2, `0xD4` = `0xD5`, `0xD6`/`0xD7` =
   word `0xDC` (reload for the next time) and the head moves back out (`034C40`-`034C58`: one step every 2 frames until `0xD2` < 0
   -> state 2, `0xC1` bit 0 cleared). About 66 frames.
   Data: r1 first head phase = 10 x 25 then 18 x 9 (29 volleys, 18333..18720); second = 10 x 25 then 18 x 7.
Part deaths change this (`0357AA`-`0357F8`, run when a part with d7 = 0x3000 or 0x5000 dies, i.e. pods 10/11 and turrets 24/25;
parts with d7 = 0x2000 (26/27) do not count): each such death does `0xDE`-- (min 1), `0xDC` -= 2, `0xDD` += 2; and the
**second** turret death (when `0xB(a4)` is already 1) additionally `0xDC`--, `0xDD`++. So after all four: straight volleys
10-8-1 = 1, sweep cycles 1+8+1 = 10, period 4+1. (r1 head phases: 9, 7, 6, then 2-4: consistent.)
`0x1A(a5)`-late (above) sets `0xDE` = 1 and `0xD6 := 0xDC` (`03456A`... `034564`) once.

### State 2 - big volleys

Runs only in state 2 (`034E80`), not paused. Two things:
- the **anti-camper fan** every 32 frames (`10126E` & 0x1F = 0), if the ship is low (below);
- the **big volley** every `0x34(a5)`+1 frames, four times in a row (`0x19C(a4)`: 0xC, 8, 4, 0; decremented by 4 at `0351E0`; a borrow
  there ends the cycle: `0x19C` = 0xC, `0x34(a5)` = `0x35(a5)` = 0xC0, `0x1F0`-- if > 0, then state 0 or 1).
  `0x34(a5)` is reloaded **0xB0 (177 frames) while `0x1F0(a4)` != 0, else 0xC0 (193 frames)** (`034F0C`-`034F1A`). `0x1F0` starts
  at 1 (record 15, word `0x1F0`) so the **first** cycle of the fight is 177 apart and **builds up** (volley 1 = D, 2 = C+D, 3 =
  B+C+D, 4 = A+B+C+D, from the jump on `0x19C` at `03502E`); after that (`0x1F0` = 0, or life < half) **every volley is
  the full A+B+C+D** (`035028 beq 035042`), 193 apart. The first volley of a cycle comes 193 frames after state 2 starts (head
  retract included: r1 18720 + ~64 + 193 = 18977).
  Dumps: r1 18977/19154/19331/19509 (n 19, 37, 53, 69: 177, 177, 178 apart) and 20693/20887/21084/21278 (n 70, 194, 197, 194).

The fight is: state 0 (while turrets live) -> 1 -> 2 -> 0/1 -> ... until the life is gone or the 10800-frame timer ends.

## 2. The attacks

Notation: `aim` = the game's **fine aim** `59C58` from a stated point to the ship, evaluated *now* (no ship: angle 0x80); `rand7` =
`57C84` (-7..7); `fan` angles are bytes (mod 256). "vol(…)" = a volley fired in one frame.

### 2.1 Head 3-way (state 1) - kind 38

Emitters (all in one frame; head part 6; angle base from P = (6(a6)+0x1200, 8(a6)), 72 px "ahead" of the head along x):

| bullet | maker | d0 (speed arg, kind) | angle | place |
|---|---|---|---|---|
| 1 | 50EB4 at 034BF6 | -13, 38 (+1 if late) | straight volley: aim. Sweep volley: aim + 4*rand7 (`034BBC`-`034BBE`) | P |
| 2 | 50F58 at 034C18 | -8, 38 | aim - s + (rand7 >> 1) (`034BFC`-`034C08`) | P + (-0x300, -0x480) |
| 3 | 50F58 at 034C38 | -4, 38 | aim + s + (rand7 >> 1) (`034C1E`-`034C2A`) | P + (-0x300, +0x480) |

(Place note: bullet 2's d2 = P - 0x3000480 i.e. x-0x300, y-0x480 (`034C12`); bullet 3 adds 0x900 to y only (`034C34`), so y+0x480.
Net: bullet 1 at P, bullets 2 and 3 at x-12 px, y -18 px / +18 px.) Here `rand7 >> 1` is an arithmetic shift: -4..3.

`s` is 0 in a straight volley and, in a sweep volley, `s` = signed byte table `035A20[0xD8]`:
`-8 -16 -24 -32 -40 -32 -24 -16 -8 0 8 16 24 32 40 32 24 16 8 0` (index 18 down to 0 is used, so `s` runs
8, 16, 24, 32, 40, 32, 24, 16, 8, 0, -8, -16, ..., -40, ..., -8): the two flank bullets open like scissors to +-40/256, close,
cross, open the other way and close.
Timeline: see state 1 (10 straight @25 frames, then 19 sweeps @ `0xDE`+1, times `0xD7`). Loop 2: bullet 1 pair (v, v+6) same angle,
bullets 2 and 3 become v+4 each: speeds -13, -7, -4, 0 (dumped).
Dump check: straight volleys have aimF = 0 exactly for bullet 1 and rel [0, -4..3, -4..3]; sweep volleys have aimF within +-28.

### 2.2 Aimed single (turret ball rounds, state 0) - kind 4

Turret A (`0353xx`) / B (`035544`...) when its `0x1B2` = 0x1E (the ball round) and **the ship is far** (below), every 4 frames:
- condition: `10126E` & 3 = 0 (`035376`), and `358D0` >= **0x29C0** (`035384`). `358D0` = the larger of `$102C92` / `$102CD2`
  (players' x, 64ths, if their live word `$102C8C` / `$102CCC` is non-negative; 0 if none): "the ship is in the lower part of the
  screen, x >= 167 px".
- shot: d0 = 4 (speed arg 0, kind 4), maker `50F94`, from P = (6(a6) + 0x80, 8(a6) + 0x1680) for A (`03539A`-`0353A6`) and
  (6(a6) + 0x80, 8(a6) - 0x1680) for B (`035560`); angle = aim from P + (`57FA2` - 1) = aim + {-1, 0, +1} (`0353AE`-`0353B6`). (a5 +4 is
  set to 1 for A and 0 for B around the call, `03538E`, `035552`; it only affects the maker's loop-2 branch and sprite side.)
- dumps: r4 26790..26910, every 4 frames, place stepping y by 0x80 (the turret travels, below), aimF 0 +-1. Loop 2: pair (4,0),(4,6).

### 2.3 Ball (kind 139) and its burst - turrets A and B

Spawned by the turret during its round (`0353CA`-`035440` for A, `03558C`-`035608` for B), one every `0x1B8(a4)`+1 = **4 frames**
(`0353D2`: 0x1B8 := 3; the first of a round after `0x1B9`+1 frames, 0x20 initially, `0x1B9` then drops by 2 per round, min 0),
13 balls per round (`0x1D0(a4)` 12..0, reloaded 0xC at `035430`), then the turret pauses (`0x1B0` = 0x20) and goes on.
Each spawn is a task (`581A8`, d3 = 0x8B000000, d0 = 7) whose parameters are: place = turret (6(a6), 8(a6)) plus the offset row
`035972[i]` (long: x, y) = (0x0140, 0x1500), (0x0140, 0x1680), (0x0140, 0x1800), (0, 0x1800), (0, 0x1680), (0, 0x1500), `i` = `0x1D4(a4)`
= 10, 8, ..., 0 cycling (`03540E`) (for B: y negated, `0355B8`), heading/timer word `03598A[i]` = 0x30F8, 0x30FC, 0x3000, 0x30EC,
0x30F0, 0x30F4 (byte +0x1A = 0x30 = 48, byte +0x1B = heading -8, -4, 0, -20, -16, -12; B negates the heading), `0x1D0` value and a
side word.
The ball (init `035A36`, per frame `035AAE`) flies, then homes: `0x1A(a6)` (starts 0x30) drops 2 per frame (`035AEA`) to 0
(`035AEE` -> `035B42`: `0x1C(a6)` = 0x40, `0x1A(a6)` = 5); while it is 5 it re-aims at the ship every frame (`035ACA`-`035AD8`,
`59C38` sets heading `0x1B(a6)`) for 0x40 frames, then `0x1A` goes 3, 1, -1 (3 more frames) and it **bursts** (`035AFA`-`035B3C`).
So **~24 + 64 + 3 = ~91 frames after spawn**, the burst is where the ball has flown to (near the ship's side; e.g. place x 0x4B57..0x507F in r1).
- **Burst**: 16 bullets from the ball's place (`0x6(a6)`), kind **27**, **one shared speed** per burst = `rand7 - 8`
  (`035AFA`-`035B08`: `ext.w; addi.w #-8`; -15..-1), angle *i* = `T[(c + 1 + i) & 255]` where T is the permutation `057DBA` and `c` the shared
  counter byte `$101273` read at that moment (advanced by 16, `035B32`), maker `50F94` (`035B28`). Model: 16 angles "random from the
  table in sequence", speed uniform -15..-1. Dumps: n 16, `(27, -15..-1)`, 4 frames apart (bursts keep the spawn order), first
  burst r1 18030; 13 per turret (26 at the start with both).
- Loop 2: 32 bullets, speeds v and v+6 (dumped (27,-9),(27,-3)).
- This **does not fit** the vocabulary: the emitter is a *moving child object* that homes on the ship and bursts later; the burst
  angles come from a global table position, not a per-volley formula. Suggested: model as `delayed(91, at=ball_position)`, with the
  ball path `homing` (see "what does not fit").

### 2.4 Seven-way random fan (state 0, any time `0x4(a4)` != 0) - kind 7

`3585C`: `subq.b #1,0x34(a5)`; at borrow (`035870`) `0x34(a5)` := 0x40 and (`035888`-`0358CC`) 7 bullets, `d5` = 0x80007 stepping
-0x40000 (speeds **8, 4, 0, -4, -8, -12, -16**, kind 7), angle each = 0x80 + 2 * rand7 (a fresh random per bullet, 114..142),
maker `50F94` at `0358BE`, place = long at `0x186(a4)` (part 12's place) + 0x1000000 (x + 4 px) (`03589A`-`0358A4`). It also sets
`0x19A(a4)` = 0x3E, `0x1FA(a4)` = 0x36 (recoil animation). Period 65 frames after the first (`0x34` = 0xC0 when state 0 starts, so the
first is 193 frames in). Also at `0x34(a5)` = 0x50 the helper `035E76` spawns child tasks (`0x8F`, 12 of them; they fire nothing).
Dumps: r1 19702/19767/19832 `n 7 (7,-16)..(7,8)`.

### 2.5 Anti-camper fan (state 2, ship low) - kind 7

Every 32 frames (`10126E` & 0x1F), state 2, not paused, if `358D0` >= **0x2A00** (168 px, `034EAC`), the fan: aim = fine aim from
(0x4900, 8(a6)) [the bottom edge, the hull's y] to the ship (`034EB2`-`034EC6`), 22 bullets at **aim-10 ... aim+11 (step 1)**, d0 =
0x80007 (speed +8 = 30, kind 7), place (0x4900, 8(a6)), maker `50F94` (`034EDC`/`034EE2`: `subi.w #10,d1`, `addq.w #1,d1`, `dbra d7`
with d7 = 21). a6 here is part 20 (a4+0x280). Dumps: r1 18992-19152 (every 32), n 22 (7, 8), place x = 0x4900. Loop 2: 44 bullets, (7,8) and (7,14).
This is the ship-position-conditional emitter; the 7-way and others do not depend on the ship, this one and 2.2 do.

### 2.6 The big volley (state 2) - four layers, fired together

At the borrow of `0x34(a5)` (`034F0C`): `0x34` := 0xB0 if `0x1F0(a4)` != 0 else 0xC0; `0x1A(a6)` := 0x3E, `0x7A(a6)` := 0x36
(recoil). From a6 = part 20, with p = (6(a6), 8(a6)):

1. **10-fan** (`034F36`-`034F5A`): place p + (0x100, 0); 10 bullets, kind 7, speeds **5, 3, 1, -1, -3, -5, -7, -9, -11, -13**
   (d5 = 0x50007 stepping -0x20000), each angle = 0x80 + (rand7 >> 1) (-4..3; a fresh random each bullet; base `d6` = -0x80). Maker
   `50F94` (034F54). Always fired.
2. **Gun pairs**, each only if that part is alive (bit 7 of its `+1` clear), place as the fan, kind 0x11 = 17, no aiming (fixed angles):
   - part 16 (`0x201`, `034F5E`): speed -6 angle 0x76 (118); speed -7 angle 0x6F (111)
   - part 17 (`0x221`, `034F84`): -6 at 0x8A (138); -7 at 0x91 (145)
   - part 10 (`0x141`, `034FAA`): -5 at 0x68 (104); -6 at 0x61 (97)
   - part 11 (`0x161`, `034FD0`): -5 at 0x98 (152); -6 at 0x9F (159)
3. **Aimed layers** (`034FF6`-`035180`): a = fine aim from (6(a6)+0xAC0, 8(a6)) now (`034FFC`-`03500C`; no ship: 0x80); place p + (0xAC0, 0)
   (`03501C`); maker `50F94`. Layer **A** (`035042`, `0x19C`=0 or `0x1F0`=0): kinds alternate 33, 39, 33, 39, ... (`eori.w #6`):
   for i = 0..7: bullets at **a-3-2i** and **a+3+2i**, speed **-1-i** (+12 if hard); A = 16 bullets. Dump (aim 143): 140/146 (33,-1), 138/148 (39,-2) ... 126/160 (39,-8). ✓
   Layer **B** (`035084`): for i = 0..7: **a-30+2i** and **a+30-2i**, speed **-9+i**, kinds 33, 39, ... (first 33). 16 bullets. ✓ (dump 113/173 (33,-9) ... )
   Layer **C** (`0350CA`): for i = 0..7: **a-30-2i** and **a+30+2i**, speed **-1-i**, kinds 33, 39, ... 16 bullets. ✓
   Layer **D** (`035110`): speed/kind/angle offset from a: (39,-10) +-4; (33,-9) +-3; (39,-8) +-2; (33,-7) +-1; (39,-6) 0. 9 bullets. ✓
     If the hard flag: first 4 more bullets (39, speed 0) at a+6, a-6 and (33, speed 0) at a+5, a-5 (`03511C`-`03514E`: d0 = 0xFFF4+12).
   Fall-through: A runs into B, C, D (each ends with `bra` to the next: `03507A`, `0350C0`, `035106`); the jump table `03502E` enters at layer
   A (`0x19C`=0), B (4), C (8), D (12). With `0x19C` starting at 12 and falling by 4: volley 1 of a cycle is D, then C+D, B+C+D, A+B+C+D.
   If `0x1F0(a4)` = 0 (`035024`-`035028`), the jump is skipped and **all four layers (A+B+C+D, 57 bullets) fire every volley**.
   Total per volley (loop 1): 10 + guns (2 each alive) + layers: n 19 ... 70 as dumped (D = 9, so 19; A+B+C+D = 57 + 10 + 2-8 = 69-75).
Loop 2: every bullet doubled as above (v, v+6, same angle); dump n 109-118.

## 3. Timeline summary (frames, loop 1, 60 fps)

| phase | length | what |
|---|---|---|
| state 0, turret | ~230-350 | 7-fan every 65 (first at 193), per alive turret one round of 13 balls (one per 4 frames, burst ~91 after spawn), aimed singles every 4 frames while ship x >= 167 px |
| state 1, head | 250 + 19 * (`0xDE`+1) * `0xD7` + ~66 | 3-way every 25 (10x), then 3-way sweeps every `0xDE`+1 (9 at start, down to 2) |
| state 2, big volleys | 4 volleys, 177 / 193 apart (first of a cycle 193 after the head is in) | 10-fan + guns + layers; anti-camper fan every 32 while ship x >= 168 px |
| overall | timer 10800 frames max | |

## 4. What does not fit the vocabulary (the useful part)

1. **Child objects that burst later** (2.3): the ball is a homing task, then 16 "random from a table" bullets at its place ~91 frames
   later, with speed random per burst. Needs `spawn(object, path=homing, then burst(n=16, kind 27, speed=rand-8, angles=table))`.
2. **Conditional on the ship's position**: 2.2 (x >= 0x29C0) and 2.5 (x >= 0x2A00) only fire when the ship is low on the screen
   (from `$102C92`/`$102CD2` via `358D0`). Needs a `when(ship.x >= N)` guard on an emitter.
3. **Layered fan by fall-through + a counter** (2.6): the same code is "D, then C+D, ..." for the first cycle and always "A+B+C+D" later;
   and the cycle count 4. Model as one attack with a `layers_active = f(cycle, life)` list.
4. **Part-death scaling**: the head's period, straight/sweep counts and the hard flag depend on which of 4 parts are dead (0xDE, 0xDC,
   0xDD, word 0xA, flag `$10E738` = +12 speed). Needs an attack parameter `p(parts_destroyed)`, and a global "hard" speed bonus.
5. **State machine over moving parts**: the emitters are parts that move (head by table `0359D8`, turrets by table `035996`, recoil by
   `035932`); positions used in the shots are offsets from those moving parts (head x+0x1200 etc.), the hull y wanders. The hull path was
   not decoded; take positions from the recordings or decode `014E3C`/`03482C`.
6. **Shared random table**: the three random sources advance one counter used by every object; individual draws are not independent.
7. The 7-fan and 2.4/2.5 turn "per-bullet random angle" (a different random per bullet in a single volley), not a per-volley turn.
8. Loop-2 doubling depends on the DIP word `$10176C` for the head's first bullet only.

## 5. Quick reference of the numbers

| site | what | count | kind | speed arg | angle | place | when |
|---|---|---|---|---|---|---|---|
| 034BF6/034C18/034C38 | head 3-way | 3 (loop 2: 4) | 38 | -13(+1 late), -8, -4 | aim, aim-s+r4, aim+s+r4 (sweep: aim+4*rand7 on #1) | P=(x+0x1200,y) and (-0x300,-0x480/+0x480) | state 1 head hold; 25 frames straight x10 then every 9..2 |
| 0353C0 / 035582 | aimed single | 1 (loop 2: 2) | 4 | 0 | aim +-1 | turret x+0x80, y+-0x1680 | state 0, every 4 frames in the ball round, ship x >= 0x29C0 |
| 035B28 | ball burst | 16 (loop 2: 32) | 27 | rand7-8 | table 057DBA run | ball place | ~91 frames after a ball spawns (4 apart) |
| 0358C4 | 7-fan | 7 (loop 2: 14) | 7 | 8,4,0,-4,-8,-12,-16 | 0x80 + 2*rand7 each | part 12 place + (0x100,0) | state 0, every 65 frames |
| 034EDC | anti-camper fan | 22 (loop 2: 44) | 7 | +8 | aim-10..aim+11 | (0x4900, y) | state 2, every 32 frames, ship x >= 0x2A00 |
| 034F54 | 10-fan | 10 (20) | 7 | 5,3,..,-13 | 0x80 + (rand7>>1) each | p + (0x100,0) | state 2 volley |
| 034F74..034FF0 | gun pairs | 2 each (4) | 17 | -6/-7, -5/-6 | 118/111, 138/145, 104/97, 152/159 | as 10-fan | alive parts 16, 17, 10, 11 |
| 035060.. A | layer A | 16 | 33,39 | -1-i (+12 hard) | aim -+(3+2i) | (x+0xAC0,y) | cycle volley 4 (or always when `0x1F0`=0) |
| 0350AE.. B | layer B | 16 | 33,39 | -9+i | aim-30+2i, aim+30-2i | same | volleys 3-4 |
| 0350F4.. C | layer C | 16 | 33,39 | -1-i | aim-30-2i, aim+30+2i | same | volleys 2-4 |
| 03512A.. D | layer D | 9 (+4 hard) | 39,33 | -10..-6 | aim +-4..0 | same | every volley |

Not decoded: how the hull's y path is made (`014E3C`), where turrets 24/25 sit relative to the hull (they move along table `035996`;
a6 = 0x105716 = part 25 in the dumps, y stepping 0x80 per 4 frames), the child tasks `0x8D`/`0x8F` (no bullets), and the exact first-round
delays of turrets from boss start. The numbers above are all from the code; where I say "~" the frame counts are derived from counters
(and agree with the dumps to within a few frames).
