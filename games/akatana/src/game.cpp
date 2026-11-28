// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Akai Katana: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "akatana.h"

#include <algorithm>
#include <iterator>
#include <string>

namespace spelunk {

namespace NS = akatana;

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
    // A member's box as the board's pixels (a horizontal game: the board's across is the game's, its down the game's up
    // from the picture's bottom; the reaches at +8 and +12 of the box are towards a greater y, up, and a smaller)
    void Box(const Board &board, uint32_t member, HitBox::Kind kind, std::vector<HitBox> &out) {
        int x, y;
        At(board, member, x, y);
        y = NS::kPictureHeight - y;
        const uint32_t box = member + NS::kBox;
        const int right = static_cast<int32_t>(Long(board, box)), left = static_cast<int32_t>(Long(board, box + 4));
        const int up = static_cast<int32_t>(Long(board, box + 8)), down = static_cast<int32_t>(Long(board, box + 12));
        out.push_back({static_cast<float>(x - left), static_cast<float>(y - up), static_cast<float>(x + right + 1),
                       static_cast<float>(y + down + 1), kind});
    }
    // As the board is after a frame: the lists last until they are next changed
    void BoxesNow(const Board &board, std::vector<HitBox> &out) {
        bool everyBody = false;
        for (int player = 0; player < 2; player++) {
            Members(board, NS::kShipKindAll + player, [&](uint32_t) { everyBody = true; });
        }
        for (int kind : NS::kBodyKinds) {
            Members(board, kind, [&](uint32_t m) {
                const uint8_t flags = board.ram[(m + 27) & (SH3::kRamSize - 1)];
                if (!Long(board, m + 12)) {
                    return;
                }
                if (everyBody || (flags & 1)) {
                    Box(board, m, HitBox::Body, out);
                } else if ((flags & (NS::kNotShot | NS::kNotShotByKind4)) != (NS::kNotShot | NS::kNotShotByKind4)) {
                    Box(board, m, HitBox::Ground, out); // (shot at, but its body never hits the ship)
                }
            });
        }
        for (int kind : NS::kShotKinds) {
            Members(board, kind, [&](uint32_t m) { Box(board, m, HitBox::Shot, out); });
        }
        Members(board, NS::kBulletKind, [&](uint32_t m) {
            if (const uint32_t owner = Long(board, m + NS::kPosition + 8); owner && Long(board, m + 12)) {
                const float x = static_cast<float>(Word(board, owner)), y = static_cast<float>(NS::kPictureHeight - Word(board, owner + 4));
                out.push_back({x, y, x + 1, y + 1, HitBox::Bullet});
            }
        }, NS::kBulletFirstOnly);
        for (int player = 0; player < 2; player++) {
            Members(board, NS::kShipKind + player, [&](uint32_t m) {
                Box(board, m, HitBox::Ship, out);
            });
            Members(board, NS::kShipKindAll + player, [&](uint32_t m) { Box(board, m, HitBox::Ship, out); });
        }
    }
    // The most the energy can be now: what the life's ceiling leaves of the bar (as the game's own 0x0C1F9470 holds it)
    uint32_t EnergyFull(const Board &board) {
        const uint32_t total = Long(board, NS::kBarTotal), life = Long(board, NS::kShips);
        return total > life ? total - life : 0;
    }
    // While a game is played (akatana.h's kGameOn)
    bool Playing(const Board &board) {
        return Long(board, akatana::kGameOn) == 1;
    }
    // Player 1's score (ten digits, the lowest first)
    long long Score(const Board &board) {
        long long n = 0;
        for (int i = 9; i >= 0; i--) {
            n = n * 10 + board.ram[(akatana::kScore + static_cast<uint32_t>(i)) & (SH3::kRamSize - 1)] % 10;
        }
        return n;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.stageArguments = {{akatana::kGoOnToStage, 5, akatana::kNewGameCaller}};
        f.shipState = akatana::kShipState;
        f.bossTunes.assign(std::begin(akatana::kBossTunes), std::end(akatana::kBossTunes));
        f.safeFrames = akatana::kSafeFrames;
        f.safeFramesHeld = 232;
        f.ships = akatana::kShips;
        f.shipsBytes = 4;
        f.shipsKept = 2 * akatana::kLifePerShip; // (two ships: one lost leaves one)
        f.bombs = akatana::kBombs;
        f.bombsMax = akatana::kBombsMax;
        f.fullGauges.push_back({"energy_full", "Energy kept full",
                                "The energy gauge (the green part of the bar) is kept at the most the life leaves it: Phantom "
                                "form whenever you want it, for as long as you want it.",
                                akatana::kEnergy, 0, 4, EnergyFull});
        f.hitBoxes = BoxesNow;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(akatana::kCredits, f);
        // (the gadgets, port/gadgets.h: Phantom form's energy as a number, against the most it can be now; and the score
        // and stage, for the score by stage)
        g.gadgets = {
            {"ENERGY", Gadget::Kind::Meter,
             [](const Board &board) {
                 using namespace akatana;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const uint32_t most = Long(board, kBarTotal) - std::min(Long(board, kShips), Long(board, kBarTotal));
                 return std::to_string(Long(board, kEnergy)) + " / " + std::to_string(most);
             },
             [](const Board &board, int &value, int &most) {
                 using namespace akatana;
                 most = static_cast<int>(Long(board, kBarTotal) - std::min(Long(board, kShips), Long(board, kBarTotal)));
                 value = static_cast<int>(std::min<uint32_t>(Long(board, kEnergy), static_cast<uint32_t>(std::max(most, 0))));
                 return Playing(board);
             },
             0xFF40FF80u},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) { return Playing(board) ? static_cast<int>(std::min<uint32_t>(Long(board, akatana::kStage), 5)) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
