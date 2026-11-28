// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::mushisam {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C056BE7u;
// The main loop's wait for the next frame (MAME's driver names it as the game's idle loop)
constexpr uint32_t kIdleLoop = 0x0C04A2AAu;


// --- A game's beginning ---
// The routine that sets a new game up takes the stage in r4 (0-4: it picks the stage's graphics by it), and the one
// that begins a stage's play takes it in r5 (its map and enemies). Both are called once with 0 as a game begins
// (found by watching the stage number at 0x0C1ECF9C being written, then trying the arguments). A game begun at a
// later stage starts with 100,000 points: the game's own doing.
// (a real game calls the first from kNewGameCaller; the attract show's demonstration from elsewhere)
constexpr uint32_t kNewGameCaller = 0x0C0D8436u;
constexpr uint32_t kNewGame = 0x0C05BCB0u;    // (stage in r4)
constexpr uint32_t kBeginStage = 0x0C05D4E0u; // (stage in r5)
constexpr uint32_t kStage = 0x0C1ECF9Cu;      // the stage being played, from 0 (32 bits)
// The sound chip's sequences a boss comes on to: 4 as it arrives, 5 its theme (found by listing the sequences
// started through the first three stages; the memory word first tried for this, 0x0C2088C4, is not at the same
// place in every game played). The last boss's are not checked
constexpr int kBossTunes[] = {4, 5, 16, 17}; // (16 and 17: the last stage's)

// --- Player 1's record (found by RAM dumps of the same moment with and without a bomb used) ---
constexpr uint32_t kShips = 0x0C1ED35Bu;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C1ED35Fu;      // the bombs left (a byte)
constexpr uint32_t kScore = 0x0C1ED370u;      // the score (64 bits); the words from here on are 32 bits
constexpr uint32_t kShipState = 0x0C1ED380u;  // 1 while the ship is flown, 2 as it is lost and comes back
constexpr uint32_t kShipX = 0x0C1ED388u;      // where the ship is (16.16; the screen's own axes, sideways as the board has it)
constexpr uint32_t kShipY = 0x0C1ED38Cu;
// The frames the ship is still out of harm for (0x97 as it appears, counted down): held up, nothing hits it
constexpr uint32_t kSafeFrames = 0x0C1ED3E8u;

// --- What hits the ship (the game's hit test at kHitTest, called each frame with the ship's record in r4) ---
// Found 2026-11-11: the ship's state (+0x30) set to 2 as it is hit, by 0x0C05F290 called from the test.
// Positions are 16.16 in the picture as it stands up, 240 across and 320 tall, counted up from the bottom.
constexpr uint32_t kHitTest = 0x0C05B110u;
constexpr uint32_t kShipRecord = 0x0C1ED350u; // (kShipState and the rest are in it)
constexpr uint32_t kShipAt = 0x0C1ED390u;     // where the ship is now (16.16, across then up; kShipX/Y is where a ship starts)
// The ship's box: a pointer at +0xB0 (0x0C05EEA0) to a box the game makes each frame while the ship can be hit (0
// while it is out of harm). A box: the position at +12 (across) and +16 (up), and how far it reaches right (+20), left
// (+24), up (+28) and down (+32), all 16.16; the ship's reaches 2.625 each way
constexpr uint32_t kShipBox = 0x0C1ED400u;
// The bullets, each a point that hits when it is inside the ship's box (the test's first part): a pointer at kBullets
// to a block where the list is at +0x62000 (32 bytes an entry, the bullet's record at +16, its position at +12, +16)
// and the count at +0x72000 (0x0C08D410)
constexpr uint32_t kBullets = 0x0C1CA084u;
constexpr uint32_t kBulletList = 0x62000u, kBulletCount = 0x72000u;
// The enemies' bodies, boxes that hit when they meet the ship's (the test's second and third parts): lists of lists,
// a header of 32 bytes each (the count at +0, the first box's record at +16; records 80 bytes apart, tested while the
// word at +4 is not 0, the box from +12 as the ship's), so many headers as the count says
constexpr uint32_t kBodyHeaders[2] = {0x0C26C030u, 0x0C23FC10u};
constexpr uint32_t kBodyHeaderCounts[2] = {0x0C2681A0u, 0x0C2BD7A8u};

