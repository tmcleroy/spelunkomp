# DoDonPachi stage 4 boss (object kind 14, task a5 = 100182): the rules of its fire

Read from `work/ddonpach/program.lst` / `program.bin` (code 01E96A..020A00), checked against the volley dumps of
runs r1..r4 (`volleys.py ... L-3`, loop 0 = first loop "0-3", loop 1 = second loop "1-3"). Nothing outside
`docs/plans/dodge-boss/` was touched; no game run was made (the recordings were enough).

Names used below: **body** = the boss's 0x20-byte records at `6(a5)` (a6 in the main routine; 0x1054F6 in the dumps, G = 0x1056F6); **G** = body+0x200, the core part that every attack uses as its emitter and for its flags;
**rank** = word 101968 (the game's difficulty 0..63, `101969`, see ddonpach.h kRank); **loop 2** = word 10197A != 0.
Places are (across, down) as the game keeps them (word +6, word +8; across grows up the screen), 64ths of a pixel:
0x2C0 = 11 px. A bullet maker is called with d0 = speed<<16 | kind, d1 angle, d2 place.

## 1. What chooses the attack

One task (a5 = 100182) runs the whole boss; the `atk` byte +0x26(a5) is not used (always 0 in the dumps). The choice
lives in the body:

| where | what |
|---|---|
| body+0x1C (word) | pause counter. Each frame at 01EF60: `subq.w #1`; while it is still > 0 nothing is dispatched. When it reaches 0 (01EF6A) the next attack is picked and starts that same frame; while it is 0 the attack's routine runs every frame (the word is -1 then +1 each frame, 01EF94). An attack ends by writing the pause for the next one into body+0x1C. |
| body+0x02 / +0x03 | index of the last / the current attack, steps of 4, wraps at 0x18 (6 entries). Start value +2 = 0xFC (template 1F10E), so the first attack is index 0. |
| body+0x04 (byte) | "cannons left" counter: 1 at the start (template). body+0x05 = a copy of it, taken at each pick (01EF6E). So +5 is 1 until the cannons die, then 0xFF (negative = **hard table**). |
| table 1EFB0 (+5 >= 0) | idx 0,4,8,12,16,20 -> **A, B, A, B, A, B** (A = 020224, B = 0204A4) |
| table 1EFC8 (+5 < 0) | idx 0..20 -> **A, C, D, A, C, D** (C = 020384, D = 020630) |
| life | long 0x16(a5), starts 0x38000 (01E994, no rank/loop scaling). Damage each frame = 0x7FFF - the lowest hit-point word of parts 0x100,0x120,0x140,0x160,0x200 (01EC9C..01ECBC); hit points then reset. |
| the switch | part 0x140 and part 0x160 each run 01F372 every frame: they are destroyed when `body+4 == 0` or life < 0x1DC00 (53% left). Destroying one does `body+4 -= 1`, `body+2 = 0xFC` (next pick is index 0 = A) and, if the pause counter is running, cuts it to 0x80 (01F3AA..01F3BA). With +4 = 1 both go in the same frame (1 -> 0 -> 0xFF). From the next pick on the hard table is used and stays (never goes back). The attack running at that moment is not interrupted. |
| time limit | 0x1A(a5) frame counter; at 0x459C the boss starts leaving, at 0x4650 (18000 frames) 01FD82 (life = -1). Not an attack. |

Pause written at the end of each attack (frames until the next attack starts), rank = r:
A: `0x100 - 4r` (0202A2..0202B0) - B: `0x80 - 2r` (020520..020538) - C: `0x100 - 2r` (0203CC..0203DA) -
D: `0x130 - 4r` (0206A4..0206B4). (r = 56..57 in the loop-2 recording r1, where A ended 28-29 frames before B's first
shot; the recorder does not keep the rank, Dodge must pick one.)

Phases in order, loop 1 and 2 alike:

1. **Normal half (life >= 53%)**: A, B, A, B ... (observed cycle A start -> next A start: 747..1076 frames, mean ~890,
   runs r2/r4 which never got the cannons below 53%).
2. **Hard half (life < 53%)**: A(hard), C(hard), D, A, C, D ... (observed in r1 and r3: A+C then D, 164 frames
   between each).

Every attack uses the same on/off flag, `G+1` bit 7: set by the attack's first call, cleared when it ends.

## 2. The attacks

Emitter notation: G = body+0x200's place (it is copied from the body's place each frame at 01EE9C). Aim = the game's
fine aim (59C66, 256 directions, from G's place; carry = no ship = skip the volley; that is the only thing that
makes the fan miss and the ring not). Every bullet maker call below also needs 5A2AC to say the boss is on the
board (across 0x1C00..0x4FFF, down 0x200..0x3C00); if not the volley is skipped but its counters still run.

