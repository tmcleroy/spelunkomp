// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// Deathsmiles: the game's own features (free play and the training features, from its facts: runtime/shmup.h).
#include <algorithm>
#include <iterator>
#include <string>

#include "../../../runtime/shmup.h"
#include "deathsml.h"

namespace spelunk {

namespace NSD = deathsml;

namespace {
    uint32_t Long(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
    }
    int Word(const Board &board, uint32_t address) {
        const uint8_t *p = &board.ram[address & (SH3::kRamSize - 1)];
        return static_cast<int16_t>(p[0] << 8 | p[1]);
    }
    uint8_t Byte(const Board &board, uint32_t address) {
        return board.ram[address & (SH3::kRamSize - 1)];
    }
    // The picture's height: the game counts up from its bottom
    constexpr float kTall = 240;

    // Each member of a kind that the game tests (its group's word at +8, its own at +8 and +12)
    template <typename Fn>
    void Members(const Board &board, int kind, Fn &&fn, bool testTwelve = true) {
        int guard = 0;
        for (uint32_t group = Long(board, deathsml::kKinds + kind * 12 + 4); group && guard < 4096; group = Long(board, group)) {
            if (!Word(board, group + 8)) {
                continue;
            }
            for (uint32_t m = Long(board, group + 28); m && guard < 4096; m = Long(board, m), guard++) {
                if (Word(board, m + 8) && (!testTwelve || Long(board, m + 12))) {
                    fn(m);
                }
            }
        }
    }
    // A member's place (across, up) as the game reads it
    void Place(const Board &board, uint32_t m, int &x, int &y) {
        x = Word(board, m + 52);
        y = Word(board, m + 56);
        if (const uint32_t owner = Long(board, m + 60)) {
            x += Word(board, owner);
            y += Word(board, owner + 4);
        }
    }
    // A box (reaching right, left, up, down from x, y) as the board's pixels
    void Box(int x, int y, int right, int left, int up, int down, HitBox::Kind kind, std::vector<HitBox> &out) {
        out.push_back({static_cast<float>(x - left), kTall - static_cast<float>(y + up), static_cast<float>(x + right + 1),
                       kTall - static_cast<float>(y - down) + 1, kind});
    }
    void HitBoxes(const Board &board, std::vector<HitBox> &out) {
        using namespace deathsml;
        // A member's own box (reaching right +32, left +36, up +40, down +44)
        auto memberBox = [&](uint32_t m, HitBox::Kind kind) {
            int x, y;
            Place(board, m, x, y);
            Box(x, y, static_cast<int>(Long(board, m + 32)), static_cast<int>(Long(board, m + 36)),
                static_cast<int>(Long(board, m + 40)), static_cast<int>(Long(board, m + 44)), kind, out);
        };
        // The enemies: those whose bodies hit the character, and the rest, which only the shots hit (unless shots fly
        // through them too). Most enemies have both: a small box that hits the character and a larger one the shots
        // hit (members with the same owner at +60); such an enemy is drawn by its body alone
        std::vector<uint32_t> bodies;
        for (int kind : kBodyKinds) {
            Members(board, kind, [&](uint32_t m) {
                if (Byte(board, m + 27) & 1) {
                    memberBox(m, HitBox::Body);
                    bodies.push_back(Long(board, m + 60));
                }
            });
        }
        for (int kind : kBodyKinds) {
            const bool canPass = std::find(std::begin(kShotsPassKinds), std::end(kShotsPassKinds), kind) != std::end(kShotsPassKinds);
            Members(board, kind, [&](uint32_t m) {
                const uint8_t flags = Byte(board, m + 27);
                const uint32_t owner = Long(board, m + 60);
                if (!(flags & 1) && !(canPass && (flags & 2)) && (!owner || std::find(bodies.begin(), bodies.end(), owner) == bodies.end())) {
                    memberBox(m, HitBox::Ground);
                }
            });
        }
        // The player's shots (both players' kinds; the main shots' long at +12 tested, the others' not, as the game does)
        for (int kind = kShotKindFirst; kind <= kShotKindLast; kind++) {
            const bool main = std::find(std::begin(kMainShotKinds), std::end(kMainShotKinds), kind) != std::end(kMainShotKinds);
            Members(board, kind, [&](uint32_t m) { memberBox(m, HitBox::Shot); }, main);
        }
        Members(board, kBulletKind, [&](uint32_t m) {
            if (const uint32_t owner = Long(board, m + 60)) {
                const float x = static_cast<float>(Word(board, owner)), y = kTall - static_cast<float>(Word(board, owner + 4));
                out.push_back({x, y, x + 1, y + 1, HitBox::Bullet});
            }
        });
        Members(board, kShipKind, [&](uint32_t m) {
            int x, y;
            Place(board, m, x, y);
            int right = static_cast<int>(Long(board, m + 32)), left = static_cast<int>(Long(board, m + 36));
            int up = static_cast<int>(Long(board, m + 40)), down = static_cast<int>(Long(board, m + 44));
            const uint32_t owner = Long(board, m + 60);
            if (owner && right == kOutOfHarmReach && left == kOutOfHarmReach && up == kOutOfHarmReach && down == kOutOfHarmReach) {
                // (out of harm the game gives the ship its items' reach: drawn is the box it has when it can be hit)
                const uint32_t ship = owner - 4, box = Long(board, kShipBoxes + 4 * Byte(board, ship + 0xB4));
                const bool turned = Long(board, ship + 0x194) == 0x80;
                right = Byte(board, box + (turned ? 8 : 9));
                left = Byte(board, box + (turned ? 9 : 8));
                up = Byte(board, box + 10);
                down = Byte(board, box + 11);
            }
            Box(x, y, right, left, up, down, HitBox::Ship, out);
        });
    }
    // While a game is played (deathsml.h's kGameOn)
    bool Playing(const Board &board) {
        return Long(board, deathsml::kGameOn) == 1;
    }
    // Player 1's score (32 bits, or ten digits, the lowest first)
    long long Score(const Board &board) {
        if (!NSD::kScoreDigits) {
            return static_cast<long long>(Long(board, NSD::kScore));
        }
        long long n = 0;
        for (int i = 9; i >= 0; i--) {
            n = n * 10 + board.ram[(NSD::kScore + static_cast<uint32_t>(i)) & (SH3::kRamSize - 1)] % 10;
        }
        return n;
    }
    // The stage now, from 0 (the stages played before it)
    int StageNow(const Board &board) {
        return static_cast<int>(std::min<uint32_t>(Long(board, deathsml::kStagesPlayed), deathsml::kStageCount - 1));
    }
} // namespace

const Game &ThisGame() {
    static const Game game = [] {
        shmup::Facts f;
        // (the places in the order a game without a choice plays them; "Off" is the map, as the board)
        f.stages = {"Port town", "Graveyard", "Forest", "Lake", "Ruins", "Cavern", "Castle (7th place)", "Castle (8th place)"};
        f.stageArguments = {{deathsml::kNewGame, 5, deathsml::kNewGameCaller}};
        f.ships = deathsml::kLife;
        f.shipsBytes = 2;
        f.shipsKept = 2 * deathsml::kLifePerShip - 1; // (two ships: one lost leaves one)
        f.bombs = deathsml::kBombs;
        f.bombsHeld = 3;
        f.safeFrames = deathsml::kSafeFrames;
        f.safeFramesHeld = 64;
        f.bossTunes.assign(std::begin(deathsml::kBossTunes), std::end(deathsml::kBossTunes));
        f.shipState = deathsml::kShipState;
        f.beginWaitsFlown = true; // (Begin at the stage: through the map of stages)
        f.hitBoxes = HitBoxes;
        f.hitKinds |= 1u << HitBox::Ground | 1u << HitBox::Shot;
        f.unlocks = {{deathsml::kFollettUnlocked}, {deathsml::kRosaUnlocked}, {deathsml::kCanyonUnlocked}};
        f.unlockWords = "follett, Rosa and the canyon's extra stage";
        Game g = shmup::MakeGame(deathsml::kCredits, f);
        // (the gadgets, port/gadgets.h: the level chosen for each stage so far, and how many at the highest; and the
        // score and stage, for the score by stage)
        g.gadgets = {
            {"LEVELS", Gadget::Kind::Words, [](const Board &board) {
                 using namespace deathsml;
                 if (!Playing(board)) {
                     return std::string();
                 }
                 std::string lines;
                 int highest = 0;
                 const int now = StageNow(board);
                 for (int n = 0; n <= now; n++) {
                     const uint32_t at = kStages + static_cast<uint32_t>(n * kStageEntry);
                     const uint32_t place = Long(board, at), level = Long(board, at + 4);
                     highest += level >= static_cast<uint32_t>(kLevelHighest);
                     const std::string name = place < std::size(kPlaceNames) ? kPlaceNames[place] : "PLACE " + std::to_string(place + 1);
                     const std::string lv = level < std::size(kLevelNames) ? kLevelNames[level] : std::to_string(level);
                     lines += (n == now ? ">" : " ") + std::to_string(n + 1) + " " + name + " LV" + lv + "\n";
                 }
                 return lines + "LV" + std::to_string(kLevelHighest) + (std::size(kLevelNames) > 4 ? "+" : "") + " STAGES " +
                        std::to_string(highest);
             }},
        };
        g.score = [](const Board &board) -> long long { return Playing(board) ? Score(board) : -1; };
        g.stage = [](const Board &board) { return Playing(board) ? StageNow(board) : -1; };
        return g;
    }();
    return game;
}

} // namespace spelunk
