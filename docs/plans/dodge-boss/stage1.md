# DoDonPachi stage 1 boss: how it builds its bullet patterns

Objects: **kind 104** (0x68), the ship's body, one task (a5 = 100182 in the recordings); its code is
update `031C30` (init `031980`, from the table at `015DCA`). **Kind 105** (0x69), two pods (a5 = 100202 and 100242),
made by the body's init (`031A4E-031A80`); update `032E10` (init `032D12`). Both fire with the makers `50DDC`
(angle of 64) and `50F94` / `50F86` (angle of 256). All code is in `work/ddonpach/program.lst`; volley dumps are
loop 0 and loop 1 stage 0 of r1..r4. Everything below marked (rec) was checked against those dumps; (code only) was
read but never seen in the four recordings (no recording got as far as destroying the pods, so phase 2 is code only).

## Shared facts

- **Bullet speed** is not the d0 high word alone: `050E62-050E82` stores `0x16 + d0.hi` (kinds 0, 3, 7, 17, 38 all
  have base 0x16 in the kind table at `05104E`). The dumps' "speed" is the d0.hi. Kind is d0's low 6 bits.
- **Random (`057C84`)** is a table walk, not a generator: counter byte `101273` is incremented on every call (by any
  caller: also `057FA2`, `032AB2`), the value is byte `057CA8[counter]`. The 256 entries are signed -7..+7, uniform,
  except 0 which is twice as likely (32 entries, 16 for each other). So "random turn" below is always
  uniform -7..+7 (call it r). It can only be replayed exactly by sharing the counter with the other callers; for
  Dodge a fresh uniform draw is the faithful model.
- **Target** of every aim (`059A58/59C58/59C66`, `05A64A`): the player chosen by byte `+5(a5)` of the task (0 = P1,
  1 = P2; the other if that one is dead; carry set, bullet skipped, if both are gone). In 1P it is just the ship.
  Aim is taken fresh at every volley from the emitter's own place (or from the muzzle point, where stated).
- **Loop 2** (word `10197A` != 0) changes only the makers, in every site of this boss (rec, all four sites of kind
  104/105 doubled in loop 1): `50F94` fires **two bullets at the same angle, speeds d0 and d0+6** (`50EE8`);
  `50DDC` the same (via `50D68`/`54E22`); `50F86` fires **two bullets at angle -8 and +8** (of 256), same speed
  (`50F40`). If task byte `+0xD(a5)` has bit 0 or 7 or the body's first word has bit 1 set, the maker instead
  makes 3 bullets, speeds d0, d0+5, d0+10 (`50F00`); not seen for this boss. Nothing else in the boss differs between
  loops (no test of `10197A` in 031900-033900).
- **Harder flag `10E73E`** (word): set to 1 at `032F44` when the pods' shared life runs out (see Phases). Not seen in
  any recording (code only).
- Muzzle places below are in 64ths of a pixel, (x high word, y low word) as the maker takes them. The guns' own place
  moves (wing parts `+0xE0` / `+0x100` of the body copy to the guns `+0x120` / `+0x140`, `031CEA-031D00`, wings
  swing by `05968A`); the boss's places are not derived here, take them from the recording.

## 1. Phases (what chooses the attack)

Body record a5: life long `+0x16` = 0x15000 (`03199A`), damage `sub.l` at `032232`; time-out word `+0x1A` = 0x2A30
(10800 frames) counted down at `03217A`, boss ends at 0 (`032242`). The low-life look (`+0x16` < 0x5400, `032380`) is
drawing only. Pods' life: 0xE000 each at `+0x76` in their body (template `032DA0` + `0x60 rec`), shared through
word `10E742` (min of the two, `032E72-032E8C`).

**Selector byte `+0x26(a5)`**, values 0, 1, 2; starts 0 (`0319BA`). Changed at `032AB2-032AE0`, when gun 1's
`+0x12` count (`0x132(a4)`) has run out, i.e. when an attack has fired all its volleys:
- the first two changes are fixed: 0 -> 2 -> 1 (counter word `+0x34(a5)` = 2 counts down; byte `+0x26` is decremented,
  below 0 it becomes 2; `032AD0-032ADA`);
