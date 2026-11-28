// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// ESP Ra.De.: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "esprade.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    uint8_t Byte(const Board &board, uint32_t address) {
        return board.ram[address & 0xFFFF];
    }
    // While a player's game is played (the mode word's 7: the attract show's demonstration is 5, a continue offered 8)
    bool Playing(const Board &board) {
        return Word(board, esprade::kMode) == esprade::kModePlay;
    }
    // Player 1's score in points (BCD tens)
    long long Score(const Board &board) {
        const long long bcd = (static_cast<long long>(Word(board, esprade::kScore1)) << 16) | Word(board, esprade::kScore1 + 2);
        long long n = 0, scale = 1;
        for (long long b = bcd; b; b >>= 4, scale *= 10) {
            n += (b & 15) * scale;
        }
        return n * 10;
    }
    // The boxes the game's hit test (005E72) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace esprade;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + kX), y = Word(board, record + kY);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((Byte(board, at) & 0xC0) == 0xC0 && ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the character (on the ground)
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((Byte(board, at) & (0xC0 | kShootable)) == (0x80 | kShootable) &&
                ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Ground);
            }
        }
        // Each character's shots and laser
        struct Player { uint32_t ship, shots, laser; };
        for (const Player &p : {Player{kShip1, kShots1, kLaser1}, Player{kShip2, kShots2, kLaser2}}) {
            if (!(Byte(board, p.ship) & 0x80)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                const uint8_t second = Byte(board, at + 1);
                if ((Byte(board, at) & 0x80) && ((second & 1) || (!(second & 0x80) && !(Word(board, at + 0x18) & 0x8000)))) {
                    box(at, HitBox::Shot);
                }
            }
            for (int n = 0; n < kLaserCount; n++) {
                const uint32_t at = p.laser + static_cast<uint32_t>(n) * kLaserSize;
                const int x = Word(board, at), y = Word(board, at + 2);
                if (!x) {
                    break;
                }
                const int x1 = (x + Word(board, at + 4)) & 0xFFFF, y1 = (y + Word(board, at + 8)) & 0xFFFF;
                out.push_back({static_cast<int16_t>(x1 - Word(board, at + 6)) / 64.0f, static_cast<int16_t>(y1 - Word(board, at + 0xA)) / 64.0f,
                               (static_cast<int16_t>(x1) + 1) / 64.0f, (static_cast<int16_t>(y1) + 1) / 64.0f, HitBox::Shot});
            }
        }
        const int bullets = Word(board, kManyBullets) ? kBulletCountMany : kBulletCount;
        for (int n = 0; n < bullets; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const int bx = Word(board, at + kX), by = Word(board, at + kY);
            if (!(Byte(board, at) & 0x59) && (bx || by)) {
                const float x = bx / 64.0f, y = by / 64.0f;
                out.push_back({x, y, x + 1 / 64.0f, y + 1 / 64.0f, HitBox::Bullet});
            }
        }
        for (uint32_t ship : {kShip1, kShip2}) {
            if ((Byte(board, ship) & 0xC0) == 0x80) {
                box(ship, HitBox::Ship);
            }
        }
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.newGame = shmup::HookAt(esprade::kNewGame);
        f.beginStage = shmup::HookAt(esprade::kBeginStage);
        f.nextStage = [](Board &board) {
            using namespace esprade;
            // The order's pointer moved on past the stages counted as played (still at the order's start: the game is
            // in its first stage), so the stage's end takes the chosen stage's map from it
            uint8_t *order = &board.ram[kOrder & 0xFFFF];
            const uint32_t at = ((order[0] << 24) | (order[1] << 16) | (order[2] << 8) | order[3]) + 2 * Word(board, kStage);
            order[0] = static_cast<uint8_t>(at >> 24);
            order[1] = static_cast<uint8_t>(at >> 16);
            order[2] = static_cast<uint8_t>(at >> 8);
            order[3] = static_cast<uint8_t>(at);
            // (then as the stage's boss beaten does: 003C36)
            board.ram[kShip1 & 0xFFFF] |= kShipLeaves;
            board.ram[kShip2 & 0xFFFF] |= kShipLeaves;
            board.ram[kBossFlags & 0xFFFF] |= kBossGone;
            board.ram[kFlags & 0xFFFF] |= kStageBeaten;
            board.ram[kStageEndSound & 0xFFFF] = 0;
            board.ram[(kStageEndSound + 1) & 0xFFFF] = 1;
        };
        f.readyWord = esprade::kShipWaits;
        f.stageWord = esprade::kStage;
        f.stagesPerLoop = 6;
        f.modeWord = esprade::kMode;
        f.playModes = {esprade::kModePlay, esprade::kModeContinue};
        f.ships = esprade::kShips;
        f.shipsKept = 2;
        f.gauges = {{esprade::kBarrier, esprade::kBarrierFull, 2}};
        f.bombsTitle = "Barrier never runs out";
        f.bombsWords = "The guard barrier's gauge is kept full.";
        f.safeFrames = esprade::kSafeFrames;
        f.safeFramesHeld = 0xF0;
        f.safeFramesBytes = 1;
        f.bossWord = esprade::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(esprade::kCredits, f);
        // (the gadgets, port/gadgets.h: the score item counter and how far it goes, the power shot's multiplier, the
        // barrier's gauge; and the score and stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"SCORE ITEMS", Kind::Meter,
             [](const Board &board) {
                 using namespace esprade;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 return std::to_string(Word(board, kItems)) + " / " + std::to_string(Word(board, kItemsMost)) +
                        (Word(board, kItemsFull) ? "  FULL" : "");
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, esprade::kItems);
                 most = Word(board, esprade::kItemsMost);
                 return Playing(board);
             },
             0xFF40D0FFu},
            {"MULTIPLIER", Kind::Words, [](const Board &board) {
                 using namespace esprade;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const int time = Word(board, kMultiplierTime);
                 return time ? "x" + std::to_string(Word(board, kMultiplier) + 1) + "  FOR " + std::to_string(time) : std::string("x1");
             }},
            {"BARRIER", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, esprade::kBarrier) * 100 / esprade::kBarrierFull) + "%" : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, esprade::kBarrier);
                 most = esprade::kBarrierFull;
                 return Playing(board);
             },
             0xFF40FFFFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        // (the stages played so far: which map each is depends on the first stage chosen)
        g.stage = [](const Board &board) { return Playing(board) ? std::min(Word(board, esprade::kStage), 5) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
