// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Espgaluda: the game's own features (free play; the training features from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "espgal.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const size_t mask = board.ram.size() - 1;
        return (board.ram[address & mask] << 8) | board.ram[(address + 1) & mask];
    }
    // While a player's game is played (the ships' byte is 0xFF while none is; the attract show's demonstration has its own)
    bool Playing(const Board &board) {
        return (Word(board, espgal::kShips - 1) & 0xFF) != 0xFF && !Word(board, espgal::kDemo);
    }
    long long Score(const Board &board) {
        return ((static_cast<long long>(Word(board, espgal::kScore1)) << 16) | Word(board, espgal::kScore1 + 2)) * 10;
    }
    // In Kakusei (the gems left: 0, Overmode)
    bool Kakusei(const Board &board) {
        return Playing(board) && Word(board, espgal::kShip1 + espgal::kShipKakusei) != 0;
    }
    bool Overmode(const Board &board) {
        return Kakusei(board) && Word(board, espgal::kGems) == 0;
    }
    // The boxes the game's hit test (11B1D0) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace espgal;
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
        group(kGroundEnemies, kGroundSlots, kEnemyCount2, kGroundMoved, (Word(board, kBossOn) & 0x0400) ? 0x10000 : kGroundOnScreen);
        // The player's shots, and the lasers while they are on
        for (int player = 0; player < 2; player++) {
            int left = Word(board, player ? kShotCount2 : kShotCount1);
            for (int n = 0; n < kShotSlots && left > 0; n++) {
                const uint32_t at = (player ? kShots2 : kShots1) + static_cast<uint32_t>(n) * kShotSize;
                if (!(Word(board, at) & 0x8000)) {
                    continue;
                }
                left--;
                if (Word(board, at + kShotPower) & 0x8000) {
                    continue;
                }
                const int x = Word(board, at + kX) + reach(at + kShotRight), y = Word(board, at + kY) + reach(at + kShotDown);
                out.push_back({(x - reach(at + kShotWidth)) / 64.0f, (y - 2 * reach(at + kShotHalfHeight)) / 64.0f,
                               (x + 1) / 64.0f, (y + 1) / 64.0f, HitBox::Shot});
            }
            const uint32_t ship = player ? kShip2 : kShip1;
            if (!(Word(board, ship) & 0x8000)) {
                continue;
            }
            const bool awake = board.ram[(ship + kShipAwake) & (board.ram.size() - 1)] != 0;
            if (!awake && !Word(board, ship + kShipLaserOn)) {
                continue;
            }
            // (awakened, each word is two further on and the laser reaches back by +8 too)
            const int more = awake ? kAwakeMoved : 0;
            const uint32_t from = awake ? (player ? kAwakeLasers2 : kAwakeLasers1) : (player ? kLasers2 : kLasers1);
            for (int n = 0; n < (awake ? kAwakeLasers : kLasers); n++) {
                const uint32_t laser = from + static_cast<uint32_t>(n) * (awake ? kAwakeLaserSize : kLaserSize);
                if (!(Word(board, laser) & 0x8000)) {
                    break;
                }
                const int x = Word(board, laser + kX), y = Word(board, laser + kY);
                const int end = std::min(x + 0x400 + reach(laser + kLaserLength), Word(board, laser + kLaserEnd + more));
                out.push_back({(x - (awake ? reach(laser + kLaserBack) : 0)) / 64.0f, (y - reach(laser + kLaserUp + more)) / 64.0f,
                               (end + 1) / 64.0f, (y + reach(laser + kLaserDown + more) + 1) / 64.0f, HitBox::Shot});
            }
        }
        const int rounds = Word(board, kBulletRounds);
        const int bullets = rounds > 6 ? (rounds + 1) * 10 : 70;
        for (int n = 0; n < bullets && n < 512; n++) {
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
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.newGame = shmup::HookAt(espgal::kNewGame);
        // (a later stage: as Dai-Ou-Jou's and Ketsui's, the game's task holds the stage it begins with, but begun at a later
        // stage that way the game shows no background and no ship. So the game is begun at the first stage and, when the
        // player's ship is in, sent on by the game's own end of a stage: runtime/shmup.h)
        f.readyWord = espgal::kShipComing;
        f.nextStage = [](Board &board) {
            using namespace espgal;
            const uint32_t mask = static_cast<uint32_t>(board.ram.size() - 1);
            auto byte = [&](uint32_t address) -> uint8_t & { return board.ram[address & mask]; };
            auto setWord = [&](uint32_t address, uint32_t value) {
                byte(address) = static_cast<uint8_t>(value >> 8);
                byte(address + 1) = static_cast<uint8_t>(value);
            };
            // (what the end of a stage does at 119538 when a boss is beaten, but its sounds and the score's tally)
            setWord(kStageEnding, 1);
            for (uint32_t ship : {kShip1, kShip2}) {
                if ((byte(ship) & 0x80) || ((byte(ship + 1) & 1) && !(byte(ship + 1) & 0x80))) {
                    byte(ship) |= 0x20;
                }
            }
            int next = Word(board, kStage) + 1;
            if (next >= kStagesInAll) {
                next = 0;
            }
            // (a task as 117ABC makes one: the new tasks' list, its count of bytes, the tasks' numbers)
            const uint32_t used = static_cast<uint32_t>(Word(board, kNewTasksUsed));
            if (used >= kNewTasksRoom) {
                return;
            }
            const uint32_t task = kNewTasks + used;
            for (uint32_t i = 0; i < kTaskSize; i++) {
                byte(task + i) = 0;
            }
            setWord(task, 0x8000 | kStageEndTask);
            setWord(task + 4, static_cast<uint32_t>(next));
            setWord(task + 0x40, 1);
            setWord(task + 0x4A, kStageEndTaskWord);
            setWord(task + 0x50, kStageEndMaker >> 16); // (who made it: the return address 117ABC keeps)
            setWord(task + 0x52, kStageEndMaker & 0xFFFF);
            setWord(kNewTasksUsed, used + kTaskSize);
            const uint32_t number = (static_cast<uint32_t>(Word(board, kTaskNumber)) << 16 | Word(board, kTaskNumber + 2)) + 1;
            setWord(kTaskNumber, number >> 16);
            setWord(kTaskNumber + 2, number & 0xFFFF);
            setWord(task + 0x4C, number >> 16);
            setWord(task + 0x4E, number & 0xFFFF);
        };
        f.beginStage = shmup::HookAt(espgal::kSetStage);
        f.stageWord = espgal::kStage;
        f.stagesPerLoop = espgal::kStagesInAll;
        f.ships = espgal::kShips;
        f.gauges = {{espgal::kBarrier, espgal::kBarrierFull, 2}};
        f.bombsTitle = "Barrier never runs out";
        f.bombsWords = "The guard barrier's gauge is kept full.";
        f.fullGauges.push_back({"gems_full", "Gems kept full",
                                "The green gems are kept at 500, the most there can be: Kakusei whenever you want it, for as "
                                "long as you want it.",
                                espgal::kGems, espgal::kGemsFull, 2});
        f.safeFrames = espgal::kSafeFrames;
        f.safeFramesHeld = 0xE0;
        f.safeFramesBytes = 1;
        f.bossWord = espgal::kBossOn;
        f.bossBits = espgal::kBossWarned;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(espgal::kCredits, f);
        // (the gadgets, port/gadgets.h: Kakusei's multiplier, Overmode's level, the gold and its step; and the score and
        // stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"KAKUSEI", Kind::Meter,
             [](const Board &board) {
                 if (!Kakusei(board) || Overmode(board)) {
                     return std::string();
                 }
                 return "x" + std::to_string(Word(board, espgal::kShip1 + espgal::kShipCancels) / 10 + 1) + " / 100";
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, espgal::kShip1 + espgal::kShipCancels) / 10 + 1;
                 most = 100;
                 return Kakusei(board) && !Overmode(board);
             },
             0xFF40FFFFu},
            {"OVERMODE", Kind::Meter,
             [](const Board &board) {
                 using namespace espgal;
                 if (!Overmode(board)) {
                     return std::string();
                 }
                 return "LEVEL " + std::to_string(std::min(Word(board, kShip1 + kShipOverLevel) / 2 + 1, kOverLevels)) + " / " +
                        std::to_string(kOverLevels);
             },
             [](const Board &board, int &value, int &most) {
                 using namespace espgal;
                 // (at the last level: full)
                 const bool last = Word(board, kShip1 + kShipOverLevel) / 2 + 1 >= kOverLevels;
                 most = std::max(Word(board, kShip1 + kShipOverNext), 1);
                 value = last ? most : Word(board, kShip1 + kShipOverWay);
                 return Overmode(board);
             },
             0xFFFF6040u},
            {"GOLD", Kind::Meter,
             [](const Board &board) {
                 using namespace espgal;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 return std::to_string(Word(board, kGold)) + "  STEP " + std::to_string(Word(board, kGoldStep) / 4 + 1) + " (" +
                        std::to_string(Word(board, kGoldIntoStep)) + "/100)";
             },
             [](const Board &board, int &value, int &most) {
                 value = Word(board, espgal::kGold);
                 most = espgal::kGoldMost;
                 return Playing(board);
             },
             0xFF40D0FFu},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) { return Playing(board) ? std::min(Word(board, espgal::kStage), espgal::kStagesInAll - 1) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
