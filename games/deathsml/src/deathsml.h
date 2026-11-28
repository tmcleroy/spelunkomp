// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Deathsmiles: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::deathsml {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C060B23u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C0519A2u;

// --- A game's beginning (found 2026-11-11) ---
// The game's record is at kGame. Its stages are played from a list of nine places (at kGame + 0xAD8, 8 bytes each: the
// place and the level chosen for it), the n-th stage played being the list's n-th; the map before a stage writes the
// place chosen into the list (0x0C06EA50), and 0x0C06D010 begins the stage from it. A new game is begun by kNewGame,
// which takes the count of stages played in r5 (0 for a new game) and keeps it at kGame + 0x58 (found as the one
// writer of that count, called once from kNewGameCaller as a game begins). Given n instead, the game skips the map and
// begins the list's n-th place as it stands at a new game: the place numbered n, its graphics loaded in full (the
// stage begun at 0x0C06D010 alone, with another number, has the map's place's graphics)
constexpr uint32_t kGame = 0x0C3886C4u;
constexpr uint32_t kNewGame = 0x0C06F040u;       // (stages played in r5)
constexpr uint32_t kNewGameCaller = 0x0C1146C0u;
constexpr uint32_t kPlace = 0x0C38867Cu;         // the place being played (32 bits)
// The sound chip's sequences a boss comes on to: 12 as it arrives then 13 its theme, or 17 then 14 (found by listing
// the sequences started through each place begun as above). The last places' (6 and 7: 28 and 29 there) are not known
constexpr int kBossTunes[] = {12, 13, 17, 14};
// 1 while the ship is flown (0 on the map and as the ship flies in; found by RAM dumps across a game)
constexpr uint32_t kShipState = 0x0C38873Cu;

// --- Player 1's record (kPlayer; the ship's object holds a pointer to it at +0x5C) ---
// The life is a count of 130 a ship (a 16-bit word, 389 for three ships: 0x0C0711D0 caps it at the ships times 130,
// less 1). The game's ship handler (0x0C07F280, called by the collision routine at 0x0C081270 when something that hits
// meets the ship) takes off a damage from a table (0x0C2734FC) and, when that crosses into the ship before, makes it
// a lost ship: the life is then the ships left times 130, less 1; at 0 the game is over
constexpr uint32_t kPlayer = 0x0C388F34u;
constexpr uint32_t kLife = kPlayer + 6;
constexpr int kLifePerShip = 130;
// The bombs left (a byte; found by RAM dumps with and without bombs used; three a ship). The bomb is button 3
constexpr uint32_t kBombs = 0x0C3887F7u;
// The frames the ship is still out of harm for (32 bits): the collision routine leaves the ship alone while this is not
// 0 (0x0C070FF0: it and a second count, of a part kept at kPlayer + 0xBC), and the ship handler sets it to 64 after a
// ship is lost. Held up, nothing hits the ship (tried through three stages). While it is not 0 the game also gives the
// ship a box of 24 pixels each way (its items' reach, 0x0C07F5D6), which hits nothing
constexpr uint32_t kSafeFrames = kPlayer + 0xAC;

