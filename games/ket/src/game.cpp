// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Ketsui: the game's own features (free play; the training features from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ket.h"

#include <algorithm>
#include <string>

namespace spelunk {

namespace {
    int Word(const Board &board, uint32_t address) {
        const size_t mask = board.ram.size() - 1;
        return (board.ram[address & mask] << 8) | board.ram[(address + 1) & mask];
    }
    void SetWord(Board &board, uint32_t address, int value) {
        const size_t mask = board.ram.size() - 1;
        board.ram[address & mask] = static_cast<uint8_t>(value >> 8);
        board.ram[(address + 1) & mask] = static_cast<uint8_t>(value);
    }
    // While a player's game is played, not the attract show's demonstration, nor the title (the stage task's kind: ket.h)
    bool Playing(const Board &board) {
        using namespace ket;
        return Word(board, kTasks) == kStageTask && !Word(board, kDemo) && (Word(board, kShips - 1) & 0xFF) != 0xFF;
    }
    // The chips the game's kills draw on now: the small number, and from the boss's warning on the big one (ket.h)
    bool ChipsOnTotal(const Board &board) {
        return (Word(board, ket::kBossOn) & ket::kBossWarned) != 0;
    }
    // The boxes the game's hit test (0B02FC) uses, read as it reads them
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ket;
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
        // The player's shots
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
        }
        const int bullets = (Word(board, kBulletRounds) + 1) * 10;
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
        f.newGame = shmup::HookAt(ket::kNewGame);
        f.newGameStage = [](M68K &cpu, Board &board, int stage) {
            const uint32_t at = (cpu.a[0] + ket::kTaskStage) & static_cast<uint32_t>(board.ram.size() - 1);
            SetWord(board, at, stage);
            if (stage == 3) {
                SetWord(board, ket::kScrollHeight, 0); // (set again by the scroll task's first run: sendOn waits for it)
            }
        };
        // Stage 4 is begun as stage 3's end begins it: ket.h
        f.sendOnStages = 1u << 3;
        f.sendOn = [](Board &board, int) {
            using namespace ket;
            if (Word(board, kScrollHeight) != kStage4Height) {
                return false; // (the scroll task has not had its first run yet)
            }
            SetWord(board, kScrollRunning, 1);
            SetWord(board, kScrollRunning2, 1);
            SetWord(board, kScrollSpeedSet, 0x0101);
            SetWord(board, kScrollSpeed, kStageEndSpeed);
            SetWord(board, kScrollSpeed2, kStageEndSpeed);
            SetWord(board, kScrollStands, 1);
            SetWord(board, kScrollTimer, 0xFFFF);
            SetWord(board, kScrollTimerCount, 1);
            SetWord(board, kScrollTimerReload, kTimerReload);
            SetWord(board, kScrollAsk, 2);
            SetWord(board, kScrollAskStage, 3);
            return true;
        };
        f.beginStage = shmup::HookAt(ket::kSetStage);
        f.flownWord = ket::kShipFlown;
        f.stageWord = ket::kStage;
        f.stagesPerLoop = 5;
        f.ships = ket::kShips;
        f.bombs = ket::kBombs;
        f.bombsHeld = 3;
        f.safeFrames = ket::kSafeFrames;
        f.safeFramesHeld = 0xE8;
        f.safeFramesBytes = 1;
        f.bossWord = ket::kBossOn;
        f.bossBits = ket::kBossWarned;
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(ket::kCredits, f);
        // (the gadgets, port/gadgets.h: the chips, the loop; and the score and stage, for the score by stage)
        using Kind = Gadget::Kind;
        g.gadgets = {
            // (the stock and the total as the HUD's two numbers, the one the game is counting now first)
            {"CHIPS", Kind::Words, [](const Board &board) {
                 using namespace ket;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const std::string stock = "STOCK " + std::to_string(Word(board, kChipStock)) + " / " + std::to_string(kChipStockMost);
                 const std::string total = "TOTAL " + std::to_string(Word(board, kChipTotal)) + " / " + std::to_string(kChipTotalMost);
                 return ChipsOnTotal(board) ? ">" + total + "\n " + stock : ">" + stock + "\n " + total;
             }},
            {"LOOP", Kind::Words, [](const Board &board) {
                 return Playing(board) ? std::string(Word(board, ket::kLoop) ? "SECOND LOOP" : "FIRST LOOP") : std::string();
             }},
        };
        g.score = [](const Board &board) -> long long {
            using namespace ket;
            if (!Playing(board)) {
                return -1;
            }
            return (static_cast<long long>(Word(board, kScore1)) << 16) | Word(board, kScore1 + 2);
        };
        // (the first loop's five stages, then the second's: the stage word begins again at 0)
        g.stage = [](const Board &board) { return Playing(board) ? Word(board, ket::kLoop) * 5 + Word(board, ket::kStage) : -1; };
        g.stageNames = {"1-1", "1-2", "1-3", "1-4", "1-5", "2-1", "2-2", "2-3", "2-4", "2-5"};
        return g;
    }();
    return game;
}

} // namespace spelunk