- after that: next = `057FB6[ctr & 63]` (64 bytes of 0/1/2), re-drawn (counter incremented) until different from the
  current one (`032AB2-032AC8`). So random, never the same twice (0/1/2 appear 22/21/21 of 64 in the table).
(rec: first three attacks of every run are 0, 2, 1; no repeats in any run.)

**One cycle** (rec, every loop and run; frames are from the end of the previous thing):

1. Gun attack `+0x26` (below): attack 0 = 40 volleys 8 frames apart (~320 frames); attack 1 = 40 volleys 5 frames apart
   (~200); attack 2 = three bursts at +0, +69, +158 frames.
2. **13-bullet fan from the body's mouth (A5)**, 42-44 frames after the last gun volley (rec: 42, 42, 43, 44).
3. Maybe the **pods' attack (A4/A5)**: chosen at the attack change: if `bset #0,0x1F(a4)` finds the bit clear (the very
   first time) or `057FA2` returns non-zero (table 0/1/2, 0 in 22 of 64, ~2 of 3), word `+0x24(a5)` is set to 1
   (`032A34-032A44`); one frame later `0324C6-0324D4` sets `10E738` long = 0x00010001 (both pods' go flags
   `10E738` and `10E73A`, = word pointed at by each pod's `+0x14(a5)`). While `10E738` != 0 **the body guns are frozen**
   (`032660`: `tst.l 10E738`, and `032BA0`). Pods fire 103 frames after the mouth fan (rec: 103), 7 aimed shots each 17
   frames apart, then a ~45-frame stream; the pods then clear their flags and the guns resume after their 161-frame
   pause (below). rec: pods followed 3 of 3 (loop 0 r1: skipped once, after the 3rd attack, as `057FA2` gave 0), 4 of 4
   loop 1.
4. Pause before the next gun attack: gun byte `+0x10` reloaded with `+0x11` = 0xA0 (`0329F4`); fires when it
   underflows: **161 frames** after the last gun volley, or after the pods release the guns (rec: 161, 162).
First cycle: the guns start ~97 frames (`+0x10` = 0x60 +1) after the boss has arrived (`0x30(a5)` != 0) and the 40-frame
(`+0x22` = 0x28) opening hold; the arrival/opening, where the pods deploy once (`0324B0-0324B6`, `033150`), is not in the dumps.

**Phase 2** (code only): when pod life < 0, `032ECC` falls to `032F44`: `10E73E` = 1, `10E738` = 0, pods
explode (`032F04-032FB6`, body parts break away). From then: guns get the "harder" numbers (below), pods no longer
fire, `+0x24(a5)` no longer re-arms pods (`0324CC`), and a new mouth attack (B1) runs.

## 2. Attacks

Gun records: gun 1 = body `+0x120` (d7 = 1, y offset d6 = +0x0A00), gun 2 = body `+0x140` (d7 = 0, d6 = 0xF600 =
-0x0A00). Per gun: `+0x10` countdown (frames-1 to the next volley), `+0x12` volleys left (set at the attack change,
`032AEA-032B0C`: 40 / 40 / 3; harder: 80 / 60 / 5, the table `032D0A` plus `032D0E`, `032AFE`), `+0x14` muzzle
table offset, `+0x16` volley counter, `+0x18` wobble index, `+0x1A/+0x1C` turns. Template values (`031A86`, rec
`+0x120`): `+0x10` 0x60, `+0x11` 0xA0, `+0x12` 0x28, `+0x14` 8, `+0x16` 12, `+0x18` 0x34, `+0x1A` 0x30, `+0x1C`
0x30. Each attack change resets (`032A64-032A9C`) `+0x1A` and `+0x1C` to 0x30 (from `+0x1B/+0x1D`), `+0x18` = 0x34,
`+0x16` = 12. Gate for all gun fire (`032658-032682`): `+0x22(a5)` = 0, `10E738` long = 0, not paused
(`101764`, `1017BE`).

Muzzle table `032CBE` (longs): E0 = (-1024, 0), E1 = (-1600, +576), E2 = (-1600, -576). Wobble table `032CCA`,
words (idx 0..0x34 step 2): triangle 0,2,4,6,8,10,12,12,10,8,6,4,2,0,-2,..-12,-12,..,-2 (26 values), then -3 at 0x34.

### A0. "Aimed random spray" (selector 0; `0326F0`... branch `032692-0326EC`)
- emitters: both guns (gun place + muzzle `E[+0x14]` + y d6), the muzzle cycling through table offsets 8, 4, 0
  (E2, E1, E0) per volley, `+0x14` decremented 4 at `0329DE`, reset to 8 when below 0. Toggles `+5(a5)` each volley.
- timeline: a volley every 8 frames (`+0x10` = 7), 40 times; harder: every 6 frames (5), 80 times. Both guns same frame.
- volley: 1 bullet per gun; base = fine aim (`59C66`, from the gun's own place) + 8 * r (r uniform -7..7: multiples of
  8, i.e. a +-56 of 256 scatter, `0326D0-0326D8`); kind 0, speed -10 (harder: kind 0 speed -6, `0326A0-0326B0`);
  maker `50F94`. (rec: first angle less aim spans -68..+48, always a multiple of 8 plus the muzzle's own offset.)
- rec: gap of 28 inside a run = the ship was dead/absent, volleys skipped (`0326C0`) but counted.

### A1. "Two converging aimed arms + periodic 5-fan" (selector 1; `0326F0-032810`)
- emitters: both guns; two muzzles E1 (`32CC2`) and E2 (`32CC6`) each, plus E0 for the fan.
- timeline: volley every 5 frames (`+0x10` = 4), 40 times (harder 60). Every 12th volley (`+0x16`: 12 -> 0, reload 12,
  harder 9 for later fans; first still 12) also fires the fan.
- volley, per gun: bullet P: fine aim (`59C58`) from point (gun + E1) to the target, **minus** `+0x1A`; bullet Q:
  fine aim from (gun + E2), **plus** `+0x1C`; kind 3, speed 0, maker `50F86` (`03278E-0327F4`). `+0x1A` and `+0x1C` both
  decrease by 1 per volley from 0x30 down to 0x10 and then stay (`0327FE-03280C`): the arms close in from +-48 to
  +-16 of 256 either side of the aim over 32 volleys. Harder: both get the same wobble word added
  (`032792-0327E0`: table[`+0x18`], `+0x18` -= 2 per volley, wraps to 0x34).
- fan (every 12th volley): fine aim from (gun + E0); 5 bullets at a, a+1, a+2, a-1, a-2 (of 256), kind 0, speed +2,
  maker `50F94` (`03272C-032770`). (rec: n 4 x 11, n 14 on the 12th, x 3, then 4: 11+1+11+1+11+1+4 = 40.)

### A2. "Three-block coarse volley burst" (selector 2; `032814-0329DA`)
- emitters: both guns, three muzzles per burst (below), mirrored between the guns.
- timeline: three bursts in this order of `+0x12` counts: at count 3 fire (parity gate), reload `+0x10` = 0x44 (69
  frames); at count 2 fire (parity gate), reload 0x58 (89 frames); at count 1 **both guns fire, ungated**
  (`032814-03283C`: `cmpi.b #1,+0x12`). Parity gate: only the gun whose d7 equals word `+0x32(a5)` fires, and that
  word is toggled each frame (`bchg #0,+0x33(a5)` at `032A26`), so which gun fires is effectively random; the other
  just counts. Harder: 5 counts, reloads 0x3C / 0x50 (0x44 / 0x58 minus 8). (rec: 27, 27, 54 bullets at gaps 69, 89.)
- volley (per firing gun, coarse aim `59A36`, kinds 0, makers `50DDC`, angle of 64):
  - block A: aim from (place + E0): 5 bullets, speed -10, angles a, a-1, a-4, a+1, a+4 (units of 64, so 1 = 4/256).
  - block B: aim from (place + E1) (gun 2: E2): 10 bullets, speed -7: a-2..a-6 and a+2..a+6.
  - block C: aim from (place + E2) (gun 2: E1): 12 bullets in 4 columns a-10, a-4, a+4, a+10, speeds -10/-6/-2 at a-4 and
    -9/-6/-2 at the others (`032950-0329D4`).
  Not changed by the harder flag.

### A3. "Mouth fan" (after every gun attack; `032B16-032B90`, part body +0x160 / `a4`)
- emitter: body place + (0x0300, 0) (`032B7A`).
- timeline: starts when the attack's last volley is done (`0xB6(a4)` = 1, `032B10`), mouth animation `+0xB4` steps
  0x6F63 -> 0x6F6A every 3 frames (`+0xB2`/`+0xB3` = 2), then `+0xB8` counts 8 frames to the shot: once.
  Total 42-44 frames after the last volley (rec).
- volley: fine aim (`59C58` from the body) + r - 42; 13 bullets, +7 each (angles base .. base+84: a +-42 fan centered on
  the aim, shifted by r); kind 7; speed from table `032CFE` indexed by the attack *just ended* (`+0xB7`): 0 -> -3,
  1 -> +8, 2 -> -7; maker `50F94` (`032B84-032B8C`). (rec: (7,-3), (7,8), (7,-7); rel 0,7,...,84.)

### A4. "Pod straight-down singles" (kind 105, part pod+0x20; `033504-0335C2`)
- emitters: each pod, two muzzles alternating (`bchg #0,+1(a6)`): pod place + (-0x0DC0, +0x180) and pod place +
  (-0x0DC0, +0x180 - 0x340) (`033588-0335A2`). The pods move on an orbit (`0x27/0x28(a5)`, `033000-0332A4`, not
  reduced here), so the place must come from the pod's own state.
- timeline: only while armed (`+0x2A(a5)`), flag `(a4)` != 0, mouth open (`+0x4` = 0x67DC), pause off. A shot every
  17 frames (`+0x12` reload 0x10), 7 each (rec), both pods in the same frame; first shot 103 frames after the mouth
  fan (rec; the template wait is `+0x12` = 0x84).
- bullet: fixed angle 32 (of 64) = straight down the screen (128 of 256); kind 38 (0x26); speed -12 + (r >> 1) = -16..-9
  (rec); maker `50DDC`. No aim.

### A5. "Pod turret stream" (kind 105, part pod+0x40; `0335D0-03366E`)
- emitters: each pod's turret, muzzle on a 16-point ring of radius ~14 px (table `027D70`, longs (x,y):
  (896,0),(832,320),(704,576),(512,640),(256,640),(-128,640),(-256,448),(-448,320),(-512,0),(-448,-320),(-256,-448),
  (-128,-640),(256,-640),(512,-640),(704,-576),(832,-320)), entry index = ((angle + 2) & 0x3C) / 4 of the turret's
  heading (`0335D4-0335DE`, `033644-03364A`).
- timeline: 1 bullet per frame (`+0x1C` reload 1) from each pod, while armed and the flag set and `+0x28(a5)` = 0
  (the pod is not moving); ~45 frames, starting 8 frames after the last A4 shot (rec: 9946 -> 9954, 45 frames).
- aim: the turret heading (byte `+0x1B`, 64ths) is **slewed 1/64 (= 4/256) per frame toward a target** (`059BF6`);
  which target alternates by cycle (bit 0 of byte `+0x61` of the pod body, toggled at `033292`): mode 1 = ship (coarse
  aim, `059BE8`), kind 17, speed -9 constant; mode 2 = the point in words `+0x34(a5)` / `+0x36(a5)` (the pod body's
  anchor place delayed two frames, `032E50`), `059A64` then slew; kind 17, speed alternating -9 / -5 each frame
  (`bchg #1,+0x21(a6)`, `033658-033660`). (rec: ship-tracking stream angles stay within a few units of the aim;
  mode 2 sweeps (e.g. 44, 40, 36, 32, 28, 32 ... 60) with alternating speeds.) Maker `50DDC`.
- Fits only partly: the angle is a stateful slew, not a function of the frame.

### B1. "Phase 2 mouth tri-fan" (code only; `0324E2-03258C`, `10E73E` = 1)
- emitter: the mouth (`+0x6(a6)` of the body record), once harder flag is set; every `+0x1C(a6)` + 1 frames; `+0x1C` is
  reloaded with (time-out counter `+0x1A(a5)` >> 5), at least 4 (so the volleys come faster as the boss's 10800-frame
  clock runs down: 337 frames apart at the start, down to 5); initial 64 (`032546-032548`).
- volley: coarse aim (`59A58`); toggles `+5(a5)`; w = |r| (0..7); three fans at aim, aim+21 and aim+42 (of 64 =
  120 degrees apart), each of w+1 bullets at -w, -w+2, ..., +w (steps of 2 of 64); kind 7; speed +4 (+8 once the
  clock has under 0x1518 = 5400 frames left); maker `50DDC` (`03254C-032588`).

## 3. What does not fit the vocabulary (and what would be needed)

1. **Cross-object gating**: the guns are frozen by the pods' flags (`10E738`) and the pods are started by the body
   (`+0x24(a5)`, random 0/1/2). A timeline of one emitter cannot say "wait until the other object finishes": needs a
   sequence of segments with explicit waits (guns segment, mouth segment, pods segment), as in the cycle in section 1.
2. **Random is a shared table walk**: r uniform -7..7 (0 double weight) is faithful; exact replay needs the global
   counter. Random used: A0 base scatter 8r, A3 base shift r, A4 speed r>>1, B1 width |r|, selector draw
   (from a 64-entry 0/1/2 table, never the same twice).
3. **Counted, state-dependent turns**: A1's two aimed arms start +-48 and close to +-16 by 1 per volley (reset per
   attack), harder adds a 26-step triangle wobble; the generator is "aim -/+ sweep(n)", with n per gun.
