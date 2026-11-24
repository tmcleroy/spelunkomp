// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The toolkit's own features, after the game's and before the game program's (game.h)
#include "game.h"

namespace spelunk {

namespace {
    const std::vector<recomp::FeatureInfo> &ToolkitFeatures() {
        static const std::vector<recomp::FeatureInfo> features = {
            {"screen", "Display", "Screen",
             "These games were made for a monitor on its side. Stood up: the picture turned to stand upright on an ordinary "
             "screen. As the cabinet: the picture as the board sends it, for a screen you have turned on its side (its top to "
             "the left, or to the right).",
             0, {"Stood up", "As the cabinet (top to the left)", "As the cabinet (top to the right)"}},
            {"scaling", "Display", "Scaling",
             "How large the board's picture is shown. Whole pixels: the largest whole multiple of the board's 320 x 240 that "
             "fits the window, every pixel the same size and sharp, with a border. A fixed multiple: that size exactly (the "
             "largest that fits, if the window is smaller). Fill the window: as large as fits, with pixels of uneven sizes.",
             0, {"Whole pixels (largest that fits)", "Fill the window", "1x", "2x", "3x", "4x", "5x", "6x", "7x", "8x"}},
            // (shader: port/shading.h; its presets kShaderPresets in port/main.cpp, in shaders/ beside the program: the
            // same list and words as cps2recomp's Display page, and CRT Newpixie after Your own; tools/librashader.sh. Your own's
            // presets: recomp-common/own_shaders.h, and its own row under this one in the menu and the launcher)
            {"shader", "Display", "Shader",
             "The picture drawn through one of RetroArch's shaders. CRT: a curved arcade monitor. CRT "
             "Lottes and CRT Guest: flat monitors, sharp and bright. CRT Royale: the closest to a real "
             "monitor's glass and the slowest. CRT Light: lines and a faint mask, for a slow computer. "
             "CRT Newpixie: a light CRT look with chromatic aberration, ghosting, a little curvature and scanlines. "
             "Scanlines: the lines alone, light, strong, as an OSSC draws them, or dark (a black line "
             "between each two, best with Whole pixels). xBRZ and ScaleFX: the pixels' edges made smooth "
             "lines and curves. Your own: a .slangp preset of yours, chosen under Your own preset, from the "
             "shaders folder every game shares (the launcher's Open shaders folder opens it) or the game's "
             "own. The launcher chooses which of these the menu goes through. The picture's "
             "shape and size stay as set; the shader draws its pixels in place of Scaling.",
             recomp::kFeatureMenuChoices,
             {"Off", "CRT", "CRT Lottes", "CRT Royale", "CRT Guest", "CRT Light", "Scanlines: light", "Scanlines: strong",
              "Scanlines: OSSC", "Scanlines: dark", "xBRZ", "ScaleFX", "Your own", "CRT Newpixie"}},
            {"scanline_direction", "Display", "Scanline direction",
             "Which way a shader's lines run in a game made for a monitor on its side. Vertical: as on the cabinet, whose "
             "monitor was turned on its side, so its lines run up and down the picture (the shader draws the board's "
             "picture as it sends it, and that is then stood up). Horizontal: the picture stood up first and then shaded, "
             "so the lines run across the screen. A game made for an upright monitor (Akai Katana, Deathsmiles) is the "
             "same either way.",
             0, {"Vertical", "Horizontal"}},
            {"run_ahead", "Game", "Run-ahead",
             "Takes out lag the game itself has. These games show the result of a press some frames after it; with run-ahead "
             "the port runs the game that many frames further each frame, unseen, and shows that picture, so the screen answers "
             "sooner. The game plays exactly the same (the same frames, scores and slowdown); only the picture is ahead. "
             "Too many frames shows a jump when you press; the right number is the game's own lag. Costs processor time.",
             0, {"Off", "1 frame", "2 frames", "3 frames", "4 frames"}},
            {"input_lag", "Display", "Input lag readout",
             "At the bottom of the picture: how many frames the game itself takes to show a press (found in play, by running "
             "the next frames from one moment with the stick pushed and without, unseen, every few seconds), what run-ahead "
             "takes off that, the port's own time from a key or button going down to handing the frame with it to the screen, "
             "and the sum. Your screen's own delay comes on top. The game plays exactly the same with it on.",
             0, {}},
#ifdef __ANDROID__
            // (port/touch.h)
            {"touch_controls", "Controls", "Touch controls",
             "A stick and the game's buttons on the screen, with Coin, Start and the menu: beside the picture with the phone "
             "held sideways, under it held upright. They go when a gamepad or a key is used, and come back at a touch; with "
             "nothing touched for a few seconds they are no longer drawn. Off: never shown.",
             0, {"On (until a gamepad is used)", "Off"}},
#endif
            {"practice_point", kTrainingPage, "Practice point",
             "Any place in the game kept to come back to. Set the point (the \\ key, a gamepad's right shoulder) and the game "
             "as it is that instant is kept in memory; go back to it (Backspace, the left shoulder) and it is there again at "
             "once, as often as you like: a pattern, a boss, a whole stage from its start. Replay (the ] key, a click of the "
             "right stick) shows your last attempt again from the point, and the controls are yours where it ends. The buttons "
             "can be changed on the launcher's Controllers page.",
             0, {}},
            {"rewind", kTrainingPage, "Rewind",
             "Hold the rewind button (R, a gamepad's left trigger) and the game runs backwards through its last minute or "
             "so; let go and play on from there. For taking a pattern again from just before it went wrong.",
             0, {}},
            {"fast_forward", kTrainingPage, "Fast forward",
             "Hold the fast forward button (Tab, a gamepad's right trigger) and the game runs several times as fast, without "
             "its sound: for getting through to the part you want to practise.",
             0, {}},
            {"input_display", kTrainingPage, "Input display",
             "The stick and the buttons as the game reads them, drawn in the corner of the picture.", 0, {}},
            // (1 was On when this was on or off: a file's 1 is On still)
            {"speed_readout", kTrainingPage, "Speed readout",
             "How fast the game itself is going, in the corner: 100% at full speed, less where it slows down (the frames of "
             "the last second in which the game was ready with a new picture). Slowdown is part of these games' scoring and "
             "survival; this shows where it is. With the host's time: beside it, how long this machine took to make each "
             "frame over the last second, in milliseconds. A board's frame is about 16.7 ms: a machine near or over that "
             "is too slow for the game, whose slowdown is then the machine's and not the board's.",
             0, {"Off", "On", "On, with the host's time"}},
#ifndef SPELUNK_CAVE68K
            // (value 0 is on: the default, as the board is. The name was "board_slowdown" when it was off unless chosen;
            // a file that kept that one's 0 is not read as off now)
            {"slowdown", "Game", "Slowdown as the board",
             "The board's CPU waits on its memory when a busy scene touches more than its small cache holds, and the game "
             "slows down: the slowdown these games are played and scored with. On: that waiting is worked out (the "
             "SH7709S's cache, bus and pipeline, within 1.5% of a near cycle-accurate model of the chip) and the games slow "
             "where a board does. Off: the CPU never waits and busy scenes run at full speed, harder than the real thing; "
             "it takes less of the processor, for a slow device.",
             0, {"On", "Off"}},
#elif defined(SPELUNK_PGM)
            {"slowdown", "Game", "Slowdown as the board",
             "The board's CPU waits on the video chip's memory: a little at every touch, and much longer when it touches it "
             "while the chip is reading the text layer for the next line of the picture. Busy scenes take longer and the game "
             "slows where it does on a board. On: those waits are worked out (as the PGM core for MiSTer describes the chip). "
             "Off: the CPU never waits. Not yet compared with a board or that core running.",
             0, {"On", "Off"}},
#endif
            {"online_ping", "Online", "Ping",
             "In an online match, at the bottom of the picture: the time a packet takes to the other player's machine and back.",
             recomp::kFeatureEnhanced, {}},
            {"online_delay", "Online", "Input delay",
             "In an online match, at the bottom of the picture: how many frames each player's controls are held back (the "
             "host's choice, 2 unless said), which keeps the guesses rollback has to make few.",
             0, {}},
            {"online_hide_names", "Online", "Hide names",
             "In an online match, the two players' names are not shown at the bottom of the picture.", 0, {}},
            // (by name, after the ToolkitFeature list's: port/main.cpp)
            {"aspect", "Display", "Aspect",
             "The picture's shape. As the cabinet: the shape the game had on its 4:3 monitor (3:4 stood up). The PGM games "
             "(Dai-Ou-Jou, Ketsui, Espgaluda) draw 448 x 224 for that monitor, so their pixels are narrower than tall (Mazinger "
             "Z's 384 x 240 too); the other boards' pixels are square either way. Square pixels: every pixel of the board square, which shows the "
             "PGM games taller and narrower than the cabinet did. With Whole pixels, the picture's long side is a whole "
             "multiple of the board's pixels and the short side is made to the 4:3 shape.",
             0, {"As the cabinet (4:3)", "Square pixels"}},
            {"boot", "Game", "Start",
             "Straight to the title: when the game starts, and when it is reset, the board's checks, the warning and the logos "
             "are run unseen and unheard, as fast as the machine goes, and the picture comes with the title. Nothing of the "
             "game is changed, only not shown. The whole boot: shown as the board does it.",
             0, {"Straight to the title", "The whole boot"}},
            // (sound_filter: port/sound_filter.h, in the frontend; only what is heard, so it is each player's own online)
            {"sound_filter", "Sound", "Sound filter",
             "Softens the board's sound: what the sound chips hand over can be harsh in the treble, a hiss and grit that "
             "headphones and good speakers bring out (the board's own amplifier took some of it away). Soft takes off the "
             "harshest of it; Softer, more, for a warmer sound. Off: the sound as the chips make it. The game plays exactly "
             "the same either way.",
             0, {"Off", "Soft", "Softer"}},
        };
        return features;
    }
} // namespace

namespace {
    std::vector<recomp::FeatureInfo> &ProgramFeatures() {
        static std::vector<recomp::FeatureInfo> features;
        return features;
    }
} // namespace

void AddProgramFeatures(std::vector<recomp::FeatureInfo> features) {
    ProgramFeatures().insert(ProgramFeatures().end(), features.begin(), features.end());
}

const std::vector<recomp::FeatureInfo> &AllFeatures() {
    static const std::vector<recomp::FeatureInfo> all = [] {
        std::vector<recomp::FeatureInfo> list = ThisGame().features;
        list.insert(list.end(), ToolkitFeatures().begin(), ToolkitFeatures().end());
        list.insert(list.end(), ProgramFeatures().begin(), ProgramFeatures().end());
        return list;
    }();
    return all;
}

size_t FeatureIndex(ToolkitFeature feature) {
    // (by name: the list has features only some builds have, touch_controls on Android, slowdown not on Cave's 68000
    // board, so a place in it is not the enum's)
    static const char *const kNames[] = {"screen", "scaling", "shader", "scanline_direction", "run_ahead", "input_lag",
                                         "practice_point", "rewind", "fast_forward", "input_display", "speed_readout",
                                         "slowdown"};
    static_assert(sizeof kNames / sizeof kNames[0] == static_cast<size_t>(ToolkitFeature::Slowdown) + 1);
    static const std::vector<size_t> at = [] {
        std::vector<size_t> list;
        for (const char *name : kNames) {
            list.push_back(FeatureIndex(std::string(name)));
        }
        return list;
    }();
    return at[static_cast<size_t>(feature)];
}

size_t FeatureIndex(const std::string &name) {
    for (size_t i = 0; i < AllFeatures().size(); i++) {
        if (AllFeatures()[i].name == name) {
            return i;
        }
    }
    return AllFeatures().size();
}

} // namespace spelunk
