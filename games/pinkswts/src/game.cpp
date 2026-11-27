// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Pink Sweets: Ibara Sorekara: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "pinkswts.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

namespace spelunk {

namespace NS = pinkswts;

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    uint16_t Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint16_t>(p[0] << 8 | p[1]);
    }
    void SetLong(Board &board, uint32_t address, uint32_t value) {
        board.cpu.NoteRamWrite(address); // (run-ahead's keeps: the pages written)
        board.cpu.NoteRamWrite(address + 3);
        for (int i = 0; i < 4; i++) {
            board.ram[(address + i) & (SH3::kRamSize - 1)] = static_cast<uint8_t>(value >> (24 - 8 * i));
        }
    }
    float Fixed(const Board &board, uint32_t address) {
        return static_cast<int32_t>(Long(board, address)) / 65536.0f;
    }
    // A list of the game's boxes (half-sizes about a point: pinkswts.h) about an owner's position, turned by the owner's
    // flags, as the board's pixels; `graze` the ship's boxes with the flag 0x20000000 are left out
    void Boxes(const Board &board, uint32_t list, uint32_t flags, float x, float y, HitBox::Kind kind, bool graze, std::vector<HitBox> &out) {
        const float across = (flags & 0x80000000u) ? -1.0f : 1.0f, up = (flags & 0x40000000u) ? -1.0f : 1.0f;
        for (int n = 0; n < 16 && list && Long(board, list); n++, list += 16) {
            const uint32_t w = Long(board, list);
            if (graze && (w & 0x20000000u)) {
                continue;
            }
            const float half = static_cast<float>(w & 0x0FFFFFFFu) / 65536.0f, tall = Fixed(board, list + 4);
            const float cx = x + across * Fixed(board, list + 8), cy = y + up * Fixed(board, list + 12);
            out.push_back({cx - half, cy - tall, cx + half, cy + tall, kind});
        }
    }
    // As the collision pass's first test is entered (the lists are the pass's)
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace NS;
        const float camera = Fixed(board, kCamera);
        int guard = 0;
        for (uint32_t at = Long(board, kEnemiesTo); at > Long(board, kEnemiesFrom) && guard < 1024; guard++) {
            at -= 8;
            const uint32_t enemy = Long(board, at + 4);
            const uint8_t *e = &board.ram[enemy & (SH3::kRamSize - 1)];
            const uint32_t flags = Long(board, enemy);
            if (e[0xC4] >= e[0xC6] && (flags & (0x00800000u | kShootable))) {
                Boxes(board, Long(board, enemy + 28), Long(board, enemy + 32), Fixed(board, enemy + 44), Fixed(board, enemy + 48) - camera,
                      (flags & 0x00800000u) ? HitBox::Body : HitBox::Ground, false, out);
            }
        }
        // The shots, as the two shot tests before this one took them
        const auto shot = [&](uint32_t record) {
            if (Long(board, record) & 0x08000000u) {
                const uint32_t byState = Long(board, kShotBoxes + (Long(board, record + 4) << 5) + 4);
                const uint32_t byFrame = Long(board, byState + (Long(board, record + 8) << 4));
                const uint32_t entry = byFrame + (Long(board, record + 12) << 3);
                Boxes(board, Long(board, entry + 4), Long(board, entry), Fixed(board, record + 20), Fixed(board, record + 24) - camera,
                      HitBox::Shot, false, out);
            }
        };
        if (Word(board, kShotsAll)) {
            for (uint32_t player = 0; player < 2; player++) {
                for (uint32_t n = 0; Word(board, kShotsOn + 2 * player) && n < Word(board, kShotCount) && n < 128; n++) {
                    shot(kShots + player * kShotsApart + n * kShotSize);
                }
            }
            const uint32_t bank = (Long(board, kOtherBank) + 1) & 1;
            for (uint32_t group = 0; Word(board, kOtherOn + 2 * bank) && group < Word(board, kOtherGroups) && group < 3; group++) {
                for (uint32_t n = 0; n < Word(board, kOtherCount) && n < 8; n++) {
                    shot(kOther + bank * kOtherBankApart + group * kOtherGroupApart + n * kShotSize);
                }
            }
        }
        guard = 0;
        for (uint32_t at = Long(board, kBulletsTo); at > Long(board, kBulletsFrom) && guard < 2048; guard++) {
            at -= 4;
            const uint32_t bullet = Long(board, at);
            if (Long(board, bullet) & 0x00800000u) {
                Boxes(board, Long(board, bullet + 32), Long(board, bullet + 36), Fixed(board, bullet + 48), Fixed(board, bullet + 52) - camera,
                      HitBox::Bullet, false, out);
            }
        }
        for (int player = 0; player < 2; player++) {
            const uint32_t ship = kShipRecord + player * kRecordSize;
            if (!(Long(board, ship) & 0x0C000000u)) {
                continue;
            }
            const uint32_t byState = Long(board, kShipBoxes + (Long(board, ship + 8) << 5) + 4);
            const uint32_t byFrame = Long(board, byState + (Long(board, ship + 12) << 4));
            const uint32_t entry = byFrame + (Long(board, ship + 16) << 3);
            Boxes(board, Long(board, entry + 4), Long(board, entry), Fixed(board, ship + 24), Fixed(board, ship + 28) - camera, HitBox::Ship, true,
                  out);
        }
    }
    // As the script's "begin stage" is entered: where it is the first of a real game's, the script is sent on past
    // the stages before the one chosen, as if they had been played
    bool ToStage(Board &board, int stage) {
        using namespace pinkswts;
        if (Long(board, kScript) != kFirstStage) {
            return false;
        }
        SetLong(board, kScript, kFirstStage + stage * kBeginStageSize);
        return true;
    }
    unsigned Word16(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<unsigned>(p[0]) << 8 | p[1];
    }
    unsigned Byte8(const Board &board, uint32_t address) {
        return board.ram[address & (SH3::kRamSize - 1)];
    }
    bool Playing(const Board &board) {
        return (Long(board, pinkswts::kPlayer1) >> 31) != 0;
    }
    // A medal's worth from the game's byte
    std::string Medal(unsigned n) {
        return n <= 18 ? std::to_string(n < 9 ? (n + 1) * 100 : (n - 8) * 1000) : std::string();
    }
    // The boss's life and phase now, as the layout reads them: a boss's record (the kinds and parts of its table) in use
    // and hittable; in two bosses' phases another part's life is the one that counts (the layout's overrides)
    bool BossLife(const Board &board, int &life, int &phase) {
        using namespace pinkswts;
        struct Boss {
            uint32_t kind, part;
            int overridePhase = -1;
            uint32_t overridePart = 0;
        };
        static const Boss kBosses[] = {
            {61, 0}, {57, 1}, {64, 0}, {69, 0}, {44, 0}, {63, 0}, {39, 0, 3, 8}, {62, 0}, {37, 0}, {42, 0},
            {65, 0}, {84, 0}, {41, 1}, {72, 0}, {66, 0}, {2, 0, 1, 1}, {67, 0}, {36, 0}, {36, 1}, {36, 2}, {36, 4},
        };
        life = -1;
        phase = -1;
        int overridePhase = -1;
        uint32_t overridePart = 0, overrideKind = 0;
        for (int i = 0; i < kEnemyCount; i++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(i * kEnemySize);
            const uint32_t flags = Long(board, at);
            const int l = static_cast<int>(Word16(board, at + kEnemyLife));
            if (!(flags & 0x80000000u) || (flags & 0x4000u) || l <= 0) {
                continue;
            }
            const uint32_t kind = Long(board, at + kEnemyKind), part = Long(board, at + kEnemyPart);
            for (const Boss &b : kBosses) {
                if (b.kind == kind && b.part == part) {
                    life = l;
                    phase = static_cast<int>(Byte8(board, at + kEnemyPhase));
                    if (b.overridePhase == phase) {
                        overridePhase = phase;
                        overridePart = b.overridePart;
                        overrideKind = kind;
                    }
                }
            }
        }
        if (overridePhase >= 0) {
            for (int i = 0; i < kEnemyCount; i++) {
                const uint32_t at = kEnemies + static_cast<uint32_t>(i * kEnemySize);
                if ((Long(board, at) & 0x80000000u) && Long(board, at + kEnemyKind) == overrideKind && Long(board, at + kEnemyPart) == overridePart) {
                    life = static_cast<int>(Word16(board, at + kEnemyLife));
                }
            }
        }
        return life > 0;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5", "Stage 6", "Stage 7"};
        f.scriptStage = pinkswts::kBeginStage;
        f.toStage = ToStage;
        f.ships = pinkswts::kShips;
        f.safeFrames = pinkswts::kSafeFrames;
        f.safeFramesBytes = 1;
        f.safeFramesHeld = pinkswts::kSafeFramesHeld;
        f.bossTunes.assign(std::begin(pinkswts::kBossTunes), std::end(pinkswts::kBossTunes));
        f.gauges = {{pinkswts::kRoseCracker, pinkswts::kRoseCrackerFull, 1}};
        f.bombsTitle = "Rose Cracker charged";
        f.bombsWords = "The Rose Cracker's gauge (the game's bomb) is kept full: every press of the shot releases one.";
        f.hitBoxes = HitBoxes;
        f.hitTest = pinkswts::kHitTest;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        Game g = shmup::MakeGame(pinkswts::kCredits, f);
        // (the M2-style gadgets, port/gadgets.h: what Olifante's MAME layout for the game shows)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) {
                 char words[24];
                 snprintf(words, sizeof words, "%.2f%%", (1.0 - Long(board, pinkswts::kRank) / 16777216.0) * 100);
                 return Playing(board) ? std::string(words) : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 most = 1 << 16;
                 value = std::clamp(static_cast<int>((0x1000000 - static_cast<long long>(Long(board, pinkswts::kRank))) >> 8), 0, most);
                 return Playing(board);
             },
             0xFF4070FFu},
            {"ZAN", Kind::Words, [](const Board &board) {
                 return Playing(board) ? std::to_string(static_cast<int>(Word16(board, pinkswts::kZan)) - static_cast<int>(Word16(board, pinkswts::kZanLess)))
                                       : std::string();
             }},
            {"NEXT DROPS", Kind::Words, [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 static const char *const kRound[8] = {"BULLET", "ROSE", "DOUBLE", "ROSE", "TRIPLE", "ROSE", "OPTION", "ROSE"};
                 const unsigned n = Byte8(board, pinkswts::kNextDrop);
                 std::string out;
                 for (unsigned i = 0; i < 4; i++) {
                     out += (i ? "\n" : ">") + std::string(i ? " " : "") + kRound[(n + i) % 8];
                 }
                 return out + "\nDROPPED\n SHOTS " + std::to_string(Byte8(board, pinkswts::kShotsDropped)) + "  OPTIONS " +
                        std::to_string(Byte8(board, pinkswts::kOptionsDropped));
             }},
            {"NEXT MEDAL", Kind::Words,
             [](const Board &board) { return Playing(board) ? Medal(Byte8(board, pinkswts::kNextMedal)) : std::string(); }},
            {"BOSS", Kind::Words, [](const Board &board) {
                 int life = 0, phase = 0;
                 return Playing(board) && BossLife(board, life, phase) ? "LIFE " + std::to_string(life) + "  PHASE " + std::to_string(phase)
                                                                      : std::string();
             }},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? static_cast<long long>(Long(board, pinkswts::kScore1)) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
