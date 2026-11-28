// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Guwange (guwange, the Japanese set of 1999 6/24): what is where in its memory, with how each was found.
#pragma once

#include <cstdint>

namespace spelunk::guwange {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the bytes that went 0, 1, 2, 3; of those, the first
// of the two a word apart, as in DoDonPachi; kept at 1, Start begins a game)
constexpr uint32_t kCredits = 0x201011;

// --- The training facts found so far (2026-11-11, by RAM dumps of headless runs and the code that reads each word) ---
// Each character's record (0x60 bytes, work RAM being at 200000 on this board): played while its first byte is negative
// (0x40 in it: not tested for hits)
constexpr uint32_t kShip1 = 0x2028DA, kShip2 = 0x20293A;
// The frames the character is still out of harm for after it appears (a byte: 0xF0 as it comes back (00B698), counted
// down a frame at a time; while it is not 0 the hit the test marks is cleared, as in ESP Ra.De.)
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;
// The bombs left (a byte at +0x1F of the record: 2 for a new game; a bomb used takes one, 015834. Found: two bombs used
// with the safe time held, the byte that went 2, 1, 0)
constexpr uint32_t kBombs = kShip1 + 0x1F;
// The game's flags (a byte): bits 0 and 1 set through a stage's end (001686, cleared at 0017CE), as in the other games
// of the board, whose tallies count the bombs left
constexpr uint32_t kFlags = 0x202642;

// --- A game's beginning, its stages and its bosses (found 2026-11-11: a run played through four stages with the safe
// time held, RAM dumped every 250 frames; then the interpreter's watch on the words that changed at a stage's end) ---
// The start of a game for player d7, from the title's start (005256: the credits taken, kMode set to 0x0B, the
// character select). The attract show's demonstration sets its game up elsewhere (00536E, from a table of its own)
constexpr uint32_t kNewGame = 0x0052DE;
// What the game is doing (a word): 0x0B through the character select, 7 while player 1 plays alone (6 player 2 alone,
// 001D06), 8 while a continue is offered (001DE8)
constexpr uint32_t kMode = 0x202644;
constexpr uint32_t kModePlay = 7, kModeContinue = 8;
// The main loop's tasks: 0x3C records of 0x40 bytes from 200002, task n running the routine at 00043A + 8n (000A34);
// a record's first word is 0x8000 + n while the task is on (06EBC2 starts one: d0 the task)
constexpr uint32_t kTasks = 0x200002;
// Task 1 is the stage (003EBC): its first state (0034DC) sets a stage up from the stage's number (kStage)
constexpr uint32_t kBeginStage = 0x0034DC;
// The stage being played, from 0 (a word: 0 to 6; past the seventh, the ending: 0017C2). Its only writers are the
// stage's end (0017A6) and the demonstration's set-up; a new game finds it cleared
constexpr uint32_t kStage = 0x202700;
// A stage's boss beaten (070E78, called by each boss as it dies): bit 5 of each character's first byte (the character
// flies on) and bit 0 of kFlags. Each frame 001C0E starts task 0x39, the stage's end (001678), while that bit is set:
// it counts the stage's tally, clears kFlags' bits 0 and 1, puts kStage one on and starts task 1 for the next stage
constexpr uint8_t kStageBeaten = 0x01, kShipLeaves = 0x20;
// The stage's coming in (a word): not 0 from a new game's character select (bits set through the stage's opening, bit
// 7 of its low byte as the characters fly in: 010728) until the characters are the player's, then 0 (as in DoDonPachi,
// a later stage is reached through the game's own stage end from the stage before: runtime/shmup.h)
constexpr uint32_t kComingIn = 0x200F38;
// While a stage's boss is on (a word: 1 while its life's bar is shown, which the interrupt draws then: 000916; 0 through
// the stages and their mid-bosses). Found by the stages played through (RAM every few thousand frames: on at the
// bosses of stages 1, 2 and 7, never before them). The first word of task 6's record, which most bosses are made in,
// was tried first: it is on at the bosses of stages 1 to 4 but not at the last's
constexpr uint32_t kBossOn = 0x20249C;

// --- Player 1's lives ---
// A ship is 0xC00 of a word (kShipSize, 0x2026B6). kLives is the ceiling (0x2400 for a new game: three), kLife the life
// left under it (from 0x2400 too): a hit takes 0x300 from kLife (0x600 or 0xC0 in some states; 00E72C) and the ship is
// lost when a whole ship has gone (kLives less kLife at 0xC00 or more): then 0xC00 is taken from both (00E8A4), and the
// game ends when kLife is then below 1. Found: the interpreter's watch on the words that round 1 saw go 36, 24, 12, 0
// (kLives' high byte) at the deaths of a run without the safe time held
constexpr uint32_t kLife = 0x2026DA, kLives = 0x2026DE;
constexpr int kShipSize = 0xC00;

// --- What hits the character (the game's own test, 007D4C, a4 its record: the players on alternate frames, 000BB2) ---
// Positions are in 64ths of the board's pixels, across the board's 320 and down its 240. A record's position is the
// words at +2 across and +4 down, and its box goes from the position less the word at +0x12 to the position plus the
// word at +0x10 across, less +0x16 to plus +0x14 down (both ends in it). A hit sets bit 4 of the record's first byte
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (007D96): records of 0x46 bytes from here, 80 of them, 120 while the word at kMoreBullets is not 0, 180
// while the next is not 0 too, 320 while the third is not 0 too; each a point that hits when it is inside the box and
// its first byte has none of the bits 0x59 (a record not in use is at 0, 0, where no box reaches)
constexpr uint32_t kBullets = 0x207010;
constexpr int kBulletSize = 0x46;
constexpr uint32_t kMoreBullets = 0x20EE2E;
// The enemies whose bodies hit (008094, when no bullet has): 115 records of 0x20 bytes from here and as many more as the
// word kMoreEnemies says, each with a box; one hits when its first byte has 0x80 and 0x40 and the boxes meet, both moved
// by 0x2800 and the enemy's left edge then below 0x7800
constexpr uint32_t kEnemies = 0x2049D0;
constexpr int kEnemySize = 0x20, kEnemyCount = 115;
constexpr uint32_t kMoreEnemies = 0x20EAB2;

// --- What the player's shots hit (the same routine on, 00836C: a0 the character's shots, a2 its shikigami's) ---
// The enemies the shots can hurt: those records whose first byte has 0x80 and 0x20 (008378 on, for all the enemies).
// One without 0x40 never hits the character: a tank, a turret, a thing on the ground (found: such records sit under the
// tanks of stage 1)
constexpr uint8_t kShootable = 0x20;
// Each character's shots: 24 records of 0x2C bytes, each with a box as an enemy's (its position at +2, +4), tested while
// the word kShotsOn is not 0; one is tested while its first word is negative and has bit 0, or has none of 0x880 with
// its power (+0x18) not negative
constexpr uint32_t kShots1 = 0x202A5A, kShots2 = 0x202E7A;
constexpr int kShotSize = 0x2C, kShotCount = 24;
constexpr uint32_t kShotsOn = 0x200F64;
// The shikigami's attack: 18 records of 0x38 bytes, boxes as the shots', tested while the word kShikigamiOn is not 0;
// one is tested while its first word is negative with bit 7 (008662)
constexpr uint32_t kShikigami1 = 0x203B9A, kShikigami2 = 0x203F8A;
constexpr int kShikigamiSize = 0x38, kShikigamiCount = 18;
constexpr uint32_t kShikigamiOn = 0x200F66;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 from the code that keeps each, checked in a RAM log of
// headless play) ---
// The rank (a byte, 0 to 63; the word at 2026EE is what the enemies' fire reads: 016A20, 0189D4, 01AFA6 and more),
// made again by 00445C as the stage's byte of a table (its address in the long at 200FBC) + the stage + each player's
// part (from 20265E, the ship's word at +0x1E and the stage: 004464 on) + the frames played over 1,024 (the long at
// 2026F6), cut to 63. The game keeps a half and a quarter of it too (2026F1, 2026F3). Found: in a log of a game it
// went 0, 1, 0, 1, 2 through the first stage, and 1, 2 in the attract show's demonstration; with the ship's word at
// +0x1E below 2 (seen only with the out-of-harm count held by hand) its part is less than 0 and carries the byte to 63
constexpr uint32_t kRank = 0x2026EF;
constexpr int kRankMost = 63;
// Player 1's score (a long in BCD: 9617 in the log as the picture showed 9617; 055018 adds to it, the new game clears it)
constexpr uint32_t kScore1 = 0x2024A4;

} // namespace spelunk::guwange
