# DoDonPachi stage 3 boss (object kind 45): how it builds its bullet patterns

Code: `work/ddonpach/program.lst` 02C814 (set up) to 02E136, data 02CEA2-02D1FD (parts table, spawn table), 02CD60/02CD68/02CD88
(attack tables). Checked against volley dumps "0-2" and "1-2" (runs r1..r4) and against the raw bullet recordings, where
every bullet was matched to the program address its maker was called from (the "site" = the return address of the `jsr`).
Every site named in the brief is accounted for (table at the end).

Conventions: angles are of 256 unless marked "of 64" (x4 to get 256). Speeds are the **added speed** in the high word of d0:
every bullet kind used here (0, 3, 5, 24, 35, 38) has the same base speed 22 in the kind table (5104E), so the bullet's real
speed is 22 + the number given. "Frame" is a game frame. `L` is the boss's level (below). `a6` = body records (0x20 bytes
each, from `+6(a5)`); record n is at a6 + 0x20*n. Places are x = high word, y = low word, 64ths of a pixel; the bullet
makers' d2.

## 1. What the boss is made of

One task (`a5`), 29 records at `a6`. Record 0 is the control record (`BASE`); the ones that matter:

| rec | offset | what | code that runs it |
| --- | --- | --- | --- |
| 0 | +0x000 | control: timers, level, attack choice | 02CB6A-02CDA8 |
| 6, 8, 10 / 7, 9, 11 | +0x0C0.. | the two **arms** (3 hit boxes each; 6 and 7 hold the state) | 02DDF0 |
| 14, 15 | +0x1C0, +0x1E0 | two **pods** (open, fire 8 bullets once, close) | 02D98E |
| 16, 17, 18, 19 | +0x200.. | four **rear turrets** (aim, 1 bullet) | 02D424 |
| 20, 21 | +0x280, +0x2A0 | two **twin-barrel turrets** | 02D628 |
| 22 | +0x2C0 | the **body**: its three big attacks | 02CC28-02D424 |
| 23, 24 | +0x2E0, +0x300 | two **spinning cannons** | 02D7C6 |
| 12, 13, 25-28 | | scenery / never run | 02E114 (12, 13) |

The initial records are the table at 02CEA2 (expanded by 059F28; I expanded it by hand, values quoted below). Every frame
02CB9E copies the (swaying) boss place into every record's place (+6); parts then add their own offset
(`0xC(a6) - 0xFC00FC00`, or `- 0xFE00FC00` for the pods). The boss sways on a small ellipse (59980, radius `+0x10(BASE)`
ramping 0..0x40 by 1 every 4 frames, phase = frame counter byte at 10126F); the emitters move with it. I treat the
sway as: emitter = boss place +- a few pixels.

Fight start: the entry script (59DEA) sets `+0x24(a5)` when the boss has arrived; from then `+0x1A(a5)` counts frames.
At count 0x1E (02C9A8-02CA5A) every part gets flag byte 0xA1 (bit 5 = running) and the 7 hit targets are registered. From
here everything below runs. Timeout: at `+0x1A(a5)` = 0x3840 (14400 frames) the boss leaves (02CA78, 02CB06). Boss life is
`+0x16(a5)`, 0x23000 at the start (02C82E).

## 2. Phases: what chooses what

There is **no fixed order of phases**. Four separate things decide what fires; all are state, not time alone.

### 2.1 The level L (0..16) - the boss's rank

`L = min(16, T + D)` (02CC32-02CC74, kept in `+0x17(BASE)`):

- `T` = `+0x16(BASE)`: +1 every 360 frames (countdown `+0x14`, 0x168, 02CC32-02CC46), capped 16; the countdown restarts whenever
  `D` changes (02CC56-02CC62) and when an arm finishes unfolding (02DD32 also adds 1 to T directly).