### A (020224): "aimed 5-way fan + rotating 23-ring" (two emitters at once, same place)

Both emitters are at G - (0x2C0, 0). Timeline: the attack lasts T+1 frames (T = body+0x18 counts down from the
value below, `subq.w` until it borrows, 020296..0202B0); both timers start at 1, so the first frame fires both.

| | normal (+5 = 1) | hard (+5 < 0) | (+5 = 0, not reachable) |
|---|---|---|---|
| T | 0x100 (256) | 0x140 (320) | 0x1B8 (440) |
| fan period (G+0x1D) | 0x20 = 32 | 32 | 0x2C = 44 |
| ring period (body+0x17) | 0x24 = 36 | 0x12 = 18 | 0x12 = 18 |
| ring start word (body+0x14) | 0x80 | 0x80 | 0x80 |

(020244..020288; initial values are the 020244..02025C ones, then `tst.b -0x1FB(a6)`: negative -> 02028A, non-zero
positive -> keep, zero -> 02026A.)

Fan (0202B6..020330): base = aim; shape 5 bullets at base-12, -6, 0, +6, +12 (`subi.b #$C`, `addq.b #6` x5); each
bullet speed 4, kind 0x18 (24), maker 50F58 (single; **loop 2: speed +4 and one bullet**, 50F58 -> 54EF4).
Loop 2 (020310): base-45, three groups of five, step 6 in a group, groups start at -45, -12, +21 (`addq.b #3` between
groups), i.e. fans centred on aim-33, aim, aim+33; speed 8 (4+4), kind 24; 15 bullets. Dump: (24,4) n5 / (24,8) n15
with `aimF` -12 / -45 and rel [0,6,12,18,24].

Ring (020330..020382): base = low byte of the word body+0x14 minus 0x79, i.e. ring k starts at `0x80+14k-0x79 =
7+14k` (the word grows by 0xE after each ring, `bcs` never happens in practice). Shape: 23 bullets, angle step 11
(`addi.b #$B`, d7=0x16): speed 0 (d0 starts as 4 and the loop's `addi.l #$FFFC0000` takes the 4 off before the first
bullet), kind 7, maker 50F94 (256 angles). The ring is not aimed; its start angle advances by 14 each ring and
resets to 7 with the attack. Dump (7,0) n23 rel [0,11,22,..], firsts 7,21,35,..,105 (8 rings in 256 frames, 14 in
the hard 320).
Loop 2: 50F94 -> 50F24: two bullets at the same angle, speeds 0 and 6 (50EE8: s, then s+6) -> n46 `(7,0),(7,6)`.
(There is a third form: when `+0xD(a5) & 0x81` is non-zero and bit 1 of (body) is set, 50F00 makes three at the same
angle s, s+5, s+10. For this boss that only happens at the kind-17 volley, below, because only 207FC sets bit 1.)

Ends: `body+0x1C = 0x100 - 4r`, `G+1 = 0`.

### B (0204A4): no bullets in loop 1; in loop 2 the two arms fire a six-row fan each

