// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Ketsui: Kizuna Jigoku Tachi (ket): what is where in its memory, with how each was found.
// (Found 2026-11-11 by RAM dumps of headless runs: a bomb used, three ships lost, a run with the safe time held through
// two stages; then the code that reads and writes each. The game's code has Dai-Ou-Jou's shapes (games/ddpdoj), by
// the same team a year on: the tasks, the stage's routine, the hit test.)
#pragma once

#include <cstdint>

namespace spelunk::ket {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the byte that went 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x801160;

// --- A game's beginning ---
// A real game is begun at 0C61BA by making the game's task (kind 0x0A, as in Dai-Ou-Jou) and putting its stage, 0, in
// its word at +4 (0C61C4); here, just after, a0 is the task. The task's first run gives that word to the routine that
// sets the stage. (The attract show's demonstration makes its task at 0C5F2C with a stage of its own.)
constexpr uint32_t kNewGame = 0x0C61CA;
constexpr int kTaskStage = 4;
// The routine that sets the stage being played (d0, and the words made from it): at a game's beginning and at each
// stage's start after a boss (the stage after is the one before plus one, 0 past the fifth: 0AE534)
constexpr uint32_t kSetStage = 0x0C98CA;
constexpr uint32_t kStage = 0x812440; // the stage, from 0 (a word)
// Stage 4: the game only ever begins it from stage 3's end, never from a new game (no attract demonstration shows it
// either: they are of stages 1 to 3, table 0C5E70). Begun from the new game's task its scroll stands still and the
// picture stays black: the scroll task (kind 1, 0CB2CE; made at 0CA05E, its first run 0CAF58) starts it at (0, 0) at a
// speed of 0, and nothing moves it on. (Found 2026-11-19: the stage reached by play, stage 3's end traced
// (SPELUNK_TRACE_AT) and the scroll's words diffed against a new game's.) Reached by play: stage 3's boss beaten
// (0AE4D2) makes the stage's end task (kind 6, 105380; with +6 = 0 it does not set the stage up anew); stage 3's script
// has left the scroll's place standing (0x812582 = 1), the scroll's speed at 0x100 (words 0x812576 and 0x81257A, taken
// when the bytes 0x812574 / 0x812575 are set), and the scroll's timer on (0x8125AA = 0xFFFF, counted down in 0x8125AC
// at each pixel the scroll moves, from 0x38 in 0x8125AE); the task's first run calls 0CAD7E (0x812520 = 1,
// 0x81251E = 1), and asks the timer (0CCE62) to begin the next stage (0x8125D2 = 2, 0x8125D4 = the stage). The timer's
// next count (0CCE86) begins it: 0C98CA, then 0CADE6, which sets the stage's scroll up from its tables (0x81247C =
// 0xFFC4 and 0x81247E = 0x6C, its script from the start) and lets the place move again.
// Begin at the stage 4 does the same: a new game at stage 4, and once the scroll task's first run is done (its word
// 0x812544, the stage's scroll height from the table at 0CB13C: 0xA8 for stage 4, no other stage's; cleared as the
// game begins) those words are set as stage 3's end leaves them, with the timer at its last count, so the game's own
// timer begins the stage on the next frame (the scroll runs in, as from stage 3, while the ship is brought in)
constexpr uint32_t kScrollHeight = 0x812544;
constexpr int kStage4Height = 0xA8;
constexpr uint32_t kScrollRunning = 0x81251E, kScrollRunning2 = 0x812520;
constexpr uint32_t kScrollSpeedSet = 0x812574, kScrollSpeed = 0x812576, kScrollSpeed2 = 0x81257A;
constexpr uint32_t kScrollStands = 0x812582;
constexpr uint32_t kScrollTimer = 0x8125AA, kScrollTimerCount = 0x8125AC, kScrollTimerReload = 0x8125AE;
constexpr uint32_t kScrollAsk = 0x8125D2, kScrollAskStage = 0x8125D4;
constexpr int kStageEndSpeed = 0x100, kTimerReload = 0x38;

// The stage's boss (the byte the enemies' code tests, 0BAF62 on; the high byte of this word): 0 through the stage and
// its mid-bosses; 1 from the boss's approach (about ten seconds before its warning), 5 from the warning, 0x85 as it is
// beaten, 0 again as the next stage begins
constexpr uint32_t kBossOn = 0x8124A6;
constexpr uint32_t kBossWarned = 0x0400; // (the warning's bit: the trip to the boss ends there)

// --- Player 1's record ---
// The ships left besides the one flown (the low byte of the word at 812472: 2 for a new game, a ship lost at 0 ends
// the game; 0xFF while no game is played)
constexpr uint32_t kShips = 0x812473;
// The bombs left (a byte: 3 for a new game; not filled again when a ship is lost)
constexpr uint32_t kBombs = 0x80FEC7;
// The ship's record (player 2's at 80FF12): flown while its first word is negative. The byte at +0x3A is the frames left
// out of harm (0xE8 as a ship comes in, counted down; 0xFF while a bomb goes off, 0AE4B8)
constexpr uint32_t kShip1 = 0x80FEA2, kShip2 = 0x80FF12;
constexpr uint32_t kSafeFrames = kShip1 + 0x3A;

// --- What hits the ship (the game's own test: 0B02FC for a ship at a4) ---
// Positions are in 64ths of the board's pixels (across its 448, down its 224; the monitor is on its side). In a record:
// the position (words at +2 across, +4 down) and the box about it: from the position less the word at +0x12 to the
// position plus +0x10 across, less +0x16 to plus +0x14 down (both ends in it). The ship's: 0xC0, 0xC0, 0x80, 0x80
// (about 6 x 4 pixels)
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (0B109C): records of 0x40 bytes from here, each a point (+2, +4) that hits when it is inside the ship's
// box and its first byte has none of the bits 0x51; ten for each the word at 81A626 holds, plus ten. (While the ship
// is out of harm the test takes a box 0x800 wider each way across and 0x600 down, and a bullet in it is turned into
// something else rather than hitting: the box drawn is the ship's own)
constexpr uint32_t kBullets = 0x81719E;
constexpr int kBulletSize = 0x40;
constexpr uint32_t kBulletRounds = 0x81A626;
// The enemies whose bodies hit the ship (0B03FA): records of 0x2A bytes from here, the first word negative while one
// is there; as many tested as the words at 815A86 and 815A88 add up to. One hits when its first word has bit 0 and the
// boxes meet (both moved by 0x2800 there: an enemy whose left edge, so moved, is past 0xF800 is left out). Tested only
// on frames whose counter (8010EF) has bit 1 set: two frames in four
constexpr uint32_t kEnemies = 0x8139B4;
constexpr int kEnemySize = 0x2A;
constexpr uint32_t kEnemyCount1 = 0x815A86, kEnemyCount2 = 0x815A88;

// --- What the player's shots hurt (the same routine, B047E on; found 2026-11-12, Dai-Ou-Jou's shapes: ddpdoj.h) ---
// The enemies' records are in two groups: 100 from kEnemies (as many there as the word at 815A86 says), then 50 from
// here (the word at 815A88): the ground's, the tanks and turrets the ship flies over. A shot hurts an enemy whose first
// word has bit 13 (0x2000) and whose box (as above) meets the shot's, its left edge on the screen (moved by 0x2800 and
// below 0x9700 in the first group, by 0x1800 and below 0x8800 in the ground's). Those without bit 0 never hit the ship:
// drawn as the ground's kind
constexpr uint32_t kGroundEnemies = 0x814A1C;
constexpr int kAirSlots = 100, kGroundSlots = 50;
constexpr int kShootable = 0x2000;
constexpr int kAirMoved = 0x2800, kAirOnScreen = 0x9700, kGroundMoved = 0x1800, kGroundOnScreen = 0x8800;
// The shots (a0 at 101D8A): records of 0x34 bytes for each player (42 of them), one there while its first word is
// negative; as many tested as the word at 8113B4 (player 2's 8113B6) says. Its box as the test takes it: across from
// the position (+2) plus the word at +0x10 less +0x12, to the position plus +0x10; down from the position (+4) plus +0x14
// less twice +0x16, to the position plus +0x14 (both ends in it). It does nothing while its power (+0x18) is negative
constexpr uint32_t kShots1 = 0x8101C2, kShots2 = 0x810A4A;
constexpr uint32_t kShotCount1 = 0x8113B4, kShotCount2 = 0x8113B6;
constexpr int kShotSlots = 42, kShotSize = 0x34;
constexpr int kShotRight = 0x10, kShotWidth = 0x12, kShotDown = 0x14, kShotHalfHeight = 0x16, kShotPower = 0x18;

// A word negative while player 1's ship is flown (its stage is set before the ship is chosen: Begin at the stage
// presses the shot on until it is)
constexpr uint32_t kShipFlown = 0x80FEA2;
// --- What the gadgets (port/gadgets.h) read (all found 2026-11-27 by RAM dumps of real play, diffed over time, then the
// code that writes each) ---
// Player 1's score (a long word in plain binary, the score as shown, 1,085,430 on the picture at
// one dump; found by the number on the picture searched for in a dump, then counted up with it over 5,000 frames and past
// 30 million). It stands on a word's boundary, not a long word's, and a copy of it follows at +4 (what the picture
// draws)
constexpr uint32_t kScore1 = 0x81A63E;
// Which loop is flown (a word: 0 the first loop, 1 the second; the stage word, kStage, begins again at 0 in it). It is
// set to 0 as a game's state is cleared and counted up by the stage task as the fifth stage's end begins stage 0
// again (0C9934; reached only from the end of stage 5: 0C98FA is never run at a new game, traced). Seen to go 0 to 1
// when a game was flown through all five stages with no ship lost and no continue, and the tables that place the
// enemies are chosen by it (0AE144). What decides whether the second loop is begun at all was not found: the stage
// task asks a word, 81244E, at the fifth stage's end (1054DC: not 0 is the ending, 0 the next loop), which nothing in
// the program's code writes but a clear, and which stayed 0 in a flown game and in games with ships lost
constexpr uint32_t kLoop = 0x812446;
// The chips, two words (the game's code adds a chip's value, 1 to 5, to both when one is taken: 0F5748 on, and the
// pick-ups' routines at 0F5982, 0F5C16 ... each counting its own value's chips up in the words from kChipsByValue):
//  - kChipStock (capped 999): the small number the picture shows below the big one. A kill (the routine at 0FD2FE,
//    for the kills with bit 4 of the killed enemy's flags: the lock-on's) turns it into chips (a division by 30, 20 or
//    10 by the ship, subtracted: 78 became 76 at a kill in the dumps) and a ship lost cuts it by a quarter (0F5868)
//  - kChipTotal (capped 9999): the big number. It never fell in play except in a boss's fight, where the game counts
//    it down a frame at a time (0BAF8A, from the boss's approach to its end) and kills use it in place of the first
// Which of the two the game's pick-ups and kills use is told by bit 2 of the boss word (kBossOn's 0x0400: from the
// boss's warning on, the total). Found by the picture ("189" over "61" matched the words 811512 and 811510 in the
// dump of that frame, and the same pair in dumps every 100 frames)
constexpr uint32_t kChipStock = 0x811510, kChipTotal = 0x811512;
constexpr int kChipStockMost = 999, kChipTotalMost = 9999;
// Chips taken, by value (five words, for values 1 to 5; player 2's from 0x817190): counted up by each pick-up's
// routine (0F59D2). In the dumps only the first counted: it equalled the chips taken
constexpr uint32_t kChipsByValue = 0x817186;
// While a player's game is played: the stage task is the first of the task list (a record of 0x54 bytes from 80B0BC,
// its first word 0x8000 or'd with its kind: 0x0A while a game is flown, the attract show's demonstration too, and
// 0x08 at the title), the attract show's demonstration is told by 80110E (1 while it runs; set at 0C5ECA, cleared as a
// game begins, 0C61CA), and the ships left (kShips' byte) is 0xFF between games, after the last ship
constexpr uint32_t kTasks = 0x80B0BC, kDemo = 0x80110E;
constexpr int kStageTask = 0x800A;
} // namespace spelunk::ket