- `D` = `+0x05(BASE)` (the byte of the word at +4): the parts destroyed, weighted: rear turret 2 each (02D4DA; 4 of them = 8),
  twin turret 1 (02D66C), pod 1 (02D9E4), spinning cannon 1 (02D86C), arm 1 (02DEC8). Total 16 = everything destroyed
  (tested as `cmpi.b #$10,+5(BASE)` at 02CC9C and 02CD0A).

L is the one number that makes every attack faster: see the tables in 2.3 and 3. Loop 2 starts L at 0 again.

### 2.2 The main schedule (the body's three attacks) - 02CC7A-02CD5C

`+0xA(BASE)` is a countdown; `+0x2C1.bit0` says which half it is timing.

```
fight start + 30 frames: rest 448 (+0xA = 0x1C0, from the table)
then forever:
  ATTACK (bit0 set): length 540 frames (720 only when D = 16, 02CCA4); A overrides with 360 (02D1FE), C with 840 when L = 16 (02D392)
  REST   (bit0 clear): 128 frames (02CCF6); 80 when both arms have been triggered (+0xE(BASE), starts 2, -1 per arm, 02DEF8);
         20 when D = 16 (02CD14)
```

Firing inside an ATTACK starts only when the arming counter `+0x12(BASE)` reaches 0x200: +8 a frame in an attack, -8 a frame
in a rest (02CD1E-02CD56), so **a 64 frame run-up at the start of every attack**, except that at L = 16 the counter never goes
down (02CD2C-02CD34) and firing is immediate. (Dump check: B episodes in loop 0 last 475 frames = 540 - 64, with L = 16 they last
~539, C 840 at L = 16: gaps and counts agree.) In a REST the body fires nothing, but the arms' rest stream does (3.5).

Which attack: 02CCAA-02CCE8. `idx = (rand57EBA + rand57C84) & 7` (57EBA is uniform 0..63, so idx is uniform); table 02CD60 =
`0 0 1 1 1 2 2 2` gives the attack: **A (aimed stream) 2/8, B (mirrored spiral) 3/8, C (rotating double fan) 3/8.** The same attack
may follow itself **once** and not twice (`+0xC(BASE)` last attack, `+0xD` repeated flag; a third in a row re-rolls). The setup
routine (02CD68 table, one per attack) runs once at the start of the attack, the firing routine (02CD88 table) every frame
once the arming counter is full. The firing code is run from the body record (`a6 + 0x2C0`): fields below are that record's.

### 2.3 Gates on the other parts (by state)

| part | starts firing when | code |
| --- | --- | --- |
| pods 14/15 | 129 (pod 14) / 193 (pod 15) frames after fight start, then a cycle | 02DAD0 |
| twin turrets 20/21 | 129 frames after fight start, every 57 frames | 02D770 |
| arms 6/7: aimed burst | always (a swing cycle from fight start) | 02DF52 |
| arms 6/7: rest stream | only while the body is in REST and the arm is not unfolding | 02DF60-02DF7E |
| rear turrets 16-19 | **armed** when boss life < 0x11800 (exactly half; 02D432) **or** when the spinning cannon on their side is destroyed (02D870: cannon 23 arms 16 and 18, cannon 24 arms 17 and 19). Until armed they cannot be hurt and do not fire | 02D424-02D5A2 |
| spinning cannons 23/24 | **armed** when their arm starts to unfold or is destroyed (02DEE8-02DEEE) | 02D7C6 |
| arms unfold | when all four of pods 14/15 and twin turrets 20/21 are destroyed (`+0x18(BASE)` word, 4 at start, -1 each, 02D670, 02D9E8; the arms test it at 02DDFC): the arm plays a 360 frame break-up (02DC90-02DD44), is then gone, T+1 | 02DDF0 |

The body's boss-life thresholds (0x1F800, 0x1C000, 0x15000 and 0x11800, 0xE000, 0xA800, 0x7000: table 02D1B6, run by 014E3C
from 02CC1A) spawn effect objects of kind 0 and 1 through 04FB6E. None of the volley dump's bullets come from them; I did
not follow them further.

## 3. The attacks, as rules

Emitter positions are "boss place + offset (x, y) in 64ths" as the part adds them. The "maker" is the entry called; loop 2's
expansion of the maker is in section 4.

