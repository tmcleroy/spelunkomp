# DoDonPachi's bullets: the kinds, the units, how they move and hit

Everything here is from `work/ddonpach/program.lst` / `program.bin` (68000). Addresses are program addresses. What was
checked against the running game (headless dumps of RAM at frames 3810, 3811, 3812 of a normal run, bullet records at
106CB6) is marked **[run]**; what was only read from the code is marked **[code]**. The velocity formula (section 2) was
also checked against the game's own speed tables for all 64 speeds x 256 angles (no difference).

## 1. The record, the maker, the table

### The maker (what 050DE4 / 050F9C do; 054E22 / 054EF4 in the second loop, same thing)
`050E62` (the 64-angle maker's store, `05101A` the 256-angle one) finds a free record in the bullet array at
`106CB6` (0x40 bytes each; 225 records, 245 in loop 2), then copies a 16-byte kind record into it:

- d0 low 6 bits = kind. The table `05104E` is **40 pointers, not 64** (kinds 0..39; its 16-byte records start at
  `0510EE`; pointer 37 and 38 are the same record). Kinds 40..63 do not exist: nothing may use them.
- d0 high word (signed) = what is **added** to the kind's own base speed (the kind record's word at +12). The final
  speed index is `s = (base + add) & 0xFF`; only 0..63 are real (the speed table has 64 entries).
- d1 low byte = the angle (of 64 for `050DE4`, of 256 for `050F9C`, which also sets bit 0x0200 of the record's first
  word to say so).
- d2 = place: x in the high word, y in the low word, in 64ths of a pixel.
- d3 = parent record (0 = none), d4 = offset from the parent (x high word, y low word): the bullet is put at
  `parent+6` + d4 and keeps `d3` at +0x20 and `d4` at +0x24. Only kinds 13, 24, 25, 26, 27 (and 9, see below) stay on
  the parent for a while; every other kind ignores it.

