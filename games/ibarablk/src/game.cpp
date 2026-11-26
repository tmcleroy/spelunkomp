// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Ibara Kuro Black Label: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ibarablk.h"

#include <iterator>

namespace spelunk {

namespace {
    uint8_t &Byte(Board &board, uint32_t address) {
        board.cpu.NoteRamWrite(address); // (run-ahead's keeps: the page written)
        return board.ram[address & (SH3::kRamSize - 1)];
    }
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    void SetLong(Board &board, uint32_t address, uint32_t value) {
        for (int i = 0; i < 4; i++) {
            Byte(board, address + i) = static_cast<uint8_t>(value >> (24 - 8 * i));
        }
    }
    float Fixed(const Board &board, uint32_t address) {
        return static_cast<int32_t>(Long(board, address)) / 65536.0f;
    }
    // A list of the game's boxes (as Ibara's: half-sizes about a point, ibara.h) about a position, as the board's
    // pixels; `flagged` the boxes with the flag 0x20000000 (0: those without it, -1: all); `point` the boxes taken as
    // points of a half-size of 1
    void Boxes(const Board &board, uint32_t list, float x, float y, HitBox::Kind kind, int flagged, bool point,
               std::vector<HitBox> &out) {
        for (int n = 0; n < 16 && Long(board, list); n++, list += 16) {
            const uint32_t w = Long(board, list);
            if (flagged < 0 || !(w & 0x20000000u)) {
                const float half = point ? 1.0f : static_cast<float>(w & 0x0FFFFFFFu) / 65536.0f;
                const float tall = point ? 1.0f : Fixed(board, list + 4);
                const float cx = x + Fixed(board, list + 8), cy = y + Fixed(board, list + 12);
                out.push_back({cx - half, cy - tall, cx + half, cy + tall, kind});
            }
        }
    }
    // After the frame: the lists last from frame to frame
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace ibarablk;
        const float camera = Fixed(board, kCamera);
        for (int i = 0; i < kEnemyCount; i++) {
            const uint32_t enemy = kEnemies + i * kEnemySize;
            const uint8_t *e = &board.ram[enemy & (SH3::kRamSize - 1)];
            const uint32_t flags = Long(board, enemy);
            if ((flags & 0x10000000u) && e[0xA9] >= e[0xAB]) {
                Boxes(board, Long(board, enemy + 28), Fixed(board, enemy + 36), Fixed(board, enemy + 40) - camera, HitBox::Body, 0, false, out);
            } else if ((flags & kShootable) && e[0xA9] >= e[0xAB]) {
                Boxes(board, Long(board, enemy + 28), Fixed(board, enemy + 36), Fixed(board, enemy + 40) - camera, HitBox::Ground, 0, false, out);
            }
        }
        // The shots (each player's every frame, though the game tests one player's a frame)
        const auto shot = [&](uint32_t record) {
            if (Long(board, record) & 0x08000000u) {
                const uint32_t byType = Long(board, kShotBoxes + Long(board, record + 4) * 8);
                const uint32_t byState = Long(board, byType + Long(board, record + 8) * 16);
                const uint32_t list = Long(board, byState + Long(board, record + 12) * 8 + 4);
                Boxes(board, list, Fixed(board, record + 16), Fixed(board, record + 20) - camera, HitBox::Shot, -1, false, out);
            }
        };
        for (uint32_t player = 0; player < 2; player++) {
            for (uint32_t n = 0; n < 32; n++) {
                shot(kShots + player * 0x680 + n * kShotSize);
            }
            for (uint32_t n = 0; n < 48; n++) {
                shot(kOtherShots + player * 0x9C0 + n * kShotSize);
            }
        }
        for (int i = 0; i < kBulletCount; i++) {
            const uint32_t bullet = kBullets + i * kBulletSize;
            if (Long(board, bullet) & 0x10000000u) {
                Boxes(board, Long(board, bullet + 32), Fixed(board, bullet + 40), Fixed(board, bullet + 44) - camera, HitBox::Bullet, -1, true, out);
            }
        }
        for (int player = 0; player < 2; player++) {
            const uint32_t ship = kShipRecord + player * 0x2A0;
            if (!(Long(board, ship) & 0x0C000000u)) {
                continue;
            }
            const uint32_t byType = Long(board, kShipBoxes + Long(board, ship + 4) * 8);
            const uint32_t byState = Long(board, byType + Long(board, ship + 8) * 16);
            const uint32_t list = Long(board, byState + Long(board, ship + 12) * 8 + 4);
            Boxes(board, list, Fixed(board, ship + 16), Fixed(board, ship + 20) - camera, HitBox::Ship, 0, false, out);
        }
    }
    // As the script's "begin stage" is entered (as Ibara's): a real game's first stage, after the ship's arrival, is
    // about to be begun, and the script is sent on past the stages before the one chosen, as if they had been played
    bool ToStage(Board &board, int stage) {
        using namespace ibarablk;
        const uint32_t script = Long(board, kScript);
        if (Byte(board, kStagesBegun) != 1 || Byte(board, script + 1) != 1) {
            return false;
        }
        SetLong(board, kScript, script + stage * kBeginStageSize);
        Byte(board, kStagesBegun) = static_cast<uint8_t>(1 + stage);
        return true;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6"};
        f.scriptStage = ibarablk::kBeginStage;
        f.toStage = ToStage;
        f.ships = ibarablk::kShips;
        f.bombs = ibarablk::kBombs;
        f.bombsHeld = 1;
        f.safeFrames = ibarablk::kSafeFrames;
        f.safeFramesBytes = 1;
        f.safeFramesHeld = ibarablk::kSafeFramesHeld;
        f.bossTunes.assign(std::begin(ibarablk::kBossTunes), std::end(ibarablk::kBossTunes));
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        return shmup::MakeGame(ibarablk::kCredits, f);
    }();
    return game;
}

} // namespace spelunk
