// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama Futari Black Label Another Ver (2009): where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::futaribl {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C0601E7u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C05176Au;


// --- As Futari 1.5 (games/futari15/src/futari15.h), found the same ways ---
constexpr uint32_t kNewGameCaller = 0x0C1025CEu;
constexpr uint32_t kNewGame = 0x0C066DE0u;    // (stage in r4)
constexpr uint32_t kBeginStage = 0x0C068B20u; // (stage in r5)
constexpr int kBossTunes[] = {17};
constexpr uint32_t kShips = 0x0C383E4Fu;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C383E52u;      // the bombs left (a byte; not tried with a bomb used)
constexpr uint32_t kShipState = 0x0C383E80u;  // 1 while the ship is flown
constexpr uint32_t kSafeFrames = 0x0C383EC8u;
// What hits the ship: the collision kinds of games/futari15/src/futari15.h, the table here (found by the shape of the
// pass that tests the ship, 0x0C064CA0; the kinds and their members' layout as Futari 1.5's)
constexpr uint32_t kKinds = 0x0C383548u;
constexpr bool kTestedLong = false;
constexpr bool kAddFixed = false;
// (each group of bullets tested by every member)
constexpr bool kBulletFirstOnly = false;
constexpr uint32_t kPosition = 52, kBox = 32;
constexpr int kShipKind = 2, kShipKindAll = 16, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the player's shots hit: as Futari 1.5's, kinds 4 to 15 tested against every member of the bodies' kinds (4 and 5
// also needing their long at +12), kinds 1 and 21 passed over with bit 1 of +27 set (found 2026-11-12 by the same
// pass's shape here, 0x0C064ED0)
constexpr int kShotKindFirst = 4, kShotKindLast = 15;
constexpr int kMainShotKinds[] = {4, 5};
constexpr int kShotsPassKinds[] = {1, 21};

// (the ship's box is the same out of harm: nothing put in its place)
constexpr uint32_t kSafeReach = 0, kShipSizes = 0, kShipType = 0, kSizesAt = 0, kSizesMode = 0, kSizesModeAt = 0;

// --- What the board's code unlocks (found 2026-11-25 by entering it after a coin, 1P holding B, 2P: B, B, down, down,
// C, up, A, C, left, A, right, B, and comparing RAM with a run without) ---
// Spiritual Larsa after God mode's Dragon Emperion even with a ship lost: a byte, 1 once the code is in (beside the
// word the code's presses are counted in, 12 when whole: the same record as Ver 1.0's Ultra code, whose byte alone
// unlocks Ultra). What it does was not played to: God mode's last boss
constexpr uint32_t kLarsaUnlocked = 0x0C7046B3u;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by dumps of headless play and the game's code) ---
// The game's rank (32 bits): made again each frame by 0x0C06CB50 as the larger of the two players' parts (32 bits
// each, made by 0x0C06CBF0: 4 x the bombs held plus 2 x the record's byte after them), plus the frames played over
// 512 (kRankFrames, one more each frame), plus the mode's base (kRankBase), then cut to the mode's most
// (255 in Original, 232 in Maniac, 248 in God, 255 there from the 4th stage on; nothing more in Original). Found: Ver 1.5's routine (futari15.h) at this place, its Original part simpler; a dump at frame 4,000 of
// a game held 158 = 144 + 12 + 1,286 frames over 512
constexpr uint32_t kRank = 0x0C38373Cu;
constexpr uint32_t kRankPart1 = 0x0C38487Cu, kRankPart2 = 0x0C384880u, kRankFrames = 0x0C383834u, kRankBase = 0x0C383838u;
// The record the routine reads (0x0C383808): 1 at +4 while a game is played (0 in the attract show's demonstration: a
// dump of it held 0), the mode at +16 (0 Original, 1 Maniac, 2 the third) and the stage played at +20 (from 0: a game
// begun at stage 3 held 2)
constexpr uint32_t kGameOn = 0x0C38380Cu, kMode = 0x0C383818u, kStage = 0x0C38381Cu;
constexpr int kRankMost[] = {255, 232, 248};
constexpr int kUltra = 2, kUltraLateStage = 3, kUltraLateMost = 255;
// Player 1's score (ten bytes, a digit each, the lowest first: Black Label's score goes past 32 bits. 5, 2, 9, 2, 9 in a
// dump as the picture showed 52,929)
constexpr uint32_t kScore = 0x0C383E6Cu;
constexpr bool kScoreDigits = true;

} // namespace spelunk::futaribl
