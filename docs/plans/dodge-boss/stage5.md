# DoDonPachi stage 5 boss: how it builds its bullet patterns

Boss object kinds 60 (the body, a big ship), 79 and 101 (drones it throws out). All addresses are program (68000)
addresses in `work/ddonpach/program.lst`; "the dump" is `volleys.py` on the four recordings, loop-stage `0-4` (first
loop) and `1-4` (second loop; only r1 and r3 have it). Everything below was read from the code and then checked against
the dumps (counts, kinds, speeds, angle steps, gaps all agree; see "Checks").

Conventions used here: a place is (x, y) in 64ths of a pixel, x the first word (across the board, growing = up the
TV picture), y the second (down the board). A speed is the bullet maker's d0 high word; the bullet's real speed index is
the kind table's (05104E, 0x16 = 22 for every kind used here) plus it, one index = 1/8 pixel per frame. Angles: "64" =
coarse maker (`50DDC`/`50DA0`), "256" = fine maker (`50F94`/`50F58`); 0 is +x (up the screen), 16 of 64 a quarter turn.
`F` = frame counter word 10126E (free running, one per frame). Every maker call is refused while a bomb is out or the
stage is over (see ddonpach.h), and every emitter below also stops firing while word 101764 is set.

## 1. Who fires what

The body (kind 60, per-frame routine `02EBF4`, init `02E7D6`..`02E8EC`, task 100182, task record a5, body a6 = 105 4F6)
does not fire by itself in a loop; it runs a **phase table** (a movement or a hold, per phase) and, inside the phases,
switches on **emitters**: bytes at a6+0x282, +0x283, +0x284, +0x285, +0x28B, +0x28C ... set to 0x80 to start them.
`03054E` runs all emitters every frame: 2FD48 (0x282), 2FFDC (0x283), 3019A (0x284), 30340 (0x28B), 30468 (0x28C),
2FECA (0x285), 304F6/30522 (0x286/0x289), 3048A/304C0 (0x28D/0x28E), 30578 (0x28F).

Only four of them make bullets or bullet-makers:

| emitter | what | who fires | name |
|---|---|---|---|
| 0x282 (2FD48) | two cannons, 6 bullets each | the body (kind 60) | **cannon fans** |
| 0x285 (2FECA) | one 15-bullet fan, jittered aim | the body (kind 60) | **aimed 15-fan** |
| 0x283/0x284 (2FFDC/3019A), kind-79 branch | throws kind 79 drones | kind 79 (296BC) | **sniper drones** |
| 0x28B (30340) | throws kind 101 drones, two per step | kind 101 (29564) | **spiral drones** |

The others only animate (side pods: 0x286, 0x289, 0x28D, 0x28E, wing retract 0x28C) or are never started (see section 5).

## 2. Phases: what chooses the attack

The boss's life never chooses anything. Attack order is a fixed table, run in a loop, by time and travel only.

- Life: long at 0x16(a5), starts 0x38000 (2E802). Each frame the weakest of seven hit parts' hit-point words
  (0xD8, 0xF8, 0x138, 0x158, 0x1D8, 0x1F8, 0x218 (a6)) is read, and 0x7FFF less it is taken off the life, the words put
  back to 0x7FFF (2EC9E..2ED28). Dead when it goes negative. Below 0x7000 only the palette flashes (2EDA0).
  Time limit: long 0x1A(a5) = 0x4650 (18000 frames), counted down each frame (2EC3C); at 0 the fight ends through 5A6AA / 2ED56.