B is the boss diving at the player and two side parts ("arms", body+0x40 and body+0x60, routine 01F4AC) swinging.
Entry (0204A8..020516): `0x15C(a6)` / `0x17C(a6)` words (the arm start timers, bytes +0x1C of parts 0x140/0x160) =
0 and 0x2800 in a random order (57C84 sign): one arm starts at once, the other 40 frames later (0x2200: 34 in loop
2). Their timers set bit 0 of body+0x41 / +0x61. `0x2C(a5) = 1` (the dive is on); body+0x1A = 0x1A (0x40 loop 2).
B ends (020516..02053C) when 0x2C(a5) is cleared again (01FB54, the boss finished the dive/retreat state
machine at 01F8EE..01FC06 keyed on body+0x1F bits 0..3, 6), then `body+0x1C = 0x80 - 2r`. Length is not fixed: it
follows the boss's flight (measured 450..650 frames, see the cycle numbers in section 1). **No bullets in loop 1**
(01F518 path: the arms only swing).

Loop 2 only (01F5BA path, `10197A != 0`): each arm, when its swing starts (counter 0x12(a6) = 1, 01F5F6), calls
01F680: **6 rows x 12 bullets = 72**, maker 50DA0 (64 directions, loop 2: single, speed +4), kind 7.
Place = arm place - (0x2C0, 0). Arm 0x40: first direction 0x18 (24 of 64 = 96 of 256), step -2 (= -8 of 256);
arm 0x60: 0x28 (160), step +2. Rows alternate direction (after a row `neg d6; add d6,d1`, 01F6C6..01F6C8) over the same
12 directions; row speeds 12, 9, 6, 3, 0, -3 (`subi.l #$30000` per row) +4 = 16, 13, 10, 7, 4, 1. Dump:
(7,1),(7,4),..,(7,16) n72, rel [0,-8,-16,..] first 96, and [0,+8,..] first 160, alternating arms every 33..35
frames (each arm cycles every ~66.5 frames while B lasts: 18 volleys per B in r1). The second arm starts 34
frames after the first (timer above), which is the dump's 35-frame offset.

### C (020384): "random-aimed singles every 2 frames + aimed single every 25 frames" (hard table only)

Emitters at G; one flag (G+1 bit 0) flips each shot so the muzzle alternates sides.
Entry: T = 0x168 (normal), **0x1A4 = 420 when +5 < 0** (always the case in the table that holds C); G+0x1C word =
0x0001 (counter 0, reload 1 -> a shot every 2 frames, the first frame fires, 0203A4), G+0x1A word = 0x8018 (counter
0x80 = 128, reload 0x18 = 24 -> first aimed shot after 129 frames, then every 25 frames, 0203AA).
Fire 1 (0203E4..020456): every 2 frames: aim (fine) + (57EBA - 0x20) = aim + uniform -32..+31 (57EBA returns
0..63); speed = -(57F4E + 11) = -11..-14 (57F4E returns 0..3, `neg; subi #$B`); kind 5; maker 50F94;
place = G + (-0x140, +0x340) for one shot, G + (-0x140, -0x340) for the next (`subi.w #$680` when G+1 bit 0 is
set after the flip; first shot is the -0x340 one). Single bullet (`d7 = 0`).
Loop 2: 50F94 -> two bullets at the same angle with speeds s and s+6 (dump `(5,-14),(5,-8)`).
Fire 2 (02045A..02049C, only when +5 < 0): every 25 frames starting at call 129: aim (fine) exactly, speed 8, kind 25
(0x19), maker 50F58 (loop 2: speed 12, single), place G - (0x2C0, 0). Dump: (25,8) / (25,12), 12 shots in a 420-frame
C (r1: 61910, 61935, ... 62185). It also flips `5(a5)` bit 0 (sprite only).
Ends: `body+0x1C = 0x100 - 2r`, `G+1 = 0`.
(If +5 were 0 fire 1 would be gated by `10126E & 3 != 0` - 3 frames in 4 - and fire 2 would not exist; not reachable:
+5 is only ever 1 or 0xFF in the normal flow.)

