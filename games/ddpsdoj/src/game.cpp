// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// DoDonPachi SaiDaiOuJou: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include "../../../runtime/shmup.h"
#include "ddpsdoj.h"

#include <cstdio>
#include <cstdlib>

namespace spelunk {

namespace NS = ddpsdoj;

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    void Write(Board &board, uint32_t address, uint32_t value) {
        board.cpu.NoteRamWrite(address); // (run-ahead's keeps: the pages written)
        board.cpu.NoteRamWrite(address + 3);
        uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        p[0] = static_cast<uint8_t>(value >> 24);
        p[1] = static_cast<uint8_t>(value >> 16);
        p[2] = static_cast<uint8_t>(value >> 8);
        p[3] = static_cast<uint8_t>(value);
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
} // namespace

// --- A game begun at a later stage, and the trip to a stage's boss (this game's own: see ddpsdoj.h) ---
namespace {
    // The game the features made (free play and the facts' features), whose beforeFrame and unseen are called on
    Game g_inner;
    // The stage a real game begun now is to go on to (0: the first, as the board), and the trip to the boss
    int g_target = 0;
    bool g_toBossWanted = false, g_toBossPending = false, g_toBoss = false;
    uint32_t g_toBossFrames = 0;
    // A call of the game's own routine put in before another's (the other's arguments and return kept, and put back as
    // it is entered again)
    bool g_reentry = false;
    bool g_realGame = false; // (the last game set up was a real one, not the attract show's)
    uint32_t g_saved[5];

    uint32_t Record(const Board &board) {
        return Long(board, ddpsdoj::kGameRecord);
    }
    // The first ship of the game is the player's (flown, not on its way in)
    bool ShipReady(const Board &board) {
        bool ready = false;
        Members(board, NS::kShipKind, [&](uint32_t m) {
            const uint32_t owner = Long(board, m + NS::kPosition + 8);
            ready = ready || (owner && Long(board, ddpsdoj::kShipComingIn) == 0 && Long(board, ddpsdoj::kShipFlown) == 1);
        });
        return ready;
    }
    // Wanted now: the game's way on to the next stage, from the first stage, to the stage chosen (found from memory, so
    // that a frame put back by run-ahead or rewind asks again)
    bool GoOnNow(const Board &board) {
        return g_target > 0 && Long(board, ddpsdoj::kStage) == 0 && Long(board, Record(board) + ddpsdoj::kNextStage) == 0 &&
               ShipReady(board);
    }

    void Install(Board &board) {
        if (g_inner.start) {
            g_inner.start(board);
        }
        g_reentry = false;
        g_realGame = false;
        g_toBoss = g_toBossPending = false;
        // A new game is set up: a real one (not the attract show's), from its first stage
        board.cpu.SetNativeCall(ddpsdoj::kNewGame, [](SH3 &cpu, void *) {
            if (cpu.R[4] == 0) {
                g_realGame = (cpu.PR & 0x1FFFFFFFu) == (ddpsdoj::kNewGameCaller & 0x1FFFFFFFu);
            }
            if ((cpu.PR & 0x1FFFFFFFu) == (ddpsdoj::kNewGameCaller & 0x1FFFFFFFu) && cpu.R[4] == 0) {
                g_toBossPending = g_toBossWanted;
                g_toBoss = false;
            } else if (cpu.R[4] == 0) {
                g_toBossPending = g_toBoss = false;
            }
            return false; // (the routine itself runs)
        }, nullptr);
        // Each frame in play: the game's way to the next stage (as when a boss is beaten) called in before the collision
        // pass, with the stage to go to
        static Board *b;
        b = &board;
        board.cpu.SetNativeCall(ddpsdoj::kEachFrame, [](SH3 &cpu, void *) {
            if (g_reentry) {
                g_reentry = false;
                cpu.PR = g_saved[4];
                for (int i = 0; i < 4; i++) {
                    cpu.R[4 + i] = g_saved[i];
                }
                return false; // (the routine itself runs, as it would have)
            }
            if (!GoOnNow(*b)) {
                return false;
            }
            for (int i = 0; i < 4; i++) {
                g_saved[i] = cpu.R[4 + i];
            }
            g_saved[4] = cpu.PR;
            cpu.R[4] = Record(*b);
            cpu.R[5] = static_cast<uint32_t>(g_target);
            cpu.R[6] = 1;
            cpu.PR = cpu.PC;
            cpu.PC = (cpu.PC & 0xE0000000u) | (ddpsdoj::kNextStageCall & 0x1FFFFFFFu);
            g_reentry = true;
            if (getenv("SPELUNK_TRACE_MODES")) {
                fprintf(stderr, "on to stage %d: frame %llu\n", g_target + 1, static_cast<unsigned long long>(b->frame));
            }
            return true;
        }, nullptr);
    }

