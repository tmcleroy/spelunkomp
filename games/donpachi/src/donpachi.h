// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DonPachi (donpachi, the US set, Ver. 1.12): what is where in its memory, with how each was found.
// (The training facts found 2026-11-11 by RAM dumps of headless runs, a stage played through with the safe time held,
// and the code that reads each word. The program is DoDonPachi's ancestor: the same main loop of tasks, the same
// ship record, a hit test of the same kind.)
#pragma once

#include <cstdint>

namespace spelunk::donpachi {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the bytes that went 0, 1, 2, 3; of those, the first
// of the two a word apart, as in DoDonPachi; kept at 1, Start begins a game)
constexpr uint32_t kCredits = 0x10139B;

// --- A game's beginning ---
// The start of a game for player d7 (the credits taken, the ships and bomb stock set, kMode set to 0x0B; DoDonPachi's
// 00456E line for line). The attract show's demonstration sets its stage up from a table of its own at 033166
constexpr uint32_t kNewGame = 0x03301E;
// What the game is doing (a word): 0x0B as a game begins (ship select, the carrier), 7 while one is played, 8 while a
// continue is offered
constexpr uint32_t kMode = 0x1018EC;
constexpr uint32_t kModePlay = 7, kModeContinue = 8;
// The main loop's tasks: 0x3D records of 0x40 bytes from 100002, task n running the routine at 00043A + 8n (000842),
// started by 0040FC. Task 1 is the stage (02D50C): its first state (02D1C6) sets a stage up from the stage number
constexpr uint32_t kBeginStage = 0x02D1C6;
// The game's flags (a byte): bit 0 set when a stage's boss is beaten (020F66, with bit 5 of each ship's first byte:
// the ship flown off by itself); the main loop (034814) then starts task 0x3B, the stage's end (035256: the tally,
// with bit 1 set: 035260; its tally counts the bombs left down one by one into points, 0359F6; then 035CBC: kStage one more, past the fifth the next loop, and task 1 started for the next stage)
constexpr uint32_t kFlags = 0x1018EA;
constexpr uint8_t kStageBeaten = 0x01, kShipLeaves = 0x20;
constexpr uint32_t kStageEndSound = 0x108852; // (set to 1 with them)
// Not 0 from a new game's start until the first stage's carrier has launched the ship (the ship's task waits while it
// is: 02BBFE; the stage's first state reads it too). As in DoDonPachi, a later stage's number put in kStage as a game
// begins would give that stage without what stage 1's carrier brings: a later stage is reached through the game's own
// stage end instead (runtime/shmup.h)
constexpr uint32_t kShipWaits = 0x108846;
constexpr uint32_t kSection = 0x101A3E; // the part of the stage being played (a word; 0 as a stage begins)
constexpr uint32_t kStage = 0x101A40;   // the stage being played, from 0 (a word: 0 to 4)
constexpr uint32_t kLoop = 0x101A42;    // the loop (a word: 0 the first time round)
// The boss (a byte): 1 from the moment the stage's boss is made (0211FA, 026E98: the boss's record kept at 108838)
// until the stage ends, 3 once it is beaten; 0 through the stage and its mid-bosses. Read as a word with the byte after
constexpr uint32_t kBossOn = 0x108848;

// --- Player 1's record ---
// The ships, the one flown among them (the low byte of a word: 3 for a new game; a ship lost takes one, and at 0 a
// continue is offered)
constexpr uint32_t kShips = 0x101A2F;
// Each ship's record (0x40 bytes): flown while its first word is negative. At +0x23 the bombs left, at +0x25 what they
// are filled to at a new ship (the stock: 3 for a new game, up to 7 as the bomb pieces are picked up; kept at 101A8A)
constexpr uint32_t kShip1 = 0x102158, kShip2 = 0x102198;
constexpr uint32_t kBombs = kShip1 + 0x23;
constexpr uint32_t kBombStock = kShip1 + 0x25;
// The frames the ship is still out of harm for after it appears (a byte: 0xB4 as it comes back (02BADC), counted down a
// frame at a time; while it is not 0 the ship's task clears the hit the test marks: 02BC9C)
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;

// --- What hits the ship (the game's own test, 0055DA, a5 the ship's record: player 1's on even frames, player 2's on
// odd: 000938) ---
// Positions are in 64ths of the board's pixels, across the board's 320 and down its 240 (as in DoDonPachi). A record's
// position is the words at +6 across and +8 down, and its box goes from the position less the word at +0x12 to the
// position plus the word at +0x10 across, less +0x16 to plus +0x14 down (both ends in it). The test moves everything by
// 0x800 before comparing, as unsigned words. A hit sets bit 4 of the ship's first byte
constexpr int kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets: 75 records of 0x40 bytes from here, each a point; one hits when its first byte has 0x80 and 0x01 and not
// 0x10, and its point is inside the ship's box
constexpr uint32_t kBullets = 0x104E94;
constexpr int kBulletSize = 0x40, kBulletCount = 75;
// The enemies whose bodies hit the ship: 150 records of 0x20 bytes from here, each with a box; one hits when its first
// byte has 0x80 and 0x40 and the boxes meet (005700)
constexpr uint32_t kEnemies = 0x103814;
constexpr int kEnemySize = 0x20, kEnemyCount = 150;

// --- What the player's shots hit (the same routine on, 00576C: the player of the frame, the word at 0x108830 0x1000
// for player 1) ---
// The enemies the shots can hurt: those records whose first byte has 0x80 and 0x20 (the shots test the first 100 and
// the other 50 for it: 00577A, 005880). One without 0x40 never hits the ship: a tank, a turret, a thing on the ground
// (found: such records sit under the tanks of stage 1)
constexpr uint8_t kShootable = 0x20;
// Each ship's shots (0057BA): 33 records of 0x20 bytes, each with a box as an enemy's; one is tested while its first
// word is negative and has not 0x2000 (or 0x1000, its hit this frame) and its power (+0x18) is not negative
constexpr uint32_t kShots1 = 0x10225A, kShots2 = 0x10267A;
constexpr int kShotSize = 0x20, kShotCount = 33;
// The two options' boxes (two records 0x60 apart: 0059B2), as an enemy's, tested while their first word is negative and
// the ship's second byte has not bit 7
constexpr uint32_t kOptions1 = 0x102D1A, kOptions2 = 0x10303A;
// The laser (005AB4), tested while its first word is negative and bit 1 of its first byte set: from the across word at
// +2 to where it stops (+0x10: the left edge of the first enemy it meets, or 0x5400), up the word at +0xA and down +8
// from the word at +4
constexpr uint32_t kLaser1 = 0x10371A, kLaser2 = 0x10373A;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 from the code that keeps each, checked in RAM logs of
// headless play) ---
// The rank (a byte, 0 to 63: what the enemies' fire reads), made again as each part of a stage begins (02DAE2 on in
// the World set) as the stage's byte of a table (its address in the long at 10121E) + the stage + each ship's part
// (twice its word at 102178 plus the next, and an eighth of its count at 101754 past 20; player 2's at 1021B8 and
// 101772; half the two players' sum when both play) + 16 x the loop + the ships at 101A2F and 101A31 + the bytes at
// 10379B and 10379F + the frames played over 2,048 (the long at 101A38), cut to 63. Found: 13, 16, 20 in a RAM log of a game, each the sum of
// those parts at that frame
constexpr uint32_t kRank = 0x101A33;
constexpr int kRankMost = 63;
// Player 1's score (a long in BCD: 84,850 and 193,480 in the log as the picture showed them; the new game's routine
// sets it, and a continue leaves only its last digit, the continues)
constexpr uint32_t kScore1 = 0x10177A;

} // namespace spelunk::donpachi
