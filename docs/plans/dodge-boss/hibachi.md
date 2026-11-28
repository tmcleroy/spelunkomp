# Hibachi, the hidden last boss (second loop only, stage index 6)

Hibachi is two bosses in a row: **form 1** (object kind 185, a mothership with six pods and a core) and, after its death
sequence, **form 2** (object kind 204, the "real" Hibachi). Around them fly helper objects that fire for themselves: kind 190
(homing needles) and kind 192 (drones) for form 1, kind 207 (wall poppers) and kind 210 (shot pods) for form 2.

Everything below is read from `work/ddonpach/program.lst` / `program.bin` and checked against the volley dumps of runs r1 and r3
(loop-stage "1-6"; r2 and r4 hold no Hibachi). Addresses are ROM addresses. Where a recording's call site is quoted it is the
**return address** of the `jsr` (the first call of a run is 6 bytes before the first address quoted), which is what the
dump's site list uses.

## 0. Conventions for reading the rest

**Makers.** All of Hibachi's own fire goes through the two *second-loop* makers, never through 50DDC/50F94, so none of the
loop-2 doubling/tripling in the brief applies to it (it only exists in loop 2 anyway):

| entry | angle | note |
|---|---|---|
| `054EF4` | fine, 256 to the turn | bit 19 set in the bullet; the recording's `fine` rows |
| `054E22` | coarse, 64 to the turn (dumps show it x4) | same table, same kinds |

Both take d0 = (speed addend << 16) | kind, d1 = angle, d2 = place (x high word, y low word, 64ths of a pixel), d3 = 0 (no
parent; nothing in Hibachi uses a parent). Kind table `054FA8` (64 pointers to 16-byte rows). The bullet's speed is the kind's base
(row word 12) **plus** the addend: base is 22 for every kind used here except kind 12 (24) and kind 16 (16). Kinds 24, 27 and 31
are the small sprite (row size 0x101), 0, 1, 5, 12, 16, 17, 18, 21, 33, 35, 38 the large (0x202). Below, "speed -9" means addend -9
(final 13 for base 22).

**Axes and angles.** x (high word) is the board's long axis, 0..~320 px; angle 0 is +x, "up the screen"; a quarter turn (64/256,
16/64) is +y, the screen's right; 128/256 is -x, toward where the ship lives (the ship's x is 30..276 px in the recordings,
the boss sits at x 150..300). A bullet moves along (cos, sin) of angle*360/256 in (x, y). Aim routines return the direction *to* the
ship.

**Time.** The code counts game ticks. The recording counts video frames, and Hibachi's bullet load makes the game slow down, so
a recorded gap is 1.0 to 1.5 times the tick period (e.g. a 9-tick timer shows as 9..12 frames, a 3-tick one as 4..6).
Periods below are ticks. A timer that is "reload R" counts R, R-1, ..., 0 and fires on the borrow, so its period is R+1 ticks
(`subq.b #1,x ; bcc skip ; move.b reload,x`).