// --- What the shots hit (found 2026-11-12: the collision pass at 0x0C05A9C0, which calls kHitTest first, then tests
// each of the bodies' lists against the shots, the lasers and the bombs, and clears the counts at its end) ---
// An enemy's box is tested against the shots while the word at +0 is set (1 in the first of kBodyHeaders, any in the
// second), whether or not it hits the ship (+4): those with +0 set and +4 clear are the ground ones (tanks, turrets)
constexpr uint32_t kHurtByShotsFirst = 1u; // (+0 must be this in the first list)
// The shots: headers as the bodies' (count, then the first record at +16; records 80 bytes apart, a shot while its
// word at +0 is set, its box from +12)
constexpr uint32_t kShotHeaders = 0x0C2681B0u, kShotHeaderCount = 0x0C26FEB0u;
// The lasers, the same: each tested against every enemy (from 0x0C09D690); the bombs' list (0x0C246D20) is not drawn
constexpr uint32_t kLaserHeaders = 0x0C2D32A0u, kLaserHeaderCount = 0x0C2D3290u;

// --- What the gadgets show (found 2026-11-27 by RAM dumps of real play, SPELUNK_WATCH on the writes, and the code that
// writes each; player 1's record is at kShipRecord and the words below are all 32 bits) ---
// The mode played: 0 Original, 1 Maniac (2 is taken to be Ultra: the code tests for 0, 1 and 2; Ultra was not played).
// Found by diffing a dump of each mode's play; it is set as play begins, so 0 before a mode is chosen
constexpr uint32_t kMode = 0x0C1ED040u;
// The counter Maniac and Ultra show under the ships as "+N" (the number was read off the picture at three moments
// and the word matched each: +30, +18, +6). It is set to a hit's value when that is higher (the write at 0x0C05EB80)
// and, once the timer below has run out, counts down in steps that grow with it: 2 up to 199, 8 up to 499, 24 up to
// 999, 48 up to 4999, 96 up to 9999 (the table at 0x0C05E732; found reading the code at 0x0C05E5E6). 0 all through
// Original, which has none. Its top is not known (the steps go on to 49999)
constexpr uint32_t kCounter = 0x0C1ED3B4u;
// A countdown beside it (written every frame by 0x0C05E5F6 and reloaded by the loop at 0x0C05E6D0): probably the
// frames the counter holds before it falls. Not checked against the counter, so the gadgets do not use it
constexpr uint32_t kCounterHold = 0x0C1ED3B8u;
// The score items taken, a count for each kind (0x0C05F110 adds one as the ship takes an item; its kind is the
// item's +48). Kind 0 matched the 100-point items (35 counted, 36 times the game gave 100 points in the same run),
// kind 2 the 200-point ones (6 and 7) and kind 3 the 500-point ones (3 and 3); kind 1 (0x0C1ED3A4) was never taken. The counts
// went back to 0 once between two looks after a ship was lost (the game sets the record up again), so they are
// since the last time the game did that
constexpr uint32_t kItemsKind0 = 0x0C1ED3A0u, kItemsKind2 = 0x0C1ED3A8u, kItemsKind3 = 0x0C1ED3ACu;
// The extra ship (found by diffing dumps taken before and after the score went past the first threshold, and the
// code at 0x0C05E5C0-0x0C05E726 that tests it). A record at kExtend (the ship record + 0x7C): +8, +12 and +16 are
// the scores to take a ship at, in order, 0xFFFFFFFF ending the list; +20 is how many are taken (it went 0 to 1 as
// the score was held just past the first); +24 is 0 for a list and 1 for "every so many points": then +8 is the next
// score and +12 is added to it each time. The record is set from a table by the board's extend setting (the tables
// are at 0x0C1C6E78 Original, 0x0C1C6EC8 Maniac, 0x0C1C6F18 Ultra, 16 bytes a setting; the default setting is 2.5
// million and 5 million in Original, 10 million and 25 million in Maniac and Ultra)
constexpr uint32_t kExtend = 0x0C1ED3CCu;
constexpr uint32_t kExtendList = 0x0C1ED3D4u, kExtendTaken = 0x0C1ED3E0u, kExtendEvery = 0x0C1ED3E4u;

} // namespace spelunk::mushisam
