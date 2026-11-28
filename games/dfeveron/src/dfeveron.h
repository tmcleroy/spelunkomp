// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Dangun Feveron (dfeveron, the Japanese set of 98/09/17): what is where in its memory, with how each was found.
// (The training facts found 2026-11-11 by RAM dumps of headless runs, the stages played through with the safe time
// held, and the code that reads each word: the same main loop of tasks, ship record and hit test as DoDonPachi's.)
#pragma once

#include <cstdint>

namespace spelunk::dfeveron {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the bytes that went 0, 1, 2, 3; of those, the first
// of the two a word apart, as in DoDonPachi; kept at 1, Start begins a game)
constexpr uint32_t kCredits = 0x101731;

// --- A game's beginning ---
// The start of a game for player d7 (the credits taken, kMode set to 0x0B: DoDonPachi's 00456E in another order). The
// attract show's demonstration does not come here
constexpr uint32_t kNewGame = 0x006532;
// What the game is doing (a word): 0x0B as a game begins (the ship select), 7 while one is played, 8 while a continue
// is offered
constexpr uint32_t kMode = 0x1050EA;
constexpr uint32_t kModePlay = 7, kModeContinue = 8;
// The main loop's tasks: 0x4B records of 0x40 bytes from 100002, task n running the routine at 000442 + 8n (000BF8).
// Task 1 is the stage (003C30): its first state (003808) sets a stage up from the stage number
constexpr uint32_t kBeginStage = 0x003808;
// The game's flags (a byte): bit 0 set when a stage's boss is beaten (0134AC, with bit 5 of each ship's first byte and
// bit 3 of kBossFlags); the main loop (0029C8) then starts task 0x47, the stage's end (its last part 001692: kStage one
// more, past the fifth kLoop one more, and task 1 started for the next stage)
constexpr uint32_t kFlags = 0x1050E8;
constexpr uint32_t kBossFlags = 0x10E960;
constexpr uint8_t kStageBeaten = 0x01, kShipLeaves = 0x20, kBossGone = 0x08;
// The ship select's state (a word, +8 of its record: bit 8 set each frame it runs, 064DFE; cleared as it ends, 0652C4,
// and the first stage begins). Dangun Feveron has no carrier: the ship is on the screen from the stage's start. A later
// stage is reached through the game's own stage end all the same, begun as the ship select ends (runtime/shmup.h)
constexpr uint32_t kShipSelect = 0x10124A;
constexpr uint32_t kStage = 0x1052D8; // the stage being played, from 0 (a word: 0 to 4)
constexpr uint32_t kLoop = 0x1052DA;  // the round (a word: 0 the first time round)
// While a stage's boss is on (a word: 0x8000 from its coming (025612 and the like) to its end, 0x8100 as it is beaten;
// cleared as a stage begins, 003984)
constexpr uint32_t kBossOn = 0x105188;

// --- Player 1's record ---
// The ships, the one flown among them (a byte: 3 for a new game; a ship lost takes one, and at 0 a continue is offered)
constexpr uint32_t kShips = 0x1052C5;
// Each ship's record (0x40 bytes): flown while its first word is negative. At +0x25 the bombs left (3 for each new
// ship, from the record's pattern at 00A4A8; one more for each bomb picked up)
constexpr uint32_t kShip1 = 0x105A16, kShip2 = 0x105A56;
constexpr uint32_t kBombs = kShip1 + 0x25;
// The frames the ship is still out of harm for after it appears (a byte: 0xB4 as it comes back (00A7DA), counted down a
// frame at a time; 0xFF while a bomb goes off, not counted; while it is not 0 the ship's task clears the hit the test
// marks: 00A8C6)
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;

// --- What hits the ship (DoDonPachi's test in all but its places: 009FB6 for the bullets, run for each ship flown, a4 its
// record; 009F36 for the bodies, from 009B60 for one ship a frame) ---
// Positions are in 64ths of the board's pixels, across the board's 320 and down its 240. A record's position is the
// words at +6 across and +8 down, and its box goes from the position less the word at +0x12 to the position plus the
// word at +0x10 across, less +0x16 to plus +0x14 down (both ends in it). A hit sets bit 4 of the ship's first byte
constexpr int kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets: records of 0x40 bytes from here, 80 of them (160 while the byte kManyBullets has bit 7: the test's count),
// each a point: it hits a ship when it is inside the ship's box and its first byte has 0x80 and none of the bits 0x51
constexpr uint32_t kBullets = 0x109B72;
constexpr int kBulletSize = 0x40, kBulletCount = 80, kBulletCountMany = 160;
constexpr uint32_t kManyBullets = 0x10E800;
// The enemies whose bodies hit the ship: 120 records of 0x20 bytes from here, each with a box; one hits when its first
// byte has 0x80 and 0x40 and the boxes meet, both moved by 0x2800 and the enemy's left edge then below 0x7800. Not
// tested while a bomb goes off (the byte kBombOn not 0) or while the ship's safe time is 0xFF
constexpr uint32_t kEnemies = 0x1075F2;
constexpr int kEnemySize = 0x20, kEnemyCount = 120;
constexpr uint32_t kBombOn = 0x10E722;

// --- What the player's shots hit (009B60, for the ship of the frame: 000CA2 takes them in turn, a0 its shots, a3 its
// formation's, a4 its record) ---
// The enemies the shots can hurt: those records whose first byte has 0x80 and 0x20 (009B96; the enemy's left edge, moved
// by 0x2800, below 0x7800). One without 0x40 never hits the ship: a tank, a turret, a thing on the ground (found: such
// records sit under the tanks of stage 1). Tested while a bomb goes off too
constexpr uint8_t kShootable = 0x20;
// Each ship's shots: 19 records of 0x30 bytes, each with a box as an enemy's; one is tested while its first word is
// negative and its power (+0x18) is not
constexpr uint32_t kShots1 = 0x105A98, kShots2 = 0x105E28;
constexpr int kShotSize = 0x30, kShotCount = 19;
// The shots of the ship's formation (the second weapon): records of 0x20 bytes from 0x20 past these, as many as the
// ship's kind says (its byte at +0x1E: 0, 8 or 0x10, choosing the test at 00A0DA, 00A1CA or 00A35C: 19, 18 or 8 of
// them), each with a box as an enemy's, tested while its first byte has bit 7 (and in the first kind not bit 0: it
// has hit)
constexpr uint32_t kFormation1 = 0x10673A, kFormation2 = 0x1069BA;
constexpr int kFormationSize = 0x20, kShipKind = 0x1E;
constexpr int kFormationCount[3] = {19, 18, 8};

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 from the code that keeps each, checked in a RAM log of
// headless play) ---
// The rank (a byte, 0 to 63: the enemies' bullets' speed), made again every 16th frame by 003F62 as the stage's byte
// of a table (its address in the long at 1015AE: 12, 22, 22, 42, 52 for the stages on this board's settings) + for each
// ship flown twice its words at +0x2A and +0x2E less 2 (not below 0) and its bombs (kBombs) + the frames played over
// 2,048 (the long at 1052D0) + the ships left (kShips) + the byte at 1052C7, cut to 63; 63 all through the second
// round (1052DA not 0). The game keeps a half and a quarter of it too (1052CB, 1052CD). Found: 18 = 12 + 3 bombs + 3
// ships at the start of a game in a RAM log, then 29, 33, 35 as the ship was powered up
constexpr uint32_t kRank = 0x1052C9;
constexpr int kRankMost = 63;
// The cyborgs taken this game (32 bits: one more for each, 06D426, but not in the time attack's stage 6; at 2,000 the
// extend is given, bit 0 of 105112. Found: it counted up one at a time in the log, 66 in a stage and a half)
constexpr uint32_t kCyborgs = 0x10510C;
constexpr int kCyborgsExtend = 2000;
// Player 1's score (a long in BCD, tens: the high score 150,000 is 15000 at 103DF0 beside it; 0067E4 clears both
// players' as a game begins)
constexpr uint32_t kScore1 = 0x103DE8;

} // namespace spelunk::dfeveron