### 3.1 Body attack A - aimed stream (sites 02D27E)

```
attack A: emitter body (offset 0)
  setup 02D1FE:  length 360 frames; interval  I = 40 - 2L  (L=0..16 -> 40..8); speed  S = floor(L/2) (0..8)
                 (stored in +0x1C/+0x1D = I, +0x1F = S, 02D212-02D222)
  timeline: a volley every I frames while the attack runs (counter +0x1C reloaded from +0x1D, fires on 0, 02D22C-02D234;
            first one I frames after firing starts), skipped for a frame if the boss is off screen (5A2AC)
  volley: 1 bullet
    angle = fine aim (59C66, 256) from the body to the ship, NOW  + (rand57FA2 - 1) in {-1,0,+1}, equal chances (02D244-02D256)
    kind 24, speed S, maker 50F94 (256), place = body
```

Dump check: aimF in {-1,0,+1} with 127/121/123 volleys; at L = 16 episodes of 360 frames have ~45 volleys (8 apart).

### 3.2 Body attack B - mirrored spiral (sites 02D326, 02D32E; loop 2: 02D366, 02D36E, 02D37C, 02D384)

```
attack B: emitter body (offset 0)
  setup 02D282:  start angle a0 = rand57DA8 - 32  (0..255 minus 32)
                 R (volley frames per burst) = 2 if L < 8; 5 if 8 <= L < 16; 7 if L = 16   [min(L,10)/2 -> 0..5; if L<16: (that/4)*3; 02D28A-02D2B6]
                 counter +0x1D = R; counter +0x1C = 2 at the start (+R more when bit 1 of the frame counter 10126F is clear)
  angle:  base b  =  a0 + 2 * (frames since the attack's firing began), EVERY frame, fired or not (addq.b #2,+0x1B, 02D2F2)
  timeline: bursts: R consecutive frames with a volley, then 3 frames without (counter +0x1C counts R-1..0 fire, -1,-2 skip, -3 reloads
            to R and skips, 02D2F6-02D30C). Period R + 3 = 5 / 8 / 10 frames.
  volley: 2 bullets, a mirrored pair
    angle b, then -b (neg.b d1)
    kind 24, speed -3, maker 50F94 (256), place = body
```

Dump check: volleys come in mirrored pairs (`rel` second = -2 x first); 191 volleys in a 475 frame episode at R = 2 (475/5*2),
297 at R = 5 (475/8*5), 378 in 539 frames at R = 7 (539/10*7). The `first` angle goes up 2 a frame.

### 3.3 Body attack C - rotating double fan (sites 02D40E, 02D418; loop 2 same sites)

```
attack C: emitter body (offset 0)
  setup 02D388:  length 540 (840 if L = 16); interval frames: counter +0x1C = 1, reload +0x1D = 22 - (L + 2) = 20 - L   (L=16: 4)
                 start angle  b0 = 64 - (rand57DA8 & 31)  (33..64);  spin term s = 0
  timeline: a volley when the counter underflows: first 2 frames after firing starts, then every (21 - L) frames (L=0: 21 .. L=16: 5)
  volley: 8 bullets
    s += 1; b = b_prev + s      (so b turns by 1, 2, 3, ... per volley: the spin accelerates for the whole attack; 02D3D8-02D3E0)
    angle  = (b - 24) + 12 k  and  (b - 24) + 128 + 12 k,   k = 0..3     [d1 = b-24; bullet; +0x80; bullet; +0x8C; x4]
    kind 24, speed -3, maker 50F86 (256), place = body
```

Dump check: `rel` = `[0,-128,12,-116,24,-104,36,-92]` exactly (= 0,128,12,140,24,152,36,164); `first` rises by 2,3,4,5,6,7 between
volleys; episode of 840 frames with 168 volleys at L = 16 (every 5).

Not a fit for plain "base + step": in C the step itself grows by 1 each volley (a second-order sweep); in B the step is per
*frame*, not per volley. Both need "angle = f(frames since start)" or "step grows".