**Part bodies.** A boss body is a run of 0x20-byte part records (task `0x6(a5)` points at the first). Form 1's run starts at
`1054F6`; form 2's at `1054F6 + 0x400 = 1058F6`. Part k is at body + 0x20*k. A part is alive while bit 7 of its byte +1 is
clear (`tst.b 1(a6); bpl` takes the live path; 037664), destroyed or dying when it is set (`move.w #$8080,(a6)` at 037E02, 038076).
Parts follow the boss by `move.l 0x6(a4),0x6(a6)` (a4 = body base, 0x6(a4) = the boss's place); a part's emitter place is
therefore the boss's place + a fixed offset, so all emitters move with the boss.

**First delays.** Each emitter has a countdown that is only decremented while its state runs, so a state begins quietly (up to 241
ticks) and the countdowns are re-armed when a state ends; the values are given with each state.

Recording references (r3 is the more complete run: it reaches the core and the final belly phase): form 1 fires from frame
198561 (r1) / 200359 (r3); form 2 from 208568 (r1) / 215937 (r3).

## 1. Overview of what chooses the attack

| | form 1, kind 185 | form 2, kind 204 |
|---|---|---|
| attack chooser | word `0x2(a6)` of the body (`1054F8`), values 0, 4, 8, C, 10; jump table `036BC4` (called at `036BBC`) | word `0x22(a5)` of the task, same values; jump table `03EB5A` (called at `03EAE6`) |
| life | long `0x16(a5)`, starts `0x38000` | long `0x16(a5)`, starts `0x46000` |
| what moves the chooser | timers, a random draw, and two body words (see 2.2) | timers only; plus life < `0x1C000` forces 0x10 |
| ends | life < 0 (or parts) -> death sequence -> kind 204 is spawned (036A5C..036A9A: kind 0xCC, d3 = 0xCC800000) | life < 0 -> explosion, task ends (`03E93C`..) |

Form 1 starts firing 20 ticks after the task starts (`0x1E(a5)` = 0x14 at 0365A6, tested at 037792, 0379F4, 037B9C, 037E60,
038276 ... for every emitter). Form 2 waits 0x78 ticks (initial `0x1E(a5)`, table `03E3A0`) and then its first volleys
come after the per-emitter delays in 3.1.

## 2. Form 1 (kind 185)

### 2.1 Parts (body at 1054F6)

Weapon parts and the fields their code uses. "angle byte" = byte +0x1F of the part. At start (`03654C`-`036592`) parts 1..5
take their angle byte from consecutive bytes of the random table `057DBA` (random index = `101272`), part 6 gets
(part 3's) + 0x60 + one more random byte.

| part | record | role | fires in state | emitter place (offset from boss place, 64ths px) | timer (countdown, reload) | HP word +0x18 |
|---|---|---|---|---|---|---|
| 1 | 1054F6+0x20 | "wing spinner" left | 0 | (+0x0DC0, -0x1E00) = (+55 px, -120 px) | 0x3A2, 0x3A3 (reload 8 -> period 9) | 0x7000 |
| 2 | +0x40 | "twin pair" left | 0 | A (-0x0200, -0x0F00), B (-0x0580, -0x11C0) | 0x3A4, 0x3A5 (reload 5 -> 6) | 0x6000 |
| 3 | +0x60 | "fan" upper | 4 | (+0x0780, -0x0F40) = (+30 px, -61 px) | 0x3AA, 0x3AB (reload 6 -> 7) | 0x7FFF |
| 4 | +0x80 | "wing spinner" right | 0 | (+0x0DC0, +0x1D80) = (+55, +118 px) | 0x3AC, 0x3AD (reload 8 -> 9) | 0x7000 |
| 5 | +0xA0 | "twin pair" right | 0 | A (-0x0200, +0x0E80), B (-0x0580, +0x1140) | 0x3AE, 0x3AF (reload 5 -> 6) | 0x6000 |
| 6 | +0xC0 | "fan" lower | 4 | (+0x0780, +0x0E80) = (+30, +58 px) | 0x3B0, 0x3B1 (reload 6 -> 7) | 0x7FFF |
| 7 | +0xE0 | belly cannon | 10 | boss place - (0x0480, 0) (the `d2 = 0x6(a6) - 0x4800000`) | 0x1F byte | |
| 11 | +0x160 | centre unit | 8 and 0 | (+0x0480, 0) from its own place | see 2.5 | |

The timers' countdown/reload bytes live in part 29 (0x3A0..0x3BF), copied from the part table at `0365CC` (decoded with the
loader at `059F28`): initial countdown/reload are 3A2:D0/08, 3A4:90/05, 3AA:A0/06, 3AC:D4/08, 3AE:93/05, 3B0:A0/06. The first volley
of a part is thus 0xD0/0x90/0xA0/0xD4/0x93/0xA0 ticks after the state first runs it. Each firing state's end re-arms them (see 2.2).

A destroyed pod costs the boss life (`subi.l #$1200,0x16(a5)` part 4 at 037DF4; `#$1000` part 5 at 038068; `#$1400` part 6 at
038210) and sets its flag 0x80. Hits on the core parts (7, 10 and 24, 25, 27: `038476`-`0384B4`: damage = 0x27FFB minus the five
HP words, each reset to 0x7FFF) take life directly.

### 2.2 The state machine (word 0x2(a6) of the body)

```
0  (036BD8)  "wing spinners": parts 1, 2, 4, 5 fire; part 11 may fire its ring (2.6)
             waits until word 0x17E(a6) == 8, or counter 0x350(a6) (first 0x2EE = 750 ticks, later reloaded to 0x5DC) runs out
             then arms the firing timers: 0x3A2=0x78, 0x3A4=0x58, 0x3AC=0x7C, 0x3AE=0x59, 0x17C=0x78 (036BE6..036C04) -> state 4
4  (036C12)  "fan sweeps": parts 3, 6 fire (2.4).  If body word 0x16(a6) == 0 it jumps straight to 8 (it is 2 at the start).
             first time: switches the two fan parts on (flags 0xE1 on 0x60, 0xC0). Counter 0x352 (reload 0x4B0 = 1200 ticks).
             at 0: 0x3AA = 0x3B0 = 0x40 (first delay 64), flips bit 0 of 0x61 and 0xC1 (the fans' sweep directions) -> state 8
8  (036C72)  "streams and needles": part 11 fires 3 sweeping streams and launches needles (2.5).
             counter 0x354: first expiry (it starts at 1, i.e. at once) arms 0x3BA/0x3BB/0x3BC = 0x60/0x68/0x70 (first delays 96, 104, 112)
             and 0x354 = 0x258 (600 ticks); second expiry (036CC8) rearms 0x3BA.. = 0x68/0x70/0x78, 0x354 = 0x58 and picks:
               random negative                      -> state C (0x3BD = 4)
               else 0x3BD counts 4 -> 0             -> state C
               else if word 0x17E == 8              -> state 4, or 0x10 when body word 0x16 == 0
               else                                 -> state 0
C  (036D24)  "core open": the boss runs to (0x5C00, 0x1E00) (code 037060.., 0371xx), opens the core (bit 2 of 0xA(a6)) and fires the
             rotating cross (2.7). Leaves when 0xA(a6) clears: 0x17E == 8 -> 4 (0x16 != 0) or 0x10 (0x16 == 0), else 0 (0x350 = 0xAF)
10 (036DF2)  "belly": part 7 fires the aimed pairs and spawns kind 192 drones (2.8). 0x358 = 0x384 (900 ticks) twice, then ->
             4 / 8 / 0 by the same 0x17E / 0x16 tests
```

In the recordings the order is 0, 4, 8, then 0 (short) / 4 / 8 repeated, one C, and in r3 a 0x10 after the first C. r3: state 0
200359-201071, 4 201234-202343, 8 202437-202997, C 203426-203713, 0 (short), 4 203982-205214, 8 205393-205950, 0 (short),
4 206190-207416, 8 207594-208147, 4 208214-209405, 8 209584-210154, 4 210222-211435, 8 211613-212164, C 212473-212785, 0x10
212995-214190, 8 214392-214943, death, form 2 from 215937. (Durations match the counters at the observed slowdown: state 4 is
0x4B0 = 1200 ticks, state 8's streams run 600 ticks less the 96-tick first delay: 103 volleys of 5 = 515 ticks.)

Open items: word 0x17E(a6) (part 11's +0x1E; table value 0, "== 8" is tested at 036BD8, 036D00, 036DAE, 036E2A) and body word 0x16(a6)
(initially 2) are not written by anything in this routine; they decide which state follows, and state 0's ring volume (2.6) depends
on the first. The recordings show the ring's count 3, 4, 4, 4, 7 as parts die; I could not find the writer.

### 2.3 Wing spinners (parts 1 and 4): state 0

`0377A2`-`0378B0` (part 1, sites 0377D2-0378B0) and `037E78`-`037F86` (part 4, sites 037EA8-037F86).

```
attack:   emitter P1 at boss + (+0x0DC0, -0x1E00); emitter P4 at boss + (+0x0DC0, +0x1D80)
timeline: volley every 9 ticks (P1 and P4 independent), while the main state is 0 and 0x1E(a5)==0
volley:   base = the part's angle byte (0x1F), then base -= 5 (P1, at 0378B0) or base -= 6 (P4, at 037F86), a spin
          shape = 4 "streaks" at base, base+64, base+128, base+192, each a line of 5 bullets (all the same angle) with the
          speed addend ramping: arms 0 and 2: 2, 5, 8, 11, 14 ; arms 1 and 3: 14, 11, 8, 5, 2
bullet:   kind 17 (large), fine angle (054EF4), d0 = 0x0002..0x000E0011 per call, i.e. addends 2,5,8,11,14
```

Verified (b1): `ang [173 x5, 237 x5, 45 x5, 109 x5]`, `spd [2,5,8,11,14,14,11,8,5,2,2,5,8,11,14,14,11,8,5,2]`, base 173, 168, 163, ...
(-5). P4: base 69, 63, 57 (-6). The two spinners run in parallel, so up to 40 bullets a period; the rotation sense is the same
(angles fall), 5 and 6 per volley, so the 4-arm patterns beat slowly against each other.

### 2.4 Twin pairs (parts 2 and 5): state 0

`037A04`-`037A7E` (sites 037A3C, 037A46, 037A74, 037A7E) and `0380DA`-`038154` (sites 038112, 03811C, 03814A, 038154).

```
attack:   P2 pair A at boss + (-0x0200, -0x0F00), pair B at boss + (-0x0580, -0x11C0)
          P5 pair A at boss + (-0x0200, +0x0E80), pair B at boss + (-0x0580, +0x1140)
timeline: every 6 ticks while state 0
volley:   base a = the part's angle byte; a += 4 (P2, 037A7E) or a -= 5 (P5, 038154) after each volley
          4 bullets: pair A at a and a+128 from the A place; pair B at a+64 and a+192 from the B place
bullet:   kind 0 (large), fine angle
          P2: pair A speed -9, pair B speed -5      P5: pair A speed -11, pair B speed -9
```

Verified: P2 `[200, 72, 8, 136]` then `[204, 76, 12, 140]` (+4), speeds `[-9,-9,-5,-5]`; P5 speeds `[-11,-11,-9,-9]`.
Each pair is two opposite bullets, i.e. a 4-spoke "plus" made of two offset diameters, turning at +4 or -5 per volley.

### 2.5 Fans (parts 3 and 6): state 4

Part 3: `037BAE`-`037D0C` (sites 037BE6-037C64 first fan, 037C8E-037D0C second fan). Part 6: `038284`-`0383E2` (sites 0382BC-03833A
first fan, 03836A-0383DA second fan, not in the recordings).

```
attack:   emitter F3 at boss + (+0x0780, -0x0F40); emitter F6 at boss + (+0x0780, +0x0E80)
timeline: volley every 7 ticks, only while the main state word is exactly 4 (cmpi.w #4,0x2(a4) at 037BA4 / 03827A)
volley:   fan X: 10 bullets, angles b3, b3+4, ... b3+36 (a fan(10, spacing 4)), speed addends 3, 1, -1, -3, -5, -7, -9, -11, -13, -15
                (a speed gradient: the lowest-numbered angle is fastest)
          fan Y: 10 bullets, angles b6, b6+4, ... b6+36, speed addends -11, -9, -7, -5, -3, -1, 1, 3, 5, 7 (the opposite gradient)
          F3 always fires fan X with b3 (part 3's angle byte) and, once part 6 is destroyed (byte 0xC1(a4) negative), also fan Y
          with b6 (the angle byte of part 6, 0xDF(a4)); F6 always fires fan Y with b6 and, once part 3 is destroyed
          (0x61(a4) negative), also fan X with b3 (0x7F(a4)).
sweep:    b3 += 8 or -= 8 each F3 volley (bit 0 of part 3's byte +1 selects: set = +8), b6 += 7 or -= 7 (bit 0 of part 6's +1).
          When the partner is dead the survivor steps both bytes (it updates the partner's 0xDF / 0x7F by 7 or 8 itself:
          037D14/037D1A for part 6's byte, 0383EA/0383F0 for part 3's), so the sweep rate is unchanged. Both bit-0 flags flip together at each state-4 -> 8 change (036C58, 036C5E).
bullet:   kind 27 (small), fine angle
```

Verified (b1): fan X `ang [88,92,...,124] spd [3,1,-1,...,-15]` then base 80, 72, ... (-8); fan Y `ang [215,...,251] spd [-11,-9,...,7]` then
208, 201, ... (-7); in b1 the second fan of part 3 starts at frame 203295, 13 frames after part 6's last volley (203282).

### 2.6 Centre unit (part 11): states 8 and 0

State 8, `03884C`-`03898E` (sites 038896-0388A8, 0388C8-0388DC, 0388F4-038908, spawn at 038914-038968); state 0 ring `0389E2`-`038A7E`
(sites 038A58-038A6C). Part 11's place is the unit anchor (it drifts slowly; recorded x 15.3k-16.3k, y 10.9k-12.4k in state 8 but
anywhere 3.9k-17.5k in the state-0 ring).

```
state 8, three sweeping streams from place + (0x0480, 0):
  S1: every 5 ticks (0x15(a6), reload 4):  base1 = byte 0x16(a6)
        3 bullets: base1-5, base1-10, base1+5   speed -5, kind 33 (large), fine
  S2: every 7 ticks (0x3BA(a4), reload 6):  base2 = byte 0x17(a6)
        3 bullets: base2-9, base2+9, base2+18   speed -3, kind 35 (large), fine
  S3: every 9 ticks (0x3BB(a4), reload 8):  base3 = byte 0x18(a6)
        3 bullets: base3, base3-14, base3+14    speed 0, kind 24 (small), fine
  the bases are table sweeps (triangle waves), advanced independently of the fire:
    base1 = table 039B46[0x3B4(a4)] 32 bytes, 136..192 up in 8s, down to 64, up to 128; index steps every 3 ticks
    base2 = table 039B66[0x3B6(a4)] 32 bytes, 122..80 down in 6s, up to 176, back to 128; steps every 5 ticks
    base3 = table 039B86[0x3B8(a4)] 56 bytes, 132..184 up in 4s, down to 72, up to 128; steps every 9 ticks
  each table index counts down and wraps (0x1F, 0x1F, 0x37)
  S4: every 9 ticks (0x3BC(a4), reload 8): launch 2 needles (kind 190), see 2.9.
```

Verified: S1 `[155,150,165]` (base 160), `[139,134,149]` (base 144), gaps 5-6 frames; S2 `[107,125,134]`; S3 `[88,74,102]`, `[84,70,98]`.
S1 streaks 8 apart in base per step, S2 6, S3 4, each unaware of the others, and with S4's needles they make state 8 the dense
phase (96..600 ticks).

```
state 0, "stacked aimed fans" (only when word 0x1E(a6) of part 11 is neither 0 nor 8; n = that word):
  every (0x1D(a6)+1) = 129 ticks (0x1C(a6)/0x1D(a6) = 0x80/0x80 in the table), place = part 11 + (0x0480, 0)
  base = fine aim at the ship from that place (059C58)  [no ship: nothing]
  repeat 2n times:  r = random byte (057C96);  c = base + r      (note r is a full byte: the 3 bullets' common centre is jittered by up to 255/256)
        3 bullets at c-14, c+14, c   speed -6 + 3*loop index (-6, -3, 0, 3, ... 15, i.e. +2 then +1 added per pass), kind 24, fine
```

(Order of fire in a loop is c-14, c+14, c; the speed rises by 3 per loop; `038A66 addi.l #$20000,d0 ; 038A74 addi.l #$10000,d0`.)
Verified: n=3: 18 bullets `[97,125,111, 90,118,104, ...]` speeds -6..9; n=4: 24 bullets; n=7: 42 bullets. Not aimed in the useful sense:
each loop has its own random jitter, so the stack is a random-ish spray at the ship's side with a speed ramp.

### 2.7 Core open (state C): rotating cross

`037398`-`0373BC` (sites 03739E, 0373A8, 0373B2, 0373BC; in r3 only). The core part 24 (0x300) and its neighbours open (the code at
037060-037216 flies the body to (0x5C00, 0x1E00), then at arrival sets bit 2 of 0xA(a6): flags 0xE1 on 0x300, 0x320, 0x360; 0x32(a5) = 3).

```
attack:   emitter at core place (0x6(a6) of part 24) + offset from table 039ACE: 30 long offsets (0x0700FC80, 0x0780FD00, ...), index 0x30(a5)
timeline: every tick (no timer) while the core is open (0x396(a4) == 0): first opening 0x8000 -> 128 ticks, later openings 4 ticks
          (0x1C(a6) set at 0372CC / 0372DA); the burst ends by 0x396(a4) = 1 (0372F6, 037368)
volley:   4 bullets at a, a+64, a+128, a+192 (a "plus"), a = byte 0x31F(a4) (starts 0), a += 7 per volley
          the emitter's place index 0x30(a5) steps -4 per volley and wraps from 0 to 0x74 (so the emitter slides along the table)
bullet:   kind 12 (large), speed -20 (final 24 - 20 = 4: very slow), fine
```

Verified (b3): `[0,64,128,192]`, `[7,71,135,199]`, ... speed -20 for 191 volleys per opening (203426-203713, 212473-212785), one volley a frame.

### 2.8 Belly (state 0x10): aimed pairs and kind 192 drones

`0385BA`-`03870E` (part 7, sites 03862E-038678; r3 only).

```
timeline: every 7 ticks (0x1F(a6) reload 6), or every 2 ticks (reload 1, 0385F2) when jsr 0358D0 (the larger of the two ships' x)
          >= 0x4400, while the main state word is exactly 0x10
place:    part 7 place - (0x0480, 0)
volley:   aim = coarse aim at the ship from part 7 (059A58; carry set = skip)
          (a) every other volley (the bchg at 038616): 1 bullet at aim + random(byte >> 1 arith), i.e. a random direction,
              speed -10, kind 17 (large), coarse (054E22)
          (b) every volley: w = table 0394EC[0x35B(a4)] (a triangle -10..10 in steps of 1, 40 entries; index -2 per volley,
              wraps to 0x4E), then 4 bullets at (aim+w)+10, +13, -10, -13 (a double narrow pair flanking the line to the ship),
              speed -8, kind 0 (large), coarse
spawn:    every 21 ticks (0x1B(a6), reload 0x14) 4 drones (kind 192): speeds/headings from the bytes at 038686..0386F6:
              (0x40,0x10), (0x30,0x16), (0x40,0x30), (0x30,0x2A) first velocity; (0x14,0x20), (0x10,0x21), (0x14,0x20), (0x10,0x1F)
              second velocity (speed index, 64-dir heading, applied through 0596C4/05AAFA)
```

Verified (b3): `[104 (kind 17, -10), 156, 168, 76, 64 (kind 0, -8)]` alternating with the 4-bullet volley `[152,164,72,60]`.

**Kind 192 drone** (init `03A62C`, frame `03A69A`; sites 03A770-03A7A8): starts at the boss place (+10 px in x), flies at its first
velocity, after a countdown (byte 0x1F(a6)) switches to its second, and every 8th tick (`101270 & 7 == 0`, 03A742) fires, from its
own place - (0x0200, 0): alternately (`bchg 0x1D(a6)`) 3 bullets at fine angles 64, 128, 192, or 4 bullets at 32, 96, 160, 224,
speed -16, kind 38 (large), fine. It dies when hit (flag 0x80, HP word -0x80) or off the board.

### 2.9 Kind 190 needles

Launched by part 11 in state 8: two per launch every 9 ticks, speed byte 0x14, headings aim-14 and aim+14 (64-dir, aim =
`059A36` from (part 11 place + 0x0480, 0), 0x20 if no ship), starting at the boss place + 10 px x (init `039E9C`; frame `039F2E`).
It moves by `0596C4(speed, heading)` per tick, steers its heading toward the ship (`059A5E`) by 1/64 turn each second tick (03A026..03A03E, bit 0 of
`10126F`), and every 16 ticks (`10126E & 15`, at 039FD2) fires **one** bullet along its heading:

```
bullet:   from its place, angle = its heading (64-dir, 054E22), speed -2, kind 5 (large)
```

Site 039FF8; verified `n 1` (one bullet per needle per volley). This is an aimed-by-trajectory trail, not an aimed
shot: the bullets fly in the needle's heading at that moment, so they hang in a curve behind a needle that turns toward the ship.

## 3. Form 2 (kind 204)

### 3.1 Chooser and timers

State word `0x22(a5)` (initial 0 from table `03E3A0`). Counters in the task (a5): 0x24 = 600 (state 0), 0x26 = 480 (state 4), 0x28 = 720
(state 8), 0x2A = 1200 (state C); end of each state loads the next state and the delays below. When life (`0x16(a5)`) < `0x1C000`
(40 % of 0x46000) the word is forced to 0x10 every tick (03E9FA..03EA02) and stays.

```
0 (03EBE0, 600 ticks)  G1+G2 rings (3 ticks), G3 down-fan (5 ticks), G4 exhaust (4 ticks)         -> 4 (03ED7A)
4 (03ED82, 480 ticks)  spawn kind 207 wall poppers, 4 per 6 ticks                                    -> 8 (03EE42)
8 (03EE4A, 720 ticks)  half ring / aimed fan alternating (7 ticks), two bullets (4 ticks)           -> C (03F002)
C (03F00A, 1200 ticks) wiper fan (5 ticks), kind 210 shot-pod fans (161 ticks)                       -> 0 (03F1BE)
10 (03F20E, forever)   bow-tie volleys (5 ticks)
```

First-visit delays (in ticks, from part 3's table row at `03E40E`; the quiet part of each state's start): G1/G2 0xC0 = 193 (0x62(a4)),
G3 and G4 193 (0x2C, 0x2D: 0xC0C0), state 4 spawn 0x40 = 65 (0x63), state 8: 0xE0 = 225 (0x64) and 0xE0 (0x6C), state C: wiper 0xC0 = 193
(0x65), kind-210 spawn 0xF0 = 241 (0x70); state 0x10 0xF0 (0x71). Re-armed to 0x80 / 0x40 / 0xE0 / 0x90, 0xC0 at each state end
(03ED68, 03ED6E, 03EE36, 03EFF0, 03EFF6, 03F1A6, 03F1B2).

Recorded (r3): state 0 215937-216541, 4 216612-217381, 8 217534-218272, C 218480-219502, then 0 219633, 4, 8, C 222187-223256, 0 223385, 4 224184, then
0x10 224995-231042 (r1: 0 208568, 4 209267, 8 210173, C 211118, 0 212264, ..., 0x10 216874-222341). The tick sums match (state 0: 193 + 136 volleys of 3 = 601).

Boss movement (not firing): `03E7B6`-`03E86C`: heading byte 0x1B(a6) steers to a random target (0x5A(a6), 0x5C(a6)) = (0x4200 + rand/2, 0x1E00 + rand) re-picked
when within 0x800; speed byte 0x1A(a6) 8..20. The boss place is the emitter place for everything below.

### 3.2 State 0: two counter-rotating rings, a down fan and an exhaust

All from the boss place `0x6(a4)`, d3 = 0, no offset.

```
G1 (03EBE0..03EC6E, every 3 ticks: 0x62(a4) reload 2): ring of 6, spacing 42/256, speed -1, kind 0 (large), fine
     base = byte 0x6A(a4) (starts 0), base += 6 per volley (clockwise in angle)           sites 03EC04..03EC36
G2 (same trigger):                  ring of 5, spacing 51/256, speed +3, kind 17 (large), fine
     base = byte 0x6B(a4) (starts 0x80), base -= 11 per volley (the other way)            sites 03EC46..03EC6E
G3 (03EC78, every 5 ticks: 0x2C(a5) reload 4): fan of 10 at coarse angles 23, 25, ..., 41 (step 2/64 = 8/256: 92..164 of 256, centred on 32 = down), 
     speed +6, kind 38 (large), coarse (054E22), fixed angles (plus a sound)             sites 03EC98..03ECE0
G4 (03ECFA, every 4 ticks: 0x2D(a5) reload 3): "exhaust": h = the boss's own heading byte 0x1B(a4) (64-dir); 6 bullets at
     coarse angles h+32, h+33, h+31, h+34, h+30, h+35 (fan(6, alternating), pointing backwards from the direction of travel)
     with speed addends 0, 5, 10, 15, 20, 25, kind 24 (small), coarse                    sites 03ED1C..03ED62
```

Verified: G1 `[0,42,84,126,168,210]` then +6; G2 `[128,179,230,25,76]` spd 3 then -11; G3 `[92..164]` spd 6; G4 `[176,180,172,184,168,188]` spd `[0,5,10,15,20,25]`.
G1 and G2 are a 11-bullet burst every 3 ticks, so the phase is two moiré rings with one gap that opens and closes as the 6-ring and 5-ring phase.

### 3.3 State 4: wall poppers (kind 207)

`03ED82`-`03EE30`. Every 6 ticks (0x63(a4), reload 5, after a 65-tick first delay) the boss spawns 4 objects of kind 207 at its place:
(speed byte, 256-heading) = (rand>>1 + 0x20, random byte from 057DBA), then three more with a fresh random speed and heading + 0x40, + 0x80, + 0xC0 (a ring
of four at random speed each): up to ~276 objects in the state.

**Kind 207** (init `03F306`, frame `03F354`; sites 03F378-03F446): moves in a straight line (velocity from `059980(speed, heading)`) and, at the first board edge, fires
one fan and is removed (`jmp 053BCA`). The four walls (position + velocity, words 0x6/0x8):

| edge | test | fan | speed | kind |
|---|---|---|---|---|
| x < 0x200 (bottom) | 03F35E | 3 bullets at coarse 0, 8, 0x38 (= 0 and +-45 degrees, up) | -15 | 33 (large) |
| x >= 0x4E00 (top) | 03F38C | 5 bullets at coarse 0x18, 0x1C, 0x20, 0x24, 0x28 (centred on down, spacing 16/256) | -11 | 33 |
| y < 0x200 (left) | 03F3D0 | 5 bullets at 8, 0xC, 0x10, 0x14, 0x18 (centred on 16 = screen right) | -13 | 33 |
| y >= 0x3A00 (right) | 03F40C | 5 bullets at 0x28, 0x2C, 0x30, 0x34, 0x38 (centred on 48 = screen left) | -13 | 33 |

All fired from the object's own place, coarse (054E22). So the attack is a field of random pop-fans, each aimed inward from the wall it touches.

### 3.4 State 8: half ring / aimed fan, and two flank bullets

`03EE4A`-`03EFEA`.

```
H  (every 7 ticks, 0x64(a4) reload 6, alternating with a toggle at bit 0 of 0x3F(a4), plus a sound):
   odd:  half ring: 17 bullets at coarse 0x10, 0x12, ..., 0x30 (16..48 of 64, step 2/64 = 8/256), speed -8, kind 27 (small), coarse   (sites 03EE90..03EF10)
   even: aimed fan: fine aim at the ship from the boss place (059C58; skip if none), then 16 bullets at aim-64, aim-56, ..., aim+56
         (spacing 8/256), speed -4, kind 27 (small), fine; then the ship toggles (eori 0x5(a5))      (sites 03EF3A..03EFBA)
F  (every 4 ticks, 0x6C(a4) reload 3): 2 bullets at coarse 0x18 and 0x28 (96 and 160 of 256), speed 14, kind 16 (large, base 16), coarse   (sites 03EFE2, 03EFEA)
```

### 3.5 State C: wiper fan and shot-pod fans (kind 210)

`03F00A`-`03F1A0`.

```
W (every 5 ticks, 0x65(a4) reload 4; place = boss place - (0x1000, 0) i.e. 64 px lower on x):
   base = table 03F1C6[0x6E(a4)] (36 words: 200 up to 218, back to 200, down to 182, up to 198; index steps -2, wraps to 0x46)
   fan of 7: base, base-24, ..., base-144 (spacing 24/256), speed -1, kind 31 (small), fine     (sites 03F042..03F07E)
P (every 161 ticks, 0x70(a4) reload 0xA0, after a 241-tick first delay): coarse aim from the boss place (059A36), a = aim - 16;
   14 kind-210 pods at the boss place - (0x0A00, 0), the first with (speed byte 6, heading a), then each +1 speed and +4 heading
   (so 14 pods, speed 6..19, headings aim-16 .. aim+36 in 64ths of a turn) (03F0C6-03F194)
```

**Kind 210** (init `03F716`, frame `03F772`; sites 03F80C-03F82C and 03F86C-03F878): a pod flying a straight line by `0596C4(speed, heading)`. The first time it is
inside the board (flag 0x1F(a6), tested at 03F7CA-03F7E6) it fires a **5-bullet aimed fan**: fine aim at the ship from its place - (0x0200,0)
(059C66), speed -8, kind 1 (large), angles aim, aim+2, aim+4, aim-2, aim-4. When it is hit it fires **2 bullets** at the fine aim,
speed -12 and speed -10, kind 21 (large), then is removed (03F84C-03F86C). Leaving the board silently removes it.

### 3.6 State 0x10: bow tie (the longest phase)

`03F20E`-`03F2AC` (sites 03F25C = 1 centre bullet, 03F272-03F294 = the 28 others).

```
attack:   emitter at boss place - (0x1000, 0) (64 px lower on x), fixed
timeline: every 5 ticks (0x71(a4) reload 4) from 241 ticks after the state starts, until the boss dies
volley:   base b = byte 0x73(a4) (starts 0x80), b += 5 per volley
          1 bullet at b (speed 4, kind 18 large)
          then for k = 1..7:  at b+k*s, b+k*s+128, b-k*s, b-k*s+128  (28 bullets), all speed 4, kind 18, fine
          s = word table 03F2AE[0x74(a4)] (44 words: 5 x6, 6..24, 24..6; index -2 per volley, wraps from 0 to 0x56): the spacing grows and shrinks
```

Verified (b1/b3): 29 bullets `[128,133,5,123,251,138,10,118,246,...]` with s=5; at s=6: `[158,164,36,152,24,170,42,...]`; 6-8 frames per volley, 968 volleys.
A "bow tie": four rays of 7 spaced bullets fan out around b and b+128, rotating at +5 per volley, the spread breathing between 5 and 24.

## 4. Loop 2 and the harder flags

Hibachi exists only in the second loop. None of these routines reads the loop word (`10197A`), the rank word, or a harder flag
(`10E73E`, `101978`): I searched the listing of 036200-03A000, 03A62C-03A7C0 and 03E2DE-03FA00 for 10197A, 10E73E, 101978, 10176C, 10E736 and 10E738, and read the code of every site. The global words they read are 1017BE / 101764 / 10E878 / 10C976 (bomb out / stage over, the makers' gate), `10E736` (form 1 is dead:
set at 03851A) and `10E738` (form 2 dead: 03E300), and `10176C` inside the makers (pool size 0x30 or 0x1D records * 6).
So **every number above holds for all players**; what differs per run is the recorded slowdown, the random draws (initial
angle bytes at 03654C, the ring's random jitter, the spawn speeds), and the ship's place (aims, 0x4400 test).

## 5. What does not fit the vocabulary

* **Emitters that are objects.** Kind 190 (a homing needle that fires along its own heading), kind 192 (a drone with two velocities that fires alternating
  3-/4-ray stars), kind 207 (a straight mover that fires a fan *and vanishes at a wall*) and kind 210 (a mover that fires a fan the
  first time it is on the board and two bullets when shot) are tasks of their own: "emitter = object with a path, volley = on an event". A volley
  dump alone cannot say where they are; the generator needs the spawn rules (2.9, 2.8, 3.3, 3.5) and a movement model (`0596C4`/`059980` with the velocity table `05AAFA`).
* **Partner takeover.** Parts 3 and 6 fire the *other's* fan with the other's angle byte when it is destroyed (2.5), which makes the pattern depend on
  the player's progress, not on time.
* **Angle sources.** (a) a part's own spin byte, stepped per volley by a signed constant that a state change flips (2.5); (b) a triangle-wave **table** index
  per emitter (2.6, 3.5, 3.6, 2.8) - note the table sweeps are advanced on their own timer, not by the fire; (c) the boss's own **heading** + 32 (3.2 G4)
  - fire away from travel; (d) a needle's heading (2.9); (e) aim plus a full random byte (2.6 ring, 2.8 single), i.e. random.
* **Stacked repeats** (2.6 ring): the volley repeats 2n times in one tick with +3 speed per pass and a fresh random jitter per pass; n is a body word nobody in these
  routines writes.
* **Nested bursts and durations decided by the player**: state 0x10 of form 1 is reached only when two body words have particular values (2.2).
* **Unresolved**: who writes word `0x17E(a6)` (part 11's +0x1E) and body word `0x16(a6)`; the launch speed units (speed bytes index the velocity table
  at `05AAFA`, they are not linear); when kind 192 switches from its first to its second velocity (the countdown byte 0x1F(a6) comes from its init row at `03A678`; the spawn also writes a byte at task +0x14 = 0xC0 or 0x40 that may be it).

## 6. Index of call sites (return addresses) to attacks

| sites | object | attack |
|---|---|---|
| 0377D2-0378B0 | 185 | 2.3 wing spinner, part 1 (4 streaks x 5, base -5) |
| 037A3C-037A7E | 185 | 2.4 twin pairs, part 2 (4 bullets, base +4) |
| 037BE6-037C64, 037C8E-037D0C | 185 | 2.5 fan X (10, speed gradient 3..-15), fan Y (10, -11..7) at part 3's place |
| 037EA8-037F86 | 185 | 2.3 wing spinner, part 4 (base -6) |
| 038112-038154 | 185 | 2.4 twin pairs, part 5 |
| 0382BC-03833A | 185 | 2.5 fan Y at part 6's place (the fan X copy at 03836A.. never recorded) |
| 038896-0388A8, 0388C8-0388DC, 0388F4-038908 | 185 | 2.6 three streams |
| 038A58-038A6C (and the loop from 038A46) | 185 | 2.6 stacked aimed 3-fans |
| 03739E-0373BC | 185 | 2.7 rotating cross (r3) |
| 03862E-038678 | 185 | 2.8 single + double pair (r3) |
| 03A770-03A7A8 | 192 | 2.8 drone stars (r3) |
| 039FF8 | 190 | 2.9 needle trail |
| 03EC04-03EC36, 03EC46-03EC6E | 204 | 3.2 G1 6-ring, G2 5-ring |
| 03EC98-03ECE0 | 204 | 3.2 G3 down fan |
| 03ED1C-03ED62 | 204 | 3.2 G4 exhaust |
| 03EE90-03EF10 | 204 | 3.4 half ring |
| 03EF3A-03EFBA | 204 | 3.4 aimed fan of 16 |
| 03EFE2, 03EFEA | 204 | 3.4 flank pair |
| 03F042-03F07E | 204 | 3.5 wiper fan of 7 |
| 03F25C, 03F272-03F294 | 204 | 3.6 bow tie |
| 03F378-03F388, 03F3A6-03F3C6, 03F3EA-03F40A, 03F426-03F446 | 207 | 3.3 edge fans (bottom 3, top 5, left 5, right 5) |
| 03F80C-03F82C, 03F86C-03F878 | 210 | 3.5 entry fan of 5, death pair |

Not covered here: kind 209 (`03F4B8`, one coarse call at 03F61C, never seen in the dumps) and kind 187/205 helpers; kind 204's death sequence (03E8B2-03E9C2).

## 7. How a generator can be driven from this (summary)

Form 1 is a set of independent emitter parts, each with its own period, base-angle step and (for the centre unit) table sweep, enabled by a five-state program; form 2
is five states of fixed emitters at the boss place plus two kinds of helper object. Both forms use the same shapes: ring(n, spacing), fan(n, spacing) with a speed
gradient, 4-arm streak(5 speeds), opposite pairs, bow tie (4 rays x 7), aimed fan, and a random spray. Counts, steps, speeds, kinds and tables above are all from the code; the
spawn speed bytes of kinds 190/192/207/210 are given as the game's speed index (they select a row of the velocity table, so a port that wants the same paths must call
`0596C4`/`059980`).
