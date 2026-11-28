// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Akai Katana: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::akatana {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C1E0D7Fu;
// The main loop's wait for the next frame (MAME's driver names it as the game's idle loop)
constexpr uint32_t kIdleLoop = 0x0C1D1346u;


// Player 1's record (found 2026-11-11): the game looks it up by its number (0x81, the routine at 0x0C1ED370) and it is
// at kRecord in every game played so far (the ship's start, 0x0C1F7E60, and its loss, 0x0C1F9200, both find it so).
constexpr uint32_t kRecord = 0x0C7E8628u;
// The frames the ship is still out of harm for (232 as it comes back, counted down; found as the countdown that,
// held, keeps an idle ship alive, and tried through the first three stages): kRecord + 0x1A0, set by 0x0C1FC4E6
constexpr uint32_t kSafeFrames = 0x0C7E87C8u;
// The ships, as a life of 1600 a ship (32 bits): kRecord + 12 is what is left, the most it can be; + 8 the life now (at
// most + 12). A ship lost takes 1600 off + 12 (0x0C1F9200) and brings + 8 down to it; the game ends when it comes to
// 0. Kept at 3200 (two ships, one lost leaves one), the game goes on (tried: an idle ship through two stages). Found by
// RAM dumps across three ships lost (3200, 1600, 0). (The ships' word once found at 0x0CAFF8A8 is a message to the
// picture, from a pool: why it moved)
constexpr uint32_t kShips = kRecord + 12;
constexpr uint32_t kLifePerShip = 1600;
// The energy that Phantom form (button 4 here: input 0x80, held) spends, 5 a frame (32 bits, kRecord + 0x14; found
// 2026-11-25 as the word that follows the green part of the life's bar, filled by the energy items a tapped shot's
// kills leave, 20 each, and spent only with button 4 held). The bar is one of kBarTotal (kRecord + 0x1C, 8000, put
// there as a game begins: 0x0C1F7A30): the life's ceiling (kShips) takes the yellow part and the energy the rest. The
// routine that adds energy, 0x0C1F9470 (r5 the energy), holds it at kBarTotal - kShips, so that is the most it can
// be: 3200 with three ships, more as ships are lost
constexpr uint32_t kEnergy = kRecord + 0x14;
constexpr uint32_t kBarTotal = kRecord + 0x1C;
// The bombs left (a byte; the most it can be is the byte after it, 3 to begin with; found by RAM dumps with and
// without bombs used). The bomb is button 2
constexpr uint32_t kBombs = kRecord + 0x28;
constexpr uint32_t kBombsMax = kRecord + 0x29;

// --- The stage a game begins at (found 2026-11-12 by watching the stage numbers below written as a game begins) ---
// A stage's number is at 0x0C7E674C (the stage manager at 0x0C7E66EC, + 0x60), set as a stage begins (0x0C1F4A70) from
// the next stage's number at + 0x64. That is put there by kGoOnToStage (r4 the manager, r5 the stage, 0 for the first,
// up to 6; r6 1), the game's own way on to a stage (as DoDonPachi SaiDaiOuJou's 0x0C1F27F0: it keeps the stage and 45
// frames on starts the sequence that begins it). A new game calls it from kNewGameCaller with stage 0; given a later
// stage there, the game plays a stage's end (sequences 8 and 9) and begins the stage chosen with the ship flown (tried:
// stages 2 to 6, the stage's number and its own tunes as in play; r5 6 shows nothing, so six stages)
// 1 (32 bits, kRecord + 0xAC) once the ship is the player's, 0 before (and through a later stage's start until its ship
// has flown in): found by RAM dumps across a game's start at stages 1 and 3
constexpr uint32_t kShipState = kRecord + 0xAC;
constexpr uint32_t kGoOnToStage = 0x0C1F48F0u;
constexpr uint32_t kNewGameCaller = 0x0C275DE2u;
// The sound chip's sequences a boss comes on to: 6 its warning and 7 its theme in stages 1 to 4, 18 and 19 the sixth
// stage's (a stage that is its boss), 20 and 21 after it. Found by listing the sequences started through each stage
// begun this way, the ship out of harm and firing; each stage has its own tune besides (3, 4, 13, 15, 17), 8 and 9 a
// stage's end. Stage 5's boss tune is not seen (its boss did not come in 18,000 frames), so its trip times out
constexpr int kBossTunes[] = {6, 7, 18, 19, 20, 21};

// --- What hits the ship (found 2026-11-12 from the ship's handler, 0x0C216E00, that loses a ship through 0x0C1F9200,
// and the tests that call it: DoDonPachi SaiDaiOuJou's engine of collision kinds, games/ddpsdoj/src/ddpsdoj.h, its
// table here at kKinds) ---
// The bullets' test (0x0C1EA398: each group of kind 20 by its first member, a point at its owner's place, inside the
// ship's box) and the bodies' read as SaiDaiOuJou's
constexpr uint32_t kKinds = 0x0C7E45F8u;
constexpr bool kTestedLong = true, kAddFixed = true, kBulletFirstOnly = true;
constexpr uint32_t kPosition = 56, kBox = 36;
constexpr int kShipKind = 2, kShipKindAll = 14, kBulletKind = 20;
constexpr int kBodyKinds[] = {0, 1, 21};
// What the shots hit (found 2026-11-12: the enemies' pass 0x0C1E8100, run after the ship's, tests each member of the body kinds whose
// 32 bits at +12 are not 0 against the player's shots: kinds 6 and 7 (0x0C1EB280) and 10 and 11 (0x0C1EB750), player 2's
// 8, 9, 12 and 13, unless bit 7 of the member's byte at +27 is set; and kind 4 (player 2's 5)
// (0x0C1EAF80), unless bit 1 of it is set. The shots' boxes are read as every other kind's.) A member they can hurt
// that does not hit the ship (bit 0 of +27 clear) is a ground enemy: a tank, a turret
constexpr int kShotKinds[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
constexpr uint8_t kNotShot = 0x80, kNotShotByKind4 = 0x02;
// (Here kind 4 is the main shot: boxes 28 x 8 in a line ahead of the ship, seen in RAM dumps with the shot held; kinds 6
// to 13 had none then)
// The game's up is the picture's down: a member's place counts up from the picture's bottom, kPictureHeight below its
// top (found 2026-11-12 with bullets and tanks in a RAM dump against the picture: each one's place mirrored about the
// middle; the ship, kept in the middle, did not show it)
constexpr int kPictureHeight = 240;

// --- What the gadgets read (port/gadgets.h; found 2026-11-28 by RAM logs of headless play) ---
// 1 while a game is played (32 bits, the stage manager + 0x54: 1 all through a game played here, 0 through the logos,
// the title and the attract show's demonstration; found by RAM logs of both. kShipState above stayed 0 through that
// game, played out of harm)
constexpr uint32_t kGameOn = 0x0C7E6740u;
// The stage being played, from 0 (32 bits: the stage manager's number above)
constexpr uint32_t kStage = 0x0C7E674Cu;
// Player 1's score (ten bytes at kRecord + 0x84, a digit each, the lowest first: 0, 7, 7, 3, 0, 1 in a dump as the
// picture showed 103,770)
constexpr uint32_t kScore = kRecord + 0x84;

} // namespace spelunk::akatana
