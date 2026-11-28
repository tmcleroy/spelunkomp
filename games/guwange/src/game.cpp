// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Guwange: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "guwange.h"

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
        return Word(board, guwange::kMode) == guwange::kModePlay;
    }
    // Player 1's score in points (BCD)
    long long Score(const Board &board) {
        const uint32_t bcd = static_cast<uint32_t>(Word(board, guwange::kScore1)) << 16 | static_cast<uint32_t>(Word(board, guwange::kScore1 + 2));
        long long n = 0, scale = 1;
        for (uint32_t b = bcd; b; b >>= 4, scale *= 10) {
            n += (b & 15) * scale;
        }
        return n;
    }
    // The boxes the game's hit test (007D4C) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace guwange;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + kX), y = Word(board, record + kY);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        const int enemies = kEnemyCount + Word(board, kMoreEnemies);
        for (int n = 0; n < enemies; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((Byte(board, at) & 0xC0) == 0xC0 && ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the character (on the ground)
        for (int n = 0; n < enemies; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            if ((Byte(board, at) & (0xC0 | kShootable)) == (0x80 | kShootable) &&
                ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Ground);
            }
        }
        // Each character's shots and its shikigami's
        struct Player { uint32_t ship, shots, shikigami; };
        for (const Player &p : {Player{kShip1, kShots1, kShikigami1}, Player{kShip2, kShots2, kShikigami2}}) {
            if (!(Byte(board, p.ship) & 0x80)) {
                continue;
            }
            for (int n = 0; Word(board, kShotsOn) && n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                const int flags = Word(board, at);
                if ((flags & 0x8000) && ((flags & 1) || (!(flags & 0x880) && !(Word(board, at + 0x18) & 0x8000)))) {
                    box(at, HitBox::Shot);
                }
            }
            for (int n = 0; Word(board, kShikigamiOn) && n < kShikigamiCount; n++) {
                const uint32_t at = p.shikigami + static_cast<uint32_t>(n) * kShikigamiSize;
                if ((Word(board, at) & 0x8080) == 0x8080) {
                    box(at, HitBox::Shot);
                }
            }
        }
        // (the test's rounds of 20: 4, or 6, 9, 16 as the three words after kMoreBullets are not 0 in turn)
        int rounds = 4;
        if (Word(board, kMoreBullets)) {
            rounds = 6;
            if (Word(board, kMoreBullets + 2)) {
                rounds = Word(board, kMoreBullets + 4) ? 16 : 9;
            }
        }
        for (int n = 0; n < rounds * 20; n++) {
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
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7"};
        f.newGame = shmup::HookAt(guwange::kNewGame);
        f.beginStage = shmup::HookAt(guwange::kBeginStage);
        f.nextStage = [](Board &board) {
            using namespace guwange;
            // (as a stage's boss beaten does, 070E78: each character played, or coming back, flies on)
            for (uint32_t ship : {kShip1, kShip2}) {
                uint8_t &first = board.ram[ship & 0xFFFF];
                if ((first & 0x80) || ((first & 0x01) && !(board.ram[(ship + 1) & 0xFFFF] & 0x80))) {
                    first |= kShipLeaves;
                }
            }
            board.ram[kFlags & 0xFFFF] |= kStageBeaten;
        };
        f.readyWord = guwange::kComingIn;
        f.stageWord = guwange::kStage;
        f.stagesPerLoop = 7;
        f.modeWord = guwange::kMode;
        f.playModes = {guwange::kModePlay, guwange::kModeContinue};
        f.ships = guwange::kLives;
        f.shipsAlso = guwange::kLife;
        f.shipsBytes = 2;
        f.shipsKept = 2 * guwange::kShipSize;
        f.bossWord = guwange::kBossOn;
        f.bombs = guwange::kBombs;
        f.bombsHeld = 2;
        f.bombsTally = guwange::kFlags;
        f.bombsTallyBits = 0x03;
        f.safeFrames = guwange::kSafeFrames;
        f.safeFramesHeld = 0xF0;
        f.safeFramesBytes = 1;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(guwange::kCredits, f);
        // (the gadgets, port/gadgets.h: the rank over time; and the score and stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(board.ram[guwange::kRank & 0xFFFF]) + " / 63" : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = std::min<int>(board.ram[guwange::kRank & 0xFFFF], guwange::kRankMost);
                 most = guwange::kRankMost;
                 return Playing(board);
             },
             0xFF4070FFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) { return Playing(board) ? std::min(Word(board, guwange::kStage), 6) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