### D (020630) + part routine 206C6: "two turrets: aim, then a 10-shot speed ramp, then a 8-way kind-17 burst", 8 times

Two turret parts P = body+0x220 (d6=1) and body+0x240 (d6=2) run 206C6 every frame (01F0A4..01F0B4) when D has
switched them on (bit 0 of P+1). Their place: body place + (0xC(P) - 0xFC00FC00), P+0xC = 0xF500ED40 / 0xF5000AC0
(template 1F10E) = (-0x700, -0xEC0) and (-0x700, +0xEC0): 28 px down the screen, 59 px either side of the body.
D entry (020630..020694): for each P: +0x10 word = 0x0001 (tick counter 0, reload 1: logic every 2 frames), +0x12 word
0x0A0A (counter 10, reload 10), +0x14 word 0x0305 (speed start 3, step 5), +0x1C word 0x2020 (tracking window 0x20,
reload 0x20), +0x1A = 3; body+0x18 = 0x10 (D is over when it has been counted down to 0: both parts count down once
per series, so 8 series each). End (02069A..0206C4): `body+0x1C = 0x130 - 4r`, parts off.

Per part, a series (0206C6..020770, 0207C4..):
1. **Tracking window**: 33 frames (0x1C counts 0x20..0, then the borrow frame) during which P+0x1B = fine aim at the ship from
   the part's place, each frame (59C46). No bullets. (After it the aim is frozen.)
2. **Ramp**: ticks every 2 frames. The first tick sets "firing" (P+1 bit 1); ticks 1..10 fire
   (counter 10 -> 0): angle = P+0x1B (the frozen aim), speed = P+0x1A = 3, 8, 13, ..., 48 (+5 per shot, `add.b
   0x15(a6),0x1A(a6)`), kind 8, maker 50F58 (loop 2: single, +4: 7, 12, ..., 52). Place = P's place +
   table27D70[((P+0x1B + 4*d6) >> 4) & 15], a 16-entry ring of radius 0x380 around the part (entries (across,down):
   (896,0), (832,320), (704,576), (512,640), (256,640), (-128,640), (-256,448), (-448,320), (-512,0), (-448,-320),
   (-256,-448), (-128,-640), (256,-640), (512,-640), (704,-576), (832,-320); sector 0 = straight up), i.e. the bullets
   leave on the side of the part that faces the aim.
3. **Burst** (the 11th tick, counter borrows, 02074E..020770 -> 0207FC): 8 bullets, angle = P+0x1B - 35 + 10k (k=0..7,
   spacing 10 = `d5`, centred: `d4 = (8*10-10)/2 = 35`), speed 1, kind 17 (0x11), maker 50F94, same muzzle
   table position. Loop 2: speed -2 (d0 = 0xFFFE0011) and each call sets bit 1 of (body) around the call, so the maker
   gives three bullets at the same angle, speeds -2, 3, 8 (50EDC/50F00: s, s+5, s+10 - the `+0xD(a5) & 0x81` form;
   the dump shows exactly (17,-2),(17,3),(17,8) n48 = 2 parts x 8 x 3). Then series counters reset: speed back to 3,
   `0x12 = 10`, `0x1C = 0x20`, and body+0x18 -= 1.
   Series length: 33 + 22 = 55 frames (dump: 54..55). D total ~ 33 + 8x55 = ~470 frames (dump 452..503 for r1/r3).

Dump check: loop 0 (8,3..48) n2, loop 1 (8,7..52) n2; rel [0,24] / [0,32]: both parts fire the same ramp at once, each
at its own aim (the two parts are 118 px apart, so their aims differ by 24-32 of 256). The (17,1) n16 volleys come
2 frames after the last ramp shot.

### Dead code (not reachable from anything in the program image)

