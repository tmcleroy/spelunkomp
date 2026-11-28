# DoDonPachi stage 6 boss (object kind 187, the final boss of each loop)

Read from `work/ddonpach/program.lst` / `program.bin`; checked against the volley dumps of recordings r1..r4
(loop-stage `0-5` and `1-5`) and against the attack timeline of r2 (a full 18000-frame fight, 6.5 cycles).
Everything below that is a number has an address; "(dump)" marks a number also seen in a recording.

## 0. Coordinates, units

- A place is a long: **x = high word, y = low word**, 64ths of a pixel. On the board x is 0..320 (growing = **up the
  screen**; the boss sits at x of about 230..310), y is 0..240 (across). Angle 0 = up the screen (x growing), 64 = screen
  right, 128 (0x80) = straight down. "Fine" angles are of 256, "coarse" (64 directions) of 64.
- Bullet maker entries, in the order the boss uses them (all registers as in BRIEF.md; d0 high word = speed, low = kind):
  `50F94` fine/256 (loop 2: 2 bullets, see section 5), `50F58` fine/256 (loop 2: speed +4, one bullet), `50DA0` coarse/64
  (loop 2: speed +4, one bullet). All three are called with the boss's own task in a5 and the firing part in a6.
- The recorded `site` (return address from the stack) is **junk when it is not 03Bxxx-03Dxxx**: in loop 2 the maker
  wrapper pushes d0/d1/a0 and the finder picked them up. Settled, not guessed:
  - `03BAC6` (loop 2 only) is not code: it is the **address of the muzzle table** (16 longs at 03BAC6) that a0 held when
    the cannon's `jsr 50DA0` was called (03D684). Same bullets as site `03D6A0`.
  - `0275FC` is the muzzle table at 0275FC (a0 at 03D4C2/03D530); same bullets as `03D54E`.
  - `020018` is d0 = 0x00020018 pushed by `50F58` in loop 2 (03BEAC); same bullets as `03BEBE..03BEEA`.
  - `060024` / `070024` are d0 = 0x000A0024 / 0x000B0024 pushed by `50F58` in loop 2 (first two volleys of 03C02A on);
    same bullets as `03C06E`. `00121E..`, `0275..`, `0081xx` etc. below 0x9000 are the same effect (stack junk).
- Not real attacks: none of 03BAC6, 0275FC, 020018, 060024, 070024.

## 1. How the boss is built

Task a5 (kind 187, observed at 100142), body a6 = `6(a5)`: **28 parts of 0x20 bytes** (init copied from the table at
03B790 by 059F28; `4(a5)` = parts-1). Part 0 is the body, the rest are the sprites/emitters. Only the parts below
fire; the others are drawn only.

State words (all read in the code, none are the "atk" byte `+0x26(a5)`, which stays 0 for this boss):

| What | Where | Proof |
| --- | --- | --- |
| boss life (long) | `0x16(a5)`, starts 0x50800 = 329,728 | init 03AE2C; damage taken 03B178-03B180 (the largest hit any part took this frame) |
| age in frames | `0x1A(a5)` | 03B026 |
| which attack | body byte `3(a6)` (and `2(a6)`): 0,4,8,12 = A0..A3, 0x10 = A4 | table 03B5F2: bra to 03CD68 / 03D278 / 03CF2A / 03D0B4 / 03D12A; set 03B5B8-03B5CE |
| rest countdown | body word `0x1C(a6)` | 03B5A2-03B5AC |
| attack's own countdown | body word `0x18(a6)` | each handler |
| second phase flag | body word `0xE(a6)` | 03B586-03B59C |
| rank | word 101968 (0..63; stays 63 in all four recordings) | 03CDC0 etc |
| loop 2 | word 10197A != 0 | 03BBC4 etc |

The attack chosen is a **fixed rotation chosen by the rest timer**, not by time of the fight, parts destroyed or random:

- Parts cannot be destroyed one by one: every part has a hit-point word that is reset to 0x7FFF each frame and the
  largest loss goes off the one life pool (03B124-03B1B4). So there is nothing "by parts destroyed".
- Time-out: at age 0x4650 = 18000 frames the boss leaves (03B07E-03B08C; the age is held back by 0xB4 at 0x459C while the
  ship is alive, 03B066-03B078). r2/r4 ran to it (fight frames 131056-149070).

## 2. Phases

