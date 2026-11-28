// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Espgaluda II: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include <algorithm>
#include <string>

#include "../../../runtime/shmup.h"
#include "espgal2.h"

namespace spelunk {

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<int16_t>(p[0] << 8 | p[1]);
    }
    // Each member of a kind's groups that the game tests (the group's word at +8, the member's at +8 and +10)
    // While a player's game is played (espgal2.h's kGameOn)
    bool Playing(const Board &board) {
        return Long(board, espgal2::kGameOn) == 1 && board.ram[espgal2::kShipRecord & (SH3::kRamSize - 1)] != 0;
    }
    bool Overmode(const Board &board) {
        using namespace espgal2;
        return Playing(board) && board.ram[kKakusei & (SH3::kRamSize - 1)] && Long(board, kGems) == 0;
    }
    template <typename Fn>
    void Members(const Board &board, uint32_t kind, bool needTen, Fn &&fn) {
        int guard = 0;
        for (uint32_t group = Long(board, kind + 4); group && guard < 4096; group = Long(board, group)) {
            if (!Word(board, group + 8)) {
                continue;
            }
            for (uint32_t member = Long(board, group + 20); member && guard < 4096; member = Long(board, member), guard++) {
                if (Word(board, member + 8) && (!needTen || Word(board, member + 10))) {
                    fn(member);
                }
            }
        }
    }
    // A member's box as the board's pixels (the board's across is the game's up, its down the game's across)
    void Box(const Board &board, uint32_t member, HitBox::Kind kind, std::vector<HitBox> &out) {
        int x = Word(board, member + 28), y = Word(board, member + 32);
        if (const uint32_t owner = Long(board, member + 36)) {
            x += Word(board, owner);
            y += Word(board, owner + 4);
        }
        out.push_back({static_cast<float>(y - Word(board, member + 24)), static_cast<float>(x - Word(board, member + 20)),
                       static_cast<float>(y + Word(board, member + 22) + 1), static_cast<float>(x + Word(board, member + 18) + 1), kind});
    }
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace espgal2;
        // (the owners of the boxes that hit the ship: their enemies' hurt boxes are not drawn again)
        std::vector<uint32_t> bodies;
        Members(board, kBodyKind, true, [&](uint32_t m) {
            Box(board, m, HitBox::Body, out);
            bodies.push_back(Long(board, m + 36));
        });
        for (int kind : kHurtKinds) {
            Members(board, kEnemyKinds + 12 * kind, true, [&](uint32_t m) {
                const uint32_t owner = Long(board, m + 36);
                if (!owner || std::find(bodies.begin(), bodies.end(), owner) == bodies.end()) {
                    Box(board, m, HitBox::Ground, out);
                }
            });
        }
        for (int kind : kShotKinds) {
            Members(board, kEnemyKinds + 12 * kind, false, [&](uint32_t m) { Box(board, m, HitBox::Shot, out); });
        }
        for (int kind : kShotKindsTen) {
            Members(board, kEnemyKinds + 12 * kind, true, [&](uint32_t m) { Box(board, m, HitBox::Shot, out); });
        }
        Members(board, kBulletKind, true, [&](uint32_t m) {
            if (const uint32_t owner = Long(board, m + 36)) {
                const float x = static_cast<float>(Word(board, owner)), y = static_cast<float>(Word(board, owner + 4));
                out.push_back({y, x, y + 1, x + 1, HitBox::Bullet});
            }
        });
        Members(board, kShipKind, false, [&](uint32_t m) { Box(board, m, HitBox::Ship, out); });
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.stageArguments = {{espgal2::kNewGame, 4, espgal2::kNewGameCaller}, {espgal2::kBeginStage, 5}};
        f.ships = espgal2::kShips;
        f.gauges = {{espgal2::kBarrier, espgal2::kBarrierFull}, {espgal2::kBarrier + 4, espgal2::kBarrierFull}};
        f.bombsTitle = "Barrier never runs out";
        f.bombsWords = "The guard barrier's gauge is kept full.";
        f.fullGauges.push_back({"gems_full", "Gems kept full",
                                "The green gems are kept at 500, the most there can be: Kakusei whenever you want it, for as "
                                "long as you want it.",
                                espgal2::kGems, espgal2::kGemsFull, 4});
        f.safeFrames = espgal2::kSafeFrames;
        f.bossTunes.assign(std::begin(espgal2::kBossTunes), std::end(espgal2::kBossTunes));
        f.shipState = espgal2::kShipState;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        f.safeFramesHeld = 0xDA;
        Game g = shmup::MakeGame(espgal2::kCredits, f);
        // (the gadgets, port/gadgets.h: Overmode's level and its way to the next, the gold; and the score and stage, for
        // the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"OVER LEVEL", Kind::Meter,
             [](const Board &board) {
                 using namespace espgal2;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 return std::to_string(std::clamp(Word(board, kOverLevel), 0, kOverLevels)) + " / " + std::to_string(kOverLevels) +
                        (Overmode(board) ? "  OVERMODE" : "");
             },
             [](const Board &board, int &value, int &most) {
                 using namespace espgal2;
                 most = kOverWayNext;
                 // (the way to the next while in Overmode, the task being player 1's; full at the last level)
                 const bool mine = Long(board, kShipTask + kTaskRecord) == kShipRecord;
                 value = Word(board, kOverLevel) >= kOverLevels ? most : Overmode(board) && mine ? Word(board, kShipTask + kOverWay) : 0;
                 return Playing(board);
             },
             0xFFFF6040u},
            {"GOLD", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, espgal2::kGold)) + " / " + std::to_string(espgal2::kGoldMost) : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, espgal2::kGold);
                 most = espgal2::kGoldMost;
                 return Playing(board);
             },
             0xFF40D0FFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? static_cast<long long>(Long(board, espgal2::kScore)) : -1; };
        g.stage = [](const Board &board) { return Playing(board) ? static_cast<int>(std::min<uint32_t>(Long(board, espgal2::kStage), 5)) : -1; };
        g.stageNames = f.stages;
        return g;
    }();
    return game;
}

} // namespace spelunk
