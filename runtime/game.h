// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What a game adds to the toolkit (games/<id>/src): its features (in the launcher, the menu and settings.ini, by
// recomp-common's rules) and what it does with them each frame.
#pragma once

#include "../machine/board.h"
#include "../recomp-common/menu.h"
#include "../recomp-common/settings.h"
#include "own_shader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace spelunk {

// What can hit what, in the board's own pixels (its 320 x 240, before the picture is turned), as the game itself tests
// it: drawn over the picture by the "Hit boxes" training feature (each kind on or off, in a colour: runtime/hit_boxes.h)
struct HitBox {
    enum Kind : uint8_t {
        Ship,   // the player's ship: what a bullet's point or an enemy's body must touch
        Bullet, // a bullet's point (its box, if the game gives bullets one)
        Body,   // an enemy whose body hits the ship (and the player's shots, where the game uses the same box)
        Shot,   // a shot of the player's: what must touch an enemy's box to hurt it
        Ground, // an enemy the shots can hurt whose body never hits the ship (a tank, a turret: on the ground)
        Hidden, // where something the game hides is, before the player finds it (a secret bonus): the game gives the
                // boxes that mark it (thin lines across the picture meeting there, a box about it)
        kKinds
    };
    float x0, y0, x1, y1; // (from its top left x0, y0 up to x1, y1: those not in it)
    Kind kind;
};

// The menu's page of modes (every game's: the training tools' page is under it, and a game's other modes' pages), and
// the training tools' page (the features' category)
constexpr const char *kModesPage = "Alternative modes", *kTrainingPage = "Training mode";
// The gadgets' page, under the Game page (the feature's category): for play of every kind, not only training
constexpr const char *kGadgetsPage = "Gadgets";

// (Game::unseen's bits besides player 1's controls: player 1's Start and a coin, held that frame)
constexpr int kUnseenStart = 1 << 16, kUnseenCoin = 1 << 17;

// A scoring counter the game hides, for the "Scoring readouts" training feature (shown down the picture's left side):
// its name and how to read its value now ("": nothing to show, as between games)
struct Readout {
    const char *name;
    std::string (*value)(const Board &board);
};

// A gadget (the feature "Gadgets", port/gadgets.h): a framed panel beside the picture, in the window's space
// the picture leaves at its sides. Read from the game's memory each
// frame shown, never written to it. A Words gadget shows `words` (lines broken at '\n'; "": the gadget not shown, as
// between games); a Meter its `level` as a bar, with `words` beside it; a Graph its `level` kept over the last
// half a minute as a line (the rank graph), the value now with `words` under it. `level` gives the value now and the
// most it can be; false: none now (the gadget not shown, and a Graph's line begun again)
struct Gadget {
    enum class Kind { Words, Meter, Graph };
    const char *title;
    Kind kind = Kind::Words;
    std::string (*words)(const Board &board) = nullptr;
    bool (*level)(const Board &board, int &value, int &most) = nullptr;
    uint32_t colour = 0xFFFFC040u; // (the bar's or the line's: red in the low byte)
    bool right = false;            // (the picture's right side; the left otherwise)
};

struct Game {
    std::vector<recomp::FeatureInfo> features;
    // Before each frame, with each feature's value (in the order of `features`)
    void (*beforeFrame)(Board &board, const std::vector<int> &values) = nullptr;
    // When the board is made (and made anew at a reset): the game's hooks put on its CPU
    void (*start)(Board &board) = nullptr;
    // Asked before each frame: -1, or (the game is being run to where a mode begins, unseen and as fast as the
    // machine goes) player 1's controls for the frame, with kUnseenStart / kUnseenCoin for the cabinet's
    int (*unseen)(Board &board, const std::vector<int> &values) = nullptr;
    // Begin a new game now (the in-game menu's "Begin at the stage"): asked just before the board is made anew, it
    // has `unseen` run the new board through its start-up, a coin and Start, to the beginning of the stage chosen
    void (*beginNow)() = nullptr;
    // After each frame, before its picture is shown
    void (*afterFrame)(Board &board, const std::vector<int> &values) = nullptr;
    // The hit boxes as the board is now (the feature "hit_boxes": the game's own test, read from its memory)
    void (*hitBoxes)(const Board &board, std::vector<HitBox> &out) = nullptr;
    // The game's own rows in the in-game menu that begin a new game its own way (an alternative mode, from its page):
    // `command` is told which was chosen just before the board is made anew, and has `unseen` run the new board to
    // where the mode begins; -1: the board is being made anew for something else (Reset, Begin at the stage: no mode)
    std::vector<recomp::MenuCommand> commands;
    void (*command)(int which) = nullptr;
    // Words for the top of the picture this frame (an alternative mode's readout: Dodge training's pattern and hits);
    // none: nothing shown
    std::string (*readout)(const Board &board) = nullptr;
    // True while a game is being played (the ship is the player's), where the game's facts say so: the input lag
    // readout measures only then
    bool (*playing)(Board &board) = nullptr;
    // The scoring counters the game hides (gems, gauges, the hidden counter), read from its memory: with any, the
    // toolkit offers the feature "readouts" on the Training mode page
    std::vector<Readout> readouts;
    // (what the readouts feature's description says they are, after "Shown down the left of the picture: ")
    const char *readoutsWhat = nullptr;
    // The game's own gadgets (Gadget above); every game also has the toolkit's: the controls held, and with `score`
    // and `stage` the score each stage made
    std::vector<Gadget> gadgets;
    // Player 1's score now, and the stage played (0 the first; a second loop's stages after the first loop's), while
    // a player's game is played; -1 otherwise (the attract show's, the title): the "Score by stage" gadget
    long long (*score)(const Board &board) = nullptr;
    int (*stage)(const Board &board) = nullptr;
    // (the stages' names for that gadget, in order; none: their numbers)
    std::vector<std::string> stageNames;
};
// The game's own (games/<id>/src)
const Game &ThisGame();

// (a port's own flag, recomp-common/settings.h's 16 to 1024: as cps2recomp's) Set on the launcher: the in-game menu
// shows it and can only put it back to its first choice (off). For a monitor's resolution, where a mode the monitor
// cannot take, chosen while playing, could harm it
constexpr unsigned kFeatureLauncherOnly = 1024;

// Every feature: the game's, then the toolkit's own (runtime/features.cpp), then the game program's
const std::vector<recomp::FeatureInfo> &AllFeatures();
// The game program's own features, after the toolkit's: the Arcade monitor page (port/main.cpp). Given once, before
// AllFeatures is first asked (the headless runners give none)
void AddProgramFeatures(std::vector<recomp::FeatureInfo> features);
// (Slowdown: the CV1000's and the PGM's, a choice whose 0 is on; a game of Cave's 68000 board has none, and its
// FeatureIndex is then past the list's end)
enum class ToolkitFeature { Screen, Scaling, Shader, ScanlineDirection, RunAhead, InputLag, PracticePoint, Rewind, FastForward, InputDisplay, SpeedReadout, Slowdown };
size_t FeatureIndex(ToolkitFeature feature);
// (the Shader feature's Your own: kYourOwnShader, runtime/own_shader.h)
size_t FeatureIndex(const std::string &name);

} // namespace spelunk