**Phase 1** (life > 0x1AD55 = 109,909, one third): the rotation **A0 -> A1 -> A2 -> A3 -> A0 ...** The first is A0
(`2(a6)`,`3(a6)` start 0xFC,0x00 -> +4 wraps to 0: 03B5B8-03B5CE; first rest 0x20 = 32 frames, init table word 0x0020).

Between two attacks there is a **rest of R = 0x100 - 2 x rank frames** (03CDBA-03CDC8, same in every handler's end): 130
frames at rank 63 (dump: cycle = 2752 frames A0 start to next A0 start = 552 + 720 + 480 + 480 + 4 x 130, exact in all 6
cycles of r2). An attack starts the same frame the rest ends.

**Phase 2** (life <= 0x1AD55): `0x18(a6)` is set to 2 and `0xE(a6)` to 1 (03B586-03B59C). The running attack ends as
soon as its own countdown reaches 0 (A1..A3: 3 frames; A0: after the wings' remaining sequence ends), then rest R, and
from then on `3(a6)` = 0x10 = **A4 forever** (03B5C8). A4 does not repeat its setup because it is never "ended" while it
fires (see A4). Seen: r1 life crossing the line mid-A1 (frame 93447), A4 began 93580.

Within A4 the cannons fire faster as life falls (section 3, A4 cannons): at 4/15, 3/15, 2/15, 1/15 of full life
(0x15777 = 87,927; 0x10199 = 65,945; 0xABBB = 43,963; 0x55DD = 21,981; 03D6E6-03D76E).

Order and lengths (counted in frames from the attack's first frame S; "first volley" delays measured in r2/r1, all
agree with the code):

| attack | handler | lasts | first volley |
| --- | --- | --- | --- |
| A0 wings | 03CD68 | until the 10th sequence end (5 sequences of 113 frames when both wings fire): 552 | S+19 |
| A1 core + arms | 03D278 | 721 (`0x18` = 0x2D0) | core S+16, arms S+39 |
| A2 ring cannon | 03CF2A | 481 (0x1E0) | S+40 |
| A3 twin sweeps | 03D0B4 | 481 (0x1E0) | S+16 |
| A4 breathing V + cannons | 03D12A | until the boss dies (refilled, see A4) | p27 cannon S+16, V S+23, p26 cannon S+36 |
| rest | | 256 - 2 x rank, 130 at rank 63 | |

Body movement is set by each attack too (`0x2E(a5)`, jump table 03CBFE; the emitters hang off the body):
A0 mode 0 (the tour of nine waypoints, table 03CCC2: (x,y) = (0x3E00,0x2400), (0x2800,0x2C00), (0x3E00,0x2C00),
(0x3800,0x2200), (0x3800,0x2C00), (0x3E00,0x2800), (0x2C00,0x2800), (0x3800,0x2800), (0x2800,0x2400), each held 0x28
frames, speed caps 8,8,8,8,0xB,0x10,0xD,8,8); A1 mode 2 (3CA44: x about 0x3A00 plus a 0xF0 circle, y = ship's y
+-0x2000, side switches every 128 frames, bit 7 of 10126F); A2 mode 1 (3C984: follows the ship's y clamped to
0xA00..0x3200, x = 0x4C00 plus a 0xFF circle); A3 mode 3 (3CAE8: parked at x 0x3A00, y 0x1E00 plus a 0x60 circle); A4
mode 4 (3CB64: parked at x 0x3A00, y 0x2800 plus a 0x60 circle). B below = the body's place.

## 3. The attacks, in the vocabulary

Bullet kinds are the low byte of d0; speeds are the high word, as passed (negative speeds exist and are as recorded).
"loop 2" lists only what differs; section 5 says what the makers add.

### A0 wings: "narrowing 3-bullet fans, aimed once per sequence"

- Emitters: parts 22 and 23 (routine 03BC52), place = part place (about B, 1-3 frames old) + (0xE00 + ramp up to 0x200,
  +-0x1200) (03BC86-03BCA2: p22 +0x1200, p23 mirrored -0x1200 via bit 2 of byte 3; ramp = word 0x1A, +0x80 a frame to 0x200,
  03BC62-03BC70), so roughly **B + (+52 px up, +-72 px across)**; the bullets come from that place + (-0x300, y offset,
  below). Both wings run **independently** with the same code; the 3 bullets of a volley are at y = part y + {-0x140,
  -0x280, 0} (p22) or {+0x140, 0, +0x280} (p23) (03BD2C, 03BD38, 03BD56, 03BD64; dump: dy -320..320 / -640..0 / 0..640).
- Timeline: the attack sets bit 0 of part byte 1 (03CD82). The wing ramps for 4 frames (bit 1 set when word 0x1A >= 0x200).
  Then a **sequence**: 10 volleys 8 frames apart (`0x1C` reload = `0x12` = 8: 03BCCC-03BCD0; dump +8), then an extra 8-frame
  tick that ends it (`subq.b #1,0x18(a6)` borrows after 10: 03BD04) and a pause of 33 frames (`0x1C` = `0x1D` = 0x20:
  03BD8A); sequence start to start = 113 (dump 89914, 90027, 90140 ...). Per sequence one aim.
  The attack ends when the **body's `0x18`** (10 at the start, `move.w #$A,0x18(a6)` 03CD78) reaches 0; each wing
  decrements it by 1 at the end of each sequence (03BD74). Two wings = 5 sequences each; one wing = 10.
- Volley: base = **fine aim at the ship from the part's place, taken once when the sequence begins** (`jsr 59C66`
  03BCEC, saved in `0x10(a6)`; `bchg #0,5(a5)` after it, so in 2 player the next sequence aims at the other ship; no ship
  = the sequence is dropped at 03BCF2 -> 03BD70). Bullets: base, base + F, base - F with
  **F = (volleys left after this one + 1) x k**, volleys left counting 9 down to 0 (`0x18(a6)` after the subq,
  03BD46-03BD62: `d5 = (0x18+1) x 0x14`), so F shrinks 10k, 9k ... 1k; and
  **k = ((body 0x18 + 1) >> 1) + 1 read when the sequence begins** (03BCDA-03BCE8): body 10 -> k 6, 8 -> 5, 6 -> 4, 4 -> 3,
  2 -> 2, giving first flanks 60, 50, 40, 30, 20 (dump: 60, 54, 48 ... 6 then 50, 45 ... 5 then 40 ... for k = 6, 5, 4).
- Bullet: **kind 31 (0x1F), speed = `0x16(a6)` counting 5, 4, 3 ... -4** over the 10 volleys of a sequence (`0x16` starts at 6
  from `0x0606`, `subq.b #1,0x16(a6)` per volley, 03BD0C; the byte is sign-extended, 03BD1C), fine maker `50F94`.
  Sequence's `0x16` is reloaded from `0x17` = 6 (03BD7E).
- **A wing only fires while it is on the screen** (`jsr 5A2AC` 03BD10 skips the volley, counters still run): that is why
  a dump shows 3 instead of 6 bullets for a whole sequence (dump 90027-90099). 5A2AC wants y in 0x200..0x3C00 and
  x in 0x1C00..0x4FFF.
- Loop 2: each `50F94` bullet becomes two (speed s and s+6): 12 bullets a volley (dump `(31,5),(31,11)`).

### A1 core + arms: "aimed 4-line every 17 frames, with two spinning single-bullet guns"

Two emitter groups run for 721 frames.

**A1a core** (part 17, routine 03BB5A; the same part is also the fight's "attack running" flag, byte 1 bit 7):
- Emitter: parent = the part, at B (part 17's place is the body's: 03B35C), bullet put at **part place + (-0x380, 0)** (d4 =
  0xFC800000, 03BBB8), 14 px below.
- Timeline: `0x1C` = 0x10, `0x1D` = 0x10 (`move.w #$1010,0x23C(a6)` 03D2C2; the handler also sets bit 0 of part 17 at 03D2B8): first volley S+16, then every **17 frames**
  (03BB7E, dump +17), 42 volleys.
- Volley: **fine aim at the ship from the part's place each volley** (`jsr 59C66` 03BB9A; on every odd count of word 0x1A it
  toggles `5(a5)` bit 0 around the call = alternates the aimed ship in 2 player, 03BB84-03BB94, 03BBA8). 4 bullets, same
  angle, `line(4, speeds -8, -4, 0, +4)`: d0 = 0xFFF80018, `+4` to the speed each (03BBBE, 03BBDA-03BBE0, 03BBD2 d7 = 3),
  **kind 24 (0x18)**, `50F58`. Loop 2 starts at -12 and `50F58` adds 4, so the same -8..+4, one bullet each (dump
  `(24,-8)..(24,4)` in both loops).

**A1b arms** (parts 18/19 move the arms, parts 20/21 shoot; routine 03D408, called from 03BC48):
- Emitters: the arm = **B + (+0x200 up, +-(0x1E00 + ramp up to 0x400) across)** = about 120..136 px each side
  (03BC1C-03BC34; ramp = `0x1A` +0x80 a frame, bit 1 when >= 0x400, 03BBF8-03BC06); the gun = arm place + (0, -0x200 for p20
  / +0x200 for p21) (03D420-03D42A); the bullet from the gun's place + the muzzle table 0275FC (16 longs, `(x,y)` words, ring of
  radius about 9 px; index = ((angle + 4 x d6) >> 4) & 15, d6 = 2 for p20, 1 for p21; 03D4C8-03D4D4 / 03D536-03D542).
