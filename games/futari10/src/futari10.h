// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama Futari Ver 1.0: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::futari10 {

// Ver 1.0's program is an earlier build of Ver 1.5's: everything here was found 2026-11-23 from games/futari15/src/futari15.h,
// its routines by their instruction bytes (most the same, a little lower; the main shot's test differs and was found
// through the routine that calls it) and its places in RAM through the literal pools of the same code (the collision
// kinds 0x26EC lower, the player's record 0x26E0 lower; the credits where they were). The rest of what each says was
// found in Ver 1.5.

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C0601E7u;
// The main loop's wait for the next frame (MAME's driver names it as the game's idle loop)
constexpr uint32_t kIdleLoop = 0x0C05176Au;


// --- A game's beginning: as Mushihimesama's (the first routine found by its code's shape, the second by a looser
// one: its registers differ). A game begun at a later stage shows the stage tally first and starts with 2,500,000
// points: the game's own doing ---
// (a real game calls the first from kNewGameCaller; the attract show's demonstration from elsewhere)
constexpr uint32_t kNewGameCaller = 0x0C0FFF26u;
constexpr uint32_t kNewGame = 0x0C066830u;    // (stage in r4)
constexpr uint32_t kBeginStage = 0x0C068440u; // (stage in r5)

// The sound chip's sequences a boss comes on to (found by listing the sequences started through the first three
// stages; the last bosses' are not checked)
constexpr int kBossTunes[] = {17};

// --- Player 1's record (found by RAM dumps as a ship idles and is lost, and with bombs used) ---
constexpr uint32_t kShips = 0x0C3883DFu;      // the ships left (a byte)
constexpr uint32_t kBombs = 0x0C3883E2u;      // the bombs left (a byte)
constexpr uint32_t kShipState = 0x0C3883FCu;  // 1 while the ship is flown, 2 as it is lost and comes back
// The frames the ship is still out of harm for (0xFE as it comes back, counted down)
constexpr uint32_t kSafeFrames = 0x0C388444u;

// --- What hits the ship (found 2026-11-11: the ship's state set to 2 by 0x0C06B550, called from the ship's hit
// handler 0x0C079AD0, which the collision pass calls; read from that pass, 0x0C064B00 for each player) ---
// The game tests collisions by kinds, as Espgaluda II does: a table of list heads, 12 bytes a kind, the first group at
// +4. A group: the next at +0, tested while its word at +8 is not 0, its first member at +28. A member: the next at +0,
// tested while its word at +8 is not 0, its position the whole parts of 16.16 words at +52 (across) and +56 (up), plus
// the whole parts of those of the block +60 points at (its owner's position, there if not 0), and a box from it reaching
// +32 right, +36 left, +40 up and +44 down (32 bits, both ends in it). Positions count across and up from the bottom of
// the picture stood up (240 x 320).
constexpr uint32_t kKinds = 0x0C387BD8u;
// (where in a group or member: the word at +8 tested, the position at +52, the box at +32; the first member at +28)
constexpr bool kTestedLong = false;
constexpr bool kAddFixed = false;
// (each group of bullets tested by every member)
constexpr bool kBulletFirstOnly = false;
constexpr uint32_t kPosition = 52, kBox = 32;
// The ship: kind 2 (player 2's 3), its box (2 each way), tested against the bullets and the bodies (0x0C064BF6). Kind 16
// (17) is tested as it is but against every body, not only those marked (none seen in play)
constexpr int kShipKind = 2, kShipKindAll = 16;
// The bullets: kind 20, points at their owner's position alone (0x0C065250: in when inside the ship's box)
constexpr int kBulletKind = 20;
// The bodies: kinds 0, 1 and 21, boxes that hit when they meet the ship's (0x0C0655E0), tested while the member's word
// at +12 is not 0, and against kind 2 only when bit 0 of its byte at +27 is set
constexpr int kBodyKinds[] = {0, 1, 21};

// --- What the player's shots hit (found 2026-11-12: the collision pass's second part, 0x0C064D30, called just after
// the ship's) ---
// The enemies are the bodies' members (kinds 0, 1 and 21), every one of them, whether or not it hits the ship, while
// the enemy member's word at +12 is not 0. Each is tested against the shots: kind 4 (player 2's 5), the main shot and
// laser, by 0x0C0657D0 (a shot member in use while its word at +8 and its long at +12 are not 0), and kinds 6 to 10
// (player 2's 11 to 15) by 0x0C0659E0 (its word at +8 alone). The shots' members are laid out as the bodies', boxes
// that hit when they meet. A member of kind 1 or 21 with bit 1 of its byte at +27 set is passed over (shots fly through
// it). The enemies whose bodies do not hit the ship (bit 0 of +27 clear) are the ground ones, drawn as such
constexpr int kShotKindFirst = 4, kShotKindLast = 15;
constexpr int kMainShotKinds[] = {4, 5};
constexpr int kShotsPassKinds[] = {1, 21};

// (the ship's box is the same out of harm: nothing put in its place)
constexpr uint32_t kSafeReach = 0, kShipSizes = 0, kShipType = 0, kSizesAt = 0, kSizesMode = 0, kSizesModeAt = 0;

// --- What the board's code unlocks (found 2026-11-25 by entering it, 2P holding A, B and C after a coin, 1P: right,
// C, B, up, down, left, B, A, C, A, and comparing RAM with a run without) ---
// Ultra mode: a byte, 1 when unlocked (set alone, Ultra is on the mode select). The code also writes it to the
// EEPROM (word 6's low byte), which the board reads back at power on; this is the game's copy in RAM, which the game
// writes back to the EEPROM as a game is played (so held at 1, the board keeps Ultra unlocked from then on, as after the code)
constexpr uint32_t kUltraUnlocked = 0x0C707CAFu;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by dumps of headless play and the game's code) ---
// The game's rank (32 bits): made again each frame by 0x0C06C240 as the larger of the two players' parts (32 bits
// each, made by 0x0C06C300: 4 x the bombs held plus 2 x the record's byte after them), plus the frames played over
// 512 (kRankFrames, one more each frame), plus the mode's base (kRankBase), then cut to the mode's most
// (208 in Original, 240 there when 0x0C068CF0 says so, which adds 32 first; 232 in Maniac, 248 in Ultra, 255 there
// from the 4th stage on). Found: Ver 1.5's routine (futari15.h) at this place, the same code; a dump at frame 4,000 of a game held
// 38 = 24 + 12 + 1,191 frames over 512
constexpr uint32_t kRank = 0x0C387DCCu;
constexpr uint32_t kRankPart1 = 0x0C388CB0u, kRankPart2 = 0x0C388CB4u, kRankFrames = 0x0C387EBCu, kRankBase = 0x0C387EC0u;
// The record the routine reads (0x0C387E90): 1 at +4 while a game is played (0 in the attract show's demonstration: a
// dump of it held 0), the mode at +16 (0 Original, 1 Maniac, 2 the third) and the stage played at +20 (from 0: a game
// begun at stage 3 held 2)
constexpr uint32_t kGameOn = 0x0C387E94u, kMode = 0x0C387EA0u, kStage = 0x0C387EA4u;
constexpr int kRankMost[] = {208, 232, 248};
constexpr int kUltra = 2, kUltraLateStage = 3, kUltraLateMost = 255;
// Player 1's score (32 bits: 13,828 in a dump as the picture showed it)
constexpr uint32_t kScore = 0x0C3883F0u;
constexpr bool kScoreDigits = false;

} // namespace spelunk::futari10
