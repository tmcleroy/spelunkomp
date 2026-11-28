// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Mushihimesama: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "mushisam.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    float Fixed(const Board &board, uint32_t address) {
        return static_cast<int32_t>(Long(board, address)) / 65536.0f;
    }
    // A box of the game's (16.16, up from the bottom of the picture stood up) as the board's pixels: the board's
    // across is the game's up, its down the game's across
    void Box(const Board &board, uint32_t box, HitBox::Kind kind, std::vector<HitBox> &out) {
        const float x = Fixed(board, box + 12), y = Fixed(board, box + 16);
        out.push_back({y - Fixed(board, box + 32), x - Fixed(board, box + 24), y + Fixed(board, box + 28), x + Fixed(board, box + 20), kind});
    }
    uint16_t Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint16_t>(p[0] << 8 | p[1]);
    }
    // Each record of a list of lists (headers of 32 bytes: the count at +0, the first record at +16, records 80 bytes)
    template <typename Each> void Records(const Board &board, uint32_t headers, uint32_t headerCount, Each each) {
        const uint32_t count = Long(board, headerCount);
        for (uint32_t h = 0; h < count && h < 256; h++) {
            const uint32_t header = headers + h * 32, records = Long(board, header), first = Long(board, header + 16);
            for (uint32_t n = 0; n < records && n < 1024; n++) {
                each(first + n * 80);
            }
        }
    }
    // At the game's hit test: what it is about to test
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace mushisam;
        for (int list = 0; list < 2; list++) {
            Records(board, kBodyHeaders[list], kBodyHeaderCounts[list], [&](uint32_t record) {
                const uint16_t shot = Word(board, record);
                if (Word(board, record + 4)) {
                    Box(board, record, HitBox::Body, out);
                } else if (list == 0 ? shot == kHurtByShotsFirst : shot != 0) {
                    Box(board, record, HitBox::Ground, out);
                }
            });
        }
        Records(board, kShotHeaders, kShotHeaderCount, [&](uint32_t record) {
            if (Word(board, record)) {
                Box(board, record, HitBox::Shot, out);
            }
        });
        Records(board, kLaserHeaders, kLaserHeaderCount, [&](uint32_t record) { Box(board, record, HitBox::Shot, out); });
        if (const uint32_t block = Long(board, kBullets)) {
            const uint32_t count = Long(board, block + kBulletCount);
            for (uint32_t n = 0; n < count && n < 4096; n++) {
                const uint32_t bullet = Long(board, block + kBulletList + n * 32 + 16);
                const float x = Fixed(board, bullet + 12), y = Fixed(board, bullet + 16);
                out.push_back({y, x, y + 1 / 65536.0f, x + 1 / 65536.0f, HitBox::Bullet});
            }
        }
        if (const uint32_t box = Long(board, kShipBox)) {
            Box(board, box, HitBox::Ship, out);
        } else {
            // (out of harm the game makes no box: drawn where it would be, its size as the game's)
            const float x = Fixed(board, kShipAt), y = Fixed(board, kShipAt + 4), r = 2.625f;
            out.push_back({y - r, x - r, y + r, x + r, HitBox::Ship});
        }
    }
    // While a player's game is played: the ship record's first word is set (it is from the coin and Start, and cleared at
    // the game over) and the ship has been put in place (0 until the shot select is over), not the title, the logos
    // or the game over after it
    bool Playing(const Board &board) {
        return Long(board, mushisam::kShipRecord) != 0 && Long(board, mushisam::kShipX) != 0;
    }
    std::string Number(uint32_t n) {
        std::string digits = std::to_string(n), out;
        for (size_t i = 0; i < digits.size(); i++) {
            out += digits[i];
            if ((digits.size() - 1 - i) % 3 == 0 && i + 1 < digits.size()) {
                out += ',';
            }
        }
        return out;
    }
    // The score the next extra ship comes at (0: none left)
    uint32_t NextExtend(const Board &board) {
        using namespace mushisam;
        if (Long(board, kExtendEvery) == 1) {
            return Long(board, kExtendList);
        }
        const uint32_t taken = Long(board, kExtendTaken);
        const uint32_t at = taken < 3 ? Long(board, kExtendList + taken * 4) : 0xFFFFFFFFu;
        return at == 0xFFFFFFFFu ? 0 : at;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.stageArguments = {{mushisam::kNewGame, 4, mushisam::kNewGameCaller}, {mushisam::kBeginStage, 5}};
        f.ships = mushisam::kShips;
        f.bombs = mushisam::kBombs;
        f.safeFrames = mushisam::kSafeFrames;
        f.safeFramesHeld = 0x97;
        f.bossTunes.assign(std::begin(mushisam::kBossTunes), std::end(mushisam::kBossTunes));
        f.shipState = mushisam::kShipState;
        f.hitTest = mushisam::kHitTest;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(mushisam::kCredits, f);
        // (the gadgets, port/gadgets.h: the counter of Maniac and Ultra, the items taken, the next extra
        // ship; and the score and stage, for the score by stage. No rank was found: see PROGRESS.md)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"COUNTER", Kind::Graph,
             [](const Board &board) {
                 return Playing(board) && Long(board, mushisam::kMode) ? "+" + std::to_string(Long(board, mushisam::kCounter)) : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = static_cast<int>(std::min<uint32_t>(Long(board, mushisam::kCounter), 1000));
                 most = 500; // (it can go higher: the bar is cut off there)
                 return Playing(board) && Long(board, mushisam::kMode);
             },
             0xFF40C0FFu},
            {"SCORE ITEMS", Kind::Words,
             [](const Board &board) {
                 using namespace mushisam;
                 return Playing(board) ? "100: " + std::to_string(Long(board, kItemsKind0)) + "  200: " + std::to_string(Long(board, kItemsKind2)) +
                                             "  500: " + std::to_string(Long(board, kItemsKind3))
                                       : std::string();
             }},
            {"NEXT EXTEND", Kind::Words,
             [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const uint32_t at = NextExtend(board);
                 return at ? Number(at) : std::string("NONE LEFT");
             }},
        };
        g.score = [](const Board &board) -> long long {
            return Playing(board) ? static_cast<long long>(Long(board, mushisam::kScore)) << 32 | Long(board, mushisam::kScore + 4) : -1;
        };
        g.stage = [](const Board &board) { return Playing(board) ? static_cast<int>(Long(board, mushisam::kStage)) : -1; };
        g.stageNames = f.stages;
        return g;
    }();
    return game;
}

} // namespace spelunk
