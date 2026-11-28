// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Muchi Muchi Pork!: where things are in the game's memory, with what each was found from.
#pragma once

#include <cstdint>

namespace spelunk::mmpork {

// The credits (a byte; found as the one byte of RAM that counts three coins in: 0, 1, 2, 3)
constexpr uint32_t kCredits = 0x0C4B75A7u;
// The main loop's wait for the next frame (where the CPU is at the end of every frame)
constexpr uint32_t kIdleLoop = 0x0C05176Au;

// Ibara's engine (games/ibara/src/ibara.h says what each is); its places found 2026-11-11 by the same routines' code.

// --- Player 1's record (0x24C bytes; found by RAM dumps as a ship idles and is lost, then the code using them) ---
constexpr uint32_t kShipRecord = 0x0C510B28u;
constexpr uint32_t kShips = 0x0C510D2Bu; // the ships left (a byte, record +0x203)
// The bombs in stock (a byte, record +0x214; 2 as a game begins, one taken by a bomb: found 2026-11-12 by RAM dumps with
// and without a bomb; the HUD's B icons go with it). The bomb is button 3 (input bit 0x40; button 2 is the lard attack)
constexpr uint32_t kBombs = 0x0C510D3Cu;
// The lard meter (a word, record +0x20E; 104 full: found 2026-11-24 by RAM dumps of made-up play, the one value that
// rose with the bar at the bottom of the picture, 26, 39, 60, 87 at a quarter, two fifths, three fifths and most of it,
// then stayed at 104 with the bar full)
constexpr uint32_t kLard = 0x0C510D36u;
constexpr uint32_t kLardFull = 104;

// --- What the gadgets read (port/gadgets.h). From the MAME layout for this game by wnka
// (github.com/wnka/muchi-muchi-pork-mame-overlay, made from Olifante's for Pink Sweets, its places found with MAME's
// debugger), checked 2026-11-27 against RAM dumps of a game here: the score at frame 4900 read 54,120 as the picture
// showed it, and the rank went down from 0xD7351A to 0xD69678 over the stage's next 1,400 frames ---
// Player 1's score (a long)
constexpr uint32_t kScore1 = 0x0C510D50u;
// The rank (a long, counted DOWN as it rises: 0xE7FFF0 is the layout's lowest, and its bar fills by 0x1000000 below it)
constexpr uint32_t kRank = 0x0C4EB5F4u;
constexpr uint32_t kRankLowest = 0xE7FFF0u, kRankSpan = 0x1000000u;
// The next medal's worth (a byte: 0 to 8 for 100 to 900, 9 to 18 for 1,000 to 10,000)
constexpr uint32_t kNextMedal = 0x0C53F4A7u;
// The enemies (192 records of 200 bytes): a record's kind (a long at +8), its part of a boss (a long at +12), its life
// (a word at +68) and the boss's phase (a byte at +180, odd numbers). The layout's table of the bosses' kinds, and for
// each phase which part's life is the one that counts (until the first is destroyed the next takes no damage), is
// game.cpp's BossLife; the layout says boss 2 and stage 4's first midboss read oddly
constexpr uint32_t kEnemies = 0x0C4B7C9Cu;
constexpr int kEnemyCount = 192, kEnemySize = 200, kEnemyKind = 8, kEnemyPart = 12, kEnemyLife = 68, kEnemyPhase = 180;
// Out of harm while flown, the frames counted to 60 while lost (a byte, record +0x21E): held at 59, both at once
// (Ibara's kSafeFrames; the first finding held it at a value the count to 60 never meets, so a ship lost at that moment
// never came back)
constexpr uint32_t kSafeFrames = 0x0C510D46u;
constexpr uint32_t kSafeFramesHeld = 59;

// --- The game's script (as Ibara's: found 2026-11-11 from the stage's writers) ---
// Command 3's routine begins a stage: kScript then points at its argument, the stage script's number (it goes to
// 0x0C48F598, 32 bits). A real game's script is a list of these commands (six bytes each), the stage scripts in the order
// 3, 1, 2, 4, 5; kFirstStage is where the first one's argument is. The attract show's demonstration plays a stage without it
constexpr uint32_t kBeginStage = 0x0C0769E0u;
constexpr uint32_t kScript = 0x0C515338u;
constexpr uint32_t kFirstStage = 0x0C110BF6u;
constexpr int kBeginStageSize = 6;
// The sound chip's sequences a boss comes on to: 2 as it arrives and 3 its theme in stages 1 to 4, 23 and 24 the last stage's (found by listing the sequences
// started through a game played with the ship out of harm; 19/20 and 21/22 are the stages' middle bosses, left out)
constexpr int kBossTunes[] = {2, 3, 23, 24};

// --- What hits the ship (found 2026-11-12 from the collision pass's tests of the ship against the enemies' bodies,
// 0x0C074250, and the bullets, 0x0C0745C0, called in that order from 0x0C06F9F8; Pink Sweets' scheme, games/pinkswts/
// src/pinkswts.h, with other places) ---
// Every box is a half-size about a point: four 32-bit words (16.16), across (with flags in its top four bits) and up,
// then where it is from its owner's position; a list ends with a word 0. A box hits another when they overlap. An
// owner's flags turn its boxes: 0x80000000 the offset across taken away, 0x40000000 the one up.
// Positions are 16.16, x across the board, y down it from kCamera (the word the ships' setup, 0x0C063180, clears first,
// as Pink Sweets' and Ibara's clear their cameras).
constexpr uint32_t kCamera = 0x0C53EDE0u;
// The ship's boxes: the table at kShipBoxes by record +8 (32 bytes an entry; its word at +4 a table by +12, 16 bytes an
// entry, whose first word is a table by +16, 8 bytes an entry: the owner's flags, then the list); the position at record
// +28, +32. A box with flag 0x20000000 is the graze box. Tested against the bodies while record +0 has 0x08000000 (one
// player a frame), against the bullets while it has 0x04000000
constexpr uint32_t kShipBoxes = 0x0C0E247Cu;
constexpr uint32_t kRecordSize = 0x24C;
// The bullets: pointers from kBulletsFrom up to the word at kBulletsTo; tested while +0 has 0x00800000; flags at +32,
// box list at +36, position +48, +52
constexpr uint32_t kBulletsFrom = 0x0C4C12E4u, kBulletsTo = 0x0C48A180u;
// The enemies whose bodies are tested: 8-byte entries (the enemy at +4) from kEnemiesFrom up to the word at kEnemiesTo;
// tested while +0 has 0x00800000; flags at +28, box list at +32, position +44, +48
constexpr uint32_t kEnemiesFrom = 0x0C53EE7Cu, kEnemiesTo = 0x0C515334u;
// The pass's first test (the bodies'): the boxes are read as it is entered
constexpr uint32_t kHitTest = 0x0C074250u;

// --- What the shots hit (found 2026-11-12 from the pass's tests before kHitTest: 0x0C073490, the shots, and
// 0x0C0737F0, the options' shots; Pink Sweets' scheme, other places) ---
// An enemy of the bodies' list is tested against the shots while +0 has kShootable (every box of it); one without
// 0x00800000 (its body never hits the ship) is a ground one
constexpr uint32_t kShootable = 0x00200000u;
// A shot: a record of 96 bytes, tested while +0 has 0x08000000; its boxes from kShotBoxes by +4, +8 and +12 as the
// ship's are by its +8, +12, +16 (the entry's first word the owner's flags, then the list); its position at +20, +24
constexpr uint32_t kShotBoxes = 0x0C0E265Cu;
constexpr uint32_t kShotSize = 96;
// The game's shots go on while the word at kShotsOn is not 0. Each player's (while the word at kPlayerOn + 4 * player
// is not 0): so many as the word at kShotCount says, from kShots + player * 0xC00
constexpr uint32_t kShotsOn = 0x0C510908u, kPlayerOn = 0x0C4C22E4u;
constexpr uint32_t kShots = 0x0C513524u, kShotsApart = 0xC00, kShotCount = 0x0C47D280u;
// The options' shots: each player's (while the word at kOptionsOn + 4 * player is not 0) options, so many as the word
// at kOptionCount says, each while the word at kOptionOn + 4 * (player * 6 + option) is not 0: so many shots as the
// word at kOptionShotCount, from kOptionShots + player * 0x2400 + option * 0x600
constexpr uint32_t kOptionsOn = 0x0C4C350Cu, kOptionCount = 0x0C4C3514u, kOptionOn = 0x0C4C12A0u;
constexpr uint32_t kOptionShots = 0x0C48591Cu, kOptionShotCount = 0x0C48A174u;

// --- What the board's codes unlock (found 2026-11-25: each code entered on the title with a credit in, 1P: Manpuku
// right, left, left, B, B, B, down, left, up, A, B, right, C; Harahara A, B, right, right, C, C, left, B, left, A, A,
// up, down. The routine at 0x0C068A38 packs these into the record at 0x0C482260 (bytes +0x14, +0x13) from a table at
// 0x0C0E0690, which names the game's own: set alone, holding up (Manpuku) or down (Harahara) as Start is pressed
// begins that mode, and without the hold a game is as ever) ---
constexpr uint32_t kManpukuUnlocked = 0x0C4EAC43u, kHaraharaUnlocked = 0x0C4824AFu; // (the low bytes of two longs: 1)

} // namespace spelunk::mmpork
