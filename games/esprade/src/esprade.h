// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// ESP Ra.De. (esprade, the World set of 1998 4/22): what is where in its memory, with how each was found.
// (The training facts found 2026-11-11 by RAM dumps of headless runs, the stages played through with the safe time
// held, and the code that reads each word: the same main loop of tasks and hit test as DoDonPachi's.)
#pragma once

#include <cstdint>

namespace spelunk::esprade {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the bytes that went 0, 1, 2, 3; of those, the first
// of the two a word apart, as in DoDonPachi; kept at 1, Start begins a game)
constexpr uint32_t kCredits = 0x101227;

// --- A game's beginning ---
// The start of a game for player d7 (the credits taken, kMode set to 0x0B: DoDonPachi's 00456E in another order). The
// attract show's demonstration sets its game up from a table of its own (0042E2)
constexpr uint32_t kNewGame = 0x0041E4;
// What the game is doing (a word): 0x0B as a game begins (the character select), 7 while one is played, 8 while a
// continue is offered
constexpr uint32_t kMode = 0x10240A;
constexpr uint32_t kModePlay = 7, kModeContinue = 8;
// The main loop's tasks: 0x3C records of 0x40 bytes from 100002, task n running the routine at 00043A + 8n (00090C).
// Task 1 is the stage (002F56): its first state (002B7E) sets a stage up from the stage's map number (kMap)
constexpr uint32_t kBeginStage = 0x002B7E;
// The game's flags (a byte): bit 0 set when a stage's boss is beaten (003C36, with bit 5 of each character's first byte
// and bit 3 of kBossFlags); bit 1 set as the stage's end begins (001318), both cleared as it ends (001660: kStage one
// more, the next map taken from the order (kOrder), and task 1 started for the next stage)
constexpr uint32_t kFlags = 0x102408;
constexpr uint32_t kBossFlags = 0x10DD1E;
constexpr uint8_t kStageBeaten = 0x01, kShipLeaves = 0x20, kBossGone = 0x08;
constexpr uint32_t kStageEndSound = 0x10DD28; // (set to 1 with them)
// Set to 1 by a new game (004E3A) while the characters are brought in, before they are the player's (each character's
// task waits while it is: 0068EE; cleared at 0349B8). As in DoDonPachi, a later stage is reached through the game's own
// stage end from the stage before (runtime/shmup.h)
constexpr uint32_t kShipWaits = 0x10DD12;
// The stages played so far (a word: 0 for the first, 5 for the sixth; the stage's end goes on to the ending at 6). It
// counts: which stage's map is played is kMap, taken at each stage's end from kOrder, a pointer (32 bits) into a list
// of the maps in the order played (at 004FCA, chosen as a game begins by the first stage the player picks: 004ED0)
constexpr uint32_t kStage = 0x1025A6;
constexpr uint32_t kOrder = 0x1025A8;
constexpr uint32_t kMap = 0x102566;
// While a stage's boss is on (a word: 0x8006, the first word of task 6's record, which the bosses are made in; 0 through
// the stages and their mid-bosses. Found by the stages played through: on from the boss's coming to its end in the first
// three, never on before)
constexpr uint32_t kBossOn = 0x100182;

// --- Player 1's record ---
// The characters left, the one played among them (the low byte of a word: 3 for a new game; one lost takes one, and at
// 0 a continue is offered)
constexpr uint32_t kShips = 0x10254F;
// Each character's record (0x50 bytes): played while its first byte is negative (0x40 in it: not tested for hits)
constexpr uint32_t kShip1 = 0x10270A, kShip2 = 0x10275A;
// The frames the character is still out of harm for after it appears (a byte: 0xF0 as it comes back (006750), counted
// down a frame at a time; 0xFF while a bomb goes off, not counted; while it is not 0 its task clears the hit the test
// marks: 0069D4)
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;
// The guard barrier's gauge (a word at +0x46 of the record: 0xFA0 full, as a new game begins; the barrier, button 3,
// is put up only while it is not 0 (006CFA): a press takes 0x400 from it over the next frames (006D56, 006CDC) and the
// barrier held takes 4 a frame (006DC0); the gems (0x1026F8) fill it while it is short of full (0009AE), and the
// stage's tally counts it into points (001748). Found 2026-11-11: button 3 pressed in play, the run diffed against one
// without it (0x0FA0 to 0x0B90 a few frames later, with bit 7 of +0x3F, the barrier on), then the code that reads it.
// Round 1's button 2 is the laser, not a bomb)
constexpr uint32_t kBarrier = kShip1 + 0x46;
constexpr uint32_t kBarrierFull = 0xFA0;

// --- What hits the character (the game's own test, 005E72, a4 its record: player 1's and player 2's on alternate
// frames, 000BFA) ---
// Positions are in 64ths of the board's pixels, across the board's 320 and down its 240. A record's position is the
// words at +2 across and +4 down, and its box goes from the position less the word at +0x12 to the position plus the
// word at +0x10 across, less +0x16 to plus +0x14 down (both ends in it). A hit sets bit 4 of the record's first byte
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (005E8E): records of 0x40 bytes from here, 200 of them (300 while the word kManyBullets is not 0), each a
// point: it hits when it is inside the character's box and its first byte has none of the bits 0x59 (a record not in
// use is at 0, 0, where no box reaches)
constexpr uint32_t kBullets = 0x106E5C;
constexpr int kBulletSize = 0x40, kBulletCount = 200, kBulletCountMany = 300;
constexpr uint32_t kManyBullets = 0x10DC7A;
// The enemies whose bodies hit (005F72, when no bullet has): 114 records of 0x20 bytes from here, each with a box; one
// hits when its first byte has 0x80 and 0x40 and the boxes meet, both moved by 0x2800 and the enemy's left edge then
// below 0x7800
constexpr uint32_t kEnemies = 0x10449C;
constexpr int kEnemySize = 0x20, kEnemyCount = 114;

// --- What the player's shots hit (the same routine on, 00606A: a0 the character's shots, a2 its laser) ---
// The enemies the shots can hurt: those records whose first byte has 0x80 and 0x20 (the shots test the first 80 and the
// other 34 for it: 00609E, 00619A). One without 0x40 never hits the character: a tank, a turret, a thing on the ground
// (found: such records sit under the tanks of stage 1)
constexpr uint8_t kShootable = 0x20;
// Each character's shots: 40 records of 0x30 bytes, each with a box as an enemy's; one is tested while its first word is
// negative and its second byte has bit 0, or has not bit 7 (set by a hit) with its power (+0x18) not negative
constexpr uint32_t kShots1 = 0x1027AA, kShots2 = 0x102F2A;
constexpr int kShotSize = 0x30, kShotCount = 40;
// The laser (button 2): up to 7 boxes of 0xE bytes (00634A; the first whose across word is 0 ends them), each the words
// across (+0) and down (+2), and its box from +4 less +6 to +4 across, +8 less +0xA to +8 down, from them
constexpr uint32_t kLaser1 = 0x1037EA, kLaser2 = 0x10384C;
constexpr int kLaserSize = 0xE, kLaserCount = 7;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 from the code that keeps each, checked against RAM logs
// of headless play) ---
// Player 1's score (a long in BCD, tens: the last digit shown is always 0; player 2's follows at +4. Found: 523 and
// 6,436 in the log as the picture showed 5,230 and 64,360; the stage's tally and the extends read it: 005332)
constexpr uint32_t kScore1 = 0x1022E4;
// Player 1's score item counter, the number under the lives (a word: added to by each score item taken, 04AB8C; drawn
// by 04F79C. Found: 123 written there drew 123), and the most it goes to before the counter is full (a word: 200 at a
// game's beginning, 001C26, 100 more each time a full counter has run out, 0009CA)
constexpr uint32_t kItems = 0x1026F8, kItemsMost = 0x102704;
// While the counter is full (a word, not 0: set to the most as the counter fills, 04ABA2; then it and the counter go
// down by one for each energy item it pays for while the barrier's gauge is short of full, 0009D2, and the counter
// begins again at 0 when the gauge is full, 04B17A)
constexpr uint32_t kItemsFull = 0x1038AE;
// Player 1's multiplier: the time it lasts (a word, frames, counted down each frame, every 4th with the laser out, every
// 16th while 10376A is set: 000AA4; set to at least 70 by a hit with the power shot's bubbles on, 041FA6) and the
// multiplier less one (a word: the bubbles on the enemy that began it, 0 to 15, the score added that many times more:
// 042038). Found: 70 counted down to 0 a frame at a time in the log, with 1, 2, 3 and 7 as the multiplier
constexpr uint32_t kMultiplierTime = 0x1026FC, kMultiplier = 0x1026FE;

} // namespace spelunk::esprade
