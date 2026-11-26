// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
#include "shmup.h"

#include "arcade.h"
#include "hit_boxes.h"
#ifdef SPELUNK_CAVE68K
#include "native68k.h"
#endif

#include <cstdio>
#include <cstdlib>

namespace spelunk::shmup {

namespace {
    // The stage a game beginning now is to start at (0: as the game would), and whether it is to be run on to its boss
    int g_stage = 0;
    // The hit boxes read at the game's hit test (Facts::hitTest), and the frame they are of (only drawn: no part of the
    // game's state)
    std::vector<HitBox> g_caught;
    uint64_t g_caughtFrame = ~0ull;
    const Facts *g_hitFacts = nullptr;
    const Board *g_hitBoard = nullptr;
    bool g_toBossWanted = false;
    // "Begin at the stage" (Game::beginNow): the board made anew is being driven, unseen, through its start-up, a coin
    // and Start to a new game, until the stage chosen begins (g_stageBegun: the drive ends there), or this long
    bool g_begin = false, g_stageBegun = false, g_sawNotFlown = false;
    uint32_t g_beginFrames = 0;
    constexpr uint32_t kBeginGivesUp = 20000;

    uint8_t &Byte(Board &board, uint32_t address) {
#ifdef SPELUNK_CAVE68K
        return board.ram[address & (board.ram.size() - 1)];
#else
        board.cpu.NoteRamWrite(address); // (a write, or maybe one: the reference is kept)
        return board.ram[address & (SH3::kRamSize - 1)];
#endif
    }
    // (both boards keep RAM big-endian)
    uint32_t Read(Board &board, uint32_t address, int bytes) {
        uint32_t value = 0;
        for (int i = 0; i < bytes; i++) {
            value = value << 8 | Byte(board, address + i);
        }
        return value;
    }
    void Write(Board &board, uint32_t address, uint32_t value, int bytes) {
        for (int i = 0; i < bytes; i++) {
            Byte(board, address + i) = static_cast<uint8_t>(value >> ((bytes - 1 - i) * 8));
        }
    }

#ifdef SPELUNK_CAVE68K
    // What the hooks need (one game a program) and the trip to the boss, kept with the board's state (run-ahead and
    // rewind put it back): featureWords[2] is where a real game's beginning is (kBegun on), featureWords[3] the
    // frames run unseen towards the boss plus one (0: not going)
    const Facts *g_facts = nullptr;
    Board *g_board = nullptr;
    uint32_t &Beginning(Board &board) {
        return board.featureWords[2];
    }
    enum : uint32_t {
        kBegun = 1,         // the stage it plays first is about to begin (a trip to the boss starts there)
        kNotReady = 2,      // begun for a later stage: its first ship is not here yet
        kComingIn = 3,      // its first ship is on its way in, not yet the player's
        kSendOn = 4,        // begun at a stage the game must still be sent on into (Facts::sendOn)
    };
    uint32_t &ToBoss(Board &board) {
        return board.featureWords[3];
    }
    bool ToBossOn(Board &board) {
        return ToBoss(board) != 0;
    }
#else
    // (a CV1000's: kept here)
    bool g_toBoss = false, g_toBossPending = false;
    uint32_t g_toBossFrames = 0;
    struct Hook {
        int reg;
        uint32_t caller;
        bool last;
    };
    Hook g_hooks[8];
    // A real game (not the show's demonstration) is being begun: its routines' stage arguments are ours to set
    bool g_beginning = false;
    // (Facts::scriptStage's hook: one game a program)
    const Facts *g_scriptFacts = nullptr;
    Board *g_scriptBoard = nullptr;
    bool ToBossOn(Board &) {
        return g_toBoss;
    }
#endif

