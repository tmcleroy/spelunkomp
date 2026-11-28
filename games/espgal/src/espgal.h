// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Espgaluda (espgal): what is where in its memory, with how each was found.
// (Found 2026-11-11 by RAM dumps of headless runs: the guard barrier used, ships lost, a run with the safe time held
// through two stages; then the code that reads and writes each. The game's code has Dai-Ou-Jou's shapes
// (games/ddpdoj): the tasks, the stage's routine, the hit test.)
#pragma once

#include <cstdint>

namespace spelunk::espgal {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the byte that went 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x8011E8;

// --- A game's beginning ---
// When the player's character is chosen the game's task (kind 0x0A) is made at 13FACA and its stage, 0, put in its word
// at +4 (13FAD4); here is just after. (Start makes one at 13F80C too, for the choosing, ended at 13FA98; the attract
// show's demonstration makes its own at 13F40E.)
constexpr uint32_t kNewGame = 0x13FADA;
constexpr int kTaskStage = 4;
// The routine that sets the stage being played (d0, and the words made from it): at a game's beginning and at each
// stage's start after a boss (the next is the one before plus one, 0 past the seventh: 1195A6)
constexpr uint32_t kSetStage = 0x147298;
constexpr uint32_t kStage = 0x81399E; // the stage, from 0 (a word)
constexpr int kStagesInAll = 7;
// 1 from a game's beginning (13FAA4, as the character is chosen) until the player's ship is in and flown (about 160
// frames later); and at a stage's end
constexpr uint32_t kShipComing = 0x812930;
// The end of a stage, when its boss is beaten (119538): 812936 set, the ships flown marked (bit 13 of their first
// word), and a task of kind 6 made with the next stage in its word at +4 (and 0 at +6): it shows the stage's tally and
// begins the next. Tasks are made by 117ABC: records of 0x54 bytes put at 808AF8 + the word at 809188 (up to 0x690),
// their first word the kind with bit 15, +0x40 1, +0x4A a word from the kinds' table (117810 + 8 x kind + 4), +0x4C a
// number counted at 809F02
constexpr uint32_t kStageEnding = 0x812936;
constexpr uint32_t kNewTasks = 0x808AF8, kNewTasksUsed = 0x809188, kNewTasksRoom = 0x690, kTaskNumber = 0x809F02;
constexpr uint32_t kTaskSize = 0x54;
constexpr uint32_t kStageEndTask = 6, kStageEndTaskWord = 0x000A, kStageEndMaker = 0x11962E;

// The stage's boss (the high byte of this word, as Ketsui's: games/ket/src/ket.h): 0 through the stage and its
// mid-bosses; 1 from the boss's approach, 5 from its warning, 0x85 as it is beaten. The trip to the boss ends at the
// warning
constexpr uint32_t kBossOn = 0x813A8E;
constexpr uint32_t kBossWarned = 0x0400;

// --- Player 1's record ---
// The ships left besides the one flown (the low byte of the word at 8139D6: 2 for a new game, a ship lost at 0 ends the
// game; 0xFF while no game is played)
constexpr uint32_t kShips = 0x8139D7;
// The guard barrier's gauge (a word: 4000 full, 0FA0; an item adds 2000, 123DA4; the barrier takes from it)
constexpr uint32_t kBarrier = 0x80EEBA;
constexpr uint32_t kBarrierFull = 0x0FA0;
// The green gems (a word, the ship's record +0x6E; 500 at most, which the game shows in blue: found 2026-11-24 by RAM
// dumps of made-up play against the number beside the gem, 100, 123, 67, 10, 0; copies at 813A02 and 813A04 are the
// number shown). Kakusei takes from it
constexpr uint32_t kGems = 0x80EF04;
constexpr uint32_t kGemsFull = 500;
// The ship's record (player 2's at 80EF36): flown while its first word is negative. The byte at +0x39 is the frames left
// out of harm (about 0xE0 as a ship comes in, counted down; 0xFE, 0xFF while the barrier is up, 11951E)
constexpr uint32_t kShip1 = 0x80EE96, kShip2 = 0x80EF36;
constexpr uint32_t kSafeFrames = kShip1 + 0x39;

// --- What hits the ship (the game's own test: 11B1D0 for a ship at a4) ---
// Positions are in 64ths of the board's pixels (across its 448, down its 224; the monitor is on its side). In a record:
// the position (words at +2 across, +4 down) and the box about it: from the position less the word at +0x12 to the
// position plus +0x10 across, less +0x16 to plus +0x14 down (both ends in it)
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (11C216): records of 0x40 bytes from here, each a point (+2, +4) that hits when it is inside the ship's
// box and its first byte has none of the bits 0x51. 70 are tested, then ten more for each the word at 81B55A holds past
// six. (While the ship is out of harm a bullet in its box is turned into something else rather than hitting)
constexpr uint32_t kBullets = 0x818352;
constexpr int kBulletSize = 0x40;
constexpr uint32_t kBulletRounds = 0x81B55A;
// The enemies whose bodies hit the ship (11B2C8): records of 0x24 bytes from here, the first word negative while one is
// there; as many tested as the words at 816762 and 816764 add up to. One hits when its first word has bit 0 and the
// boxes meet (both moved by 0x2800: an enemy whose left edge, so moved, is past 0xF800 is left out). Tested only on
// frames whose counter (801165) has bit 1 set
constexpr uint32_t kEnemies = 0x815680;
constexpr int kEnemySize = 0x24;
constexpr uint32_t kEnemyCount1 = 0x816762, kEnemyCount2 = 0x816764;

// --- What the player's shots hurt (the same routine, 11B348 on; found 2026-11-12, Dai-Ou-Jou's shapes: ddpdoj.h) ---
// The enemies' records are in two groups: 90 from kEnemies (as many there as the word at 816762 says), then 30 from
// here (the word at 816764): the ground's, the tanks and turrets the ship flies over. A shot hurts an enemy whose first
// word has bit 13 (0x2000) and whose box (as above) meets the shot's, its left edge on the screen (moved by 0x2800 and
// below 0x9600 in the first group, by 0x1800 and below 0x8800 in the ground's, or anywhere in the ground's while the
// boss's bit 0x0400 is set: kBossOn). Those without bit 0 never hit the ship: drawn as the ground's kind
constexpr uint32_t kGroundEnemies = 0x816328;
constexpr int kAirSlots = 90, kGroundSlots = 30;
constexpr int kShootable = 0x2000;
constexpr int kAirMoved = 0x2800, kAirOnScreen = 0x9600, kGroundMoved = 0x1800, kGroundOnScreen = 0x8800;
// The shots (a0 at 17C8E0): records of 0x34 bytes for each player (35 of them), one there while its first word is
// negative; as many tested as the word at 812920 (player 2's 812922) says. Its box as the test takes it: across from
// the position (+2) plus the word at +0x10 less +0x12, to the position plus +0x10; down from the position (+4) plus +0x14
// less twice +0x16, to the position plus +0x14 (both ends in it). It does nothing while its power (+0x18) is negative
constexpr uint32_t kShots1 = 0x80EFD6, kShots2 = 0x80F6F2;
constexpr uint32_t kShotCount1 = 0x812920, kShotCount2 = 0x812922;
constexpr int kShotSlots = 35, kShotSize = 0x34;
constexpr int kShotRight = 0x10, kShotWidth = 0x12, kShotDown = 0x14, kShotHalfHeight = 0x16, kShotPower = 0x18;
// The lasers (button 2): while the ship's word at +0x40 is not 0 (and it is not awakened: its byte at +0x5F 0), up to
// four records of 0x18 bytes from a3 at 17C8E0 (11B6AA tests each, in turn while their first words are negative);
// awakened (+0x5F not 0), up to five of 0x1A bytes from a2 (11C00A). A laser's box: across from its position (less the
// word at +8, awakened) to the position plus 0x400 plus the word at +6, but cut at the first enemy it meets (+0x10,
// awakened +0x12, where the test leaves it); down from the position less +0xA (awakened +0xC) to plus +8 (+0xA)
constexpr uint32_t kLasers1 = 0x810E26, kLasers2 = 0x810E86;
constexpr int kLasers = 4, kLaserSize = 0x18;
constexpr uint32_t kAwakeLasers1 = 0x812572, kAwakeLasers2 = 0x8125F4;
constexpr int kAwakeLasers = 5, kAwakeLaserSize = 0x1A;
constexpr int kShipLaserOn = 0x40, kShipAwake = 0x5F;
constexpr int kLaserLength = 6, kLaserBack = 8, kLaserDown = 8, kLaserUp = 0xA, kLaserEnd = 0x10, kAwakeMoved = 2;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 from the code that keeps each, checked against RAM logs
// of headless play) ---
// 1 while the attract show's demonstration plays (13F39A sets it as the demonstration's game task is made; a real
// game's start clears it, 13F7B4)
constexpr uint32_t kDemo = 0x801186;
// Player 1's score (a long, tens: the last digit shown is always 0. Found: 3,978 and 5,419 in the log as the picture
// showed 39,780 and 54,190; 178F32 reads it)
constexpr uint32_t kScore1 = 0x81B57E;
// Player 1's gold (a word, the number beside the ingot: 1,000 at most, 137C5E; halved when a ship is lost, 137AFE, and
// a quarter taken as a boss comes, 137B6E. Found: 24, 16, 4 in the log as the picture showed them), the step it is on
// (a word, 4 x the step, 0 to 0x24: one more each 100, 137CB6) and the gold into that step (a word, 0 to 100). Player 2's
// are the words after each (81293C, 812940, 812944)
constexpr uint32_t kGold = 0x81293A, kGoldStep = 0x81293E, kGoldIntoStep = 0x812942;
constexpr int kGoldMost = 1000;
// In the ship's record: Kakusei on (a word, not 0: +0x66); with no gems left in it, Kakusei Overmode. The bullets a
// Kakusei has turned into gold so far (a word, +0x8A: 10 more for each, 170658, to 1,000; each bullet takes the value
// before its own, so the multiplier is a tenth of it plus one, 1 to 100; cleared as Kakusei begins, 122AE8, and as the
// gems run out, 1252C2). Overmode's level (a word, +0x76: 0, 2, 4, 6 for the levels 1 to 4) and its way to the next
// (a word, +0x74, against +0x7A: 1 a frame while shooting, 4 a frame after 64 frames without, 125302 on; at 999 the
// level goes up. Found: in Overmode without shooting the log showed +0x74 go up 0x3C every 15 frames and the level 0,
// 2, 4 each time it passed 999)
constexpr int kShipKakusei = 0x66, kShipCancels = 0x8A, kShipOverLevel = 0x76, kShipOverWay = 0x74, kShipOverNext = 0x7A;
constexpr int kOverLevels = 4;

} // namespace spelunk::espgal