### 3.4 Pods 14, 15 (rec 14, 15; site 02DB74) - fixed 8-bullet spray, once a cycle

```
pod: emitter = body + (-48, 0) px for pod 14, (-64, 0) px for pod 15  (0xC(a6) - 0xFE00FC00: x -0xC00 / -0x1000, y 0)
  cycle (02DAA6-02DB7E, one volley per cycle):
    wait W frames (+0x1C counts down) -> open 8 frames (sprite +2 a frame, 6968 -> 6978) -> wait 17 frames (0x10) -> FIRE -> hold 17 frames
    -> close 8 frames -> W' frames -> ...
    W (first) = 129 (pod 14, 0x80) or 193 (pod 15, 0xC0); W' (reload +0x1D) = 177 (pod 14, 0xB0) or 201 (pod 15, 0xC8)
    => pod 14 fires first 154 frames after fight start then every 227; pod 15 64 frames later, every 251.  (dump: 64 frames apart)
  volley: 8 bullets, kind 0, maker 50DDC (64), table 02DB82  (speed, angle of 64, dx, dy in 64ths)
       (-12, 38, +192, -512) (-12, 32, +192, 0) (-12, 26, +192, +512)
       (-6, 35, 0, -256)     (-6, 29, 0, +256)
       (0, 40, -192, -512)   (0, 32, -192, 0)   (0, 24, -192, +512)
    -> in 256: 152, 128, 104, 140, 116, 160, 128, 96. A FIXED spray straight at 128 (does not aim). 02DB4A skips the volley if off screen.
  hit rule: a hit counter (+0x1B = 3 per window, +0x1A = 9 total, reset to 3 when it closes); 9 hits kill it (also `+0x18` life word 0x1600 hits <0)
```

Dump check: `first 152 ... rel [0,-24,-48,-12,-36,8,-24,-56]`, speeds (0,-12),(0,-6),(0,0), n = 8.

### 3.5 Arms (rec 6/7, with 8/9, 10/11; sites 02E044 and 02E108)

Both arms share the code, mirrored by bit 2 of byte 3 of the record (rec 6: clear, rec 7: set). Each arm has TWO independent
fires.

**Aimed burst (02E108, the "swing" machine 02E04E, runs from fight start whatever the body does):**

```
arm cycle (counter +0x84(rec 10)):
  extend   : every frame  +0x9C += 0x30 until 0x400 (22 frames); then counter 8
  fire     : counter 8 -> burst; every 3 frames a shot, 7 shots; the first re-aims (coarse aim 59A58 from the arm's tip, kept in +0x9F),
             the other 6 reuse that angle; then counter 8
  retract  : -0x30 a frame to 0 (22 frames); then counter +0x85 = 0x50 (rec 10 byte 5)
  tip (where bullets come from) = arm place + (+0x200, +0xEC0)  (left arm), (+0x200, -0xEC0)  (right arm)   [02E0AC, 02E0BC]
  shot: 1 bullet, angle = aim (of 64) taken at the start of the burst, kind 38, speed 6, maker 50DA0 (64)
```

Dump check: 7 shots at 3 frame spacing with constant angle (`35712,15,18,...,30`: all `first 104`).

**Rest stream (02E044, only while the body is in REST, 02DF60-02DF7E):**

```
  emitter = arm place + (+0xC0, +0x1B80) left arm / (+0xC0, -0x1B80) right arm   [02E002, 02E008: +0x1A(rec) word is +-0x1B80]
  timeline: shot every 5 frames (counter +0x1C reload +0x1D = 4); every 7th shot (counter +0x5A 6..0, reload +0x5B = 6, 02DFDC-02DFE2):
            speed += 2 (+0x5C word 4,6,8,...,22,... no cap seen) and the wait before the next shot is +0x5F + 1 frames
            (+0x5F = 30, then -6 each time: 31, 25, 19, 13, 7, 1 frames; when it would go below 0 it becomes 4: 5 from then on)
  shot: 1 bullet, angle 32 of 64 (= 128, straight down) fixed, kind 3, speed +0x5C, maker 50DA0 (64); the segment (rec 8) wobbles +-0x80..0x100 (only visual)
  reset: whenever a body ATTACK is running the speed goes back to 4, the pause table to 30 (02DF60-02DF72)
  (the 7-shot counter +0x5A is not reset)
```