4. **Aim from a muzzle, not from the emitter**: A1 and A2 aim from muzzle points offset from the gun, per block;
   A0/A5 from the gun / turret. The shape needs "aim from offset point".
5. **Parity-gated gun** (A2, counts 3 and 2): which gun fires depends on a per-frame toggle, effectively random;
   model as "one gun, chosen at random" (the ungated count-1 burst fires both).
6. **Stateful turret slew** (A5): angle changes by 4/256 per frame toward a target (ship or the boss's delayed place),
   muzzle on a ring that follows the heading, alternating speeds -9/-5.
7. **Loop 2** is a maker property (doubling at speed +6, or +-8 for `50F86`), not part of any attack: put it in the
   bullet's maker entry.
8. **Phase 2 numbers** (B1 and the longer harder counts 80/60/5, interval 6, speed -6, pauses 0x60) are read from
   code and not confirmed by any recording; the pod life (0xE000) and the point where the pods go depend on damage,
   so Dodge should offer phase 2 only as an explicit choice.
9. Emitter motion (wings swing, pods orbit) is in code at `05968A` / `059980` / `0332A4` and was not reduced; take
   emitter places from the recording or move the emitter through a table.

## Rec summary (loop 0, r1; loop 1 doubles every bullet as above)

| step | frames after previous |
|---|---|
| gun attack 0: 40 x A0, 8 apart | 320 |
| A3 fan | +42 |
| pods: 7 x A4 at 17 apart | +103 first |
| A5 stream | +8 after last A4, ~45 |
| next gun attack | +161 after the guns are released |
| attack 2: A2 at 0, +69, +158; attack 1: 40 x 5 apart | |