- Timeline: bit 0 of part byte 1 set on parts 18..21 (03D29E-03D2B4); `0x29C(a0)` = 0x2000 gives `0x1C` = 0x20, `0x1D` = 0
  (03D2AA): **first shot S+39** (arm ramp 8 frames, 33 frames), then **one bullet every frame** per gun (reload `0x1D` = 0,
  03D508), but **only when the gun is on the screen** (`jsr 5A2AC` 03D51A; hence dump bursts of 8-19 frames and gaps of
  30-60: 560-640 bullets in an A1, not 1442).
- Bullet: **angle = the gun's own counter `0x1B(a6)`, +7 each shot** (03D514-03D516), so a spiral of 360/7 = 36.6 shots to
  turn all round; the counter is never reset (phase carries from one A1 to the next); **kind 17 (0x11), speed 2**
  (d0 = 0x00020011 03D522), `50F94`, fine. Loop 2: speed 2 and 8 (dump `(17,2),(17,8)`).
  (Seen both guns shoot the same frame with their own counters; the two start at 0x1B = the table's value.)

### A2 ring cannon: "5-way fan down every 17 frames, two kinds"

- Emitter: part 7 (routine 03BD9C), a cannon at about B; bullets parent = the part, at **part place + (-0xBC0, 0)** (d4 =
  0xF4400000 03BE40 / 03BEB2, 47 px below; dump dx -3008).
- Timeline: bit 0 set (and bit 2, 03CF3A, 03CF58, unused here); the cannon opens by a sprite animation word `0x1A` = 0x6F6B ..
  0x6F83 in steps of 4 every 3 frames (03BDF0-03BE0C), after which bit 1 is set and `0x1C` = 8; the first volley is at **S+40**
  (dump: 132609 = S+40 in r2), then every **17 frames** (`0x1D` = 0x10, 03BE1C), 26 volleys.
- Volley: **fixed angle 0x80 = straight down, fan of spacing 0x0C**: angles 0x80, 0x8C, 0x98, 0x74, 0x68 (03BE22-03BE6E) =
  `fan(5, spacing 12)`. Two bullets at each angle: **kind 35 (0x23), speed -4** (d0 = 0xFFFC0023, `50F94`) and **kind 24
  (0x18), speed +4** (d0 = 0x00040018, `50F58`) (dump `[(24,4),(35,-4)]`, rel `0,12,24,-12,-24` twice).
- Loop 2: **7-way** (adds 0x5C and then 0xA4 = 0x80 -+ 0x24, 03BE78-03BE8A and 03BEEA-03BEFA), kind 35 doubled by `50F94`
  (speeds -4 and 2), kind 24 d0 = 0x00020018 +4 by `50F58` = speed 6 (dump `(24,6),(35,-4),(35,2)`, n = 21).

### A3 twin sweeps: "two guns that sweep their aim back and forth around the ship"

- Emitters: parts 24 and 25 (routine 03C310, mode bit 1), place = part place (about B) + **(-0x140, +0x1680)** for p24 (non
  mirrored), **(-0x140, -0x1680)** for p25 (mirrored, bit 2 of byte 3) and a further 0x240 across in the sweep's direction
  (`bit 6` of byte 1: 03C3F2-03C424), so **B + (-5 px, +-90..99 px)**. The place is set before the aim and fired from, and
  the original place restored (03C3E0, 03C4A6).
- Timeline: bits 0 and 1 set at once (03D0DA-03D0E0), so **first shot S+16** (`0x1C` = 0x10, `0x1D` = 1 from `0x31C`
  0x1001, 03D0F2); then **one shot every 2 frames** per gun (reload 1, 03C3EC), 232 volleys in the attack (dump +2).
- Volley: the gun's angle `0x1B(a6)` starts at 0x80 and **steps by +5 or -5 each shot** (`moveq #5,d1`, `neg` when
  bit 6, 03C46C-03C478); the first shot is 0x85 (dump 133). It **turns round when it has gone past the ship's direction
  by >= 60** (loop 2: >= 72): d = (fine aim at the ship from the gun's place) - angle; direction + and d < 0 with |d| >= 60,
  or direction - and d >= 60 -> `bchg #6` (03C430-03C466; dump reversals at +60..+64 from the aim: 63, -62, 62, -63, 59, -61,
  64 ...). Then one bullet: **kind 1, speed -4** (d0 = 0xFFFC0001), `50F94`, fine, at the new angle. Both guns keep their
  own angle but run in step (dump `rel [0,0]`).
- Loop 2: two bullets per shot (speeds -4 and 2, dump `(1,-4),(1,2)`) and the limit 72 instead of 60.

### A4 (phase 2, the last 1/3 of the life): "breathing V from the body, plus two tracking cannon bursts"

Starts when A4 is chosen (rest ended after phase 2 began); setup 03D12A-03D1CA.

**A4a breathing V** (part 16, routine 03BF0A, bits 0 and 2 -> branch 03BFDC):
- Emitter: part 16 at **B + (0xE00 + ramp to 0x400 (ramp +0x80 a frame, bit 1 after 8 frames), 0) = B + 72 px up**
  (03BF3E-03BF46). Bullets are put at that place with x lowered: pair 1 by 0x500 (`subi.l #$5000000`, 03C044), pair 2 by a
  further 0x1C0 (`subi.l #$1C00000`, 03C07A) = 0x6C0, loop 2 pair 3 raised again by 0xC0 (`addi.l #$C00000`, 03C0BA) = 0x600
  (dump dx -1280, -1728); same y.
- Timeline: first volley S+23 (`0x1C` = 0x10 from `0x21C` 0x1001), then **every 2 + (n >> 2) frames where n counts 16..1
  (a counter `0x11C(a6)`, set 0x10 at the start, minus 1 per volley, 03C02A-03C03C)**, i.e. gaps 6,5,5,5,5,4,4,4,4,3,3,3,3,2,2,2
  then every 2 forever (dump 93603, 09, 14, 19, 24, 29, 33, 37, 41, 45, 48, 51, 54, ...).
- Volley: **fine aim at the ship from the part's place each volley** (`jsr 59C66` 03C00C; carry = skip). Pairs of
  bullets **aim - w and aim + w**: pair 1: w = T(c1) + 4, pair 2: w = T(c2) + 4, loop 2 pair 3: w = T(c3) + 4, where
  c1 = byte `0x11E`, c2 = byte `0x11F`, c3 = byte `0x13F` (03C04C-03C0E4), each **+2 every volley** (03C04C, 03C080, 03C0C0) and
  **T(c) = triangle wave: v = c & 0x7F; if v >= 0x40: v = ~v & 0x3F; result v** (03C050-03C05E), 0..63; so w breathes
  between 4 and 67 with a period of 64 volleys. Start phases: c1 = 0, c2 = 0x40 (loop 1) or 0x2A (loop 2), c3 = 0x54 (loop 2) (03D16C, 03D180,
  03D186; dump first volley 105: rel 0,12 = w 6 and -59, 71 = w 65). (The counters are rewritten each time A4 starts, 03D166-03D186.)
- Bullet: **kind 36 (0x24), speed = 26 - n** (n 16..1, then 26; d0 = 0x001A0024 minus n in the high word, 03C016 / 03C034-03C038),
  so 10 .. 25 then 26; fine `50F58`. Loop 2: d0 = 0x00160024, `50F58` +4, same 10 .. 26 (the junk sites 060024/070024
  are the first two volleys).
- Bomb: while a bomb is out (word 1017BE) the part writes `0x11C` = 0x10 again and does not fire (03BFE6-03BFEE): the
  ramp starts over after a bomb. And **03BFF8 `move.w #$A,0x18(body)` every frame it is allowed to fire**: this holds the
  attack's 900-frame countdown (0x384 set at 03D13C) at 10, so **A4 lasts as long as the part keeps firing, i.e. to
  the boss's death**; a bomb of more than ~10 frames lets it run out, ending A4 and restarting it after the rest R.

**A4b cannons** (parts 26 and 27, routine 03D552; they sit on parts 24 and 25):
- Emitters: place = part 24/25's place + ((0x0380 + 0x400, +-(0x0C00 + 0x400)) = (+30 px up, +-64 px across) (03D552,
  03D570-03D57A), plus the muzzle table 03BAC6 (16 longs, ring of radius about 13 px, index = ((angle + d6) >> 2) & 15
  where `0x1B` is a 64-direction angle, d6 = 2 for p26, 1 for p27: 03D684-03D694).