    // The game's features with ours (the stage, the trip to the boss) after free play
    std::vector<int> Inner(const std::vector<int> &values) {
        std::vector<int> inner{values[0]};
        inner.insert(inner.end(), values.begin() + 3, values.end());
        return inner;
    }
    void BeforeFrame(Board &board, const std::vector<int> &values) {
        g_target = values[1];
        g_toBossWanted = values[2] != 0;
        g_inner.beforeFrame(board, Inner(values));
        if (g_toBoss) {
            Write(board, ddpsdoj::kSafeFrames, 219);
        }
    }
    int Unseen(Board &board, const std::vector<int> &values) {
        // (Begin at the stage: the stage chosen is the player's, its ship flown)
        if (shmup::Beginning() && g_realGame && board.frame >= 1200 && static_cast<int>(Long(board, ddpsdoj::kStage)) == g_target && ShipReady(board)) {
            shmup::StageBegun();
        }
        // (the trip begins at the stage chosen, its ship the player's)
        if (g_toBossPending && board.frame >= 1200 && static_cast<int>(Long(board, ddpsdoj::kStage)) == g_target && ShipReady(board)) {
            g_toBossPending = false;
            g_toBoss = true;
            g_toBossFrames = 0;
        }
        if (!g_toBoss) {
            return g_inner.unseen ? g_inner.unseen(board, Inner(values)) : -1;
        }
        const int tune = board.sound.SequencePlaying(0);
        bool there = false;
        for (int boss : ddpsdoj::kBossTunes) {
            there = there || tune == boss;
        }
        if (getenv("SPELUNK_TRACE_MODES") && (there || g_toBossFrames % 2000 == 0)) {
            fprintf(stderr, "to boss: frame %llu, %u on the way, tune %d\n", static_cast<unsigned long long>(board.frame), g_toBossFrames, tune);
        }
        // (there, or it has taken longer than any stage is: something is not as expected, and the game is the player's)
        if (there || ++g_toBossFrames > 30000) {
            g_toBoss = false;
            return -1;
        }
        return kInButton1; // (the shot, held)
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        f.ships = ddpsdoj::kShips;
        f.bombs = ddpsdoj::kBombs;
        f.safeFrames = ddpsdoj::kSafeFrames;
        f.safeFramesHeld = 219;
        f.hitBoxes = BoxesNow;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        g_inner = shmup::MakeGame(ddpsdoj::kCredits, f);
        Game g = g_inner;
        // (as runtime/shmup.cpp offers them for the other games)
        g.features.insert(g.features.begin() + 1,
                          {{"start_stage", kTrainingPage, "Start at stage",
                            "A new game begins at the stage chosen, through the game's own start of it (the ships, bombs and power of a new "
                            "game; the game may give a score to start with). Off: the first stage, as the board. Here the first stage's "
                            "start is played, and the game's own way on to the next stage (its tally) takes the ship to the stage chosen.",
                            0,
                            {"Off", "Stage 2", "Stage 3", "Stage 4", "Stage 5"}},
                           {"to_boss", kTrainingPage, "Straight to the boss",
                            "A new game is run on unseen, in ten to thirty seconds, to the boss of the stage it begins at (the stage chosen "
                            "above), the ship firing and out of harm on the way. With the practice point on, the point is set there.",
                            0,
                            {}}});
        g.beforeFrame = BeforeFrame;
        g.start = Install;
        g.unseen = Unseen;
        return g;
    }();
    return game;
}

} // namespace spelunk
