// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Espgaluda II: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::espgal2 {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C05EB57u;
// The main loop's wait for the next frame (MAME's driver names it as the game's idle loop)
constexpr uint32_t kIdleLoop = 0x0C05177Au;


// --- A game's beginning: the same two routines as Mushihimesama's (found by their code's shape), six stages ---
// (a real game calls the first from kNewGameCaller; the attract show's demonstration from elsewhere)
constexpr uint32_t kNewGameCaller = 0x0C0DBD40u;
constexpr uint32_t kNewGame = 0x0C0649B0u;    // (stage in r4)
constexpr uint32_t kBeginStage = 0x0C0669D0u; // (stage in r5)

// The sound chip's sequences a boss comes on to (found by listing the sequences started through the first three
// stages; the last bosses' are not checked)
constexpr int kBossTunes[] = {9, 10};

// --- Player 1's record (found by RAM dumps as a ship idles and is lost, and with the guard barrier used) ---
constexpr uint32_t kShips = 0x0C37D6FFu;      // the ships left (a byte)
constexpr uint32_t kScore = 0x0C37D70Cu;      // the score's low word
constexpr uint32_t kShipState = 0x0C37D718u;  // 1 while the ship is flown, 2 as it is lost and comes back, 3 with none left
constexpr uint32_t kBarrier = 0x0C37D788u;    // the guard barrier's gauge, and again in the next word (4000 full)
constexpr uint32_t kBarrierFull = 4000;
// The green gems (32 bits, + 0x70 of the record at 0x0C37D6FC; 500 the most): found 2026-11-25 as the one word of the
// record that is the count the picture shows, and by watching what writes it: the routine that adds gems, 0x0C068BD0
// (r4 the record, r5 the gems), puts 500 there in place of any count above 499, so 500 is the game's own most
constexpr uint32_t kGems = 0x0C37D76Cu;
constexpr uint32_t kGemsFull = 500;
// The frames the ship is still out of harm for (0xDA as it comes back, counted down)
constexpr uint32_t kSafeFrames = 0x0C37D7B0u;

// --- What hits the ship (found 2026-11-11, from the ship's hit handler 0x0C07EF50 back to its callers) ---
// The game tests collisions by kinds: two tables of list heads, 12 bytes a kind, the first group at +4. A group: the
// next at +0, tested while its word at +8 is not 0, its first member at +20. A member: the next at +0, tested while its
// words at +8 and +10 are not 0, its position the whole parts of 16.16 words at +28 (across) and +32 (up), plus those of
// the block +36 points at (its owner's position, there if not 0), and a box from it reaching +18 right, +20 left,
// +22 up and +24 down (words, both ends in it). 0x0C0631E0 tests each kind's members against the others.
// Positions count across and up from the bottom of the picture stood up (240 x 320).
constexpr uint32_t kShipKind = 0x0C37D2A0u;   // the ship: its box (2 each way)
constexpr uint32_t kBulletKind = 0x0C37D348u; // the bullets: points, at their owner's position alone (0x0C063AD0)
constexpr uint32_t kBodyKind = 0x0C37D33Cu;   // the enemies whose bodies hit (0x0C063D30)

// --- What the player's shots hit (found 2026-11-12, from the collision pass 0x0C062BC0, which calls the ship's test
// 0x0C0631E0 above and then the enemies') ---
// The two tables above are one, its kinds 12 bytes apart from kEnemyKinds: the ship's kind (kShipKind) is its kind 2,
// the bodies its 15 and the bullets its 16. The enemies the shots can hurt are the members of kinds 0 and 1 (tested
// while their words at +8 and +10 are not 0), boxes laid out as the ship's; each is tested against the shots of each
// player by 0x0C0633D0 (kinds 4 and 5), 0x0C0635F0 (kinds 6, 7, 12 and 13) and 0x0C063710 (kinds 8 and 9). These hurt
// boxes are not the boxes that hit the ship (kind 15, another member of the same enemy where it has one, found by the
// owner at +36 being the same): an enemy with no body box is one the ship can fly over, drawn as a ground enemy
constexpr uint32_t kEnemyKinds = 0x0C37D288u;
constexpr int kHurtKinds[] = {0, 1};
// The shots' kinds (both players'); those tested by 0x0C0633D0 and 0x0C063710 also need their word at +10 not 0
constexpr int kShotKinds[] = {6, 7, 12, 13};
constexpr int kShotKindsTen[] = {4, 5, 8, 9};

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by RAM logs of headless play and the code that writes
// each, SPELUNK_WATCH and SPELUNK_BREAK on the interpreter) ---
// 1 from Start in a real game until the title after it (32 bits; 0 through the attract show and its demonstration:
// found by RAM logs of the attract show, a game played to its end and the title after it), with the first byte of
// player 1's record (kShipRecord: 1 while a game's ship is in it, 0 from the game's end and through the logos). (The
// 32 bits at 0x0C37D390, which the new game's routine sets, go back to 0 at the same frame in two games, about 2,250
// frames after Start, and kShipState above goes 1, 2, 3 as ships are lost, staying 0 in a game played out of harm)
constexpr uint32_t kGameOn = 0x0C37D4E4u;
// Player 1's score (32 bits at kScore: 80,962 in the log as the picture showed it). The stage being played (32 bits,
// from 0: written by the routine that begins a stage, kBeginStage, from its r5, 0x0C066A08; a game begun at stage 3
// and 5 held 2 and 4. A stage's end played through to the next was not seen)
constexpr uint32_t kStage = 0x0C37D428u;
// Kakusei on (a byte, the record + 0x0A: 1 from the press of button 2 to its end; found in the log, the gems going down
// only while it is 1). With no gems left in it, Kakusei Overmode
constexpr uint32_t kKakusei = 0x0C37D706u;
// The gold (a word, the record + 0x74, the number under the gems: 25 in the log as the picture showed 25; the routine
// that adds gold, 0x0C068C60, puts 1,000 there in place of any count above 999)
constexpr uint32_t kGold = 0x0C37D770u;
constexpr int kGoldMost = 1000;
// Overmode's level (a word, the record + 0x84: 0 to 3, kept when a ship is lost; 0x0C068D00 adds one and stops at 3) and
// the way to the next (a word, +0xC2 of player 1's ship task, which is at kShipTask throughout: 2 more each frame of
// Overmode, 0x0C07E5E0, the level up past 499: 250 frames a level. Found: the level 0 to 1 at 249 frames into an
// Overmode in the log, and the task's place, the same in two games begun at other times, by SPELUNK_BREAK there). The
// task's word at +0x50 points to player 1's record (kShipRecord): read only while it does
constexpr uint32_t kOverLevel = 0x0C37D780u;
constexpr uint32_t kShipTask = 0x0C3F9374u, kShipRecord = 0x0C37D6FCu;
constexpr int kOverWay = 0xC2, kOverWayNext = 500, kTaskRecord = 0x50, kOverLevels = 3;

} // namespace spelunk::espgal2