- Timeline: each cannon fires **bursts of 10 bullets, one a frame** (`0x14` = 0: reload 0, 03D652; `0x1E` counts 9 down
  to 0 over the burst, `0x1F` = 9, 03D658-03D66E), then a pause `0x1D` + 1 frames (`0x1D` = 0x1C = 28, from `0x35C` 0x101C, so
  the burst-to-burst time is 38). **p27 begins S+16, p26 S+36** (p26's `0x1C` gets +0x14, 03D1C8-03D1CA; dump
  93596 and 93616). `0x1D` falls as the boss's life does: 28 -> 25 -> 22 -> 20 -> 18 at the four thresholds of section 2
  (03D6E6-03D76E: -3, -3, -2, -2 on both cannons, evaluated at a burst's end), so the cycle is 38, 35, 32, 30, 28.
- Aim: the cannon's angle `0x1B` (64 directions) **turns toward the ship by 1/64 turn a frame, but only while it is not
  mid-burst** (`0x1E` = 0, i.e. in the pause and on the burst's last frame: 03D6A6 `tst.b 0x1E(a6)`; `jsr 59BE8` = coarse aim
  + one step 59BF6); during a burst it is fixed, so a burst is a straight line of 10 bullets. p27's aim is toward the
  other ship (it toggles `5(a5)`, since its byte 0 bit 0 is set, 03D6AC-03D6BE); same ship in 1 player. Start angle
  0x20 = down (`0x35B` = 0x20 at 03D1AE).
- Bullet: **kind 38 (0x26), speed 16** (`0x10(a6)` long = 0x00100026, 03D1A0), **coarse maker `50DA0`** (03D69A); loop 2: speed 20, one bullet.

## 4. Code that is there but did not fire in any recording

None of these are reachable from the attack table (03B5F2 has five `bra` entries and then code; no other reference to the
handlers was found in the listing or as 32-bit constants in the program):
- handlers 03CDDA, 03CF8A, 03CFE8, 03D04A, 03D214, 03D2FE, 03D380 (they would switch on parts 12/13, 18-21, 24/25 and part 16's
  other modes), and the routines they turn on: part 12/13 `03C23C` (two kind 34 bullets, aimed, 03C2CE-03C2F0), part 24/25's
  non-sweep mode `03C338-03C3CA` (kind 34, 8 bullets rotating), parts 26/27's other mode `03D5BA-03D646` (kind 1, speed -4),
  part 20/21 `03D460` mode, part 16 modes bit 0 (03BF96: 4 bullets, kind 24 speed -8..+4) and bits 3 and 4 (03C0EE: 4-way x 0x33,
  03C198: 4 x 0x80). They fit the same vocabulary (fixed aim, aim + turn, fans). Not in the dump, not in A0-A4, not worth
  an editor entry until something calls them.

## 5. Loop 2 makers (global rule, not per attack)

`10197A != 0` and the maker entry: `50F94` -> `50F24`: if body `(a6 of 6(a5))` byte 0 bit 1 is clear (it is in all dumps): **2
bullets, speed s and s+6**, same angle; bit set: 3 bullets s, s+5, s+10 (if `0xD(a5) & 0x81`) or angle -8/+8 (otherwise)
(50EDC-50F22, 50F40-50F4C). `50F58` and `50DA0`: **one bullet, speed +4** (50F5E-50F6A, 50DA6-50DB2). In both the bullet is
made by the loop-2 routines 054EF4 / 054E22 (their own move routine 0552EA, DODGE.md). Everything else in the loop 2 changes is
in the numbers above: 7-way ring, 3 breathing pairs, bounce 72, kind 31 doubled, kind 17 doubled, kind 1 doubled.

## 6. What does not fit the vocabulary (and what to add)

1. **State that changes the pattern during an attack, from outside the attack's own timeline**:
   a. A0's fan spacing and number of sequences depend on a counter shared by two emitters (the body's `0x18`), so a
      wing's "volley" parameters are not an attribute of the wing: `k = ((remaining+1)>>1)+1` read when a sequence
      begins; the attack ends when the *sum* of sequence ends reaches 10. If one wing is off screen the other runs 10
      sequences.
   b. A4's cannon fire rate and A4's end depend on the boss's life (thresholds, 03D6E6) and on a refill by a part
      (03BFF8). A4 can end only by a bomb.
