// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Muchi Muchi Pork!: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "mmpork.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace spelunk {

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
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
    // A list of the game's boxes (half-sizes about a point: mmpork.h) about an owner's position, turned by the owner's
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
        using namespace mmpork;
        const float camera = Fixed(board, kCamera);
        int guard = 0;
        for (uint32_t at = Long(board, kEnemiesTo); at > kEnemiesFrom && guard < 1024; guard++) {
            at -= 8;
            const uint32_t enemy = Long(board, at + 4);
            const uint32_t flags = Long(board, enemy);
            if (flags & (0x00800000u | kShootable)) {
                Boxes(board, Long(board, enemy + 32), Long(board, enemy + 28), Fixed(board, enemy + 44), Fixed(board, enemy + 48) - camera,
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
        if (Long(board, kShotsOn)) {
            for (uint32_t player = 0; player < 2; player++) {
                for (uint32_t n = 0; Long(board, kPlayerOn + 4 * player) && n < Long(board, kShotCount) && n < 32; n++) {
                    shot(kShots + player * kShotsApart + n * kShotSize);
                }
                for (uint32_t option = 0; Long(board, kOptionsOn + 4 * player) && option < Long(board, kOptionCount) && option < 6; option++) {
                    for (uint32_t n = 0; Long(board, kOptionOn + 4 * (player * 6 + option)) && n < Long(board, kOptionShotCount) && n < 16; n++) {
                        shot(kOptionShots + player * 0x2400 + option * 0x600 + n * kShotSize);
                    }
                }
            }
        }
        guard = 0;
        for (uint32_t at = Long(board, kBulletsTo); at > kBulletsFrom && guard < 2048; guard++) {
            at -= 4;
            const uint32_t bullet = Long(board, at);
            if (Long(board, bullet) & 0x00800000u) {
                Boxes(board, Long(board, bullet + 36), Long(board, bullet + 32), Fixed(board, bullet + 48), Fixed(board, bullet + 52) - camera,
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
            Boxes(board, Long(board, entry + 4), Long(board, entry), Fixed(board, ship + 28), Fixed(board, ship + 32) - camera, HitBox::Ship, true,
                  out);
        }
    }
    // As the script's "begin stage" is entered: where it is the first of a real game's, the script is sent on past
    // the stages before the one chosen, as if they had been played
    bool ToStage(Board &board, int stage) {
        using namespace mmpork;
        if (Long(board, kScript) != kFirstStage) {
            return false;
        }
        SetLong(board, kScript, kFirstStage + stage * kBeginStageSize);
        return true;
    }
    // While a game is played: the lard meter's word is the record's, nonsense before the first game
    bool Playing(const Board &board) {
        const uint32_t a = mmpork::kLard & (SH3::kRamSize - 1);
        const unsigned lard = static_cast<unsigned>(board.ram[a]) << 8 | board.ram[a + 1];
        return lard <= mmpork::kLardFull && Long(board, mmpork::kScore1) < 1000000000u;
    }
    unsigned Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<unsigned>(p[0]) << 8 | p[1];
    }
    // The boss's life now and its phase (-1: no boss), as the MAME layout reads it (mmpork.h's kEnemies)
    struct BossPhase {
        uint32_t kind;
        int phase;
        std::vector<uint32_t> parts; // (the parts whose life counts, first to last)
    };
    bool BossLife(const Board &board, int &life, int &phase) {
        using namespace mmpork;
        // (kind, the part its phase is read from, then each phase's parts: the layout's table)
        struct Boss {
            uint32_t kind, phasePart;
            std::vector<BossPhase> phases;
        };
        static const std::vector<Boss> kBosses = {
            {73, 0, {{73, 1, {13}}}},                                   // stage 1's midboss
            {80, 0, {{80, 1, {22, 0}}}},                                // stage 1's boss
            {74, 0, {{74, 1, {0}}}},                                    // stage 2's midboss
            {81, 0, {{81, 1, {51}}, {81, 3, {7}}, {81, 5, {5}}}},       // stage 2's boss
            {75, 0, {{75, 1, {0}}}},                                    // stage 3's midboss
            {82, 0, {{82, 1, {0, 1}}}},                                 // stage 3's boss
            {76, 0, {{76, 1, {45, 0}}}},                                // stage 4's first midboss
            {77, 0, {{77, 1, {0}}}},                                    // stage 4's second
            {83, 0, {{83, 1, {62}}, {83, 3, {48}}, {83, 5, {0}}}},      // stage 4's boss
            {79, 0, {{79, 0, {0}}}},                                    // stage 5's first midboss
            {78, 0, {{78, 1, {0}}}},                                    // stage 5's second
            {21, 0, {{21, 0, {3}}}},                                    // stage 5's painting
            {84, 0, {{84, 1, {56}}, {84, 3, {26}}, {84, 5, {0, 42}}}},  // stage 5's boss
        };
        phase = -1;
        life = 0;
        const Boss *boss = nullptr;
        // (the boss's phase, from the part the table names; then the first of that phase's parts still alive)
        for (int i = 0; i < kEnemyCount; i++) {
            const uint32_t at = kEnemies + static_cast<uint32_t>(i * kEnemySize);
            const uint32_t kind = Long(board, at + kEnemyKind), part = Long(board, at + kEnemyPart);
            for (const Boss &b : kBosses) {
                if (b.kind == kind && b.phasePart == part) {
                    boss = &b;
                    phase = board.ram[(at + kEnemyPhase) & (SH3::kRamSize - 1)];
                }
            }
        }
        if (!boss) {
            return false;
        }
        for (const BossPhase &p : boss->phases) {
            if (p.phase != phase) {
                continue;
            }
            for (const uint32_t want : p.parts) {
                for (int i = 0; i < kEnemyCount; i++) {
                    const uint32_t at = kEnemies + static_cast<uint32_t>(i * kEnemySize);
                    if (Long(board, at + kEnemyKind) == boss->kind && Long(board, at + kEnemyPart) == want) {
                        if (const int l = static_cast<int>(Word(board, at + kEnemyLife)); l > 0) {
                            life = l;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.stages = {"Stage 1", "Stage 2", "Stage 3", "Stage 4", "Stage 5"};
        f.scriptStage = mmpork::kBeginStage;
        f.toStage = ToStage;
        f.ships = mmpork::kShips;
        f.safeFrames = mmpork::kSafeFrames;
        f.safeFramesBytes = 1;
        f.safeFramesHeld = mmpork::kSafeFramesHeld;
        f.bossTunes.assign(std::begin(mmpork::kBossTunes), std::end(mmpork::kBossTunes));
        f.bombs = mmpork::kBombs;
        f.bombsHeld = 2;
        f.fullGauges.push_back({"lard_full", "Lard meter kept full",
                                "The lard meter at the bottom of the picture is filled again as it is used: a lard attack "
                                "whenever you want one.",
                                mmpork::kLard, mmpork::kLardFull, 2});
        f.hitBoxes = HitBoxes;
        f.hitTest = mmpork::kHitTest;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        f.unlocks = {{mmpork::kManpukuUnlocked}, {mmpork::kHaraharaUnlocked}};
        f.unlockWords = "Manpuku mode (hold up as Start is pressed) and Harahara mode (hold down)";
        Game g = shmup::MakeGame(mmpork::kCredits, f);
        // (the scoring readouts: the lard meter as a number, which the game shows only as its bar)
        g.readouts = {{"LARD", [](const Board &board) {
                           const uint32_t a = mmpork::kLard & (SH3::kRamSize - 1);
                           const unsigned lard = static_cast<unsigned>(board.ram[a]) << 8 | board.ram[a + 1];
                           return lard <= mmpork::kLardFull ? std::to_string(lard) + "/" + std::to_string(mmpork::kLardFull) : std::string();
                       }}};
        g.readoutsWhat = "the lard meter as a number (104 full)";
        // (the M2-style gadgets, port/gadgets.h: what wnka's MAME layout for the game shows)
        using Kind = Gadget::Kind;
        g.gadgets = {
            {"RANK", Kind::Graph,
             [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 char words[24];
                 const double v = (static_cast<double>(mmpork::kRankLowest) - static_cast<int32_t>(Long(board, mmpork::kRank))) / mmpork::kRankSpan;
                 snprintf(words, sizeof words, "%.2f%%", v * 100);
                 return std::string(words);
             },
             [](const Board &board, int &value, int &most) {
                 const long long below = static_cast<long long>(mmpork::kRankLowest) - static_cast<int32_t>(Long(board, mmpork::kRank));
                 most = 1 << 16;
                 value = static_cast<int>(std::clamp<long long>(below >> 8, 0, most));
                 return Playing(board);
             },
             0xFF4070FFu},
            {"LARD", Kind::Meter,
             [](const Board &board) {
                 return Playing(board) ? std::to_string(Word(board, mmpork::kLard)) + " / " + std::to_string(mmpork::kLardFull) : std::string();
             },
             [](const Board &board, int &value, int &most) {
                 value = static_cast<int>(Word(board, mmpork::kLard));
                 most = mmpork::kLardFull;
                 return Playing(board);
             },
             0xFF8080FFu},
            {"NEXT MEDAL", Kind::Words, [](const Board &board) {
                 if (!Playing(board)) {
                     return std::string();
                 }
                 const int n = board.ram[mmpork::kNextMedal & (SH3::kRamSize - 1)];
                 return n <= 18 ? std::to_string(n < 9 ? (n + 1) * 100 : (n - 8) * 1000) : std::string();
             }},
            {"BOSS", Kind::Words, [](const Board &board) {
                 int life = 0, phase = 0;
                 if (!Playing(board) || !BossLife(board, life, phase)) {
                     return std::string();
                 }
                 return "LIFE " + std::to_string(life) + "  PHASE " + std::to_string(phase);
             }},
        };
        return g;
    }();
    return game;
}

} // namespace spelunk
