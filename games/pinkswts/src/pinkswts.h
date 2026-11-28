// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Pink Sweets: Ibara Sorekara: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::pinkswts {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C4164B0u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C05176Au;

// Ibara's engine (games/ibara/src/ibara.h says what each is); its places found 2026-11-11 by the same routines' code.

// --- Player 1's record (0x2B8 bytes; found by RAM dumps as a ship idles and is lost, then the code using them) ---
constexpr uint32_t kShipRecord = 0x0C49C5DCu;
constexpr uint32_t kShips = 0x0C49C857u; // the ships left (a byte, record +0x27B)
// Out of harm while flown, the frames counted to 60 while lost (a byte, record +0x28F): held at 59, both at once
// (Ibara's kSafeFrames; the first finding held it at a value the count to 60 never meets, so a ship lost at that moment
// never came back)
constexpr uint32_t kSafeFrames = 0x0C49C86Bu;
constexpr uint32_t kSafeFramesHeld = 59;
// The game has no bombs: its Rose Cracker (released by the shot button while its gauge is full, the gauge filling while
// the ship does not shoot) stands for them. The gauge is a byte (record +0x286), full at 128: the shot's routine
// (0x0C07AA00) releases a Rose Cracker (0x0C0984B0) when the shot is pressed with it at exactly 128. Found 2026-11-12
// from the shot's code; tried: held at 128, every tap of the shot releases one ("Bomber!")
constexpr uint32_t kRoseCracker = 0x0C49C862u;
constexpr uint32_t kRoseCrackerFull = 128;

// --- The game's script (as Ibara's: found 2026-11-11 from the stage's writers) ---
// Command 2's routine begins a stage: kScript then points at its argument, the stage script's number (it goes to
// 0x0C3B64A6). A real game's script is a list of these commands (six bytes each), the stage scripts in the order
// 1, 6, 2, 3, 4, 5, 7; kFirstStage is where the first one's argument is. The attract show's demonstration plays a stage without it
// (seven stages: begun at the 2nd to 7th they are Ocean, Ruins, Waterway, Desert, Clouds and Hall, as the game
// names them)
constexpr uint32_t kBeginStage = 0x0C0798B0u;
constexpr uint32_t kScript = 0x0C4ABEDCu;
constexpr uint32_t kFirstStage = 0x0C13102Au;
constexpr int kBeginStageSize = 6;
// The sound chip's sequences a boss comes on to: 22 as it arrives and 23 its theme in stages 1, 2 and 6, 24 and 25 in stages 3 to 5 (found by listing the
// sequences started through a game played with the ship out of harm; each stage has its own pair besides, from 0/1)
constexpr int kBossTunes[] = {22, 23, 24, 25};

// --- What hits the ship (found 2026-11-12 from the collision pass, 0x0C070AFC: its tests of the ship against the
// enemies' bodies, 0x0C076AE0, and the bullets, 0x0C076E50; read by their code, not from Ibara's, whose differ) ---
// Every box is a half-size about a point: four 32-bit words (16.16), across (with flags in its top four bits) and up,
// then where it is from its owner's position; a list of them ends with a word 0. A box hits another when they overlap:
// |x - x'| < w + w', |y - y'| < h + h'. An owner's flags (the top two bits of a word of its own) turn its boxes: 0x80000000
// the offset across is taken away, 0x40000000 the one up (both: both).
// Positions are 16.16: x across the board, y down it in a field wider than the screen, shown from kCamera on (16.16;
// the word Ibara's ship setup clears first, 0x0C063460, as Ibara's clears its camera).
constexpr uint32_t kCamera = 0x0C5BFF8Cu;
// The ship's boxes: the table at kShipBoxes by the record's +8 (32 bytes an entry; its word at +4 a table by +12, 16
// bytes an entry, whose first word is a table by +16, 8 bytes an entry: the owner's flags, then the list); its position
// at record +24, +28. A box with flag 0x20000000 is the graze box (a bullet there scores); the others are the ship's hit
// box. The ship is tested against the bodies while record +0 has 0x08000000 (one player a frame, by the frame's low
// bit), against the bullets while it has 0x04000000
constexpr uint32_t kShipBoxes = 0x0C107F0Cu;
constexpr uint32_t kRecordSize = 0x2B8;
// The bullets: a list of pointers from the word at kBulletsFrom up to the word at kBulletsTo, made for the pass; a
// bullet is tested while its +0 has 0x00800000; its box list at +32, its flags at +36, its position at +48, +52
constexpr uint32_t kBulletsFrom = 0x0C106BF8u, kBulletsTo = 0x0C5C178Cu;
// The enemies whose bodies are tested: a list of 8-byte entries (the enemy's address at +4) from the word at
// kEnemiesFrom up to the word at kEnemiesTo, tested while +0 has 0x00800000 and the byte at +0xC4 is not below the one
// at +0xC6; its box list at +28, its flags at +32, its position at +44, +48 (every box of an enemy takes the ship)
constexpr uint32_t kEnemiesFrom = 0x0C106BF4u, kEnemiesTo = 0x0C5C3790u;
// The pass's first test (the bodies'): the boxes are read as it is entered, as the game tests them
constexpr uint32_t kHitTest = 0x0C076AE0u;