    bool HasStageStart(const Facts &facts) {
        return !facts.stageArguments.empty() || (facts.scriptStage && facts.toStage) ||
               (facts.newGame && facts.stageWord && facts.beginStage && (facts.newGameStage || (facts.readyWord && facts.nextStage)));
    }
    bool HasBossCue(const Facts &facts) {
#ifdef SPELUNK_CAVE68K
        return facts.bossWord != 0 || facts.bossOn;
#else
        return !facts.bossTunes.empty() && (facts.shipState || (facts.scriptStage && facts.toStage));
#endif
    }
    bool HasToBoss(const Facts &facts) {
        return HasBossCue(facts) && facts.safeFrames && HasStageStart(facts);
    }
} // namespace

std::vector<recomp::FeatureInfo> Features(const Facts &facts) {
    std::vector<recomp::FeatureInfo> features;
    if (HasStageStart(facts)) {
        std::vector<std::string> stages{"Off"};
        for (size_t i = 1; i < facts.stages.size(); i++) {
            stages.push_back(facts.stages[i]);
        }
        features.push_back({"start_stage", kTrainingPage, "Start at stage",
                            "A new game begins at the stage chosen, through the game's own start of it (the ships, bombs and power of a new "
                            "game; the game may give a score to start with). Off: the first stage, as the board.",
                            0, stages});
    }
    if (HasToBoss(facts)) {
        features.push_back({"to_boss", kTrainingPage, "Straight to the boss",
                            "A new game is run on unseen, in ten to thirty seconds, to the boss of the stage it begins at (the stage chosen "
                            "above), the ship firing and out of harm on the way. With the practice point on, the point is set there.",
                            0, {}});
    }
    if (facts.safeFrames) {
        features.push_back({"out_of_harm", kTrainingPage, "Out of harm",
                            "Nothing can hit the ship: for watching a pattern through, or getting to the place you want to set a practice "
                            "point at.",
                            0, {}});
    }
    if (facts.bombs || !facts.gauges.empty()) {
        features.push_back({"endless_bombs", kTrainingPage, facts.bombsTitle, facts.bombsWords, 0, {}});
    }
    if (facts.ships) {
        features.push_back({"endless_ships", kTrainingPage, facts.shipsTitle, facts.shipsWords, 0, {}});
    }
    for (const Facts::FullGauge &g : facts.fullGauges) {
        features.push_back({g.name, kTrainingPage, g.title, g.words, 0, {}});
    }
    if (facts.hitBoxes) {
        // (the page of their own, under Training: runtime/hit_boxes.h)
        const std::vector<recomp::FeatureInfo> boxes = hitboxes::Features(facts.hitKinds);
        features.insert(features.end(), boxes.begin(), boxes.end());
    }
    return features;
}

#ifdef SPELUNK_CAVE68K

void Install(Board &board, const Facts &facts) {
    g_facts = &facts;
    g_board = &board;
    ClearHooks();
    if (!HasStageStart(facts)) {
        return;
    }
    // A real game begins: the stage it begins at is ours to choose (the attract show's demonstration begins its games
    // by another way). Not a player joining a game, or continuing one
    AddHook(facts.newGame, "shmup: a new game", [](M68K &c) {
        Board &b = *g_board;
        const Facts &f = *g_facts;
        bool playing = false;
        for (uint32_t mode : f.playModes) {
            playing = playing || (f.modeWord && Read(b, f.modeWord, 2) == mode);
        }
        const bool first = Read(b, f.stageWord, 2) == 0 && (!f.loopWord || Read(b, f.loopWord, 2) == 0);
        if (getenv("SPELUNK_TRACE_MODES")) {
            fprintf(stderr, "new game: frame %llu, d7 %u, mode %u%s\n", static_cast<unsigned long long>(b.frame), c.d[7],
                    f.modeWord ? Read(b, f.modeWord, 2) : 0, playing ? " (in play: left alone)" : "");
        }
        // (a game that takes its stage at its beginning: no stage word is set yet there, and whatever it holds is the
        // last game's)
        if (playing || (!first && !f.newGameStage) || (g_stage == 0 && !g_toBossWanted && !g_begin)) {
            return false; // (nothing for the training features to do: with them off, nothing here touches the game)
        }
        if (f.newGameStage) {
            f.newGameStage(c, b, g_stage);
            Beginning(b) = f.sendOn && (f.sendOnStages >> g_stage & 1) ? kSendOn : kBegun;
            return false;
        }
        Beginning(b) = g_stage > 0 ? kNotReady : kBegun;
        return false; // (the routine itself runs)
    });
    // The first stage of a game begun just now begins: from here to its boss, if that is wanted
    AddHook(facts.beginStage, "shmup: a stage begins", [](M68K &) {
        Board &b = *g_board;
        if (Beginning(b) == kBegun) {
            Beginning(b) = 0;
            StageBegun();
            if (g_toBossWanted) {
                ToBoss(b) = 1; // (the ship firing and out of harm from now until its boss is on)
            }
        }
        return false;
    });
}

#else

void Install(Board &board, const Facts &facts) {
    g_toBoss = g_toBossPending = g_beginning = false;
    for (size_t i = 0; i < facts.stageArguments.size() && i < 8; i++) {
        g_hooks[i] = {facts.stageArguments[i].reg, facts.stageArguments[i].caller, i + 1 == facts.stageArguments.size()};
        board.cpu.SetNativeCall(facts.stageArguments[i].address, [](SH3 &cpu, void *context) {
            const Hook &hook = *static_cast<const Hook *>(context);
            uint32_t &stage = cpu.R[hook.reg];
            if (hook.caller) {
                g_beginning = (cpu.PR & 0x1FFFFFFFu) == (hook.caller & 0x1FFFFFFFu) && stage == 0;
            }
            // (only a game's beginning asks for the first stage: the stages after are asked for by their numbers)
            if (g_beginning && stage == 0) {
                stage = static_cast<uint32_t>(g_stage);
                if (hook.last) {
                    // (the last of them: the choices are made and the stage begins)
                    g_beginning = false;
                    StageBegun();
                    g_toBossPending = g_toBossWanted; // (it begins when the ship is first flown: the choices are the player's)
                }
            }
            return false; // (the routine itself runs)
        }, &g_hooks[i]);
    }
    if (facts.scriptStage && facts.toStage) {
        g_scriptFacts = &facts;
        g_scriptBoard = &board;
        board.cpu.SetNativeCall(facts.scriptStage, [](SH3 &, void *) {
            // (with the stage start off and no trip to the boss wanted, the game is not touched)
            if ((g_stage > 0 || g_toBossWanted || g_begin) && g_scriptFacts->toStage(*g_scriptBoard, g_stage)) {
                g_toBossPending = g_toBossWanted; // (it begins when the ship is first flown)
                StageBegun();
            }
            return false; // (the routine itself runs)
        }, nullptr);
    }
    if (facts.hitTest && facts.hitBoxes) {
        g_hitFacts = &facts;
        g_hitBoard = &board;
        board.cpu.SetNativeCall(facts.hitTest, [](SH3 &, void *) {
            // (once a frame for each ship tested: the frame's first starts the frame's boxes afresh)
            if (g_caughtFrame != g_hitBoard->frame) {
                g_caught.clear();
                g_caughtFrame = g_hitBoard->frame;
            }
            g_hitFacts->hitBoxes(*g_hitBoard, g_caught);
            return false; // (the routine itself runs)
        }, nullptr);
    }
}

#endif

void BeforeFrame(Board &board, const Facts &facts, const int *values) {
    // (the values are in the order Features gives the features in)
    int at = 0;
    const int startStage = HasStageStart(facts) ? values[at++] : 0;
    const bool toBoss = HasToBoss(facts) && values[at++];
    bool outOfHarm = (facts.safeFrames && values[at++]) || ToBossOn(board);
    g_toBossWanted = toBoss;
    const bool endlessBombs = (facts.bombs || !facts.gauges.empty()) && values[at++];
    const bool endlessShips = facts.ships && values[at++];
    std::vector<bool> gaugesFull;
    for (size_t i = 0; i < facts.fullGauges.size(); i++) {
        gaugesFull.push_back(values[at++] != 0);
    }
    g_stage = startStage;
    if (board.frame < 1200) {
        return; // (the board's start-up owns RAM)
    }
#ifdef SPELUNK_CAVE68K
    // A game begun for a later stage, its first ship now the player's, goes on to that stage: from the stage before
    // it, by the game's own way on
    uint32_t &beginning = Beginning(board);
    // (and out of harm on the way, when the stage is begun now from the menu: its first stage is not the player's)
    outOfHarm = outOfHarm || (g_begin && beginning != 0 && facts.safeFrames);
    if (beginning == kSendOn) {
        if (facts.sendOn(board, g_stage)) {
            beginning = kBegun; // (the stage's own beginning comes next)
            if (getenv("SPELUNK_TRACE_MODES")) {
                fprintf(stderr, "sent on into stage %d: frame %llu\n", g_stage + 1, static_cast<unsigned long long>(board.frame));
            }
        }
    } else if (beginning == kNotReady && Read(board, facts.readyWord, 2) != 0) {
        beginning = kComingIn;
    } else if (beginning == kComingIn && Read(board, facts.readyWord, 2) == 0) {
        const int per = facts.stagesPerLoop > 0 ? facts.stagesPerLoop : static_cast<int>(facts.stages.size());
        const int before = g_stage - 1;
        Write(board, facts.stageWord, static_cast<uint32_t>(before % per), 2);
        if (facts.loopWord) {
            Write(board, facts.loopWord, static_cast<uint32_t>(before / per), 2);
        }
        facts.nextStage(board);
        beginning = kBegun;
        if (getenv("SPELUNK_TRACE_MODES")) {
            fprintf(stderr, "on to stage %d: frame %llu\n", g_stage + 1, static_cast<unsigned long long>(board.frame));
        }
    }
#endif
    if (outOfHarm) {
        Write(board, facts.safeFrames, facts.safeFramesHeld, facts.safeFramesBytes);
    }
    if (endlessBombs) {
        const bool tally = facts.bombsTally && (Byte(board, facts.bombsTally) & facts.bombsTallyBits);
        if (facts.bombs && !tally) {
            uint8_t &bombs = Byte(board, facts.bombs);
            const int full = facts.bombsMax ? Byte(board, facts.bombsMax) : facts.bombsHeld;
            if (bombs < full) {
                bombs = static_cast<uint8_t>(full);
            }
        }
        for (const Facts::Gauge &g : facts.gauges) {
            Write(board, g.address, g.full, g.bytes);
        }
    }
    for (size_t i = 0; i < facts.fullGauges.size(); i++) {
        if (gaugesFull[i]) {
            const Facts::FullGauge &g = facts.fullGauges[i];
            Write(board, g.address, g.fullNow ? g.fullNow(board) : g.full, g.bytes);
        }
    }
    if (endlessShips) {
        const uint32_t count = Read(board, facts.ships, facts.shipsBytes), mask = facts.shipsMask ? facts.shipsMask : ~0u;
        const uint32_t ships = count & mask;
        if (ships < static_cast<uint32_t>(facts.shipsKept)) {
            Write(board, facts.ships, (count & ~mask) | static_cast<uint32_t>(facts.shipsKept), facts.shipsBytes);
            if (facts.shipsAlso) {
                const uint32_t also = Read(board, facts.shipsAlso, facts.shipsBytes);
                Write(board, facts.shipsAlso, also + (static_cast<uint32_t>(facts.shipsKept) - ships), facts.shipsBytes);
            }
        }
    }
}

bool Beginning() {
    return g_begin;
}

void StageBegun() {
    g_stageBegun = g_stageBegun || g_begin;
}

namespace {
    // Begin at the stage: the presses that take a board just made to a new game (none through its start-up, then a
    // coin, Start and the shot, over and over: through the attract show, the press-start screen and any choice of
    // ship or mode, its first one), until the stage chosen begins
    int BeginDrive(Board &board, const Facts &facts) {
        if (!g_begin) {
            return -1;
        }
        // (a game whose stage is set before its ship is chosen, or that goes on to a map of stages (Deathsmiles): the shot
        // pressed on until its ship is flown, by the game's word for that; a word that may still say so from the attract
        // show's game as the stage is set, so it must be seen not flown first)
        const bool word = facts.flownWord || (facts.beginWaitsFlown && facts.shipState);
        const bool flown = facts.flownWord ? (Read(board, facts.flownWord, 2) & 0x8000) != 0
                                           : facts.shipState && Read(board, facts.shipState, 4) == 1;
        g_sawNotFlown = g_sawNotFlown || (g_stageBegun && !flown);
        const bool there = g_stageBegun && (!word || (flown && g_sawNotFlown));
        if (there || ++g_beginFrames > kBeginGivesUp) {
            if (getenv("SPELUNK_TRACE_MODES")) {
                fprintf(stderr, "begin now: %s at frame %llu\n", there ? "the stage begins" : "given up",
                        static_cast<unsigned long long>(board.frame));
            }
            g_begin = false;
            return -1;
        }
        if (board.frame < 1200) {
            return 0; // (the board's start-up)
        }
        const uint64_t phase = board.frame % 90;
        if (g_stageBegun) {
            return phase >= 60 && phase < 68 ? static_cast<int>(kInButton1) : 0;
        }
        return phase < 6 ? kUnseenCoin : phase >= 30 && phase < 38 ? kUnseenStart : phase >= 60 && phase < 68 ? static_cast<int>(kInButton1) : 0;
    }
} // namespace

int Unseen(Board &board, const Facts &facts, const int *values) {
    (void)values;
    if (const int drive = BeginDrive(board, facts); drive >= 0) {
        return drive;
    }
#ifdef SPELUNK_CAVE68K
    uint32_t &frames = ToBoss(board);
    if (!frames) {
        return -1;
    }
    // (there, or it has taken longer than any stage is: something is not as expected, and the game is the player's)
    const bool there = facts.bossOn ? facts.bossOn(board) : (Read(board, facts.bossWord, 2) & facts.bossBits) != 0;
    if (getenv("SPELUNK_TRACE_MODES") && (there || frames % 2000 == 0)) {
        fprintf(stderr, "to boss: frame %llu, %u on the way\n", static_cast<unsigned long long>(board.frame), frames - 1);
    }
    if (there || ++frames > 30000) {
        frames = 0;
        return -1;
    }
#else
    // (a game begun by its script, Facts::scriptStage, is past its ship's arrival as its first stage begins)
    if (g_toBossPending && (!facts.shipState || Read(board, facts.shipState, 4) == 1)) {
        g_toBossPending = false;
        g_toBoss = true;
        g_toBossFrames = 0;
    }
    if (!g_toBoss) {
        return -1;
    }
    // (there, or it has taken longer than any stage is: something is not as expected, and the game is the player's)
    const int tune = board.sound.SequencePlaying(0);
    bool there = false;
    for (int boss : facts.bossTunes) {
        there = there || tune == boss;
    }
    if (getenv("SPELUNK_TRACE_MODES") && (there || g_toBossFrames % 2000 == 0)) {
        fprintf(stderr, "to boss: frame %llu, %u on the way, tune %d\n", static_cast<unsigned long long>(board.frame), g_toBossFrames, tune);
    }
    if (there || ++g_toBossFrames > 30000) {
        g_toBoss = false;
        return -1;
    }
#endif
    return kInButton1; // (the shot, held)
}

namespace {
    // Game::hitBoxes where the boxes are read at the game's hit test: those of the frame just run (none if it ran no
    // test: no ship, or the game not in play)
    void CaughtHitBoxes(const Board &board, std::vector<HitBox> &out) {
        if (g_caughtFrame + 1 == board.frame) {
            out.insert(out.end(), g_caught.begin(), g_caught.end());
        }
    }
} // namespace

Game MakeGame(uint32_t creditsAddress, const Facts &facts) {
    static Facts kept;
    static uint32_t credits;
    kept = facts;
    credits = creditsAddress;
    Game g;
    g.features = {arcade::FreePlayFeature()};
    // (the hidden content, beside free play on the Game page: online, the host's choice is the match's)
    static size_t first;
    first = 1;
    if (!kept.unlocks.empty()) {
        g.features.push_back({"unlock_hidden", "Game", "Unlock hidden content",
                              "Unlocked without the board's secret code: " + kept.unlockWords + ". Off: as the board, where the "
                              "code unlocks it.",
                              0, {}});
        first = 2;
    }
    const std::vector<recomp::FeatureInfo> training = Features(kept);
    g.features.insert(g.features.end(), training.begin(), training.end());
    g.beforeFrame = [](Board &board, const std::vector<int> &values) {
#ifdef SPELUNK_CAVE68K
        arcade::CreditAtStart(board, credits, values[0] != 0, kept.startAgain);
#else
        arcade::KeepCredit(board, credits, values[0] != 0);
#endif
        // (as the code would have set them; not through the board's start-up, whose memory check owns RAM)
        if (first == 2 && values[1] && board.frame >= 1200) {
            for (const Facts::Unlock &u : kept.unlocks) {
                Write(board, u.address, u.value, 1);
            }
        }
        BeforeFrame(board, kept, &values[first]);
    };
    g.start = [](Board &board) { Install(board, kept); };
    g.unseen = [](Board &board, const std::vector<int> &values) { return Unseen(board, kept, &values[first]); };
    g.beginNow = [] {
        g_begin = true;
        g_stageBegun = g_sawNotFlown = false;
        g_beginFrames = 0;
    };
    g.hitBoxes = kept.hitTest ? CaughtHitBoxes : kept.hitBoxes;
    // (in play: the ship flown (a CV1000's word), or the game's mode one of play's (a 68000 board's))
    if (kept.shipState) {
        g.playing = [](Board &board) { return Read(board, kept.shipState, 4) == 1; };
    } else if (kept.modeWord && !kept.playModes.empty()) {
        g.playing = [](Board &board) { return Read(board, kept.modeWord, 2) == kept.playModes[0]; };
    }
    return g;
}

} // namespace spelunk::shmup