- 0205BE: a variant of D's set-up (`0x230/0x250 = 6`, `0x23C/0x25C = 0x2003`, counts 0x2D0 frames).
- 02015C: a spiral: G+0x1C period 2 (`addq.b #2` after the `subq`), 33 shots (0x21) per pass, 6 passes (0x18(body)=5),
  angle = (running +0xA7 byte) +- (aim), speed grows with the counter (c + c/4), kind 7, maker 50F94, place G - (0x2C0, 0),
  direction flips each pass, ends with `body+0x1C = 0x100`. Neither is in table 1EFB0/1EFC8 or called from code
  (scanned every bra/bsr/jsr/lea/long in the program for these entries); the dumps never show it.

## 3. What loop 2 and the flags change (summary)

- Makers: 50F58 (single) fires one bullet of speed +4; 50F94 (fine) fires two at the same angle (s, s+6) - or three
  (s, s+5, s+10) / a +-8 pair when bit 1 of (body) is set around the call (only the kind-17 burst sets it here).
- A: fan 5 -> 3x5 at speed 8; ring 23 -> 2x23 (speeds 0 and 6).
- B: arms fire the 72-bullet fans (loop 2 only); the second arm's delay is 34 instead of 40.
- B's flight-speed fields (0x2A(a5), body+0x1A) are 0x40 instead of 0x1A at entry (020502..020510): the dive is faster.
- C: singles become pairs (s, s+6); the aimed one is speed 12.
- D: ramp +4 (7..52), kind-17 burst speed -2 with 3 bullets per angle.
- Flags that make the boss harder: only body+5 negative (hard table, A is 64 frames longer with the ring 18 apart
  instead of 36, C has the aimed 25-shot). Word 10E73E is not read by this boss. 101764 / 1017BE / 10E858 stop the
  makers (ship pause, bomb, boss dead): the parts check them (206C6, 01F4F4).
- Rank only changes the pauses (formulas above); it changes no bullet count/speed in this boss.

## 4. Random and what does not fit the vocabulary

- Random: only C's two draws (aim +-32 via 57EBA, uniform 0..63; speed via 57F4E, uniform 0..3 -> -11..-14) and
  B's 50/50 arm order (57C84 sign). The game's "random" is a table lookup with a counter that every call moves
  (57C84, 57EBA, 57F4E share the counter at 101273); Dodge can use any uniform generator.
- The pauses depend on rank (not a constant) and the attack order depends on life crossing 0x1DC00; neither is a
  property of one volley. Dodge needs "phase = A / C / D chosen by a flag" and a rank to run a boss timeline.
- B is not a pattern: it is the boss moving (a dive at the player and back, with the arms) with timing from a
  flight state machine; in loop 1 it fires nothing. Only loop 2 gives it bullets (the arms' fans).
- Emitters that are parts: D's turrets and B's arms have their own places (D's: body + fixed offset, with a muzzle on a
  radius of 0x380 turning with the aim; the arms: body across history + swing, positions not decoded, the dump keeps
  their bullet places).
- The vocabulary's "previous base + step" covers the ring (start 7, +14 per ring, reset per attack; shape ring(23)
  with step 11, not 256/23). The ramp (speed +5 per shot, same angle) is a `line` generator in time rather than in
  space: "same angle, speed from 3 step 5 for 10 shots, every 2 frames". The kind-17 burst is `fan(8, 10)`.
- Multiplicity added by the maker in loop 2 (the same angle two or three times, speed steps of 6 or 5) belongs to the
  maker entry, not to the attack; the vocabulary should carry it as a per-bullet "twins" setting with the entry (50F94
  vs 50F58 vs the bit-1 flag) so the loop 2 numbers fall out.
- Volley holes: the fan is skipped when the ship is absent (59C66 carry), every volley when the boss is off the board
  (5A2AC); counters keep running, so a respawn leaves the pattern's phase unchanged. In the dumps this shows as a
  few missing volleys (A episodes of 181..221 frames; the A-hard episode in r1 whose first 90 frames have none).