2. **Emission gated by the world**: on-screen windows for the wings and the arm guns (5A2AC), a bomb out (1017BE), the ship
   dead (carry from the aim). A "timeline" must say whether a skipped volley still advances counters: it does for the
   wings (03BD04 before 03BD10) and for the guns' `0x1C`.
3. **A parameter that is a counter changed per volley** (needs "per-volley value = f(n)"): speed counts down 5..-4
   (A0: 03BD0C), speed 26 - n and interval 2 + n/4 (A4a: `0x11C`), flank = (left + 1) x k (A0), triangle-wave half-width
   `T(c)` with a +2 phase (A4a, three counters with set phases). "base + step" and "random turn" are not enough; the
   needed generator is *per-emitter integer counters with initial value, step, wrap/triangle rule*.
4. **Emitters that are guns with their own angle state**: A1b's spinning gun (+7 a shot, carried from attack to attack),
   A3's bouncing sweep (+-5, reverse when the aim has been passed by >= 60/72), A4b's cannon (1/64-turn tracking
   that locks during a burst). The sweep and cannon use the *ship's aim from the gun's place as feedback each shot*, so they
   cannot be played back as a table: "base = previous base + step, bounce when |previous - aim| >= L" and "base = turn toward
   aim by 1 per frame while idle, frozen during a burst" are new base-angle kinds.
