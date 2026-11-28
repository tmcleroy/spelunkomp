// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama Futari: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include <algorithm>
#include <iterator>
#include <string>

#include "../../../runtime/shmup.h"
#include "futari15.h"

namespace spelunk {

namespace NS = futari15;

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<int16_t>(p[0] << 8 | p[1]);
    }
    // Whether the game tests a group or a member: its word (or 32 bits) at +8 not 0
    bool Tested(const Board &board, uint32_t at) {
        return NS::kTestedLong ? Long(board, at + 8) != 0 : Word(board, at + 8) != 0;
    }
    // Each member of a kind's groups that the game tests
    template <typename Fn>
    void Members(const Board &board, int kind, Fn &&fn, bool firstOnly = false) {
        int guard = 0;
        for (uint32_t group = Long(board, NS::kKinds + 12 * kind + 4); group && guard < 4096; group = Long(board, group), guard++) {
            if (!Tested(board, group)) {
                continue;
            }
            for (uint32_t member = Long(board, group + 28); member && guard < 4096; member = Long(board, member), guard++) {
                if (Tested(board, member)) {
                    fn(member);
                }
                if (firstOnly) {
                    break;
                }
            }
        }
    }
    // A member's position, across and up: its own and its owner's added (the whole parts of each, or the whole part of
    // the two 16.16 values added, as the game does)
    void At(const Board &board, uint32_t member, int &x, int &y) {
        const uint32_t owner = Long(board, member + NS::kPosition + 8);
        if (NS::kAddFixed) {
            x = static_cast<int16_t>((Long(board, member + NS::kPosition) + (owner ? Long(board, owner) : 0)) >> 16);
            y = static_cast<int16_t>((Long(board, member + NS::kPosition + 4) + (owner ? Long(board, owner + 4) : 0)) >> 16);
            return;
        }
        x = Word(board, member + NS::kPosition);
        y = Word(board, member + NS::kPosition + 4);
        if (owner) {
            x += Word(board, owner);
            y += Word(board, owner + 4);
        }
    }
    // A member's box as the board's pixels (the board's across is the game's up, its down the game's across)
    void Box(const Board &board, uint32_t member, HitBox::Kind kind, std::vector<HitBox> &out) {
        int x, y;
        At(board, member, x, y);
        const uint32_t box = member + NS::kBox;
        const int right = static_cast<int32_t>(Long(board, box)), left = static_cast<int32_t>(Long(board, box + 4));
        const int up = static_cast<int32_t>(Long(board, box + 8)), down = static_cast<int32_t>(Long(board, box + 12));
        out.push_back({static_cast<float>(y - down), static_cast<float>(x - left), static_cast<float>(y + up + 1),
                       static_cast<float>(x + right + 1), kind});
    }
    // While the ship is out of harm (its safe time) the game gives it a box of kSafeReach each way, one that cannot be hit:
    // drawn instead is the box it is hit by when that ends, from the game's table of each ship's reaches
    void SafeShip(const Board &board, uint32_t member, HitBox &box) {
        const uint32_t owner = Long(board, member + NS::kPosition + 8);
        bool safe = owner != 0;
        for (int i = 0; i < 4; i++) {
            safe = safe && Long(board, member + NS::kBox + 4 * i) == NS::kSafeReach;
        }
        if (!safe) {
            return;
        }
        // (the ship's block is 4 before the position its member is placed by)
        const uint32_t ship = owner - 4, type = board.ram[(ship + NS::kShipType) & (SH3::kRamSize - 1)];
        const uint32_t sizes = Long(board, NS::kShipSizes + 4 * type) +
                               (NS::kSizesMode && Long(board, NS::kSizesMode) == 1 ? NS::kSizesModeAt : NS::kSizesAt);
        auto reach = [&](int i) { return static_cast<float>(board.ram[(sizes + i) & (SH3::kRamSize - 1)]); };
        int x, y;
        At(board, member, x, y);
        box = {y - reach(3), x - reach(1), y + reach(2) + 1, x + reach(0) + 1, HitBox::Ship};
    }
    // As the board is after a frame: the lists last until they are next changed
    void BoxesNow(const Board &board, std::vector<HitBox> &out) {
        bool everyBody = false;
        for (int player = 0; player < 2; player++) {
            Members(board, NS::kShipKindAll + player, [&](uint32_t) { everyBody = true; });
        }
        // The enemies: those whose bodies hit the ship, and the rest, which only the shots hit (on the ground), unless
        // shots fly through them too. An enemy may have both (a small box that hits and a larger one the shots hit,
        // members with the same owner at +60): it is drawn by its body alone
        std::vector<uint32_t> bodies;
        for (int kind : NS::kBodyKinds) {
            Members(board, kind, [&](uint32_t m) {
                if (Long(board, m + 12) && (everyBody || (board.ram[(m + 27) & (SH3::kRamSize - 1)] & 1))) {
                    Box(board, m, HitBox::Body, out);
                    bodies.push_back(Long(board, m + NS::kPosition + 8));
                }
            });
        }
        for (int kind : NS::kBodyKinds) {
            const bool canPass = std::find(std::begin(NS::kShotsPassKinds), std::end(NS::kShotsPassKinds), kind) != std::end(NS::kShotsPassKinds);
            Members(board, kind, [&](uint32_t m) {
                const uint8_t flags = board.ram[(m + 27) & (SH3::kRamSize - 1)];
                const uint32_t owner = Long(board, m + NS::kPosition + 8);
                if (Long(board, m + 12) && !everyBody && !(flags & 1) && !(canPass && (flags & 2)) &&
                    (!owner || std::find(bodies.begin(), bodies.end(), owner) == bodies.end())) {
                    Box(board, m, HitBox::Ground, out);
                }
            });
        }
        // The player's shots (both players' kinds)
        for (int kind = NS::kShotKindFirst; kind <= NS::kShotKindLast; kind++) {
            const bool main = std::find(std::begin(NS::kMainShotKinds), std::end(NS::kMainShotKinds), kind) != std::end(NS::kMainShotKinds);
            Members(board, kind, [&](uint32_t m) {
                if (!main || Long(board, m + 12)) {
                    Box(board, m, HitBox::Shot, out);
                }
            });
        }
        Members(board, NS::kBulletKind, [&](uint32_t m) {
            if (const uint32_t owner = Long(board, m + NS::kPosition + 8); owner && Long(board, m + 12)) {
                const float x = static_cast<float>(Word(board, owner)), y = static_cast<float>(Word(board, owner + 4));
                out.push_back({y, x, y + 1, x + 1, HitBox::Bullet});
            }
        }, NS::kBulletFirstOnly);
        for (int player = 0; player < 2; player++) {
            Members(board, NS::kShipKind + player, [&](uint32_t m) {
                Box(board, m, HitBox::Ship, out);
                if (NS::kShipSizes) {
                    SafeShip(board, m, out.back());
                }
            });
            Members(board, NS::kShipKindAll + player, [&](uint32_t m) { Box(board, m, HitBox::Ship, out); });
        }
    }
    // While a player's game is played (the rank's record says so: NS::kGameOn)
    bool Playing(const Board &board) {
        return Long(board, NS::kGameOn) == 1;
    }
    // Player 1's score (32 bits, or Black Label's ten digits, the lowest first)
    long long Score(const Board &board) {
        if (!NS::kScoreDigits) {
            return static_cast<long long>(Long(board, NS::kScore));
        }
        long long n = 0;
        for (int i = 9; i >= 0; i--) {
            n = n * 10 + board.ram[(NS::kScore + static_cast<uint32_t>(i)) & (SH3::kRamSize - 1)] % 10;
        }
        return n;
    }
    // The most the rank goes to now (the mode's, as the rank's routine cuts it)
    int RankMost(const Board &board) {
        const uint32_t mode = Long(board, NS::kMode);
        if (mode >= std::size(NS::kRankMost)) {
            return 255;
        }
        return static_cast<int>(mode) == NS::kUltra && Long(board, NS::kStage) >= static_cast<uint32_t>(NS::kUltraLateStage) ? NS::kUltraLateMost
                                                                                                                      : NS::kRankMost[mode];
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.stageArguments = {{futari15::kNewGame, 4, futari15::kNewGameCaller}, {futari15::kBeginStage, 5}};
        f.ships = futari15::kShips;
        f.bombs = futari15::kBombs;
        f.safeFrames = futari15::kSafeFrames;
        f.bossTunes.assign(std::begin(futari15::kBossTunes), std::end(futari15::kBossTunes));
        f.shipState = futari15::kShipState;
        f.safeFramesHeld = 0xFE;
        f.hitBoxes = BoxesNow;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(futari15::kCredits, f);
        // (the gadgets, port/gadgets.h: the rank over time, with what it is made of; and the score and stage, for the
        // score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Long(board, NS::kRank)) + " / " + std::to_string(RankMost(board)) : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 most = RankMost(board);
                 value = static_cast<int>(std::min<uint32_t>(Long(board, NS::kRank), static_cast<uint32_t>(most)));
                 return Playing(board);
             },
             0xFF4070FFu},
            // (what the rank is made of: the mode's base, the ship's part, the time played)
            {"RANK FROM", Kind::Words, [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const uint32_t ship = std::max(Long(board, NS::kRankPart1), Long(board, NS::kRankPart2));
                 return "BASE " + std::to_string(Long(board, NS::kRankBase)) + "  SHIP " + std::to_string(ship) + "\nTIME " +
                        std::to_string(Long(board, NS::kRankFrames) >> 9);
             }},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) {
            return Playing(board) ? static_cast<int>(std::min<uint32_t>(Long(board, NS::kStage), 4)) : -1;
        };
        g.stageNames = f.stages;
        return g;
    }();
    return game;
}

} // namespace spelunk
