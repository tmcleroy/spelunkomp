// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Deathsmiles MegaBlack Label: where things are in the game's memory, with what each was found from. The game is
// Deathsmiles' (games/deathsml/src/deathsml.h says what each is and how it was found); these are its places here,
// found 2026-11-11 by the same means (the countdown after a hit at 0x0C66098A, then the ship handler 0x0C2023B0 and the
// collision routine 0x0C204650 that calls it).
#pragma once

#include <cstdint>

namespace spelunk::dsmbl {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C1E109Fu;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C1D1346u;

// --- A game's beginning ---
// The new-game routine takes the count of stages played in r5 (it allows 0 to 10; it keeps it at the game's record
// 0x0C660A18 + 0x5C), called once from kNewGameCaller as a game begins. Given n, the game skips the map and begins the
// place numbered n: 0 to 6 as Deathsmiles', 7 a canyon of this version's (its tunes 30 and 31), 8 the castle that is
// Deathsmiles' 8th place. 9 begins with broken graphics and is not offered
constexpr uint32_t kNewGame = 0x0C1EFE60u;       // (stages played in r5)
constexpr uint32_t kNewGameCaller = 0x0C2C6C10u;
// The bosses' tunes as Deathsmiles' (listed through each place begun as above; the castles' and the canyon's not known)
constexpr int kBossTunes[] = {12, 13, 17, 14};
constexpr uint32_t kShipState = 0x0C660A98u;     // 1 while the ship is flown

// --- Player 1's record ---
constexpr uint32_t kPlayer = 0x0C661458u;
constexpr uint32_t kLife = kPlayer + 6;          // 130 a ship (the same table of damages, 0x0C4737BC)
constexpr int kLifePerShip = 130;
constexpr uint32_t kBombs = 0x0C660B67u;         // (button 3; found by RAM dumps with and without bombs used)
// The safe count: the collision routine leaves the ship alone while it is not 0 (0x0C1F1EC0 reads kPlayer + 0xBC + 28)
constexpr uint32_t kSafeFrames = kPlayer + 0xD8;

// --- What hits the ship ---
// The collision kinds as Deathsmiles' (12 bytes each, kind k at kKinds + 12k), except that a group's and a member's
// in-use flags at +8 are 32 bits. The ship is kind 2, the bullets kind 20 (tested as Deathsmiles', 0x0C1E68C0: points
// inside the ship's box). The bodies are tested as Deathsmiles' (read 2026-11-12: the pass 0x0C1E61E4 takes each member
// of the ship's kind and calls 0x0C1E6D00 with the kinds 0, 1 and 21: members whose words at +8 and +12 are not 0 and
// whose byte at +27 has bit 0 set, boxes that hit when they meet the ship's). 0x0C1E7120 and 0x0C1E7390, which round 1
// took for the bodies' test, test the enemies against the player's shots (kinds 4 and up), not the ship
constexpr uint32_t kKinds = 0x0C65E548u;
constexpr int kShipKind = 2, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the player's shots hit: as Deathsmiles' (the enemies' pass 0x0C1E6340 tests every member of kinds 0, 1 and 21
// against the shots' kinds 4 and 5 by 0x0C1E7120, those needing their long at +12, and 6 to 15 by 0x0C1E7390; found
// 2026-11-12)
constexpr int kShotKindFirst = 4, kShotKindLast = 15;
constexpr int kMainShotKinds[] = {4, 5};
constexpr int kShotsPassKinds[] = {1, 21};
// The ship's box per character (five here) and the ship object's fields that choose it (0x0C203690)
constexpr uint32_t kShipBoxes = 0x0C47C388u;
constexpr uint32_t kCharacter = 0xB4, kTurned = 0x198;
constexpr int kOutOfHarmReach = 24;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by dumps of headless play: Deathsmiles' places, deathsml.h,
// at this version's) ---
// The list of places (kGame + 0xD08, 8 bytes a stage: the place, then the level chosen for it: 1, 2, 3, and 4 for
// level 999; the map's four levels chosen for a first stage held 1 to 4 there) and the stages played before this one
// (kGame + 0x5C, the new-game routine's r5)
constexpr uint32_t kGame = 0x0C660A18u;
constexpr uint32_t kStages = kGame + 0xD08, kStagesPlayed = kGame + 0x5C;
constexpr int kStageEntry = 8, kStageCount = 9, kLevelHighest = 3;
constexpr const char *kPlaceNames[] = {"PORT", "GRAVEYARD", "FOREST", "LAKE", "RUINS", "CAVERN", "CASTLE", "CANYON", "CASTLE"};
constexpr const char *kLevelNames[] = {"", "1", "2", "3", "999"};
// 1 while a game is played (32 bits, kGame + 0x4C: 1 through the map and the stage of games here, 0 through the logos,
// the title and the attract show's demonstration, whose ship is flown too. Not seen after a game's end)
constexpr uint32_t kGameOn = kGame + 0x4C;
// Player 1's score (ten bytes at kPlayer + 0x38, a digit each, the lowest first: 0, 6, 4, 2, 4 in a dump as the picture
// showed 42,460)
constexpr uint32_t kScore = kPlayer + 0x38;
constexpr bool kScoreDigits = true;

} // namespace spelunk::dsmbl
