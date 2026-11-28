// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi Dai-Ou-Jou Black Label: the game's own features (free play; the training features from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ddpdojblk.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const size_t mask = board.ram.size() - 1;
        return (board.ram[address & mask] << 8) | board.ram[(address + 1) & mask];
    }
    // A byte of work RAM, and a word kept in BCD (the game's counters as it shows them) as a number
    int Byte(const Board &board, uint32_t address) {
        return board.ram[address & (board.ram.size() - 1)];
    }
    int Bcd(int bcd) {
        int n = 0, scale = 1;
        for (; bcd; bcd >>= 4, scale *= 10) {
            n += (bcd & 15) * scale;
        }
        return n;
    }
    // While a player's game is played (not the attract show's demonstration, nor the title or the logos after one,
    // whose memory still holds its last game's): the readouts and the hidden bees show nothing otherwise
    bool Playing(const Board &board) {
        using namespace ddpdojblk;
        return Word(board, kMode) == kModePlayed && !Word(board, kDemoInputs) && !Word(board, kDemoInputs + 2);
    }
    // The boxes the game's hit test (244D62) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ddpdojblk;
        // (64ths of a pixel; the far edges are in the box, so it reaches to the 64th after them)
        // (the reaches are signed: a negative one puts that edge on the other side of the position)
        auto reach = [&](uint32_t address) { return static_cast<int>(static_cast<int16_t>(Word(board, address))); };
        auto box = [&](uint32_t record, HitBox::Kind kind) {
            const int x = Word(board, record + kX), y = Word(board, record + kY);
            out.push_back({(x - reach(record + kBoxLeft)) / 64.0f, (y - reach(record + kBoxUp)) / 64.0f,
                           (x + reach(record + kBoxRight) + 1) / 64.0f, (y + reach(record + kBoxDown) + 1) / 64.0f, kind});
        };
        auto body = [&](uint32_t at) {
            return (Word(board, at) & 1) && ((Word(board, at + kX) + 0x2800 - Word(board, at + kBoxLeft)) & 0xFFFF) < 0xF800;
        };
        int enemies = Word(board, kEnemyCount1) + Word(board, kEnemyCount2);
        for (uint32_t at = kEnemies; enemies > 0 && at < kEnemyCount1; at += kEnemySize) {
            if (!(Word(board, at) & 0x8000)) {
                continue;
            }
            enemies--;
            if (body(at)) {
                box(at, HitBox::Body);
            }
        }
        // The enemies the shots hurt (in either group) that the ship's test leaves out: the ground's
        auto group = [&](uint32_t from, int slots, uint32_t count, int moved, int onScreen) {
            int left = Word(board, count);
            for (int n = 0; n < slots && left > 0; n++) {
                const uint32_t at = from + static_cast<uint32_t>(n) * kEnemySize;
                const int flags = Word(board, at);
                if (!(flags & 0x8000)) {
                    continue;
                }
                left--;
                if ((flags & kShootable) && !body(at) && ((Word(board, at + kX) + moved - Word(board, at + kBoxLeft)) & 0xFFFF) < onScreen) {
                    box(at, HitBox::Ground);
                }
            }
        };
        group(kEnemies, kAirSlots, kEnemyCount1, kAirMoved, kAirOnScreen);
        group(kGroundEnemies, kGroundSlots, kEnemyCount2, kGroundMoved, kGroundOnScreen);
        // The player's shots, and the laser while it is on
        for (uint32_t shots : {kShots1, kShots2}) {
            for (int n = 0; n < kShotSlots; n++) {
                const uint32_t at = shots + static_cast<uint32_t>(n) * kShotSize;
                if (!(Word(board, at) & 0x8000) || (Word(board, at + kShotPower) & 0x8000)) {
                    continue;
                }
                const int x = Word(board, at + kX) + reach(at + kShotRight), y = Word(board, at + kY) + reach(at + kShotDown);
                out.push_back({(x - reach(at + kShotWidth)) / 64.0f, (y - 2 * reach(at + kShotHalfHeight)) / 64.0f,
                               (x + 1) / 64.0f, (y + 1) / 64.0f, HitBox::Shot});
            }
        }
        for (int player = 0; player < 2; player++) {
            const uint32_t ship = player ? kShip2 : kShip1;
            const int flags = Word(board, ship);
            if (!(flags & 0x8000) || (flags & 0x80) || !board.ram[(ship + kShipLaserOn) & (board.ram.size() - 1)]) {
                continue;
            }
            const uint32_t other = player ? kLaserOther2 : kLaserOther1, aura = player ? kLaserAura2 : kLaserAura1;
            if ((Word(board, other) & 0x8000) && Word(board, other + kX) < 0x7000) {
                box(other, HitBox::Shot);
            }
            if (Word(board, aura) & 0x8000) {
                box(aura, HitBox::Shot);
            }
            const uint32_t laser = player ? kLaser2 : kLaser1;
            if ((Word(board, laser) & 0x8200) == 0x8200) {
                const int x = Word(board, laser + kX), y = Word(board, laser + kY);
                const int end = std::min(x + 0x400 + reach(laser + kLaserLength), Word(board, laser + kLaserEnd));
                out.push_back({x / 64.0f, (y - reach(laser + kLaserUp)) / 64.0f, (end + 1) / 64.0f,
                               (y + reach(laser + kLaserDown) + 1) / 64.0f, HitBox::Shot});
            }
        }
        int more = 0;
        while (more < 4 && Word(board, kMoreBullets + 2 * more)) {
            more++;
        }
        for (int n = 0; n < kBulletCounts[more]; n++) {
            const uint32_t at = kBullets + static_cast<uint32_t>(n) * kBulletSize;
            const int flags = Word(board, at);
            if ((flags & 0x8000) && !(flags & 0x5100)) {
                const float x = Word(board, at + kX) / 64.0f, y = Word(board, at + kY) / 64.0f;
                out.push_back({x, y, x + 1 / 64.0f, y + 1 / 64.0f, HitBox::Bullet});
            }
        }
        for (uint32_t ship : {kShip1, kShip2}) {
            if (Word(board, ship) & 0x8000) {
                box(ship, HitBox::Ship);
            }
        }
        // The hidden bees: on the screen (or coming down onto it), a line through each along the board's row (up and
        // down the picture as it stands), one along its column once it is on, and a box about it
        auto bees = [&](uint32_t from, int slots) {
            if (!Playing(board)) {
                return;
            }
            for (int n = 0; n < slots; n++) {
                const uint32_t at = from + static_cast<uint32_t>(n) * kEnemySize;
                if (!(Word(board, at) & 0x8000)) {
                    continue;
                }
                const uint32_t picture = static_cast<uint32_t>(Word(board, at + kPicture)) << 16 | Word(board, at + kPicture + 2);
                if (picture != kBeePicture && picture != kBeePictureLit) {
                    continue;
                }
                const float x = static_cast<int16_t>(Word(board, at + kX)) / 64.0f, y = static_cast<int16_t>(Word(board, at + kY)) / 64.0f;
                if (y < 0 || y >= 224 || x < 0 || x >= 448 + 64) {
                    continue;
                }
                out.push_back({0, y - 0.5f, 448, y + 0.5f, HitBox::Hidden});
                if (x < 448) {
                    out.push_back({x - 0.5f, 0, x + 0.5f, 224, HitBox::Hidden});
                    out.push_back({x - 6, y - 6, x + 6, y + 6, HitBox::Hidden});
                }
            }
        };
        bees(kEnemies, kAirSlots);
        bees(kGroundEnemies, kGroundSlots);
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.newGame = shmup::HookAt(ddpdojblk::kNewGame);
        f.newGameStage = [](M68K &cpu, Board &board, int stage) {
            const uint32_t at = (cpu.a[0] + ddpdojblk::kTaskStage) & static_cast<uint32_t>(board.ram.size() - 1);
            board.ram[at] = static_cast<uint8_t>(stage >> 8);
            board.ram[at + 1] = static_cast<uint8_t>(stage);
        };
        f.beginStage = shmup::HookAt(ddpdojblk::kSetStage);
        f.flownWord = ddpdojblk::kShipFlown;
        f.stageWord = ddpdojblk::kStage;
        f.stagesPerLoop = 5;
        f.ships = ddpdojblk::kShips;
        f.bombs = ddpdojblk::kBombs;
        f.bombsMax = ddpdojblk::kBombStock;
        f.safeFrames = ddpdojblk::kSafeFrames;
        f.safeFramesHeld = 0xF0;
        f.safeFramesBytes = 1;
        f.bossWord = ddpdojblk::kBossOn;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot | 1u << HitBox::Hidden;
        Game g = shmup::MakeGame(ddpdojblk::kCredits, f);
        // (the scoring readouts: what the game keeps and does not show)
        using namespace ddpdojblk;
        g.readouts = {
            // (the game shows the chain's hits, the hyper gauge as its bar and the hyper items held itself: the most hits
            // in a chain and the gauge as a number it does not)
            {"BEST ", [](const Board &board) {
                 return Playing(board) ? std::to_string(Bcd(Word(board, kChainBest))) : std::string();
             }},
            // (one line: the gauge, or while a hyper is on its level and the time it has left; the items held)
            {"HYPER", [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 if (Byte(board, kHyperOn)) {
                     return "ON LV" + std::to_string(Byte(board, kHyperLevel)) + " " +
                            std::to_string(std::min(Word(board, kHyperLeft), kHyperFull) * 100 / kHyperFull) + "%";
                 }
                 return std::to_string(std::min(Word(board, kHyperGauge), kHyperFull) * 100 / kHyperFull) + "% x" +
                        std::to_string(Byte(board, kHyperStock));
             }},
            // (the ranks and what they make of the enemies' fire: the bullets' speed, the game rank, the hyper rank)
            {"RANK ", [](const Board &board) {
                 return Playing(board) ? "SPEED " + std::to_string(Word(board, kBulletSpeed)) + " INTERVAL " +
                                             std::to_string(Word(board, kGameRank)) + " HYPER " +
                                             std::to_string(Word(board, kHyperRank))
                                       : std::string();
             }},
            {"BEES ", [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, kBees)) + "/10" : std::string();
             }},
            // (the second loop's terms as the game tests them at the first's end, while the first is played)
            {"LOOP2", [](const Board &board) {
                 if (!Playing(board) || Word(board, kStage) >= 5) {
                     return std::string();
                 }
                 const int lost = Word(board, kShipsLost), bombs = Word(board, kBombsUsed), bees = Word(board, kBeeStages) / 4;
                 const bool on = !Word(board, kContinued) && (lost <= 2 || bombs <= 3 || bees >= 3);
                 return "MISS " + std::to_string(lost) + "/2 BOMB " + std::to_string(bombs) + "/3 BEE " +
                        std::to_string(bees) + "/3 " + (on ? "OK" : "OUT");
             }},
        };
        // (the gadgets, port/gadgets.h: the ranks over time and as meters, the chain's and the hyper's gauges,
        // the second loop's terms; and the score, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) { return Playing(board) ? "GAME RANK " + std::to_string(Word(board, kGameRank)) : std::string(); },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, kGameRank);
                 most = 255; // (240 at most, 255 in a hyper)
                 return Playing(board);
             },
             0xFF4070FFu},
            {"BULLET SPEED", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, kBulletSpeed)) + " / 15  HYPER RANK " + std::to_string(Word(board, kHyperRank))
                                       : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, kBulletSpeed);
                 most = 15; // (8 at most in the first loop, 15 in the second)
                 return Playing(board);
             },
             0xFF40B0FFu},
            {"CHAIN", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Bcd(Word(board, kChain))) + " HITS  BEST " + std::to_string(Bcd(Word(board, kChainBest)))
                                       : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, kChainGauge);
                 most = kChainGaugeFull;
                 return Playing(board);
             },
             0xFF40FFFFu},
            {"HYPER", Kind::Meter,
             [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 if (Byte(board, kHyperOn)) {
                     return "ON  LEVEL " + std::to_string(Byte(board, kHyperLevel));
                 }
                 return std::to_string(std::min(Word(board, kHyperGauge), kHyperFull) * 100 / kHyperFull) + "%  HELD " +
                        std::to_string(Byte(board, kHyperStock));
             },
             [](const Board &board, int &value, int &most) {
                 value = std::min(Word(board, Byte(board, kHyperOn) ? kHyperLeft : kHyperGauge), kHyperFull);
                 most = kHyperFull;
                 return Playing(board);
             },
             0xFF60FF60u},
            {"BEES", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, kBees)) + " / 10  ALL TEN: " + std::to_string(Word(board, kBeeStages) / 4)
                                       : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, kBees);
                 most = 10;
                 return Playing(board);
             },
             0xFF30D0F0u},
            // (the second loop's terms as the game tests them at the first's end, while the first is played)
            {"TO THE 2ND LOOP", Kind::Words, [](const Board &board) {
                 if (!Playing(board) || Word(board, kStage) >= 5) {
                     return std::string();
                 }
                 const int lost = Word(board, kShipsLost), bombs = Word(board, kBombsUsed), bees = Word(board, kBeeStages) / 4;
                 const bool continued = Word(board, kContinued) != 0;
                 const bool on = !continued && (lost <= 2 || bombs <= 3 || bees >= 3);
                 auto mark = [](bool ok) { return ok ? "  OK" : ""; };
                 return std::string("ONE OF:\n") + " MISSES " + std::to_string(lost) + " / 2" + mark(lost <= 2) + "\n BOMBS  " +
                        std::to_string(bombs) + " / 3" + mark(bombs <= 3) + "\n BEES   " + std::to_string(bees) + " / 3" + mark(bees >= 3) +
                        "\n" + (continued ? "CONTINUED: OUT" : on ? "ON THE WAY" : "OUT");
             }},
        };
        g.score = [](const Board &board) -> long long {
            if (!Playing(board)) {
                return -1;
            }
            // (BCD, in tens: the last digit shown is always 0)
            const long long bcd = (static_cast<long long>(Word(board, kScore1)) << 16) | Word(board, kScore1 + 2);
            long long n = 0, scale = 1;
            for (long long b = bcd; b; b >>= 4, scale *= 10) {
                n += (b & 15) * scale;
            }
            return n * 10;
        };
        g.stage = [](const Board &board) { return Playing(board) ? Word(board, kStage) : -1; };
        g.readoutsWhat = "the most hits in a chain this game; the hyper gauge as a number (or, while a hyper is on, its "
                         "level and the time it has left) and the hyper items held; the ranks (the bullets' speed, the "
                         "game rank and the hyper rank); the bees taken in this stage; and the second loop's terms (ships "
                         "lost, bombs used, stages with all ten bees) as the game tests them";
        return g;
    }();
    return game;
}

} // namespace spelunk
