// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DonPachi (Japan, Ver. 1.01): the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "donpachij.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    // While a player's game is played (the mode word's 7: the attract show's demonstration is 5, a continue offered 8)
    bool Playing(const Board &board) {
        return Word(board, donpachij::kMode) == donpachij::kModePlay;
    }
    // Player 1's score in points (BCD)
    long long Score(const Board &board) {
        const uint32_t bcd = static_cast<uint32_t>(Word(board, donpachij::kScore1)) << 16 | static_cast<uint32_t>(Word(board, donpachij::kScore1 + 2));
        long long n = 0, scale = 1;
        for (uint32_t b = bcd; b; b >>= 4, scale *= 10) {
            n += (b & 15) * scale;
        }
        return n;
    }
    // The boxes the game's hit test (0055DA) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace donpachij;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + 6), y = Word(board, record + 8);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((board.ram[at & 0xFFFF] & 0xC0) == 0xC0) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the ship (on the ground)
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((board.ram[at & 0xFFFF] & (0xC0 | kShootable)) == (0x80 | kShootable)) {
                box(at, HitBox::Ground);
            }
        }
        // Each ship's shots, options and laser
        struct Player { uint32_t ship, shots, options, laser; };
        for (const Player &p : {Player{kShip1, kShots1, kOptions1, kLaser1}, Player{kShip2, kShots2, kOptions2, kLaser2}}) {
            if (!(Word(board, p.ship) & 0x8000)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                if ((Word(board, at) & 0xA000) == 0x8000 && !(Word(board, at + 0x18) & 0x8000)) {
                    box(at, HitBox::Shot);
                }
            }
            if (board.ram[(p.ship + 1) & 0xFFFF] & 0x80) {
                continue;
            }
            for (uint32_t option : {p.options, p.options + 0x60}) {
                if (Word(board, option) & 0x8000) {
                    box(option, HitBox::Shot);
                }
            }
            if ((Word(board, p.laser) & 0x8200) == 0x8200) {
                const int x = Word(board, p.laser + 2), y = Word(board, p.laser + 4);
                out.push_back({x / 64.0f, (y - Word(board, p.laser + 0xA)) / 64.0f, Word(board, p.laser + 0x10) / 64.0f,
                               (y + Word(board, p.laser + 8) + 1) / 64.0f, HitBox::Shot});
            }
        }
        for (int n = 0; n < kBulletCount; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & 0x81) == 0x81 && !(flags & 0x10)) {
                const float x = Word(board, at + 6) / 64.0f, y = Word(board, at + 8) / 64.0f;
                out.push_back({x, y, x + 1 / 64.0f, y + 1 / 64.0f, HitBox::Bullet});
            }
        }
        for (uint32_t ship : {kShip1, kShip2}) {
            if (Word(board, ship) & 0x8000) {
                box(ship, HitBox::Ship);
            }
        }
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.newGame = shmup::HookAt(donpachij::kNewGame);
        f.beginStage = shmup::HookAt(donpachij::kBeginStage);
        f.nextStage = [](Board &board) {
            // (as the stage's boss beaten does: 020F66)
            board.ram[donpachij::kShip1 & 0xFFFF] |= donpachij::kShipLeaves;
            board.ram[donpachij::kShip2 & 0xFFFF] |= donpachij::kShipLeaves;
            board.ram[donpachij::kFlags & 0xFFFF] |= donpachij::kStageBeaten;
            board.ram[donpachij::kStageEndSound & 0xFFFF] = 0;
            board.ram[(donpachij::kStageEndSound + 1) & 0xFFFF] = 1;
        };
        f.readyWord = donpachij::kShipWaits;
        f.stageWord = donpachij::kStage;
        f.loopWord = donpachij::kLoop;
        f.stagesPerLoop = 5;
        f.modeWord = donpachij::kMode;
        f.playModes = {donpachij::kModePlay, donpachij::kModeContinue};
        f.ships = donpachij::kShips;
        f.shipsKept = 2;
        f.bombs = donpachij::kBombs;
        f.bombsMax = donpachij::kBombStock;
        f.bombsTally = donpachij::kFlags; // (bits 0 and 1 through a stage's end: its tally counts the bombs left down)
        f.bombsTallyBits = 0x03;
        f.safeFrames = donpachij::kSafeFrames;
        f.safeFramesHeld = 0xB4;
        f.safeFramesBytes = 1;
        f.bossWord = donpachij::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(donpachij::kCredits, f);
        // (the gadgets, port/gadgets.h: the rank over time; and the score and stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(board.ram[donpachij::kRank & 0xFFFF]) + " / 63" : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = std::min<int>(board.ram[donpachij::kRank & 0xFFFF], donpachij::kRankMost);
                 most = donpachij::kRankMost;
                 return Playing(board);
             },
             0xFF4070FFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) {
            return Playing(board) ? (Word(board, donpachij::kLoop) ? 5 : 0) + std::min(Word(board, donpachij::kStage), 4) : -1;
        };
        g.stageNames = {"1-1", "1-2", "1-3", "1-4", "1-5", "2-1", "2-2", "2-3", "2-4", "2-5"};
        return g;
    }();
    return game;
}

} // namespace spelunk
