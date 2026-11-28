// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Dangun Feveron: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "dfeveron.h"

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
    // While a player's game is played (the mode word's 7: the attract show's demonstration has another, a continue 8)
    bool Playing(const Board &board) {
        return Word(board, dfeveron::kMode) == dfeveron::kModePlay;
    }
    uint32_t Long(const Board &board, uint32_t address) {
        return static_cast<uint32_t>(Word(board, address)) << 16 | static_cast<uint32_t>(Word(board, address + 2));
    }
    // Player 1's score in points (BCD tens)
    long long Score(const Board &board) {
        long long n = 0, scale = 1;
        for (uint32_t b = Long(board, dfeveron::kScore1); b; b >>= 4, scale *= 10) {
            n += (b & 15) * scale;
        }
        return n * 10;
    }
    // The boxes the game's hit tests (009FB6, 009F36) use, read as they read them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace dfeveron;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + 6), y = Word(board, record + 8);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        if (!Byte(board, kBombOn) && Byte(board, kSafeFrames) != 0xFF) {
            for (int n = 0; n < kEnemyCount; n++) {
                const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
                if ((Byte(board, at) & 0xC0) == 0xC0 && ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                    box(at, HitBox::Body);
                }
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the ship (on the ground)
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((Byte(board, at) & (0xC0 | kShootable)) == (0x80 | kShootable) &&
                ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Ground);
            }
        }
        // Each ship's shots and its formation's
        struct Player { uint32_t ship, shots, formation; };
        for (const Player &p : {Player{kShip1, kShots1, kFormation1}, Player{kShip2, kShots2, kFormation2}}) {
            if (!(Word(board, p.ship) & 0x8000)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                if ((Word(board, at) & 0x8000) && !(Word(board, at + 0x18) & 0x8000)) {
                    box(at, HitBox::Shot);
                }
            }
            const int kind = Byte(board, p.ship + kShipKind) / 8;
            for (int n = 0; kind < 3 && n < kFormationCount[kind]; n++) {
                const uint32_t at = p.formation + 0x20 + static_cast<uint32_t>(n) * kFormationSize;
                if ((Byte(board, at) & 0x80) && !(kind == 0 && (Byte(board, at) & 1))) {
                    box(at, HitBox::Shot);
                }
            }
        }
        const int bullets = (Byte(board, kManyBullets) & 0x80) ? kBulletCountMany : kBulletCount;
        for (int n = 0; n < bullets; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const uint8_t flags = Byte(board, at);
            if ((flags & 0x80) && !(flags & 0x51)) {
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
        f.newGame = shmup::HookAt(dfeveron::kNewGame);
        f.beginStage = shmup::HookAt(dfeveron::kBeginStage);
        f.nextStage = [](Board &board) {
            // (as the stage's boss beaten does: 0134AC)
            board.ram[dfeveron::kShip1 & 0xFFFF] |= dfeveron::kShipLeaves;
            board.ram[dfeveron::kShip2 & 0xFFFF] |= dfeveron::kShipLeaves;
            board.ram[dfeveron::kBossFlags & 0xFFFF] |= dfeveron::kBossGone;
            board.ram[dfeveron::kFlags & 0xFFFF] |= dfeveron::kStageBeaten;
        };
        f.readyWord = dfeveron::kShipSelect;
        f.stageWord = dfeveron::kStage;
        f.loopWord = dfeveron::kLoop;
        f.stagesPerLoop = 5;
        f.modeWord = dfeveron::kMode;
        f.playModes = {dfeveron::kModePlay, dfeveron::kModeContinue};
        f.ships = dfeveron::kShips;
        f.shipsKept = 2;
        f.bombs = dfeveron::kBombs;
        f.bombsHeld = 3;
        f.bombsTally = dfeveron::kFlags; // (bits 0 and 1 through a stage's end: its tally counts the bombs left down)
        f.bombsTallyBits = 0x03;
        f.safeFrames = dfeveron::kSafeFrames;
        f.safeFramesHeld = 0xB4;
        f.safeFramesBytes = 1;
        f.bossWord = dfeveron::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(dfeveron::kCredits, f);
        // (the gadgets, port/gadgets.h: the rank over time, the cyborgs on the way to the extend; and the score and
        // stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) { return Playing(board) ? std::to_string(Byte(board, dfeveron::kRank)) + " / 63" : std::string(); },
             [](const Board &board, int &value, int &most) {
                 value = std::min<int>(Byte(board, dfeveron::kRank), dfeveron::kRankMost);
                 most = dfeveron::kRankMost;
                 return Playing(board);
             },
             0xFF4070FFu},
            {"CYBORGS TO 1UP", Kind::Meter,
             [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const uint32_t n = Long(board, dfeveron::kCyborgs);
                 return n >= static_cast<uint32_t>(dfeveron::kCyborgsExtend) ? std::to_string(n) + "  GIVEN"
                                                                            : std::to_string(n) + " / " + std::to_string(dfeveron::kCyborgsExtend);
             },
             [](const Board &board, int &value, int &most) {
                 most = dfeveron::kCyborgsExtend;
                 value = static_cast<int>(std::min<uint32_t>(Long(board, dfeveron::kCyborgs), static_cast<uint32_t>(most)));
                 return Playing(board);
             },
             0xFF40D0FFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        // (the first round's five stages, then the second's)
        g.stage = [](const Board &board) {
            return Playing(board) ? (Word(board, dfeveron::kLoop) ? 5 : 0) + std::min(Word(board, dfeveron::kStage), 4) : -1;
        };
        g.stageNames = {"1-1", "1-2", "1-3", "1-4", "1-5", "2-1", "2-2", "2-3", "2-4", "2-5"};
        return g;
    }();
    return game;
}

} // namespace spelunk
