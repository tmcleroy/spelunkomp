// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What every game of the board offers in the same way, given the game's own facts (its names header): free play.
#pragma once

#include "../machine/board.h"
#include "game.h"

namespace spelunk::arcade {

inline recomp::FeatureInfo FreePlayFeature() {
    return {"free_play", "Game", "Free play",
            "No coins needed: Start always begins a game or continues one. Off: as the board, a coin (5, or a gamepad's Select) "
            "for each credit.",
            recomp::kFeatureEnhanced, {}};
}

// Keeps a credit in the game's counter (a byte of main RAM) once the game is running: the board's own code takes it
// at Start and it is there again for the next
inline void KeepCredit(Board &board, uint32_t creditsAddress, bool on) {
    // (not through the board's start-up: its memory check owns RAM then; every game is past it well before this)
    if (!on || !creditsAddress || board.frame < 1200) {
        return;
    }
#ifdef SPELUNK_CAVE68K
    uint8_t &credits = board.ram[creditsAddress & (board.ram.size() - 1)];
#else
    uint8_t &credits = board.ram[creditsAddress & (SH3::kRamSize - 1)];
#endif
    if (credits == 0) {
#ifndef SPELUNK_CAVE68K
        board.cpu.NoteRamWrite(creditsAddress); // (run-ahead's keeps: the page written)
#endif
        credits = 1;
    }
}

#ifdef SPELUNK_CAVE68K
// Free play for a game that goes to its "press start" screen only for a credit it sees arrive (one simply there from
// power on leaves it in its attract show, where Start does nothing): a press of Start with no credit gives the game
// one, as a coin would have, and holds Start for it until the game has had the time to see both.
// `again`: a game that answers that Start with a screen asking for Start (Air Gallet: a credit that did not arrive as
// a coin only takes its attract show to the title, and not from every part of it) has Start pressed again for the
// player every `again` frames, until the game has taken the credit (or 15 seconds have gone)
inline void CreditAtStart(Board &board, uint32_t creditsAddress, bool on, uint32_t again = 0) {
    constexpr uint32_t kStarts = kSysStart1 | kSysStart2;
    // (the second word: the frames Start is still held for in its low byte; above it, for `again`, whose Start it was
    // and the frames since)
    uint32_t &before = board.featureWords[0], &word = board.featureWords[1];
    const uint32_t starts = board.system & kStarts, pressed = starts & ~before;
    before = starts;
    if (!creditsAddress) {
        return;
    }
    uint8_t &credits = board.ram[creditsAddress & (board.ram.size() - 1)];
    uint32_t hold = word & 0xFF, who = (word >> 8) & 0xFF, since = word >> 16;
    if (on && pressed && credits == 0 && board.frame > 1200) {
        credits = 1;
        board.systemHeld = pressed;
        hold = 24;
        who = again ? pressed : 0;
        since = 0;
    } else if (hold && --hold == 0) {
        board.systemHeld = 0;
    }
    if (who) {
        since++;
        if (credits == 0 || since > 900) {
            who = since = 0;
        } else if (since % again == 0) {
            board.systemHeld = who;
            hold = 6;
        }
    }
    word = hold | (who << 8) | (since << 16);
}
#endif

} // namespace spelunk::arcade