- No phase depends on life, loop flags, or parts destroyed (the pod words 10E738..10E73E and a5+0x34/0x35 only drive
  the side pods' animation and the choice in 2FBCE).
- **Phase table**: 82 rows of 8 bytes at **2F4E4** (last row at 2F76C, then a byte 0xFF at 2F774 that wraps to row 0;
  0x24(a5) is the offset, +8 per row). Fetch at 2F056..2F082 when byte 0x22(a5) is 0: row = {a6+0x1A boss speed (1,4,8
  or 16), a5+0x23 the phase routine's index, a5+0x2A a countdown, a5+0x2B a flag, 0x2C/0x2D unused, 0x2E word 0}.
  Then each frame the routine `2F77C[index]` runs (2F084..2F098) until it clears 0x22(a5).
- **How a phase ends**: movement phases (index 0, 6, 8, 9, 10, 13, 15, 16, 17, 19, 20, 21; code 2FD02) steer toward a
  target (x, y) (y less the scroll word 10E85A): heading turns at most one 64th per frame (59BF6), moves by the speed
  index of the row (5968A; speed byte index 1 = 1/8 px per frame). Distance by 59FE4 (max + min/2): under 0x200 the
  speed byte is set to 8 (1 px per frame), under 0x100 to 6, under 0x40 (1 px) 0x22(a5) is cleared and the phase is
  over. So a movement phase lasts as long as the trip: distance / (speed/8 px per frame) (a trip of 126 px at speed 16
  is 63 frames; checked against the dump). Hold phases (index 1, 18) end on their countdown (a5+0x2A, 1 per frame).
- Where a countdown fires emitters, it is a trigger by value; for indices 9, 10, 15, 19, 20, 21 the countdown is
  decremented first and so stops at 0 (and the triggers fire then or never); in 1 and 18 it ends the phase at 0.

Phase routines used by the table (the others, indices 2-5, 7, 11, 12, 14, are never in the table):

| idx | routine | target / hold | emitters (frame of the phase) |
|---|---|---|---|
| 0 | 2F982 | move to (0x3200, 0x1E00) | none |
| 6 | 2FA68 | move to (0x2800, 0x1E00) | none |
| 8 | 2FA76 | move to (0x5000, 0x1E00) | none |
| 13 | 2FB2A | move to (0x4C00, 0x0400) | none |
| 1 | 2F7D4 | hold, 2A frames (0x40 = 64, or 0xF0 = 240) | if flag 2B=1 (the 240 row): 0x282 at frame 49 (2A=0xC0), 0x285 at frame 113 (2A=0x80) |
| 9 | 2FA84 | move to (0x4A00, 0x0A00) | 0x282 at frame 1, 0x285 at frame 65 (2A = 0x70 down to 0x6F and 0x30) |
| 10 | 2FAD0 | move to (0x3C00, 0x3200) | same as 9 |
| 15 | 2FB38 | move to (0x4C00, 0x1E00) | 0x282 at frame 8 (2A=0x10 down to 8), 0x285 at frame 15 (2A=1) |
| 18 | 2F906 | hold 160 (0xA0) | 0x283 at frame 16 (2A=0x90), 0x284 at frame 48 (2A=0x70), both kind-79 branch (2B3=2B4=1) |
| 19 | 2FC78 | move to (0x3600, 0x2C00) | 0x28B at frame 8 (2A=0x10 down to 8), 0x2B6 = 0x10 |
| 20 | 2FCA4 | move to (0x3600, 0x1000) | same as 19 (restarts 0x28B, see below) |
| 21 | 2FCD0 | move to (0x5000, 0x1E00) | wing retract 0x28C at frame 8 (+ a counter 0x2B7 up to 2, unused by bullets) |
| 16, 17 | 2FB84, 2FBFE | move to (0x4C00, 0x2C00) / (0x4C00, 0x1000) | only the pod animation (0x286/0x289, 0x28D/0x28E), no bullets |

The first boss speed byte (table byte 0) for the movement phases: row speeds 4, 8, 16 as listed below. In hold phases
the speed byte 1 steers by stale registers (d2/d3, not a real target): the boss drifts about 0.09 px per frame
(measured: the cannon's x falls 84 per 14 frames in the dump). Treat as still.

**The table** (row: speed, phase), with the attacks each starts:

```
row  0: s8  idx0  centre                          row 41: s16 idx6
     1: s1  idx1  hold 64 (no fire)                    42: s1  idx18 sniper drones (hold 160)
     2: s16 idx8  to the top-edge middle (5000,1E00)   43: s16 idx8
     3: s1  idx1  hold 240: cannon fans @49, 15-fan @113   44: s1 idx1 hold 240 cannon+15fan
     4: s16 idx13                                  45-48: s8 idx16,17,16,17 pods
     5: s8  idx15 cannon @8, 15-fan @15            49: s8 idx0
     6: s16 idx6                                   50: s4 idx19 spiral drones
     7: s1  idx18 sniper drones (hold 160)         51: s4 idx20 spiral drones
     8: s16 idx8                                   52: s16 idx21 retract
     9: s1  idx1  hold 240 cannon+15fan            53: s16 idx6
  10-13: s4 idx16,17,16,17 (pods)                  54: s1 idx18 sniper drones
    14: s8  idx0                                   55: s16 idx8   56: s1 idx1 hold 240 cannon+15fan
    15: s4  idx19 spiral drones                57-60: s8 idx16,17,16,17     61: s8 idx0
    16: s4  idx20 spiral drones                62,63: s4 idx19,20 spiral    64: s16 idx21
    17: s16 idx21 retract                      65: s4 idx9  66: s8 idx10   67: s8 idx15 (cannon+15fan, moving)
    18: s4  idx9  cannon @1, 15-fan @65        68,69: s4 idx16,17           70: s16 idx6
    19: s8  idx10 same                         71: s1 idx18 sniper drones   72: s16 idx8
    20: s8  idx15 cannon @8, 15-fan @15        73: s1 idx1 hold 240         74-77: s8 idx16,17,16,17
  21,22: s4 idx16,17                           78: s8 idx0                  79,80: s4 idx19,20   81: s16 idx21
    23: s16 idx6                               (then back to row 0)
    24: s1  idx18 sniper drones
    25: s16 idx8   26: s1 idx1 hold 240
 27-30: s8 idx16,17,16,17     31: s8 idx0
    32,33: s4 idx19,20 spiral   34: s16 idx21   35: s4 idx9   36: s8 idx10   37: s8 idx0
    38: s8 idx15   39,40: s4 idx16,17
```

Every row's four bytes are in `2F4E4 + 8*row`; the whole table is the reference. Its shape: row 0-9 is an opener
(centre, 64-frame pause, the 240-frame hold with cannon fans and the 15-fan, one moving cannon/fan row, sniper drones,
another 240 hold); from row 10 on it is a cycle of about 17 rows (pods x4, centre, spiral drones x2, retract, cannon+15-fan
moving x2 or x3, sniper drones, 240 hold) that repeats with small changes (rows 18-26 vs 35-44 vs 65-73), ending at 81
and wrapping to row 0. In the dumps the order seen is exactly the opener, then rows 5, 7 (sniper drones, about +600
frames after the first cannon fans), 9, 15-16 (spiral drones, about +2100) and 18-20 (cannon/15-fan again, +2530).

## 3. The attacks in the vocabulary

### 3.1 Cannon fans (emitter 0x282, body kind 60; sites 02FDB6-02FEB8)

Emitters: two cannons, at the boss's place (a6+6, the centre; a6+0x1E6 and +0x206 are the centre moved by
(-0xE00, +0xE00) and (-0xE00, -0xE00), i.e. 56 px behind it, one each side: 02EE66..02EE78; the dump agrees: the first
bullets sit 3520 above and below the centre y). Bullet places, relative to the boss centre (64ths): x = -3200 for every bullet
(cannon x -3584 plus 0x17F + the carry from the low word, 02FD9A..02FDA4 for A, 02FE2E..02FE38 for B), y as listed.

Timeline: the emitter is started (0x282 = 0x80, countdown a6+0x2A2 = 0 then 0x2AB=1, 0x2AC = 14, count 0x274 = 10 (set
at every trigger)): it fires on the frame it is started and then every 14 frames, 10 volleys in all (frames 0, 14, ..,
126), then stops (02FEBE). At the start the attacked ship alternates (5(a5) ^= 1, 02FD78): ship 1 first, ship 2 on the
next start (59A58 chooses the first alive ship starting from it; 5A64A).

Each volley: aim = **fine aim (256) from the boss centre to the ship now** (59C66, 02FD90; re-taken for cannon B at
02FE28), if no ship alive nothing is fired but the count goes on. Then, in this order (fine maker 50F94 for kind 17,
50F58 for kind 0), A = aim:

| cannon | y offset | angle | kind, speed (d0) | maker |
|---|---|---|---|---|
| A1 | +3520 | A | 0, -5 (FFFB0000) | 50F58 |
| A2 | +3264 | A+8 | 17, -9 (FFF70011) | 50F94 |
| A3 | +3008 | A+16 | 0, -5 | 50F58 |
| A4 | +3776 | A-8 | 17, -9 | 50F94 |
| A5 | +4032 | A-16 | 0, -5 | 50F58 |
| A6 | +4288 | A-24 | 17, -9 | 50F94 |
| A7 (loop 2 only, 02FE12) | +2752 | A+24 | 17, -9 | 50F94 |
| B1 | -3520 | A | 0, -5 | 50F58 |
| B2 | -3776 | A+8 | 17, -9 | 50F94 |
| B3 | -4032 | A+16 | 0, -5 | 50F58 |
| B4 | -3264 | A-8 | 17, -9 | 50F94 |
| B5 | -3008 | A-16 | 0, -5 | 50F58 |
| B6 | -4288 | A+24 | 17, -9 | 50F94 |
| B7 (loop 2 only, 02FEA8) | -2752 | A-24 | 17, -9 | 50F94 |

(A5..A6 steps are `subq.b #8,d1` etc.; every angle step is the instruction between the calls: 02FDBA +8, 02FDCC +8,
02FDDE -0x18, 02FDF2 -8, 02FE04 -8, 02FE1E +0x30 (loop 2); B: 02FE4E +8, 02FE60 +8, 02FE72 -0x18, 02FE86 -8,
02FE98 +0x28, 02FEB4 -0x30 (loop 2).) The negative speeds are real: speed index 22-5 = 17 (kind 0) and 22-9 = 13
(kind 17), 2.1 and 1.6 px per frame.

Loop 2: the maker entries do the extras: 50F58 adds +4 to the speed (kind 0 flies at -1 -> index 21) and 50F94 makes two
bullets on the same angle at the speed and speed+6 (kind 17 at -9 and -3), because 0xD(a5) bits 0x81 are clear and the
body's first byte bit 1 is clear (50F24, 50EE8); plus the two A7/B7 bullets. So 12 -> 22 bullets per volley (dump: 12 and
22, and 27/53 when the 15-fan volley is in the same frame).

### 3.2 Aimed 15-fan (emitter 0x285, body kind 60; sites 02FF2E..02FFCA)

Place: the boss centre moved by -0x500 in x (-20 px), y same (a6+0x1C6 plus 0xFB000000, 02FF14..02FF18).
Timeline: started like 0x282 (0x5(a5) ^= 1, a6+0x26A word = 0x0808, 02FEE8..02FEF2): fires every 8 frames, first 8 frames
after the start, **10 volleys** (0x26C = 10, 02FFD0), then stops. Same no-ship rule.

Volley (aim A = fine aim from the centre to the ship now, 02FF06; d7 = A):
1. Centre bullet: angle A + j, j = (R - 1) * 4 with R from the random table `57FA2` (a 64-byte table of 0, 1, 2; the
   index is a free-running counter, 057FB6), so j in {-4, 0, +4} (02FF1E..02FF2C). Kind 24, speed +2 (d0 = 00020018), maker 50F58.
2. Loop 2 only (02FF3A): the same bullet again at A+j+4 and A+j-4 (50F58, so the speed is +4 more: dump (24, 6)).
3. Fan: kind 38, speed +2 (d0 = 00020026), maker 50F94, angles A+16, A+22, A+28, A+34, A+40, A+46, A+52, then A-16, A-22,
   A-28, A-34, A-40, A-46, A-52 (02FF5A..02FFCA: A+0x10, then `addq.b #6` six times; `subi.b #$10` from A, then `subq.b #6`
   six times). 14 bullets, 6/256 apart, a gap of 32 across A. Note the fan is centred on the plain aim A, not on the
   jittered centre bullet (the dump's "first" bullet is the jittered one, so its relative angles are 16-j, .. : 12, 16, 20
   as seen).
Loop 2: 50F94 doubles each (speed +2 and +8), kind 24 gets 50F58's +4: n = 15 -> 31 (dump 15 and 31).

Bullets per volley: 15 (loop 1), 31 (loop 2). Fire interval 8 frames.

### 3.3 Sniper drones (emitters 0x283 and 0x284, kind 79; sites 029904 and the throwing code 0300EE..030122 / 0302AC..0302E8)

Two emitters throw kind 79 objects (task group 7, a pool of 48 shared with the pods and the other drones; a throw with no
free slot is silently skipped, 58166). Started by phase idx 18 (frame 16 for 0x283, frame 48 for 0x284), with
0x2B3 = 1 / 0x2B4 = 1 (selects kind 0x4F, 79; with 0 it would throw kind 100, never done in this boss).

Throw timeline (the same for both emitters, 0x283 shown, a5+0x26 wing offset; 0x284 uses a5+0x28 and a6+0x267/0x268):
1. wings open: a5+0x26 += 0x40 per frame until 0x600 (24 frames, 02FFFA..030024);
2. wait 16 frames (a6+0x263 = 0x10);
3. **throws**: on frames where `F & 7 == 0` (loop 1; `F & 1 == 0` in loop 2: 030068..030074), count a6+0x264 (5 in loop 1,
   0x20 = 32 in loop 2; set by 2F906 at frame 16 / 02F938), one drone each; a6+0x265 += 4 per throw (0x268 -= 4 for the
   other side);
4. wait 16, wings close (a5+0x26 -= 0x40 per frame).
So the first throw is 40 frames plus up to 7 after the trigger; loop 1: 5 drones a side, 8 frames apart; loop 2: 32 a side
(64 for both, but the pool of 48 limits the number alive), 2 frames apart.

Where a drone starts: boss centre + (tx, ty + 0xE00) for 0x283 (y +) and + (tx, -(ty + 0xE00)) for 0x284, with
(tx, ty) = table 3017A index (a6+0x264 & 7) (a6+0x267 & 7): (1664,960) (1920,384) (2240,-192) (2624,-768) (1344,448)
(1728,-192) (2048,-768) (2240,-1344). Loop 1 uses indexes 5,4,3,2,1; loop 2 0,7,6,5,4,3,2,1 repeated.
Heading at birth (64ths, 0 = +x): 0x283: 0, 4, 8, 12, 16 (and on in loop 2: 4k, wrapped at 64); 0x284: 0, 60, 56, 52, 48 (mirror).

The drone's life (init 02967E..0296BA, per-frame 0296BC; its own a5: state byte 0x39, birth 0x39 = 9): a6 body from table
29662 (speed 0x20 = 4 px per frame, hit points 0x300 (loop 2: 0x80), box 8 px); lifetime a5+0x24 = 0x1E0 = 480 frames;
dies off screen (5A322) or when shot. States:
- **move** (bit 3): speed byte a6+0x1A falls 1 per frame from 0x20 (+ a random 0..11 on later laps) to 8, then to
  state *aim*; if x < 0x1E00 and not yet done on this lap the heading is turned a half turn (+0x20) once (029734..029748).
- **aim** (bit 2): each frame turn the heading one 64th toward the coarse aim at the ship (59BE8: 59A58 then 59BF6);
  when it no longer changes (aligned) or no ship: state *fire*, count 4 (a5+0x22), wait 16 frames (a5+0x20 = 0x10).
  With no ship at all a5+0x26 = 1, which stops the firing.
- **fire** (bit 4): after the 16-frame wait, on frames `F & 7 == 0`: **one bullet** at the drone's place, angle = its heading
  (64), kind 5, speed +8 (d0 = 0x00080005), maker `50DA0` (`jsr` at 0298FE; **loop 2: speed +4, so 12**); then a recoil
  of 0x30 speed in the opposite heading (596C4: 6 px per shot, 029904..029918). 4 shots, 8 frames apart (dump: gaps 8).
- **turn away** (bit 5): pick a heading = current + random (-7..+7) (`57C84`, a 15-value table 0..7, 249..255, 029928..029944),
  wait 16, turn toward it one 64th per frame, then **hold** (bit 6) 16 frames, then new speed 0x20 + (0..11) (`5810C`, table 058120)
  and back to *move*.
The lap is about 75..130 frames (dump: 96..115 between bursts of 4). Its fire is always 4 single bullets, aimed at the
ship at the moment it aligned (the ship may move after: dump aimC 0..+12 over the burst), never a pattern in one frame.

### 3.4 Spiral drones (emitter 0x28B, kind 101; sites 029630, 02963E, 02964C, 02965A)

Started by phase idx 19 and 20 at frame 8 of each (idx 20 restarts it, resetting the count). 0x28B (30340):
1. wings open (a5+0x26 and +0x28 += 0x40 until a5+0x28 = 0x600, 24 frames); 
2. **throws** on frames `F & 7 == 0` (no loop-2 change): a **pair** each throw, count a6+0x2B6 = 0x10 (so 16 throws = 32
   drones per start). Drone P at the centre + (tx, -(ty + 0xE00)) (from 0xA6), Q at the centre + (tx, ty + 0xE00) (from 0x86),
   (tx, ty) = table 3017A[(0x2B6) & 7] (the sequence 0,7,6,5,4,3,2,1,0,7,...). Headings (64ths): P `(4 - k) & 63`, Q
   `(3 - k) & 63` where k = a6+0x2B5 counts 0..7 and wraps, +1 per throw (030418..030442, 0303EC..0303FE); a6+0x2B7 is
   copied to a5+0x15 (0, 1 or 2; no effect on bullets).
3. after 16 throws the emitter ends (wings stay out until idx 21).

The drone (init 0294D4, per-frame 029564): birth place as above; velocity = (vx, vy) from `596C4` speed index 0x10 (2 px
per frame) in its heading (a5+0x20/0x22), added to its place every frame; **vx -= 2 every frame** (an acceleration of 2/64 px
per frame^2 toward -x: it curves down the board, 295BC); dies at x < 0xFC00 or after 0x258 = 600 frames or when shot
(hit points 0x200); also stops existing if killed.
Fire: a5+0x1A word = 0x1003: first volley **17 frames** after birth (byte 0x10 counts to a borrow), then every **4 frames**
(reload byte 3); a5+0x18 = 8, so **8 volleys** (dump: 8 per task); a5+0x1E = coarse aim (59A58 from the birth place to the
ship at that moment; 0x20 if there is none: 0x1E(a5) is set to 0x20 first, 02953C), +2 before each volley (`addq.b #2,0x1E(a5)`, 029614), so
each volley is rotated 2 of 64 (11.25 degrees, 8 of 256) from the last. A volley is **4 bullets**, angle (64) 0x1E, +16, +32, +48
(the cross), kind 5, speed +0 (d0 = 5), maker `50DA0` from the drone's place (`jsr` 02962A, 029638, 029646, 029654), **loop 2:
speed +4 (dump (5,4))**. Total on a pass: about 32 drones x 8 volleys x 4 bullets = 1024 bullets over about 128 + 60 frames,
limited by the pool (task group 7 of 48, bullets 205/245).

## 4. Differences of loop 2 (flag word 10197A)

- Makers: 50F58 and 50DA0 add +4 to the speed; 50F94 (and 50DDC, not used here) make two bullets of different speeds at one
  angle (speed, speed+6) because the boss's first byte bit 1 is clear and 0xD(a5) is 0; so every `50F94` call doubles.
- Cannon fans: A7, B7 added (02FE12, 02FEA8). 15-fan: the two extra centre bullets (02FF3A).
- Sniper drones: throws every 2nd frame instead of every 8th, 32 each instead of 5 (2F938, 2F96C, 030068), drone hit points
  0x80 (029728..); shot speed +4 (from 50DA0).
- Spiral drones: speed +4 only.
- Pod emitters (phases 16, 17): the triggers are 0x1F-masked in loop 1, unmasked in loop 2 (02FB84, 02FBFE), and 0x18 instead
  of 0x28 counts (02FBB4); no bullets either way.
- No other change: the phase table, the volley counts (10 and 10), the triggers, the timings are the same.

## 5. What does not fit the vocabulary, and dead code

1. **Drones are objects, not volleys.** Kind 79 is a little machine (move, aim, fire 4, turn away, hold, again) with random
   turns and speeds; kind 101 is a ballistic spiral thrower. The pattern is "throw N objects of kind K at times t_i from
   places p_i with headings h_i; each object runs behaviour B". Needed in the vocabulary: *spawner* (interval, count,
   place table, heading rule, loop-2 count/interval) and *object* (lifetime, motion, fire rule). The pool limit of 48 and
   its silent skip matter in loop 2.
