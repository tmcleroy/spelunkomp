// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ddonpach.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & 0xFFFF];
        return (p[0] << 8) | p[1];
    }
    int Byte(const Board &board, uint32_t address) {
        return board.ram[address & 0xFFFF];
    }
    // A word kept in BCD (the game's counters as it shows them) as a number
    int Bcd(int bcd) {
        int n = 0, scale = 1;
        for (; bcd; bcd >>= 4, scale *= 10) {
            n += (bcd & 15) * scale;
        }
        return n;
    }
    // While a player's game is played (the mode word's 7: the attract show's demonstration is 5, a continue offered 8)
    bool Playing(const Board &board) {
        return Word(board, ddonpach::kMode) == ddonpach::kModePlay;
    }
    // Player 1's score in points (BCD tens, the last digit shown always 0)
    long long Score(const Board &board) {
        const long long bcd = (static_cast<long long>(Word(board, ddonpach::kScore)) << 16) | Word(board, ddonpach::kScore + 2);
        long long n = 0, scale = 1;
        for (long long b = bcd; b; b >>= 4, scale *= 10) {
            n += (b & 15) * scale;
        }
        return n * 10;
    }
    // The boxes the game's hit test (0057EE) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ddonpach;
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + 6), y = Word(board, record + 8);
            // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
            out.push_back({(x - Word(board, record + kBoxLeft)) / 64.0f, (y - Word(board, record + kBoxUp)) / 64.0f,
                           (x + Word(board, record + kBoxRight) + 1) / 64.0f, (y + Word(board, record + kBoxDown) + 1) / 64.0f, kind});
        };
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & 0xC0) == 0xC0 && ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots can hurt whose bodies do not hit the ship (on the ground)
        for (int n = 0; n < kEnemyCount; n++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(n) * kEnemySize;
            const uint8_t flags = board.ram[at & 0xFFFF];
            if ((flags & (0x80 | kShootable)) == (0x80 | kShootable) && !(flags & 0x40) &&
                ((Word(board, at + 6) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0x7800) {
                box(at, HitBox::Ground);
            }
        }
        // Each ship's shots, and its laser while it is held
        struct Player { uint32_t ship, shots, laser, laserParts; };
        for (const Player &p : {Player{kShip1, kShots1, kLaser1, kLaserParts1}, Player{kShip2, kShots2, kLaser2, kLaserParts2}}) {
            if (!(Word(board, p.ship) & 0x8000)) {
                continue;
            }
            for (int n = 0; n < kShotCount; n++) {
                const uint32_t at = p.shots + static_cast<uint32_t>(n) * kShotSize;
                if ((Word(board, at) & 0xA000) == 0x8000 && !(Word(board, at + 0x18) & 0x8000)) {
                    box(at, HitBox::Shot);
                }
            }
            if ((board.ram[(p.ship + 1) & 0xFFFF] & 0x80) || !board.ram[(p.ship + 0x3F) & 0xFFFF]) {
                continue;
            }
            for (uint32_t part : {p.laserParts, p.laserParts + 0x60}) {
                if (Word(board, part) & 0x8000) {
                    box(part, HitBox::Shot);
                }
            }
            if ((Word(board, p.laser) & 0x8200) == 0x8200) {
                const int x = Word(board, p.laser + 2), y = Word(board, p.laser + 4);
                out.push_back({x / 64.0f, (y - Word(board, p.laser + 0xA)) / 64.0f, Word(board, p.laser + 0x10) / 64.0f,
                               (y + Word(board, p.laser + 8) + 1) / 64.0f, HitBox::Shot});
            }
        }
        const int bullets = Word(board, kLoop) ? kBulletCountLoop2 : kBulletCount;
        for (int n = 0; n < bullets; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const uint8_t flags = board.ram[at & 0xFFFF];
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

// (dodge.cpp)
void AddDodgeTraining(Game &game);

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.newGame = shmup::HookAt(ddonpach::kNewGame);
        f.beginStage = shmup::HookAt(ddonpach::kBeginStage);
        f.nextStage = [](Board &board) {
            uint8_t *task = &board.ram[ddonpach::kStageEndTask & 0xFFFF];
            std::memset(task, 0, 0x40);
            task[0] = 0x80;
            task[1] = 0x3A;
            board.ram[ddonpach::kBossState & 0xFFFF] |= ddonpach::kBossBeaten;
        };
        f.readyWord = ddonpach::kShipWaits;
        f.stageWord = ddonpach::kStage;
        f.loopWord = ddonpach::kLoop;
        f.stagesPerLoop = 6;
        f.modeWord = ddonpach::kMode;
        f.playModes = {ddonpach::kModePlay, ddonpach::kModeContinue};
        f.ships = ddonpach::kShips;
        f.shipsKept = 2;
        f.bombs = ddonpach::kBombs;
        f.bombsMax = ddonpach::kBombStock;
        f.safeFrames = ddonpach::kSafeFrames;
        f.safeFramesHeld = 0xEF;
        f.safeFramesBytes = 1;
        f.bossWord = ddonpach::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(ddonpach::kCredits, f);
        // (the gadgets, port/gadgets.h: the rank over time, the chain's timer, the bees, the second loop's test;
        // and the score and stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) { return Playing(board) ? "RANK " + std::to_string(Byte(board, ddonpach::kRank)) + " / 63" : std::string(); },
             [](const Board &board, int &value, int &most) {
                 value = Byte(board, ddonpach::kRank);
                 most = ddonpach::kRankMost;
                 return Playing(board);
             },
             0xFF4070FFu},
            {"CHAIN", Kind::Meter,
             [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const bool on = Word(board, ddonpach::kChainTimer) > 0;
                 return std::to_string(on ? Bcd(Word(board, ddonpach::kChainHits)) : 0) + " HITS  BEST " +
                        std::to_string(Bcd(Word(board, ddonpach::kChainBest)));
             },
             [](const Board &board, int &value, int &most) {
                 value = std::min(Word(board, ddonpach::kChainTimer), ddonpach::kChainTimerFull);
                 most = ddonpach::kChainTimerFull;
                 return Playing(board);
             },
             0xFF40FFFFu},
            {"BEES", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, ddonpach::kBees)) + " / " + std::to_string(ddonpach::kBeeSetSize) +
                                             "  SETS " + std::to_string(Word(board, ddonpach::kBeeSets)) + " / " +
                                             std::to_string(ddonpach::kLoopBeeSets)
                                       : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = std::min(Word(board, ddonpach::kBees), ddonpach::kBeeSetSize);
                 most = ddonpach::kBeeSetSize;
                 return Playing(board);
             },
             0xFF30D0F0u},
            // (the second loop's terms as the game tests them at the first loop's end, while the first is played)
            {"TO THE 2ND LOOP", Kind::Words, [](const Board &board) {
                 using namespace ddonpach;
                 if (!Playing(board) || Word(board, kLoop)) {
                     return std::string();
                 }
                 const int lost = Word(board, kShipsLost), kind = Word(board, kShipKind), best = Bcd(Word(board, kChainBest));
                 const int bees = Word(board, kBeeSets), need = kind < 3 ? kLoopChains[kind] : kLoopChains[0];
                 const long long score = Score(board);
                 const bool continued = Word(board, kContinues) != kNoContinues;
                 const bool misses = lost <= kLoopMissesMost, chain = best >= need, points = score >= kLoopScore, set = bees >= kLoopBeeSets;
                 auto mark = [](bool ok) { return ok ? "  OK" : ""; };
                 return std::string("ONE OF:\n") + " MISSES " + std::to_string(lost) + " / " + std::to_string(kLoopMissesMost) + mark(misses) +
                        "\n CHAIN  " + std::to_string(best) + " / " + std::to_string(need) + mark(chain) + "\n SCORE  " +
                        std::to_string(score / 1000000) + "M / " + std::to_string(kLoopScore / 1000000) + "M" + mark(points) +
                        "\n BEES   " + std::to_string(bees) + " / " + std::to_string(kLoopBeeSets) + mark(set) + "\n" +
                        (continued ? "CONTINUED: OUT" : misses || chain || points || set ? "ON THE WAY" : "NOT YET");
             }},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        // (the first loop's six stages, then the second's seven: the game's own count, 001402 and 001412)
        g.stage = [](const Board &board) { return Playing(board) ? (Word(board, ddonpach::kLoop) ? 6 : 0) + Word(board, ddonpach::kStage) : -1; };
        g.stageNames = {"1-1", "1-2", "1-3", "1-4", "1-5", "1-6", "2-1", "2-2", "2-3", "2-4", "2-5", "2-6", "2-7"};
        AddDodgeTraining(g);
        return g;
    }();
    return game;
}

} // namespace spelunk