### The bullet record (0x40 bytes) **[run]**
| off | what |
|---|---|
| +0x00 word | state. Byte 0 bits: 0x80 alive, 0x40 cancelled (popping into its end animation), 0x20 (set while popping), 0x10 hit the ship / to be removed, 0x02 angle is of 256, 0x01 *first frame* (the spawn flash). Byte 1 = kind (0..39); its bit 0x80 (kinds 10,14,15,16,19,20,23) = the kind moves itself each frame (see 4). |
| +0x02 byte | palette (colour set) of the sprite; +0x03 byte priority 0x30, bits 0x0C = flip across / down |
| +0x04 word | sprite number (the animation frame; the kind's handler steps it) |
| +0x06 word | **x** (the board's "across", 0..0x5000 = 0..320 px, 64ths of a pixel) |
| +0x08 word | **y** (the board's "down", 0..0x3C00 = 0..240 px) |
| +0x0A word | sprite size in 16 px tiles (high byte across, low byte down): 0x0101 = 16x16, 0x0202 = 32x32 |
| +0x0C, +0x0E words | sprite top-left offset from the place (0xFE00 = -8 px, 0xFC00 = -16 px: the sprite is centred on the place) |
| +0x10..+0x1E | scratch the kind's handler uses (animation counters, state bits) |
| +0x1A byte | speed index s (0..63) |
| +0x1B byte | angle (64 or 256 per bit 0x0200 of +0x00; after the first frame kinds 10,14,15,16,19,20,23 read it as 64) |
| +0x20 long, +0x24 long | parent record, offset from it |
| +0x2A word, +0x2C word | velocity per frame (dx, dy) in 64ths of a pixel, computed once at spawn (plain kinds) |
| +0x3C long | the kind's per-frame routine (a pointer) |

Screen orientation (ddonpach.h): the board is sideways. Angle 0 = +x = **up** the screen, a quarter turn (64 of 256,
16 of 64) = +y = **right**. In a web preview with screen_x = y, screen_y = 320 - x (x/y in pixels) angle 0 is up and
angles grow clockwise.

## 2. Speed and angle to pixels per frame (exact)

Per frame (see 3 for what else happens) a plain bullet's place changes by (dx, dy) in 64ths of a pixel:

```
s   = (kindBase + addFromD0) & 0xFF        // 0..63 valid;  s/8 pixels per frame:  s=22 -> 2.75 px/frame
a   = angle256   (angle64 * 4 for the coarse maker; the game does &0x3F first for 64 angles)
i   = a & 63;  if (i > 32) i = 64 - i      // 0..32
bx  = trunc(128*cos(2*PI*i/256));  by = trunc(128*sin(2*PI*i/256))   // the table: 33 pairs, exactly these
x   = (s*bx) >> 4;  y = (s*by) >> 4        // arithmetic shift; = floor(s*bx/16)
switch (a >> 5) {                          // octant
  case 0: dx= x; dy= y; break;   case 1: dx= y; dy= x; break;
  case 2: dx=-y; dy= x; break;   case 3: dx=-x; dy= y; break;
  case 4: dx=-x; dy=-y; break;   case 5: dx=-y; dy=-x; break;
  case 6: dx= y; dy=-x; break;   case 7: dx= x; dy=-y; break; }
```

This is the game's table walk (`052FEC`, 256 path `0532B4`; loop 2 `056E4E`, `057116`; the self-moving kinds `0596C4`),
exactly: speed-pointer table `05AAFA` (64 pointers, each to 33 pairs of longs = s*bx, s*by), offset table `053074`
(index a: 8*i), then `asr.l #4`, then the octant swap/negate on the low words. Verified for all s (0..63) and a (0..255).
So the speed is about `8*s` 64ths of a pixel = `s/8` px per frame, magnitude within ~1% of the circle (the table is truncated
to integers, so use the formula above rather than a plain `cos`). Angle 0 gives (8s, 0); 64 gives (0, 8s); 128 (-8s, 0);
192 (0, -8s).

A frame is one 60 Hz-ish game frame (the game runs the loop once per frame; counts below are frames).

Base speeds by kind (s, px/frame): 22 (2.75) for most; kind 10: 16 (2.0); 11, 12: 24 (3.0); 13: 0 (stays);
14: 32 (4.0); 15, 16: 16 (2.0); 19, 20, 23: 1 (0.125, they accelerate themselves); 34, 39: 18 (2.25). Callers add
d0's high word (e.g. `#$10004` = +1 on kind 4; `#$FFEB000B` = -21 on kind 11 = s 3).

## 3. Per frame, for every bullet (loop `051390`; loop 2 `0552EA`)

Order of a frame for a live bullet (checked in the run: bullets 0..5, position change = predicted dx, dy):

1. If a bomb/stage-end flag (`10C976`, `10E858`) is set the bullets are cleared.
2. **First frame (byte 0 has 0x01):** the velocity is computed (section 2) and stored at +0x2A/+0x2C, the kind's
   routine runs once (`jsr` through the table at `05158E`, 40 entries, same order as the kinds) which draws the spawn
   flash (`05162E`: a small flash sprite from tables `052BEC`/`052CEC` chosen by the angle), sets the bullet's real look
   and animation and clears 0x01. **No movement and no hit test this frame.**
3. Later frames, **plain kinds** (all except the bit-0x80 ones): `x += dx; y += dy - S` where S is the word at
   `10E85C` (the playfield's sideways pan: it is 0 almost always and +-0x40 (1 px) now and then; it is the same for
   every bullet; ignore it in a preview or add `-S` to y). Then, if the new x is outside 0..0x5000 or y outside
   0..0x3C00 (unsigned compare: negative is outside) the bullet is removed, **the moment its place leaves the 320x240
   board** (the sprite's size does not matter). Then the kind's routine runs (animation, special motion).
4. **Self-moving kinds** (10, 14, 15, 16, 19, 20, 23: first byte bit 0x80): `y -= S`, same off-screen test, then
   each frame `x += dx(s, angle64)`, `y += dy(s, angle64)` computed afresh from the *current* speed byte and angle
   byte (`05968A`: angle & 0x3F, 64 directions, same table; no move if s = 0 or `101764` is nonzero), then the sprite
   is drawn, then the kind's routine runs and may change s / angle for the next frame.
5. A cancelled bullet (bit 0x40: bomb, the stage ending) stops moving and plays its end animation (frames `AE88`..`AE8D`,
   16x16 sprite) then goes.

Bullets are processed in slot order; the maker uses the first free slot (a bullet spawned by a bullet's routine in the
same pass can be handled in the same frame).

## 4. Hit box

**A bullet is a point**: its place (+6, +8). It hits the ship when the point is inside the ship's box (the game's test,
`0057EE`: both ships, each frame, every bullet):

```
x in [shipX - w12, shipX + w10]   y in [shipY - w16, shipY + w14]     (both ends inclusive, 64ths of a pixel)
```
with the words at the ship's record (ship 1 at `102C8C`, ship 2 at `102CCC`): +0x10 = 0x100 (4 px), +0x12 = 0xC0 (3 px),
+0x14 = 0xC0 (3 px), +0x16 = 0x40 (1 px; it was 0xC0 in the attract demo before the ship was flying, 0x40 in play). So in
play the box is x from -3 px to +4 px, y from -1 px to +3 px (about 7 x 4 px; x is up the screen).
A bullet whose byte 0 has any of 0x51 (spawn frame 0x01, cancelled 0x40, already hit 0x10) does not hit. The sprite
size, the animation and the 5x1-tile streak of kind 9 do **not** matter: only the centre point counts. (The game has
no grazing in this routine.) A hit sets 0x10 on the bullet and the ship.

## 5. The 40 kinds

`base` = kind's base speed index (px/frame = s/8). `look` = palette byte and sprite numbers (CAVE sprite numbers,
`+0x04`) the kind's routine sets; "dir" = the sprite is chosen by the angle: 9 frames for a quarter turn, mirrored for
the other quarters by flip bits (tables `0529F4`, `052BAC`), animated through 3 steps (+9 per step) where it says so.
All bullets here are points for hitting; size is the sprite's. Handlers = the address in the table `05158E`.

| kind | handler | base | look (palette; frames; size) | motion |
|---|---|---|---|---|
| 0 | 05166A | 22 | 35; 83B0..83B5 (6, +1 a frame); 16 | straight |
| 1 | 0516B4 | 22 | 1A; 83B6..83B9 (4); 16 | straight |
| 2 | 051700 | 22 | 35; 83BA..83BF (6); 16 | straight |
| 3 | 0527CA | 22 | 35; dir, 3 animation steps; 16 | straight |
| 4 | 05280E | 22 | 35; dir, 4 steps (table 052840); 16 | straight |
| 5 | 052852 | 22 | 35; dir, 4 steps (052884)..; 16 | straight |
| 6 | 052896 | 22 | 1A; dir, 3 steps (0528C8); 16 | straight |
| 7 | 05174C | 22 | 35; 8A57..8A5A (4); 16 | straight |
| 8 | 052914 | 22 | 35; dir, 3 steps (052944); 16 | straight |
| 9 | 051798 | 22 | 1F; 8AFC then a growing streak (table 051832, 7 steps) ending 5x1 tiles (80x16 px, long axis along x = up the screen), frames 850E+; | straight; **still a point for hits** (the streak is only a look) |
| 10 | 052A9E | 16 | 35 (flag 0x80); dir 84F3..84FB (9) +3 steps; 16 | **homing**: every 4th frame (`10126E & 3 == 0`) the angle (of 64) steps one toward the ship (`059BE8`), speed constant. No stop: it keeps homing |
| 11 | 051872 | 24 | 35; 85DE..85E0 (3, +1 every 2nd frame); 16 | straight |
| 12 | 0518AA | 24 | 35; 85E1..85E3 (3); 16 | straight |
| 13 | 051902 | 0 | 35; 853D.. (charging orb 16 -> 32 -> 48 -> 64 px, table 051A9E) | **stays on its parent** (d3/d4: place = parent + offset), plays the charge animation, then **bursts into a ring of bullets** (see 6). Does not move on its own |
| 14 | 051B6E | 32 | 35 (flag 0x80); dir frames 84F3..; 16 | speed falls by 1 each frame down to s = 2, then rises by 1 *every frame without end* (leaves the screen); the angle homes one step toward the ship every 2nd frame while the nearest ship is farther than 0x1000 (64 px; distance = max(|dx|,|dy|) + min/2 in 64ths, `059F88`) |
| 15 | 051C3A | 16 | 35 (flag 0x80); 8A89..8A8E (6); 32 | **curves**: target = angle + 0x14 (of 64); every 4th frame (`& 3 == 0`) the angle +1 until it equals the target (clockwise 20/64 = 112.5 deg over 80 frames), speed constant |
| 16 | 051CB4 | 16 | 37 (loop 2: 03); 8A5B..; table 051DE0; 32 | **stop, spin, drop**: speed -1 a frame to s = 4; the animation index +2 every other frame; when it reaches 0x10 with s = 4 -> state 2 (to 0x18, 5 steps) -> state 4 (to 0x3A, 18 steps) -> state 8: angle := 0x20 (= 128/256: straight *down* the screen), then s +1 every other frame up to 0x20 (4 px/frame). About 12 + 16 + 10 + 36 frames before it drops. Loop 2 (055C26): everything every frame, s +4 a frame up to 0x18 |
| 17 | 051E22 | 22 | 35; 8034..8039 (6); 32 | straight |
| 18 | 051E6E | 22 | 35; dir 84F3.. table (052B30), 3 steps; 32 | straight |
| 19 | 051EEE | 1 | 3D (flag 0x80); dir (table 052956); 32 | **accelerates**: s +1 every 4th frame without end; also y -= 1 px (0x40) a frame for the first 32 frames (a drift sideways on top of the motion); sprite turns as it goes |
| 20 | 051FFC | 1 | 3D (flag 0x80); same as 19 | as 19 but the sideways drift lasts 64 frames |
| 21 | 05212A | 22 | 32 (37 when stage `101978` > 3); 8A83..8A88 (6); 16 | straight |
| 22 | 052188 | 22 | 35; 8A89..8A8E (6); 16 | straight |
| 23 | 052B42 | 1 | 35 (flag 0x80); 803A..803D (4); 16 | **homing + accelerating**: angle one step toward the ship every 4th frame; s +2 every 8th frame, no end |
| 24 | 0521D4 | 22 | 08; 7160..7165 then 7165..7169; 16 | **attached then released**: for 10 frames (anim 7160 -> 7165, +1 every 2nd frame) it sits on its parent (place = parent + offset, updated every 2nd frame) with velocity 0, then flies with the stored (dx, dy) (anim loops 7165..7169) |
| 25 | 052250 | 22 | 08; as 24 (7165..716D) | as 24 |
| 26 | 0522D2 | 22 | 35; 716D..7171 (16), then 32x32 7179 | attached on the parent through two stages (anim 716D->7171 at +1, then 4 per step to 7179, every 2nd frame), then released with its stored (dx, dy); looks 7179..7199 loop at +4 |
| 27 | 0523A0 | 22 | 35; 7199..71AD (32x32) | attached to the parent with a growing animation 7199->719D, then released (anim 719D..71AD, a rolling start offset from `10E88A`) |
| 28 | 05243E | 22 | 35; 71AD..71CC (+2 every 2nd frame); 32 across x 16 down (0x0201) | straight |
| 29 | 052492 | 22 | 35; dir (table 0524FA); 16 | straight |
| 30 | 05254C | 22 | 35; 71F1 (pulsing 6 frames); 16 | straight |
| 31 | 0525B0 | 22 | 35; 71F5; 16 | straight |
| 32 | 052608 | 22 | 35; 71F9; 16 | straight |
| 33 | 05266C | 22 | 08; 71FD pulsing; 16 | straight |
| 34 | 051E22 | **18** | as 17 | straight (2.25 px/frame) |
| 35 | 0526DC | 22 | 08; 7207 pulsing; 16 | straight |
| 36 | 05274A | 22 | 35; dir (table 0527B8); 16 | straight |
| 37, 38 | 052968 | 22 | 08; dir (table 052994); 32 | straight |
| 39 | 0526DC | **18** | 08; 7207 pulsing; 32 (record 0x0202) | straight (2.25 px/frame) |

The records for 0..9, 12, 15..23, 33..35, 37..39 have 32x32 spawn size 0x0202 only for the *first frame flash*; the
size a bullet is drawn at afterwards is what its handler sets (16 for most).

For the preview, the motion classes are: **plain straight** (kinds 0-9, 11, 12, 17, 18, 21, 22, 28-39), **homing**
(10, 14, 23), **curving** (15), **stop-spin-drop** (16), **accelerating** (14 after its dip, 19, 20, 23), **attached to
the parent first** (13, 24, 25, 26, 27).

## 6. The two special ones

### Kind 13 (the ring bomb) **[code]**
Made with d3 = the boss part (so it sits on it) at base speed 0. Its routine `051926` runs a state machine on +0x10/+0x12
(table `051A9E`: sprite, size, offset, one entry per 8 bytes): +0x12 steps by 8 every other frame; state 1 (to +0x12 =
0x70, 14 steps) the orb charges while following the parent; state 2 (0x70..0x90, repeated `+0x14` = 4 times, then 9 for
the burst count) follows or flies; at +0x12 = 0x70 the speed is set to 8; state 8 (to 0xD0) is the burst: **32 bullets**
(`d7 = 0x1F`, 11 in stage 5 (`101978 == 5`)), each by calling `050DDC` with kind 0xB then 0xC alternately, a random
angle (`057EBA`: the table of random bytes at `057ECE`, used as the angle of 64), place = the orb's place - 4 px in x
(`d2 + 0xFF000000`), d0 high word starting at -21 (so s = 24-21 = 3) and rising by 1 per bullet (s from 3 to 34, 0.4 to
4.2 px/frame; stage 5: starts at -15). Loop 2 (`055850`): the orb advances every frame by 4, 16 (0x10) or 14 bullets
(0xD) instead of 32, speed steps of 2 (stage 5: 10 / 8 bullets), and in stage 5 the stored velocity is halved.
The orb is removed after the burst. Each burst bullet goes through `050DDC` so in loop 2 they come out doubled (7).

### Kind 9 **[code]**
Plain straight motion at base 22; its routine only animates a growing streak. It is not attached.

## 7. Loop 2 (word `10197A != 0`)

- The whole machinery is a copy: bullet records are made by `054E22` (64) / `054EF4` (256) into the same array
  at `106CB6` (245 records), tables are copies (`054FA8` pointers, same 16-byte records: checked equal), the speed
  and angle function is the same (`056E4E`, offset table `056ED6` = `053074`), the mover is `0552EA`, its routine
  table `0554E8` (same kind order, handlers `0555C4`.. `0569A6`).
  The handlers are re-written, mostly the same effect; the differences found (diff of the two loops'
  code): kind 13 (above), kind 16 (above), kinds 19/20 (counters at 0x18/0x2C kept in other words), kinds 24..27 (the
  parent-follow for 27 is gone and a call to a helper replaces it; read `0560DC`, `05615A`, `0561E4`, `0562AE` before
  trusting the 24..27 timings in loop 2), kind 16's palette 03.
- **The callers' bullet routines make several bullets in loop 2** (`050DDC` coarse angle, `050F94` fine angle; the
  others `050DA0/050F58`, `050DBE/050F76`, `050DCE/050F86`). With `F = byte +0xD of the calling task (a5) & 0x81` and
  `B = bit 0x02 of the byte at the address held in the caller's +6 (a5)`:

  | entry | loop 1 | loop 2 |
  |---|---|---|
  | `050DDC` / `050F94` | one bullet (`050DE4` / `050F9C`) | F != 0: B ? 3 bullets at s, s+5, s+10 : 2 bullets at s, s+6; F == 0: B ? 2 bullets fanned at angle-8 / angle+8 (of 256; for the coarse maker the angle is angle64*4) at s : 2 bullets at s, s+6 |
  | `050DA0` / `050F58` | one bullet | one bullet at s+4 |
  | `050DBE` / `050F76` | one bullet | 2 bullets at s, s+6 |
  | `050DCE` / `050F86` | one bullet | 2 bullets fanned +-8/256 at s |

  (all same kind, same place, same parent; the "s+5/s+6" mean the d0 high word + 5 / + 6, so the speed index; the extra
  bullets of the 64-angle maker are made with the 64-angle routine in the "line" cases and the 256-angle routine in
  the "fan" cases.)
- The slot limit for a maker call to fire: it stops when `1017BE`, `101764`, `10E878` are nonzero (bomb, ...) and when
  no free slot is found among the first 95 / 205 (150 / 245 in loop 2).

## 8. Gaps and cautions

- Sprite looks are given as the game's sprite numbers and palette bytes; the editor will not have the images, so a
  coloured dot sized 16 or 32 px (the kind's size) is the faithful stand-in. The palette byte is an index into the
  board's palette at `C00000` (0x35, 0x1A, 0x32, 0x08, 0x37, 0x3D, 0x1F, 0x03 appear).
- Kinds 10, 14, 15, 16, 19, 20, 23 read +0x1B as an angle of 64 each frame. A 256-angle maker with one of those
  kinds would be truncated to the low 6 bits; I did not find a caller that does that, but did not exhaustively check.
- The speeds of kinds 14, 19, 20, 23 grow without end in the code; they are off the board long before an index above
  63 (outside the table) is reached.
- Kinds 24..27 and 13 follow the parent record: the editor needs the emitter's place each frame during the first
  ~10 frames after firing.
- Not decoded line by line: the animation frame numbers of the directional kinds (3, 4, 5, 6, 8, 18, 29, 36, 37/38; the
  tables are at `052840`, `052884`, `0528C8`, `052944`, `052956`, `0524FA`, `0527B8`, `052994`), and the exact timings of
  the loop-2 kinds 24..27.
- Verified against the running game: the velocity formula, the add-speed-to-base rule, the speed/angle bytes at
  +0x1A/+0x1B, the `y += dy - S` update and the record layout (bullets of kinds 0 and 17 over three consecutive
  frames: observed position changes equal the predicted (dx, dy - S) in all 6 bullets). The special kinds (9..16,
  19, 20, 23..27) are from reading the code, not a run.