2. **Boss motion decides the timing.** Phases end by arriving, not by a timer, and fire places move with the boss (it
   drifts or steers while firing; the 282 volleys fired while moving in rows 18-20 are from a moving centre).
   Durations: travel time = distance / (speed/8 px per frame) with turning at one 64th per frame; use the table for order,
   measure the trips (the dump gives them: e.g. cannon fans at +0, +262, +782, +2529 from the first fire).
3. **Overlap**: the cannon fans (126 frames) and the 15-fan (80 frames) run together in the same frame in phases 3, 5, 9, 10,
   15, 20 (27 / 53-bullet volleys in the dump); sniper drones also overlap with a second 240 hold.
4. **Aim choice alternates**: the ship (player 1/2) the whole attack aims at flips each start of 0x282 and 0x285 (5(a5) ^= 1).
5. **The dump's `atk` column does not mean an attack here**: for the body (kind 60) it is the high byte of the wing offset
   0x26(a5) (0 when firing), for kind 79 it is the no-ship flag, for kind 101 it is the throw counter 0x2B7 (0 to 2).
   Nothing in this boss selects by it.
6. **Dead code**: emitter 0x28F (30578: 15 bullets, kind 7 speed -4..+, a fan of 5+5 of 256, 50F94) is never started (its
   byte is set nowhere; the dumps show no kind 7); the kind-100 throw (0x283/0x284 with 0x2B3/0x2B4 = 0: phase idx 7, 12,
   14, with kind 100's routine 020F00 that fires 4-way crosses like kind 101) is not in the table; at 02F0AE..02F0D6 a
   volley of kind 100 near the ship (`and.w #7` every 8 frames, aim 59A58, kind byte 0x30, `moveq #7,d0; move.l #$64000000,d3`)
   is built but there is no `jsr 581A8` (bytes 7007 263C 64000000 536E 0296): the call was removed.
7. Side pods (kinds 61 and 62, tasks from 02E88E on) and the death debris (kinds 42 and 43 at 0309E4/030BB8) were not
   checked for fire; the dumps of stage 5 show only kinds 60, 79 and 101.

## 6. Checks against the dumps

- Loop 1: n 12 = 2 x 6 (kinds 0 and 17, speeds -5/-9); 15 (24 and 38, speed 2; first-bullet relative angles
  16-j, 12/16/20 as the three jitters); 27 = 12 + 15 together. Intervals: 14 (cannon fans), 8 (15-fan), 8 (sniper shots), 4 (spirals).
- Loop 2: 22 (kinds 0 at -1, 17 at -9 and -3), 31 (24 at 6 and 2, 38 at 2 and 8), 53 together; spirals (5, 4); snipers (5, 12).
- Spiral: 4 bullets rel 0, 64, -128, -64 (of 256); consecutive volleys' first angle +8 of 256 (dump: 128, 136, 144, ..);
  8 volleys a drone; 4 frames apart.
- Cannon fan places in the dump (x -3200, y +-3520, 3264, 3008, ..) match the table above (r2, frame 101942).
- Order seen: cannon fans + 15-fan first (row 3), moving fans (row 5), sniper drones (row 7, +598 frames), fans (row 9),
  spirals (+2096, rows 15-16), fans (+2529, rows 18-20); r2 and r1 agree to a few frames.