Dump check: speeds 4,4,4,4,4,4,6 (the 7th shot is the first at the new speed), then a 31 frame gap, then 6,6,6,... `first 128`;
both arms fire on the same frames (pairs).

Arm state also: `+0x18(rec 6)` life 0x4000 (hits of its 3 boxes; damage taken from segments `0x58`/`0x98` words, 02DE60-02DE78);
dead -> D+1, cannon armed (02DEB2-02DEEE).

### 3.6 Twin-barrel turrets 20, 21 (sites 02D7AC, 02D7BE)

```
turret: emitter = body + (-40 px, +32 px) (rec 20), (-40 px, -32 px) (rec 21)   [x -0xA00, y +-0x800]
  facing f (of 64) = turns ONE step toward the coarse aim (59BE8: 59A58 + 59BF6) every 8 frames (frame counter bits 0-2 clear);
         it does not aim, it follows the ship's direction slowly (+0x1B(a6))
  timeline: a volley every 57 frames (counter +0x1C reload +0x1D = 0x38); first 129 frames after fight start (+0x1C = 0x80); skipped if off screen
  volley: 2 bullets from two muzzles, same angle f (of 64), kind 5, speed -6, maker 50DDC (64)
          muzzle A = turret + table 027B1E at byte offset (f + 2) & 0x3C, muzzle B = turret + table 027B5E at the same offset   (16 long entries each: x, y in 64ths)
```

Dump check: every 57 frames, `first` creeps; n = 2 with rel [0,0]; speeds (5,-6).

### 3.7 Spinning cannons 23, 24 (sites 02D948, 02D984)

```
cannon: emitter = body + (-1 px, +72 px) (rec 23), (-1 px, -72 px) (rec 24)
  armed only after their arm unfolds / dies (see 2.3). Counters +0x1C and +0x1D both start at 0x60 and count while armed
  spin:  angle g (of 256, +0x1B) starts at 0x20 and moves -4 a frame (rec 23) or +4 a frame (rec 24, bit 2 of its byte 1)  - 64 frames a turn
  timeline: both counters reload to 2 when they fire: one shot of each every 3 frames (first 97 frames after being armed)
  volley: 2 bullets, same angle g (of 256), maker 50F94 (256), skipped per bullet if off screen (5A2AC):
          bullet 1: kind 5, speed -2, place = cannon + table 028058 at byte offset ((g + 8)>>2) & 0x3C   (02D938-02D942)
          bullet 2: kind 5, speed -8, place = cannon + table 028098 at the same offset                   (02D974-02D97E)
```

Dump check: `n 2, rel [0,0]`, speeds (5,-8),(5,-2), `first` steps -12 each 3 frames (80, 68, 56, ...), gaps of 3.

### 3.8 Rear turrets 16-19 (site 02D61E)

```
turret: emitter = body + (+20, +27) px (16), (+20, -27) px (17), (+32, +16) px (18), (+32, -16) px (19)   [0xC(a6)-0xFC00FC00]
  armed: see 2.3 (boss life < half, or its side's cannon dead). While unarmed a hit just resets life to 0x1800.
  facing f of 64: same one-step-per-8-frames follow of the coarse aim as 3.6
  timeline: counter +0x1C starts 0x60 (96) and counts only while armed; reload = (0x40 + 8 if L >= 10) - 4 * max(0, L - 6), then fires
            on underflow: period  L<=6: 65, 7: 61, 8: 57, 9: 53, 10: 57, 11: 53, 12: 49, 13: 45, 14: 41, 15: 37, 16: 33 frames
            special case: if (arms finished + D) == 14 the reload is 10 (period 11)  [02D5DA-02D5E0]; unexplained
  volley: 1 bullet, angle f of 64, kind 0, speed -12 + (D>>1) + (D>>2)  (D=0: -12 ... D=8: -6, D=12: -3, D=16: 0), maker 50DDC (64)
          place = turret + table 0275FC at byte offset (f + 2) & 0x3C (16 long entries, 64ths); skipped if off screen
```