5. **Emitters that move** (not at B): wings (B + (52, +-72) px), arms (B + (0.1, +-120..136) px, +-8 px for the gun, + a 9 px
   muzzle circle that follows the angle), core (B), A2 cannon (B + (-47 px) down), A3 guns (B + (-5, +-90..99) px), V (B + 72 px up
   minus 20/27/24 px), cannons (B + (30, +-64) px + a 13 px muzzle circle). The parts follow the body with 1-3 frames of lag
   (the body's recent places 0x34, 0x38, 0x3C, 0x54 ... are copied into parts, 03B34C-03B4DC, constraints 03BB06). For Dodge
   training that makes the fixed emitter at B an approximation of a few px, except for the arms, wings and cannons
   (about +-70..135 px apart): decide there whether the editor keeps them as separate emitters with offsets (recommended).
6. **Two-player targeting**: `5(a5)` bit 0 is toggled around some aims (A0 per sequence, A1a per odd volley, A1b per shot,
   A4b by part 27 only): with two ships the aim alternates; with one it does nothing.
7. **Attack choice is a rest timer, not random**: R = 256 - 2 x rank frames (rank 63 in every recording), no random numbers
   anywhere in this boss's fire (the `jsr 57C84`/`57FA2` calls in 03AD4C-03AD6C belong to debris and the explosion, not to
   bullets). The `atk` byte in the dumps is not the selector (task `+0x26` = 0 here); the selector is body byte `3(a6)`.
8. **Death**: when the boss dies (`10E858` bit 7) all fire stops (every part's routine tests 10E858) and 03C504-03C610
   make explosions only, no bullets.

## 7. Cross-reference: sites in the dumps -> what they are

| site(s) | what |
| --- | --- |
| 03BBDA | A1a core, 4-line (kind 24) |
| 03BD46, 03BD60, 03BD6E | A0 wing fan: centre, +F, -F |
| 03BE4C..03BE78 (+03BE8A, 03BE94 loop 2) | A2 kind 35 fan (5, 7 in loop 2) |
| 03BEBE..03BEEA | A2 kind 24 fan (loop 2: 020018 is the same, see section 0) |
| 03C06E, 03C078 | A4a pair 1; 03C0A2, 03C0AC pair 2; 03C0E0, 03C0EA pair 3 (loop 2) |
| 03C4A2 | A3 sweep, kind 1 |
| 03D54E (0275FC in loop 2) | A1b spinning gun, kind 17 |
| 03D6A0 (03BAC6 in loop 2) | A4b cannon, kind 38 |
| 03BAC6, 0275FC, 020018, 060024, 070024, 0081xx, 00121E.. | junk, section 0 |