// --- What the shots hit (found 2026-11-12 from the two tests the pass makes before kHitTest: 0x0C075BA0, the shots,
// and 0x0C076150, the other shots; each tests every box of a shot against every box of each enemy of the bodies' list
// whose +0 has kShootable, with the same byte test as the bodies') ---
// An enemy the shots hurt whose body never hits the ship has kShootable and not 0x00800000: a ground one
constexpr uint32_t kShootable = 0x00200000u;
// A shot: a record of kShotSize bytes, tested while +0 has 0x08000000; its boxes from the table at kShotBoxes by +4
// (32 bytes an entry; its word at +4 a table by +8, 16 bytes an entry, whose first word is a table by +12, 8 bytes an
// entry: the owner's flags, then the list), as the ship's; its position at +20, +24
constexpr uint32_t kShotBoxes = 0x0C10816Cu;
constexpr uint32_t kShotSize = 0x84;
// The shots: each player's (while the word at kShotsOn + 2 * player is not 0; all while the word at kShotsAll is not
// 0) from kShots + player * kShotsApart, so many as the word at kShotCount says
constexpr uint32_t kShotsAll = 0x0C49C2C2u, kShotsOn = 0x0C42379Au;
constexpr uint32_t kShots = 0x0C4A3ACCu, kShotsApart = 0x4200, kShotCount = 0x0C3AD0E0u;
// The other shots: the set (bank) chosen by the word at kOtherBank (32 bits) plus one, its low bit; tested while the
// word at kOtherOn + 2 * bank is not 0: so many groups as the word at kOtherGroups, each so many shots as the word at
// kOtherCount, from kOther + bank * 0xC60 + group * 0x420
constexpr uint32_t kOtherBank = 0x0C3B0F24u, kOtherOn = 0x0C4240B6u, kOtherGroups = 0x0C4240BAu, kOtherCount = 0x0C3B298Eu;
constexpr uint32_t kOther = 0x0C3B108Cu, kOtherBankApart = 0xC60, kOtherGroupApart = 0x420;

// --- What the gadgets read (port/gadgets.h). From Olifante's MAME layout for the game (v0.9, linked from
// epozzobon.it/re), checked 2026-11-27 against RAM dumps here at frames 3,500 and 5,000 of a game: the score 38,100 as
// the picture showed it, the rank, the zan counter, the drop and the medal all moving as play went on ---
// Player 1's score (a long), and player 1's record (its first long's top bit set while a player flies it)
constexpr uint32_t kScore1 = 0x0C49C874u, kPlayer1 = 0x0C49C5DCu;
// The rank (a long, counted DOWN as it rises: the layout shows 1 - rank / 0x1000000; 19% to 20% over those frames)
constexpr uint32_t kRank = 0x0C453AB0u;
// The zan counter (the layout's: the word at kZan less the word at kZanLess; 78 then 132)
constexpr uint32_t kZan = 0x0C4A3AB6u, kZanLess = 0x0C3AE102u;
// The next drop (a byte: its place in the game's round of eight, bullet, rose, double, rose, triple, rose, option,
// rose); the shot and option power-ups dropped (bytes)
constexpr uint32_t kNextDrop = 0x0C4240A4u, kShotsDropped = 0x0C5C16F8u, kOptionsDropped = 0x0C5C16F9u;
// The next medal's worth (a byte: 0 to 8 for 100 to 900, 9 to 18 for 1,000 to 10,000)
constexpr uint32_t kNextMedal = 0x0C5C0162u;
// The enemies (256 records of 204 bytes): a long of flags (top bit: in use; 0x4000: not hittable), its kind (a long at
// +4), its part (+8), its life (a word at +68), its phase (a byte at +184). The bosses' kinds and the part whose life
// counts are the layout's table (game.cpp's BossLife)
constexpr uint32_t kEnemies = 0x0C416968u;
constexpr int kEnemyCount = 256, kEnemySize = 204, kEnemyKind = 4, kEnemyPart = 8, kEnemyLife = 68, kEnemyPhase = 184;

} // namespace spelunk::pinkswts
