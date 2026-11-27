// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Ou-Jou Black Label (ddpdojblk): what is where in its memory, with how each was found.
// (Found 2026-11-11. Black Label runs from the B half of the program, 200000 on: its code is Dai-Ou-Jou's (ddpdoj.h,
// where each place is described) moved by about 100000, and its work RAM partly moved; each place below was found
// by the same code in this half, and checked by RAM dumps of headless runs: ships lost, a bomb used, a run with the
// safe time held through three stages.)
#pragma once

#include <cstdint>

namespace spelunk::ddpdojblk {

// The credits, a byte of work RAM (found 2026-11-11: three coins, the byte that went 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x80395A;

// --- A game's beginning (Dai-Ou-Jou's 15B892, 15E88E) ---
// A real game is begun at 25CA78 by making the game's task (kind 0x0A) and putting its stage, 0, in its word at +4
// (25CA82); here, just after, a0 is the task. (The attract show's demonstration makes its task at 25C5E2, from a table
// of its own; 25CEA2 is a third way in, not taken by a game begun with player 1's Start.)
constexpr uint32_t kNewGame = 0x25CA88;
constexpr int kTaskStage = 4;
// The routine that sets the stage being played (d0): at a game's beginning and at each stage's start after a boss
constexpr uint32_t kSetStage = 0x25FD0C;
constexpr uint32_t kStage = 0x813092; // the stage, from 0 (a word)

// While a stage's boss is on (a word: 0 through the stage and its mid-bosses; set from its warning to the stage's
// end; bits 3 and 4 of the high byte set as the boss is beaten, 242958)
constexpr uint32_t kBossOn = 0x8130F8;

// What the game is doing (a word, found 2026-11-27 by RAM logs of a whole round: the boot, the title, a ship chosen, a
// game played to its end, the continue, the logos and the attract show): 0x0A while a stage is played, 9 as a ship is
// chosen and for a frame at a ship lost, 0 on the title, the logos and the continue. The attract show's demonstration
// plays as a game does (0x0A), with its inputs read from a table whose place is kept here (a long, set at 25C5D6: not 0
// only then). The enemies' records a demonstration or a game leaves are not cleared until the next begins
constexpr uint32_t kMode = 0x80E3CA;
constexpr int kModePlayed = 0x0A;
constexpr uint32_t kDemoInputs = 0x812E98;

// --- Player 1's record ---
// The ships left besides the one flown (the low byte of the word at 8130BE: 2 for a new game, 0xFF while no game is
// played)
constexpr uint32_t kShips = 0x8130BF;
// The bombs left, and what they are filled to at a new ship (bytes)
constexpr uint32_t kBombs = 0x81040A;
constexpr uint32_t kBombStock = 0x81040B;
// The ship's record (player 2's at 810448): flown while its first word is negative. The byte at +0x3E is the frames
// left out of harm (0xF0 as a ship comes back, 2493F2; counted down at 249536; 0xFF held at a bomb, 242938)
constexpr uint32_t kShip1 = 0x8103E6, kShip2 = 0x810448;
constexpr uint32_t kSafeFrames = kShip1 + 0x3E;

// --- What hits the ship (the game's own test: 244D62 for a ship at a4; Dai-Ou-Jou's 1440A2) ---
// As in Dai-Ou-Jou: positions in 64ths of the board's pixels, a record's position at +2 and +4 and its box about it
// (+0x12 left, +0x10 right, +0x16 up, +0x14 down; both ends in it)
constexpr int kX = 2, kY = 4, kBoxRight = 0x10, kBoxLeft = 0x12, kBoxDown = 0x14, kBoxUp = 0x16;
// The bullets (2459D0): records of 0x40 bytes, each a point that hits when inside the ship's box and its first byte
// has none of the bits 0x51; 70 tested, more while the words from 81B414 are set in turn: 110, 160, 190, 210
constexpr uint32_t kBullets = 0x817F8C;
constexpr int kBulletSize = 0x40;
constexpr uint32_t kMoreBullets = 0x81B414;
constexpr int kBulletCounts[5] = {70, 110, 160, 190, 210};
// The enemies whose bodies hit the ship (244E5C): records of 0x20 bytes; as many tested as the words at 815E9E and
// 815EA0 add up to; one hits when its first word has bit 0 and the boxes meet (moved by 0x2800, left edge below 0xF800)
constexpr uint32_t kEnemies = 0x81459C;
constexpr int kEnemySize = 0x20;
constexpr uint32_t kEnemyCount1 = 0x815E9E, kEnemyCount2 = 0x815EA0;

// --- What the player's shots hurt (the same routine after the ship's test, 244EE0 on; Dai-Ou-Jou's 144220) ---
// As in Dai-Ou-Jou (ddpdoj.h, where each is described): the enemies in two groups, 100 from kEnemies, then the
// ground's 50 from here; a shot hurts those with bit 13 (0x2000); those without bit 0 are drawn as the ground's
constexpr uint32_t kGroundEnemies = 0x81521C;
constexpr int kAirSlots = 100, kGroundSlots = 50;
constexpr int kShootable = 0x2000;
constexpr int kAirMoved = 0x2800, kAirOnScreen = 0x9700, kGroundMoved = 0x1800, kGroundOnScreen = 0x8800;
// The shots (a0 at 28B6B8): 36 records of 0x30 bytes for each player, their boxes as Dai-Ou-Jou's
constexpr uint32_t kShots1 = 0x810572, kShots2 = 0x810C32;
constexpr int kShotSlots = 36, kShotSize = 0x30;
constexpr int kShotRight = 0x10, kShotWidth = 0x12, kShotDown = 0x14, kShotHalfHeight = 0x16, kShotPower = 0x18;
// The laser's three records (a2, a3 the aura, a1 the beam at 28B6B8), tested while the ship's byte at +0x3F is not 0
constexpr uint32_t kLaserOther1 = 0x811802, kLaserOther2 = 0x811E02;
constexpr uint32_t kLaserAura1 = 0x811892, kLaserAura2 = 0x811E92;
constexpr uint32_t kLaser1 = 0x811EF2, kLaser2 = 0x811F12;
constexpr int kShipLaserOn = 0x3F;
constexpr int kLaserLength = 6, kLaserDown = 8, kLaserUp = 0xA, kLaserEnd = 0x10;

// The hidden bees (found 2026-11-26: the laser held on one from a player's save, 2767E6 run with a6 the bee): an
// enemy's record each (in kEnemies' 100, or the ground's), put there by the stage as the scroll comes to it, its
// picture (the long at +0xA) the bee's, 1BCA80 (1BCA34 while it flickers under the laser). Its position as an enemy's.
// The laser's hit lets the bee go (27F92A: into ten records of 0x2C bytes from 817DC6) and clears the record
constexpr int kPicture = 0xA;
constexpr uint32_t kBeePicture = 0x1BCA80, kBeePictureLit = 0x1BCA34;

// --- Player 1's scoring (found 2026-11-26 from headless runs' RAM logs, each against the picture: the hit counter
// read off the screen frame by frame, the hyper items picked up and used; the code that caps the gauge at 2875E2) ---
// The chain: the hits now, in BCD (a word: 0x24 for 24 hits), counted up a kill at a time and set to 0 as the next
// chain begins; and its gauge, a word counted down a frame at a time from 0x38 (a kill with the shot refills it to the
// top; with the laser, part way), the chain broken at 0
constexpr uint32_t kChain = 0x81B5DA;
constexpr uint32_t kChainGauge = 0x81B5C0;
constexpr int kChainGaugeFull = 0x38;
// The most hits in one chain this game (BCD, a word)
constexpr uint32_t kChainBest = 0x81B632;
// The hyper gauge (a word): filled by kills, chains and bees, a hyper item let go when it reaches kHyperFull (the
// game holds it there, 287686, and sets it to 0 as the item comes)
constexpr uint32_t kHyperGauge = 0x81B64A;
constexpr int kHyperFull = 0x95F;
// The hyper items held (a byte, up to 5; all used at once by the bomb button)
constexpr uint32_t kHyperStock = 0x81B65D;
// The hyper rank (a word): raised by each hyper's level as it begins, and cut to a quarter when a ship is lost (the
// loss's routine, 24A006). It speeds the enemies' bullets
constexpr uint32_t kHyperRank = 0x81B646;
// What the ranks make of the enemies' fire (found 2026-11-27 in the code that works them out each frame, and its
// numbers watched through a hyper): the game rank (a word, 2608D2), the stage's own start (a table's byte for the
// difficulty) and one more each 256 frames played, and while a hyper is on the hyper rank 16 times over; 240 at most
// (255 in a hyper), set outright in the second loop. The bullets' speed (a word, 252BD0): from a table by the higher
// of the players' hyper ranks (a quarter of it while no hyper is on), and more at some bosses; 8 at most (15 in the
// second loop)
constexpr uint32_t kGameRank = 0x81309E, kBulletSpeed = 0x812950;
// A byte that is 1 while a hyper is on, and the time it has left (a word, from kHyperFull down to 0)
constexpr uint32_t kHyperOn = 0x81B659;
// The level of the hyper on now (a byte: the items used for it; 0 again as it ends)
constexpr uint32_t kHyperLevel = 0x81B655;
constexpr uint32_t kHyperLeft = 0x81B642;

// The bees taken in this stage (a word: the bee's routine, 27FBF4, counts it up and gives the tenth its bonus; a
// stage's start and a ship lost, 27F898, set it to 0, so ten means ten without a ship lost), and the stages whose
// ten were all taken, four for each (a word: 27FC0C; the bee's value is read from a table by it)
constexpr uint32_t kBees = 0x817F80;
constexpr uint32_t kBeeStages = 0x817F82;
// What the game asks of the first loop to go on to the second (its own test, 290232): player 1's ships lost (a word,
// 2 at most), bombs used (a word, 3 at most) or 12 in kBeeStages (three stages' bees), and none of it if the game was
// continued (a word not 0)
constexpr uint32_t kShipsLost = 0x812938, kBombsUsed = 0x812940, kContinued = 0x81B49A;

// A word negative while player 1's ship is flown (its stage is set before the ship is chosen: Begin at the stage
// presses the shot on until it is)
constexpr uint32_t kShipFlown = 0x8103E6;
} // namespace spelunk::ddpdojblk