// --- What hits the ship (the game's collision kinds) ---
// A table of 26 kinds at kKinds, 12 bytes each, its groups' list at +4 (each group: the next at +0, in use while its
// word at +8 is not 0, its first member at +28). A member: the next at +0, in use while its word at +8 and its word at
// +12 are not 0; its place is the high words of +52 (across) and +56 (up), added to the high words of its owner's +0
// and +4 (the owner at +60, if any); its box reaches right +32, left +36, up +40, down +44 (whole pixels). Up is from
// the bottom of the picture. The routine at 0x0C065B40 tests, for each member of the ship's kind (2 for player 1):
// - the bullets (kind 20, 0x0C066250): each a point, its owner's place alone, that hits when inside the ship's box;
// - the bodies (kinds 0, 1 and 21, 0x0C066690): boxes that hit when they meet the ship's, of members whose byte at +27
//   has bit 0 set;
// kinds 18 (items) and 19 are tested too, and do not hit (the ship handler sees their flags 0x40 and 0x80, not 0xC18)
constexpr uint32_t kKinds = 0x0C3869C0u;
constexpr int kShipKind = 2, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the player's shots hit (found 2026-11-12, as Mushihimesama Futari's: the enemies' pass 0x0C065CA0, called after
// the ship's): every member of the bodies' kinds, whether or not it hits the character, while its long at +12 is not 0,
// is tested against the shots: kind 4 (player 2's 5), the main shot, by 0x0C066AC0 (a shot member in use while its
// word at +8 and its long at +12 are not 0), and kinds 6 to 10 (player 2's 11 to 15) by 0x0C066D40 (its word at +8
// alone); members laid out as the bodies', boxes that hit when they meet. A member of kind 1 or 21 with bit 1 of its
// byte at +27 set is passed over. The enemies whose bodies do not hit the character (bit 0 of +27 clear) are the
// ground ones, drawn as such
constexpr int kShotKindFirst = 4, kShotKindLast = 15;
constexpr int kMainShotKinds[] = {4, 5};
constexpr int kShotsPassKinds[] = {1, 21};
// The ship's box as the game sets it (0x0C080340): per character, from a record whose pointer is at kShipBoxes + 4 *
// the character (the byte at the ship's object + 0xB4; the object is the ship member's owner less 4); its bytes +9 and
// +8 are the reach right and left (+8 and +9 when the long at the object + 0x194 is 0x80), +10 up, +11 down
constexpr uint32_t kShipBoxes = 0x0C27AAB0u;
constexpr int kOutOfHarmReach = 24;

// --- What the board's codes unlock (found 2026-11-25 by entering each, 2P holding A, B and C after a coin, and
// comparing RAM with a run without; each byte set alone puts its part on the choices) ---
// Follett (1P: up x4, B, B, A, A, left, left, right, right, C x3, down, C), Rosa (C, B, A, C, B, up, A, right, C, left,
// B, down, C, B, A, A, A) and the canyon's extra stage (C x3, A, up, left, right, down, B, C x3, down, down, A, B, C, B,
// A): a byte each, 1 when unlocked. Kept in RAM only: a code is for the board's power on, never written to the EEPROM
constexpr uint32_t kFollettUnlocked = 0x0C38861Cu, kRosaUnlocked = 0x0C38861Du, kCanyonUnlocked = 0x0C38861Eu;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by dumps of headless play) ---
// The list of places above (kGame + 0xAD8, 8 bytes a stage: the place, then the level chosen for it, 1 to 3: a game
// with the map's level 1, 2 and 3 chosen for its first stage held 1, 2 and 3 there), and the stages played before
// this one (32 bits at kGame + 0x58: the new-game routine's r5; a stage's end seen to add one was not played here)
constexpr uint32_t kStages = kGame + 0xAD8, kStagesPlayed = kGame + 0x58;
constexpr int kStageEntry = 8, kStageCount = 9, kLevelHighest = 3;
// (the places' names, by number, and each level's as the map shows it)
constexpr const char *kPlaceNames[] = {"PORT", "GRAVEYARD", "FOREST", "LAKE", "RUINS", "CAVERN", "CASTLE", "CASTLE"};
constexpr const char *kLevelNames[] = {"", "1", "2", "3"};
// 1 while a game is played (32 bits, kGame + 0x48: 1 through the map and the stages of a game played here, 0 through
// the logos, the title and the attract show's demonstration, whose ship is flown too; found by dumps of each. Not
// seen after a game's end)
constexpr uint32_t kGameOn = kGame + 0x48;
// Player 1's score (32 bits, kPlayer + 0x2C: 72,300 in a dump as the picture showed it)
constexpr uint32_t kScore = kPlayer + 0x2C;
constexpr bool kScoreDigits = false;

} // namespace spelunk::deathsml