Dump check: periods 57 and 53 seen (L = 8..9), speeds (0,-8), (0,-5), (0,-3), (0,-2).

### 3.9 Who fires together (the "phases" you will see)

- Frame 30 on: pods, twin turrets, arm bursts run at once; the body is in its 448 frame rest, so the arms' rest stream runs too.
- Body attacks add to those; during an attack the arms' rest stream is off.
- At half life: rear turrets 16-19 join. When 14, 15, 20, 21 are all dead: arms unfold (360 frames), cannons 23, 24 join.
- Everything speeds up with L (time + parts destroyed).

## 4. What the second loop (word 10197A != 0) changes

The program has no extra branches; only the makers expand (50DDC/50F94 at 50D68/50F24; flag word `+0xD(a5)` bits 0x81 and `(a6)` bit 1 pick
the 3-bullet and +-8 variants, but **bit 1 of the boss record is never set here and the dumps show only the 2-bullet forms**, so these apply):

| maker | loop 1 | loop 2 |
| --- | --- | --- |
| 50DDC (64), 50F94 (256) | 1 bullet | 2 bullets, same angle, speeds s and s+6 |
| 50F86 (256) | 1 bullet | 2 bullets, angles -8 and +8, same speed |
| 50DA0 (64) | 1 bullet | 1 bullet, speed s+4 |

Per attack:

- A: speed **16** (0x100018), kind 24, so 2 bullets (16, 22) every I frames. (`ks [(24,16),(24,22)]`.)
- B: kind **35** speed -3 (0xFFFD0023); **angle step 1 a frame** (02D332 addq #1) instead of 2; counters halved (R = 1 / 2 / 3, period 4 / 5 / 6); a
  firing frame throws **two mirrored pairs**: angles b, -b, then b+1 and -(b+1) (02D350-02D37E), each call 2 bullets: **8 bullets** (speeds -3 and +3). 
- C: kind **35**; each call is 50F86 so a firing frame has 16 bullets (8 x +-8, speed -3).
- Pods (0, -12/-6/0 and +6/0/6), twin turrets (5, -6 and 0), cannons (5, -8/-2/4), rear turrets: all doubled s, s+6.
- Arm burst: speed 10 (6 + 4). Arm rest stream: speed 8, 10, ... (start 4 + 4).
- The 10197A branch in the body setup (02D2C8): B's counters are halved (02D2D0-02D2E0). Nothing else changes: same timers, same L.

## 5. What does not fit the vocabulary (the findings)

1. **Level L is the real parameter.** Every interval and several speeds are `f(L)` where L comes from time since fight start (1 per 360
   frames, restarted by any part dying) plus the weighted count of dead parts. A pattern must carry L (0..16) and `D`.
2. **Time-driven angle, not volley-driven**: B turns 2 (loop 2: 1+1) *every frame* including the 3 pause frames, so the angle of
   a volley depends on the frame count since the attack started, not on how many volleys came before. C's step grows by 1 each volley.
3. **Slow-follow facing** (rear turrets, twin turrets): the barrel is a state (`+0x1B`) that turns one 64th every 8 frames toward the
   coarse aim; the bullet angle is that state, not the aim now. Needs "track(aim, 1 step per 8 frames)" with the state kept between
   volleys (and it keeps tracking between volleys).
4. **Muzzle depends on facing** (tables 0275FC, 027B1E, 027B5E, 028058, 028098: 16 entries each of x, y).
5. **Spin states of the cannons**: +-4 per frame from 0x20, started when armed; they fire two bullets from two different muzzle tables at speeds
   -2 and -8.
6. **Gating by state**: parts become armed by boss life (< 0x11800 of 0x23000), by other parts dying, and the arms' stream by the body's attack
   phase. A pattern editor needs conditions (`life < 50%`, `part X dead`, `body resting`).
7. **A random schedule with a memory**: attack picked by `(57EBA + 57C84) & 7` through table `0 0 1 1 1 2 2 2`, same one allowed once more.
   Durations depend on the attack and on L / D (360 / 540 / 720 / 840), rests 448 (first), 128, 80, 20.
8. **A ramp that delays firing** (`+0x12`, +-8 a frame, 64 frames) unless L = 16.
9. **Burst-with-gap counters** (B: R frames fire, 3 skip) where the first count depends on a frame-counter bit (`10126F` bit 1).
10. **Arm stream**: speed +2 every 7th shot, a wait of 31, 25, 19, 13, 7, 1, then 5; reset by the boss attacking; the arm's tip offset is fixed.
11. Odd one: rear turret reload forced to 10 when `(arms done + D) == 14` (02D5D2-02D5E0).
12. All bullet makers skip when off screen (`5A2AC`: y (+8) outside 0x200..0x3C00 or x (+6) outside 0x1C00..0x5000): A, pods, twin turrets, cannons, rear turrets, arm
    bursts. B, C and the arm rest stream do not test.
13. Dump bookkeeping: `atk` is always 0 here (the boss keeps its attack in `+2(BASE)`, 0..28, not in task `+0x26`). `bchg #0,+5(a5)` after every shot
    is a toggle I did not trace (it looks like a shot-sound or sprite alternation, not part of the pattern).

Not read: the body's death sequence (02CAxx) and the effect objects (581A8 callers), the entry script data, 5A6AA (used in the timeout and the
boss-life underflow check), the spawn table's objects (02D1B6).

