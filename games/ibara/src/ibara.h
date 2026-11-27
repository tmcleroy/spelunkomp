// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Ibara: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::ibara {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C26C878u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C04A0AAu;

// --- Player 1's ship (a record of 0x280 bytes at 0x0C283794; player 2's follows; found 2026-11-11 from the code that
// loses a ship, 0x0C066270, and the per-frame routines the ship's state picks, a table at 0x0C16D130) ---
constexpr uint32_t kShipRecord = 0x0C283794u;
constexpr uint32_t kShipState = 0x0C28379Cu; // (32 bits) 1-9 flown (the ship's lean), 0x10 more as it is hit, 0x20 lost
// The ships left besides the one flown (a byte, record +0x253: set from the board's settings as a game begins, one
// taken as a lost ship comes back, 0x0C0663C0; with none left a lost ship ends the game)
constexpr uint32_t kShips = 0x0C2839E7u;
// The bombs in stock (a byte, record +0x256; 0 as a game begins), and the bomb gauge after it (+0x257, up to 40): the
// bomb button (0x0C062550) uses one from the stock, or with none, the gauge's worth as a weaker bomb
constexpr uint32_t kBombs = 0x0C2839EAu;
// A byte (record +0x269) that is two things: while the ship is flown, the frames it is still out of harm for (the hit
// test, 0x0C05F310, lets the ship be only while it is 0; counted down, the ship blinks), and while it is lost, the
// frames counted up to 60 before the next comes. Held at 59 it is both at once: nothing hits a ship, and a ship lost
// before it was held still comes back (the first finding, 0x0C2839FC held at 137 as 16 bits, kept a ship lost at that
// moment from ever coming back)
constexpr uint32_t kSafeFrames = 0x0C2839FDu;
constexpr uint32_t kSafeFramesHeld = 59;

// --- The game's script (found 2026-11-11 from the stage counter's writers) ---
// The game is run by a script of its own: a word 0xFFnn is command nn (the commands' routines are a table at
// 0x0C163354), its arguments follow. kScript points at the script's next word. A real game's script (from 0x0C1EEEC8)
// is a list of "begin stage" commands (FF 02, 00 ss, 00 00: six bytes each), the stage's own script number ss in the
// order 0 (the ship's arrival), 1, 3, 4, 2, 5, 6 (the six stages); each runs that stage's script and comes back.
// kBeginStage is command 2's routine: kScript then points at its argument word; it takes the number, adds one to
// kStagesBegun (0 as a game begins; one more than the stage played from then on, which the game reads for the stage's
// number) and calls. The attract show's demonstration plays a stage without it (kStagesBegun stays 0).
constexpr uint32_t kBeginStage = 0x0C061550u;
constexpr uint32_t kScript = 0x0C2859A0u;
constexpr uint32_t kStagesBegun = 0x0C392B08u; // (a byte)
constexpr uint32_t kStageScript = 0x0C22486Eu; // the stage script being played (a byte)
constexpr int kBeginStageSize = 6;              // (one "begin stage" command, with its arguments)
// The sound chip's sequences a boss comes on to: 9 as it arrives and 10 its theme in stages 1 to 5, 7 and 8 the last
// stage's (found by listing the sequences started through stages 1 to 4 played with the ship out of harm, and stages 4
// to 6 begun on their own; each stage has its own tune besides: 15/16, 19/20, 21/22, 17/18, 23/24, 25/26). 33 after
// the last boss's is not one
constexpr int kBossTunes[] = {9, 10, 7, 8};

// --- What hits the ship (found 2026-11-11 from the code that loses a ship, 0x0C066270, and its two callers) ---
// Every box is a half-size about a point: two 32-bit words (16.16) across and up the list, the first with flags in its
// top four bits, then where the box is from its owner's position (16.16, two words). A list of them ends with a word 0.
// Positions are 16.16: x across the board (the game's up), y down it (the game's across) in a field wider than the
// screen, shown from kCamera on (16.16). A box hits another when they overlap: |x - x'| < w + w', |y - y'| < h + h'.
constexpr uint32_t kCamera = 0x0C3929E0u;
// The ship's boxes: the table at 0x0C16BE68 by the record's +4 (8 bytes an entry; its first word a table by the state,
// +8, 16 bytes an entry, its first word a table by +12, 8 bytes an entry, whose second word is the list); position at
// record +16, +20. A box with flag 0x20000000 is the graze box (a bullet there scores); the others are the ship's hit
// box. The ship is tested while record +0 has 0x04000000 (bullets) or 0x08000000 (bodies)
constexpr uint32_t kShipBoxes = 0x0C16BE68u;
// The bullets (0x0C05F460): 256 records of 84 bytes from kBullets, in use while +0 has 0x10000000; the box list's
// address at +32, the position at +40, +44
constexpr uint32_t kBullets = 0x0C27E0FCu;
constexpr int kBulletCount = 256, kBulletSize = 84;
// The enemies whose bodies are tested (0x0C05F1E0): 192 records of 180 bytes from kEnemies, tested while +0 has
// 0x10000000 and the byte at +0xA9 is not below the one at +0xAB; the box list's address at +28, the position at +36,
// +40; a box with flag 0x20000000 does not take the ship
constexpr uint32_t kEnemies = 0x0C26CD40u;
constexpr int kEnemyCount = 192, kEnemySize = 180;

// --- What the shots hit (found 2026-11-12 from the tests the frame makes before the ship's, 0x0C05E6E0 the shots and
// 0x0C05EAA0 the other shots: each takes one player a frame, by the frame's low bit) ---
// An enemy is tested against the shots while +0 has kShootable and the same bytes as for the bodies allow; every box
// of it is tested, those with the flag 0x20000000 stopping a shot without being hurt. One whose +0 lacks 0x10000000
// (its body never hits the ship) is a ground one
constexpr uint32_t kShootable = 0x02000000u;
// A shot: a record of 52 bytes, tested while +0 has 0x08000000; its boxes from kShotBoxes by +4, +8 and +12 as the
// ship's are from kShipBoxes by its +4, +8, +12; its position at +16, +20 (as the ship's)
constexpr uint32_t kShotBoxes = 0x0C16BEACu;
constexpr int kShotSize = 52;
// The shots: 16 for each player from kShots (player 2's 0x340 on); the other shots: three groups of 16 for each from
// kOtherShots (0x340 apart, player 2's 0x9C0 on). Found by watching a shot's +0 set as it is fired and cleared as it hits
constexpr uint32_t kShots = 0x0C285318u, kOtherShots = 0x0C21F8A0u;

// --- What the gadgets read (port/gadgets.h). From the MAME layout for the game linked from epozzobon.it/re, its
// places for the first program (MAME's ibarao) less 0x30 for this set's later one (the layout's own test), checked
// 2026-11-27 against RAM dumps here at frames 3,500 and 5,000 of a game: the rank 19.7% then 21.5%, the drop's place
// moving on ---
// The rank (a long, counted DOWN as it rises: the layout shows 1 - rank / 0x1000000)
constexpr uint32_t kRank = 0x0C27DFF8u;
// The next drop (a byte: its place in the game's round of 25: shot, medal, shot, medal, option, four times over, then
// shot, medal, shot, medal and a large shot)
constexpr uint32_t kNextDrop = 0x0C275DC8u;
// The next medal's worth (a byte: 0 to 8 for 100 to 900, 9 to 18 for 1,000 to 10,000)
constexpr uint32_t kNextMedal = 0x0C392D78u;

} // namespace spelunk::ibara