## 6. Addresses that prove it (every site in the brief)

| site | what | maker | proves |
| --- | --- | --- | --- |
| 02D27E | attack A volley | 50F94 | 02D244 fine aim, 02D24E rand 57FA2, 02D254-02D258 turn -1..+1, 02D25E speed from +0x1F, 02D260 kind 0x18, 02D26C loop 2 speed 16 |
| 02D326, 02D32E | B pair (loop 1) | 50F94 | 02D2F2 +2 a frame, 02D310 b, 02D326 neg, 02D314 0xFFFD0018 |
| 02D366, 02D36E, 02D37C, 02D384 | B 2 pairs (loop 2) | 50F94 | 02D332 +1, 02D36E extra +1, 02D354 0xFFFD0023 |
| 02D40E, 02D418 | C (8 / 2 x 8 bullets) | 50F86 | 02D3D8-02D3E0 spin, 02D402 -0x18, 02D40E +0x80, 02D418 +0x8C, loop count 4 (02D406) |
| 02D61E | rear turret | 50DDC | 02D5C6-02D5CE interval, 02D5F4-02D606 speed, 02D60E table 0275FC |
| 02D7AC, 02D7BE | twin turret 2 barrels | 50DDC | 02D792 0xFFFA0005, 02D79C table 027B1E, 02D7B0 table 027B5E |
| 02D948, 02D984 | cannon streams | 50F94 | 02D92A 0xFFFE0005, 02D938 table 028058, 02D966 0xFFF80005, 02D974 table 028098 |
| 02DB74 | pod spray | 50DDC | 02DB52-02DB72 table 02DB82 |
| 02E044 | arm rest stream | 50DA0 | 02DFDC-02DFFA counters, 02E02E speed, 02E02C d1 = 0x20 |
| 02E108 | arm aimed burst | 50DA0 | 02E0C6-02E0F2 7 shots, 02E0F6 0x60026, 02E0E6 aim 59A58 |

Raw-recording check: bullets by site, run r1, loop 1 (first loop), stage 3 ("0-2"): 02D27E 397, 02D326/2E 2146 each, 02D40E/18 3496 each, 02D61E 258, 02D7AC/BE 13,
02D948/84 1276, 02DB74 128, 02E044 90, 02E108 70.
