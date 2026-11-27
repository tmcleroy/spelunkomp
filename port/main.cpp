// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The game in a window: the board's picture each frame, the keyboard and gamepads as the cabinet's controls.
// What is the board's own way (its picture, its training tools' memory, its EEPROM and sound) is in port/hw_cv1000.h
// and port/hw_cave68k.h. A CV1000's picture is the native renderer's (port/gpu.h: the game's draw lists run on the
// GPU); --software shows the software blitter's through SDL's renderer instead.
//
//   <game> --roms SET.zip|FOLDER [--settings settings.ini] [--saves FOLDER] [--fullscreen]
// (the launcher starts it so; game.cfg is built in)
//
// Keys: arrows and Z X C V (player 1's stick and buttons), Enter start, 5 coin, F2 the board's test switch, F3 or 9
// service, Esc or F1 the in-game menu (save states, speed, features, coin, reset, quit; Tab too if no control has
// it), F5 / F9 save and load a state, F6 / F7 the slot before and after, P pause, O a frame on while paused (as do
// Tab and a gamepad's right trigger, fast forward's, while paused), - and = slower and faster, F11 or Alt+Enter full
// screen, F12 a screenshot (the saves folder's screenshots/). A key a control has been given (the launcher's
// Controllers page) does only that. Gamepads: the Controllers page says which is which player's (or Not used), the rest
// go, in the order they were connected, to the players without one; Guide or Start and Select together
// opens the menu and closes it (the launcher's Menu button can add the left stick's click, or leave only Start and
// Select: recomp-common/menu_buttons.h). The menu's own controls and the mouse: recomp-common/menu_sdl.h.
//
// Input lag (docs/LAG.md): the host's devices are read when the game reads its ports (once a frame, in its vertical
// sync interrupt), not at the top of the host's loop, and each frame is shown as soon as it is done.
#include "../machine/board.h"
#include "../machine/zip.h"
#include "../recomp-common/arcade_screen.h"
#include "../recomp-common/menu.h"
#include "../recomp-common/menu_layout.h"
#include "../recomp-common/menu_sdl.h"
#include "../recomp-common/menu_turn.h"
#include "../recomp-common/own_shaders.h"
#include "../runtime/board_settings.h"
#include "../runtime/dodge.h"
#include "../runtime/game.h"
#include "../runtime/hit_boxes.h"
#include "../runtime/romset.h"
#include "../runtime/state.h"
#include "bindings.h"
#include "build_note.h"
#include "crash_log.h"
#include "host_time.h"
#include "hud.h"
#include "i18n.h"
#include "online.h"
#include "png_write.h"
#include "shading.h"
#include "sound_filter.h"
#include "touch.h"
#ifdef __ANDROID__
#include "android.h"
#endif
#if defined(SPELUNK_PGM)
#include "hw_pgm.h"
#elif defined(SPELUNK_CAVE68K)
#include "hw_cave68k.h"
#else
#include "hw_cv1000.h"
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // (on Android the game is a library: its main is called by its activity, port/android.cpp)

#include <algorithm>
#include <deque>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <zlib.h>

using namespace spelunk;

namespace {

// The Alternative modes' Attract mode page (its Start: the port's own, every game's)
constexpr const char *kAttractPage = "Attract mode";

using port::Hw;

// The board's picture for an arcade monitor (recomp-common/arcade_sdl.h and arcade_screen.h): its columns and lines as
// it sends them, a game made for a monitor on its side too (the monitor is turned as the cabinet's was), and its frame
// rate. CV1000 320 x 240 at 60.02 Hz; Cave's 68000 board 320 x 240 (Mazinger Z 384) at 57.44 Hz; the PGM 448 x 224 at
// 59.19 Hz
const recomp::sdl::ArcadeBoard kArcadeBoard{Hw::kWidth, Hw::kHeight, 1e9 / static_cast<double>(Hw::kFrameTime.count())};

// The Arcade monitor page, the game program's own features (runtime/game.h: AddProgramFeatures), as cps2recomp has
// it: the resolution, the screen, the mode made at the board's rate and its monitor (where Switchres is built in:
// Windows) are set on the launcher, so a mode the monitor may not take is never chosen in the menu while playing
std::vector<recomp::FeatureInfo> ArcadeFeatures() {
    using namespace recomp::sdl;
    std::vector<recomp::FeatureInfo> list;
    list.push_back({"crt_resolution", "Arcade monitor", "Full screen resolution", ArcadeDescription(kArcadeBoard), kFeatureLauncherOnly,
                    {"Off", ArcadeBoardName(kArcadeBoard), "Super resolution"}});
    list.push_back({"crt_screen", "Arcade monitor", "Screen", kScreenDescription, kFeatureLauncherOnly, {"Main", "Second", "Third", "Fourth"}});
    if (SwitchresBuilt()) {
        list.push_back({"crt_switchres", "Arcade monitor", "Make the mode at the board's rate", SwitchresDescription(kArcadeBoard),
                        kFeatureLauncherOnly, {}});
        list.push_back({"crt_monitor", "Arcade monitor", "Monitor", kMonitorDescription, kFeatureLauncherOnly,
                        std::vector<std::string>(std::begin(kMonitorNames), std::end(kMonitorNames))});
    }
    // (off, the default: in full screen at an arcade mode the frames are paced by the screen's refreshes, with the
    // frame delay; on: by the board's clock, as in a window)
    list.push_back({"crt_board_pace", "Arcade monitor", "Keep the board's pace", BoardPaceDescription(kArcadeBoard), 0, {}});
    list.push_back({"crt_compact_menu", "Arcade monitor", "Compact menu", kCompactDescription, 0, {}});
    return list;
}

// The board's picture turned `degrees` clockwise and drawn `outW` x `outH` (each pixel the nearest board pixel's: the
// Aspect feature's shape can make the two directions' scales unlike)
void TurnTo(const uint32_t *in, int degrees, int outW, int outH, std::vector<uint32_t> &out) {
    const int w = Hw::kWidth, h = Hw::kHeight;
    const int tw = degrees % 180 ? h : w, th = degrees % 180 ? w : h;
    out.resize(static_cast<size_t>(outW) * outH);
    for (int oy = 0; oy < outH; oy++) {
        for (int ox = 0; ox < outW; ox++) {
            const int tx = static_cast<int>(static_cast<int64_t>(ox) * tw / outW), ty = static_cast<int>(static_cast<int64_t>(oy) * th / outH);
            int x = tx, y = ty;
            switch (degrees) {
            case 90: x = ty; y = h - 1 - tx; break;
            case 180: x = w - 1 - tx; y = h - 1 - ty; break;
            case 270: x = w - 1 - ty; y = tx; break;
            default: break;
            }
            out[static_cast<size_t>(oy) * outW + ox] = in[static_cast<size_t>(y) * w + x];
        }
    }
}

// ... at `scale` times its size in both directions
void Turn(const uint32_t *in, int degrees, int scale, std::vector<uint32_t> &out, int &outW, int &outH) {
    outW = (degrees % 180 ? Hw::kHeight : Hw::kWidth) * scale;
    outH = (degrees % 180 ? Hw::kWidth : Hw::kHeight) * scale;
    TurnTo(in, degrees, outW, outH, out);
}

// The game's hit boxes (Game::hitBoxes) drawn on the tools' picture: turned as the picture is, at its scale, each kind
// in the colour the Hit boxes page gives it (runtime/hit_boxes.h)
void MarkHitBoxes(port::Hud &hud, const Board &board, int degrees, const std::vector<int> &features) {
    // The boxes of the frame on show: the board shows a frame's picture some frames after the code that made it ran
    // (Hw::kPictureLag: a CV1000's draw list is drawn the frame after it is made and shown the frame after that), so the
    // boxes are kept by the board's frame and those of the frame the picture is from drawn (a frame not kept, after a
    // rewind or a state loaded: the boxes as they are now)
    struct Kept {
        uint64_t frame = ~0ull;
        std::vector<HitBox> boxes;
    };
    static Kept kept[8];
    Kept &now = kept[board.frame % 8];
    now.frame = board.frame;
    now.boxes.clear();
    ThisGame().hitBoxes(board, now.boxes);
    const Kept &then = kept[(board.frame - Hw::kPictureLag) % 8];
    const std::vector<HitBox> &boxes = then.frame == board.frame - Hw::kPictureLag ? then.boxes : now.boxes;
    const float w = Hw::kWidth, h = Hw::kHeight;
    // (the tools' picture is the picture's shape as shown: its scale across and down may differ, the Aspect feature)
    const float sx = hud.width / (degrees % 180 ? h : w), sy = hud.height / (degrees % 180 ? w : h);
    uint32_t fill[HitBox::kKinds], edge[HitBox::kKinds];
    bool shown[HitBox::kKinds];
    for (int k = 0; k < HitBox::kKinds; k++) {
        const size_t at = FeatureIndex(hitboxes::Kind(static_cast<HitBox::Kind>(k)).name);
        shown[k] = hitboxes::Style(static_cast<HitBox::Kind>(k), at < features.size() ? features[at] : 0, fill[k], edge[k]);
    }
    for (const HitBox &b : boxes) {
        if (b.kind >= HitBox::kKinds || !shown[b.kind]) {
            continue;
        }
        // (the box's corners where Turn puts the board's pixels)
        float x0 = b.x0, y0 = b.y0, x1 = b.x1, y1 = b.y1;
        switch (degrees) {
        case 90: x0 = h - b.y1; x1 = h - b.y0; y0 = b.x0; y1 = b.x1; break;
        case 180: x0 = w - b.x1; x1 = w - b.x0; y0 = h - b.y1; y1 = h - b.y0; break;
        case 270: x0 = b.y0; x1 = b.y1; y0 = w - b.x1; y1 = w - b.x0; break;
        default: break;
        }
        int px0 = static_cast<int>(std::floor(x0 * sx)), py0 = static_cast<int>(std::floor(y0 * sy));
        int px1 = std::max(px0 + 1, static_cast<int>(std::ceil(x1 * sx))), py1 = std::max(py0 + 1, static_cast<int>(std::ceil(y1 * sy)));
        if (px1 - px0 < 3 && py1 - py0 < 3) {
            // (a point: a dot a board pixel across about it, to be seen over the bullet)
            px0 = (px0 + px1) / 2 - 1;
            py0 = (py0 + py1) / 2 - 1;
            px1 = px0 + 3;
            py1 = py0 + 3;
        }
        uint32_t paint = fill[b.kind];
        if (b.kind == HitBox::Bullet && (px1 - px0 > 3 || py1 - py0 > 3)) {
            // (a bullet the game gives a box, not a point: see-through, the bullet to be seen in it)
            paint = (paint & 0x00FFFFFFu) | 0x70000000u;
        }
        hud.Mark(px0, py0, px1, py1, paint, edge[b.kind]);
    }
}

// The Display page's Shader's presets, in the order of its choices after Off (the folders and files of RetroArch's
// slang-shaders, copied to shaders/ beside the program by tools/shaders.py; cps2recomp's list, so the ports' menus
// match, with CRT Newpixie among the CRTs), then Your own (kYourOwnShader, runtime/own_shader.h), which has none. A
// choice is kept by name (kShaderIds there), so this order is free.
const char *const kShaderPresets[] = {
    "crt/crt-geom-mini.slangp", "crt/crt-lottes.slangp", "crt/crt-royale.slangp", "crt/crt-guest-advanced-fast.slangp",
    "crt/zfast-crt.slangp", "crt/newpixie-crt.slangp", "scanlines/scanline-fract.slangp",
    "pixel-art-scaling/sharp-bilinear-scanlines.slangp", "scanlines/ossc.slangp",
    "scanlines/integer-scaling-scanlines.slangp", "edge-smoothing/xbrz/xbrz-freescale-multipass.slangp",
    "edge-smoothing/scalefx/scalefx.slangp",
};
static_assert(std::size(kShaderPresets) + 2 == kShaderCount, "a preset for every choice but Off and Your own");

// Your own's presets (recomp-common/own_shaders.h): the shaders folder every game shares, in the launcher's folder
// (named so; the launcher says where in RECOMP_SHADERS), and this game's own, beside its settings.ini
constexpr const char *kLauncherName = "Spelunkomp";
struct OwnShaderFolders {
    std::filesystem::path game, shared;
};
OwnShaderFolders OwnShaderFoldersOf(const std::string &settingsPath) {
    if (settingsPath.empty()) {
        return {{}, recomp::shaders::SharedFolder({}, kLauncherName)}; // (RECOMP_SHADERS, if any)
    }
    const std::filesystem::path folder = recomp::shaders::PathOf(settingsPath).parent_path();
    return {recomp::shaders::GameFolder(folder), recomp::shaders::SharedFolder(folder, kLauncherName)};
}

// The preset file the Shader feature's choice `value` names; "" for none (Off; or Your own with none, and `why`).
// Your own: the player's preset named `own` ("": the first found), in this game's shaders folder or the shared one
std::string ShaderPreset(int value, const std::string &settingsPath, const std::string &own, std::string &why) {
    namespace fs = std::filesystem;
    std::error_code error;
    why.clear();
    if (value == kYourOwnShader) {
        const OwnShaderFolders folders = OwnShaderFoldersOf(settingsPath);
        const std::vector<recomp::shaders::Preset> found = recomp::shaders::Find(folders.game, folders.shared);
        const recomp::shaders::Preset *preset = recomp::shaders::Pick(found, own, why);
        if (!preset) {
            why += " (" + recomp::shaders::Utf8(folders.shared) + ")";
        }
        return preset ? recomp::shaders::Utf8(preset->file) : "";
    }
    const int entry = value - 1; // (kShaderPresets' entry)
    if (value < 1 || entry >= static_cast<int>(std::size(kShaderPresets))) {
        return "";
    }
    // (beside the program, or in a macOS app its Resources; SPELUNK_SHADERS for another folder)
    if (const char *folder = getenv("SPELUNK_SHADERS")) {
        return (fs::path(folder) / kShaderPresets[entry]).string();
    }
    const char *base = SDL_GetBasePath();
    const fs::path program = base ? fs::path(base) : fs::path(".");
    for (const fs::path &at : {program / "shaders", program / ".." / "Resources" / "shaders"}) {
        if (fs::exists(at / kShaderPresets[entry], error)) {
            return (at / kShaderPresets[entry]).string();
        }
    }
    return (program / "shaders" / kShaderPresets[entry]).string();
}

// What the frontend reads the host's devices into, when the game asks
struct Controls {
    Board *board = nullptr;
    const port::BoundTable *table = nullptr;
    const recomp::Bindings *bindings = nullptr;
    SDL_Gamepad *pads[recomp::kBindMaxPlayers] = {}; // (each player's, given out by the frontend; nullptr: none)
    port::TouchControls *touch = nullptr; // (a touch screen's: player 1's, beside the keys and player 1's gamepad)
    bool readThisFrame = false;
    bool coinHeld = false, testHeld = false;
    int coinFrames = 0, testFrames = 0;
    // (a press of the test switch is held at least this long: Guwange looks for it held about half a second, so a tap of
    // F2 or the menu's command, 10 frames, went unseen)
    static constexpr int kTestPressFrames = 60;
    bool nineService = true; // (9 is the service switch too, unless a control has been given it)
    uint32_t scripted[2] = {0, 0}, scriptedSystem = 0; // (checks: --press and --system)
    uint64_t reads = 0;
    bool attract = false; // (attract mode: the game is given no controls; the port's loop ends it at Start or a coin)

    // The controls given instead of the host's (a replay, a game run unseen): players 1 and 2, then the switches
    const uint32_t *forced = nullptr;
    // The features (autofire's, the Controls page: runtime/board_settings.h)
    const std::vector<int> *features = nullptr;
    // (forced controls as held: autofire put in as the frame reads them, as for the host's own. Run-ahead's kept frames)
    bool forcedAutofire = false;

    // The controls as Read would make them now, with nothing changed (the coin's pulse, the test switch's hold): the
    // players' as held (before autofire) and the switches. True when nothing but the players' sticks and buttons is
    // held or under way (no coin, Start, test or service): only then may run-ahead keep its frames
    bool Sample(uint32_t out[3]) const {
        SDL_PumpEvents();
        const bool *keys = SDL_GetKeyboardState(nullptr);
        if (touch && touch->Shown()) {
            return false; // (the touch screen's controls are read once a frame, a tap lasting some frames: not asked twice)
        }
        const uint32_t p1 = port::BoundBits(*table, *bindings, 0, keys, pads[0]);
        const uint32_t p2 = port::BoundBits(*table, *bindings, 1, keys, pads[1]);
        out[0] = (p1 & 0xFF) | scripted[0];
        out[1] = (p2 & 0xFF) | scripted[1];
        out[2] = scriptedSystem;
        const bool switches = ((p1 | p2) & (port::kStartBit | port::kCoinBit)) || coinHeld || coinFrames > 0 || testFrames > 0 ||
                              keys[SDL_SCANCODE_F2] || keys[SDL_SCANCODE_F3] || (nineService && keys[SDL_SCANCODE_9]);
        return !switches && out[2] == 0 && !attract;
    }

    void Read() {
        if (readThisFrame) {
            return;
        }
        readThisFrame = true;
        reads++;
        if (forced) {
            board->inputs[0] = forced[0];
            board->inputs[1] = forced[1];
            board->system = forced[2];
            if (forcedAutofire && features) {
                board->inputs[0] = Autofire(board->inputs[0], board->frame, *features);
                board->inputs[1] = Autofire(board->inputs[1], board->frame, *features);
            }
            return;
        }
        SDL_PumpEvents();
        const bool *keys = SDL_GetKeyboardState(nullptr);
        uint32_t p1 = port::BoundBits(*table, *bindings, 0, keys, pads[0]);
        if (touch) {
            touch->Feed(); // (the fingers as they are now, as the keys are: no frame later)
            p1 |= touch->Read(false);
        }
        const uint32_t p2 = port::BoundBits(*table, *bindings, 1, keys, pads[1]);
        board->inputs[0] = (p1 & 0xFF) | scripted[0];
        board->inputs[1] = (p2 & 0xFF) | scripted[1];
        // (autofire: the buttons it is on for let go on some frames while held, by the board's frame number, so a
        // replay of what was read, rewind and run-ahead's frames have the same)
        if (features) {
            board->inputs[0] = Autofire(board->inputs[0], board->frame, *features);
            board->inputs[1] = Autofire(board->inputs[1], board->frame, *features);
        }
        uint32_t system = scriptedSystem;
        system |= (p1 & port::kStartBit) ? kSysStart1 : 0;
        system |= (p2 & port::kStartBit) ? kSysStart2 : 0;
        // (a coin is a pulse a few frames long, however long the button is held)
        const bool coin = ((p1 | p2) & port::kCoinBit) != 0;
        if (coin && !coinHeld) {
            coinFrames = 4;
        }
        coinHeld = coin;
        if (coinFrames > 0) {
            coinFrames--;
            system |= kSysCoin1;
        }
        const bool test = keys[SDL_SCANCODE_F2];
        if (test && !testHeld) {
            testFrames = kTestPressFrames;
        }
        testHeld = test;
        if (test || testFrames > 0) {
            system |= kSysTest;
            testFrames -= testFrames > 0;
        }
        if (keys[SDL_SCANCODE_F3] || (nineService && keys[SDL_SCANCODE_9])) {
            system |= kSysService;
        }
        if (attract) {
            board->inputs[0] = board->inputs[1] = 0;
            system &= ~static_cast<uint32_t>(kSysStart1 | kSysStart2 | kSysCoin1);
        }
        board->system = system;
    }
};

} // namespace

int main(int argc, char **argv) {
#ifdef __ANDROID__
    port::StartOnAndroid();
#else
    port::KeepLogOnCrash(); // (the output written as it goes, so a crash keeps logs/game.log: port/crash_log.h)
    AddProgramFeatures(ArcadeFeatures()); // (not on a phone: it has no arcade monitor)
#endif
    std::string configPath, romFolder = "rom", savesFolder, settingsPath, forcedFeatures;
    std::string shaderFile; // (--shader FILE: that preset, whatever the Shader feature says)
    bool fullscreen = false, mute = false, softwareOnly = false, checkVideo = false, rollbackTest = false;
    uint64_t stopAfter = 0, frameCount = 0, netMenuHostAt = 0, beginNowAt = 0, attractAt = 0;
    // (checks: --real-time, a run of --frames paced as play is, with its sound; --speed, the Speed row's place;
    // --step-test FRAME:N, stopped at FRAME and then N frames on one at a time, then on as before)
    bool realTime = false;
    int speedAtStart = -1;
    uint64_t stepTestAt = 0, stepTestLeft = 0;
    std::map<uint64_t, std::string> shots, windowShots; // (--window-shot: the window's own pixels, as scaled)
    std::map<uint64_t, std::string> ramDumps;           // (--dump-ram FRAME:file: main RAM as the board holds it, for finding a game's facts)
    std::string windowShotNow, pictureLog; // (--picture-log FILE: a line per frame, the frame and a hash of the picture shown)
    std::string soundLog; // (--sound-log FILE: the sound as the board made it, each frame's, raw: checks)
    std::string startState; // (--load-state FILE: the board as a headless runner's state, at the start)
    std::string inputLog; // (--input-log FILE: a line per frame, the board's frame and the controls the game was given: checks)
    struct Held {
        uint64_t from, count;
        uint32_t bits;
        int player;
    };
    std::vector<Held> held;
    // (checks: --drag FRAME:COUNT:X0,Y0:X1,Y1[:HOLD], a finger on the touch screen put down at FRAME at X0,Y0 (the
    // window's width and height 1), moved evenly to X1,Y1 over COUNT frames, held there HOLD frames and lifted; each
    // frame's steering logged)
    struct Drag {
        uint64_t from, count, hold;
        float x0, y0, x1, y1;
    };
    std::vector<Drag> drags;
    struct MenuTest {
        uint64_t frame;
        std::string keys, shot;
    };
    std::vector<MenuTest> menuTests;
    port::OnlineOptions online; // (--host, --join, --net-*: port/online.h)
    for (int i = 1; i < argc; i++) {
        if (online.Take(argc, argv, i)) {
            continue;
        }
        const std::string a = argv[i];
        if (a == "--list-features") {
            if (!BoardSettingsProblem().empty()) {
                fprintf(stderr, "%s\n", BoardSettingsProblem().c_str()); // (game.cfg's board options: runtime/board_settings.h)
                return 1;
            }
            fputs(port::FeatureListFor(AllFeatures()).c_str(), stdout); // (the launcher's Features page, and its note)
            return 0;
        }
        if (a == "--game" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (a == "--roms" && i + 1 < argc) {
            romFolder = argv[++i];
        } else if (a == "--settings" && i + 1 < argc) {
            settingsPath = argv[++i];
        } else if (a == "--saves" && i + 1 < argc) {
            savesFolder = argv[++i];
        } else if (a == "--fullscreen") {
            fullscreen = true;
        } else if (a == "--mute") {
            mute = true;
        } else if (a == "--software") {
            softwareOnly = true; // (the software blitter's picture through SDL's renderer: the reference)
        } else if (a == "--check-video") {
            checkVideo = true; // (the software blitter run beside the GPU, and the two pictures compared each frame)
        } else if (a == "--frames" && i + 1 < argc) {
            stopAfter = strtoull(argv[++i], nullptr, 0); // (checks: quit after this many frames)
        } else if (a == "--shot" && i + 1 < argc) {
            const std::string v = argv[++i]; // FRAME:file.png (the picture as the window shows it)
            shots[strtoull(v.substr(0, v.find(':')).c_str(), nullptr, 0)] = v.substr(v.find(':') + 1);
        } else if (a == "--dump-ram" && i + 1 < argc) {
            const std::string v = argv[++i];
            ramDumps[strtoull(v.substr(0, v.find(':')).c_str(), nullptr, 0)] = v.substr(v.find(':') + 1);
        } else if (a == "--picture-log" && i + 1 < argc) {
            pictureLog = argv[++i];
        } else if (a == "--sound-log" && i + 1 < argc) {
            soundLog = argv[++i];
        } else if (a == "--load-state" && i + 1 < argc) {
            startState = argv[++i]; // (checks, profiling: the board from a state the headless runner saved, --save-state)
        } else if (a == "--input-log" && i + 1 < argc) {
            inputLog = argv[++i];
        } else if (a == "--window-shot" && i + 1 < argc) {
            const std::string v = argv[++i];
            windowShots[strtoull(v.substr(0, v.find(':')).c_str(), nullptr, 0)] = v.substr(v.find(':') + 1);
        } else if ((a == "--press" || a == "--system") && i + 1 < argc) {
            unsigned long long from = 0, count = 1;
            unsigned bits = 0;
            int player = 1;
            sscanf(argv[++i], "%llu:%llu:%i:%d", &from, &count, &bits, &player); // (checks: FRAME:COUNT:BITS[:PLAYER])
            held.push_back({from, count, bits, a == "--system" ? -1 : (player == 2 ? 1 : 0)});
        } else if (a == "--drag" && i + 1 < argc) {
            Drag d{};
            unsigned long long from = 0, count = 1, hold = 0;
            sscanf(argv[++i], "%llu:%llu:%f,%f:%f,%f:%llu", &from, &count, &d.x0, &d.y0, &d.x1, &d.y1, &hold);
            d.from = from;
            d.count = std::max(1ull, count);
            d.hold = hold;
            drags.push_back(d);
        } else if (a == "--menu" && i + 1 < argc) {
            const std::string v = argv[++i]; // FRAME:KEYS:shot.png (u d l r c b: the menu's buttons; 0-9 a finger's tap at that tenth of the way down), for checks
            const size_t c1 = v.find(':'), c2 = v.find(':', c1 + 1);
            menuTests.push_back({strtoull(v.substr(0, c1).c_str(), nullptr, 0), v.substr(c1 + 1, c2 - c1 - 1), v.substr(c2 + 1)});
        } else if (a == "--rollback-test") {
            rollbackTest = true; // (checks: every frame run first with other controls, and put back: online play's rollback)
        } else if (a == "--shader" && i + 1 < argc) {
            shaderFile = argv[++i]; // (a RetroArch shader preset, .slangp)
        } else if (a == "--net-menu-host" && i + 1 < argc) {
            netMenuHostAt = strtoull(argv[++i], nullptr, 0); // (tests: the menu's Host a match chosen at this frame)
        } else if (a == "--real-time") {
            realTime = true;
        } else if (a == "--speed" && i + 1 < argc) {
            speedAtStart = atoi(argv[++i]);
        } else if (a == "--step-test" && i + 1 < argc) {
            unsigned long long at = 0, count = 0;
            sscanf(argv[++i], "%llu:%llu", &at, &count);
            stepTestAt = at;
            stepTestLeft = count;
        } else if (a == "--attract" && i + 1 < argc) {
            attractAt = strtoull(argv[++i], nullptr, 0); // (tests: Attract mode's Start chosen at this frame)
        } else if (a == "--begin-now" && i + 1 < argc) {
            beginNowAt = strtoull(argv[++i], nullptr, 0); // (tests: the menu's Begin at the stage chosen at this frame)
        } else if (a == "--features" && i + 1 < argc) {
            forcedFeatures = argv[++i]; // name=N (or a choice by name),name,-name whatever settings.ini says (checks)
        }
    }
    if (port::kInterpretedBuild) {
        fprintf(stderr, "%s\n", port::kInterpretedLog); // (the game runs on the interpreters: port/build_note.h)
    }
    GameConfig config;
    std::string error;
    auto board = std::make_unique<Board>();
    Hw hw(board);
    bool ok = true;
    if (configPath.empty()) {
        ParseGameConfig(BuiltInGameConfig(), config);
    } else {
        ok = ReadGameConfig(configPath, config, error);
    }
    if (ok && !BoardSettingsProblem().empty()) {
        error = BoardSettingsProblem(); // (a declaration in game.cfg that could not be read: runtime/board_settings.h)
        ok = false;
    }
    if (!ok || !LoadSet(romFolder, config, *board, error)) {
        fprintf(stderr, "%s\n", error.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "spelunkomp", error.c_str(), nullptr);
        return 1;
    }
    const int rotate = atoi(config["rotate"].c_str());
    const std::string gameId = config.count("id") ? config["id"] : "cv1000";

    // The features, as settings.ini (from the launcher) has them
    std::vector<int> features(AllFeatures().size(), 0);
    if (!settingsPath.empty()) {
        recomp::Ini ini(settingsPath);
        ini.load();
        // (a file from before a choice was kept by name, the Shader's: made over once, recomp-common/settings.h)
        if (recomp::MigrateFeatures(ini, AllFeatures())) {
            ini.save();
        }
        features = recomp::FeatureValues(ini, AllFeatures());
    } else {
        for (size_t i = 0; i < features.size(); i++) {
            features[i] = (AllFeatures()[i].flags & recomp::kFeatureEnhanced) ? 1 : 0;
        }
    }
    // (name=N, or a choice by its name: shader=crt-newpixie)
    for (size_t from = 0; !forcedFeatures.empty() && from <= forcedFeatures.size();) {
        const size_t comma = forcedFeatures.find(',', from);
        std::string name = forcedFeatures.substr(from, comma == std::string::npos ? std::string::npos : comma - from);
        int value = 1;
        if (!name.empty() && name[0] == '-') {
            name.erase(0, 1);
            value = 0;
        }
        std::string text;
        if (const size_t equals = name.find('='); equals != std::string::npos) {
            text = name.substr(equals + 1);
            value = atoi(text.c_str());
            name.resize(equals);
        }
        if (const size_t at = FeatureIndex(name); at < features.size()) {
            const std::vector<std::string> &ids = AllFeatures()[at].ids;
            if (const auto id = std::find(ids.begin(), ids.end(), text); !text.empty() && id != ids.end()) {
                value = static_cast<int>(id - ids.begin());
            }
            features[at] = value;
        }
        if (comma == std::string::npos) {
            break;
        }
        from = comma + 1;
    }
    // The Renderer setting (Display page): the software model's picture in place of the GPU's. Read here once: the
    // window and the picture's whole path are made for one or the other, so a change in the menu waits for the next
    // start (said when it is made). --software chooses Software whatever the setting says
    const size_t rendererAt = FeatureIndex("renderer");
    const int rendererAtStart = rendererAt < features.size() ? features[rendererAt] : 0;
    if (rendererAtStart == 1 && !softwareOnly) {
        softwareOnly = true;
        printf("renderer: software (the Renderer setting)\n");
    } else if (softwareOnly) {
        printf("renderer: software (--software)\n");
    }
    fflush(stdout);

    // The Arcade monitor page's choices as the features have them now (none on a phone: all off)
    auto arcadeChoices = [&]() {
        auto value = [&](const char *name) {
            const size_t at = FeatureIndex(name);
            return at < features.size() ? features[at] : 0;
        };
        recomp::sdl::ArcadeChoices choices;
        choices.mode = static_cast<recomp::sdl::ArcadeMode>(std::clamp(value("crt_resolution"), 0, 2));
        choices.screen = std::clamp(value("crt_screen"), 0, 3);
        choices.switchres = value("crt_switchres") != 0;
        choices.monitor = std::clamp(value("crt_monitor"), 0, recomp::sdl::kMonitorPresetCount - 1);
        choices.boardPace = value("crt_board_pace") != 0;
        return choices;
    };
    // The arcade monitor (recomp-common/arcade_screen.h): the window's full screen in the board's mode, the frames
    // synced to the screen's refreshes there with a frame delay, and the desktop's mode put back at the end
    recomp::sdl::ArcadeScreen arcade(kArcadeBoard);

    // The EEPROM (the game's settings and scores): from the saves folder when there, written back when it changes
    const std::string eepromPath = savesFolder.empty() ? std::string() : savesFolder + "/eeprom.bin";
    if (!eepromPath.empty()) {
        std::vector<uint8_t> data;
        size_t size = 0;
        uint8_t *saved = hw.Saved(size);
        if (ReadWholeFile(eepromPath, data) && data.size() == size) {
            std::memcpy(saved, data.data(), size);
        }
    }
    // The Board page's settings (runtime/board_settings.h): put in over the board's own as the board is made, and the
    // board's own kept in the saves folder. A store the game has never written (a CV1000's EEPROM before the game's first
    // power on) has the game's first power on run unseen first, on a board of its own, so that the game's own settings
    // are there to be changed and the game does not write them over
    BoardSettingsInPlay boardSettings;
    boardSettings.Loaded(hw.Store());
    if (BoardSettingsInPlay::AnyChosen(features) && StoreBlank(hw.Store())) {
        auto first = std::make_unique<Board>();
        std::string why;
        if (LoadSet(romFolder, config, *first, why)) {
            Hw firstHw(first);
            Hw::StartCpu(*first);
            // (written once what it holds has stood for two seconds: an EEPROM's every word, a battery RAM's settings,
            // the rest of it being the game's work)
            auto look = [&]() {
                const SettingsStore store = firstHw.Store();
                std::vector<uint32_t> now;
                if (store.words) {
                    now.assign(store.words, store.words + store.wordCount);
                } else {
                    for (const BoardSetting &setting : BoardSettings()) {
                        now.push_back(SettingValue(store, setting));
                    }
                }
                return now;
            };
            std::vector<uint32_t> before;
            uint64_t still = 0, ran = 0;
            auto written = [&]() {
                return !StoreBlank(firstHw.Store()) && std::any_of(before.begin(), before.end(), [](uint32_t v) { return v != 0; });
            };
            for (; ran < 3600 && (!written() || still < 120); ran++) {
                first->RunFrame();
                std::vector<uint32_t> now = look();
                still = now == before ? still + 1 : 0;
                before.swap(now);
            }
            size_t size = 0, firstSize = 0;
            uint8_t *saved = hw.Saved(size);
            const uint8_t *firstSaved = firstHw.Saved(firstSize);
            if (written() && firstSize == size) {
                std::memcpy(saved, firstSaved, size);
                printf("board settings: the game's own written at its first power on (%llu frames, unseen)\n", static_cast<unsigned long long>(ran));
            }
        }
        boardSettings.Loaded(hw.Store());
    }
    {
        SettingsStore store = hw.Store();
        boardSettings.Apply(store, features);
    }
    bool netGuest = false; // (in another's online match: the EEPROM is the host's, never written here)
    auto saveKept = [&]() {
        if (eepromPath.empty() || !hw.SavedChanged() || netGuest) {
            return;
        }
        const std::string temp = eepromPath + ".new";
        if (FILE *f = fopen(temp.c_str(), "wb")) {
            size_t size = 0;
            hw.Saved(size);
            // (the board's own settings where the Board page's were put in)
            const std::vector<uint8_t> kept = boardSettings.Kept(hw.Store());
            const bool wrote = kept.size() == size && fwrite(kept.data(), 1, size, f) == size;
            fclose(f);
            if (wrote && std::rename(temp.c_str(), eepromPath.c_str()) == 0) {
                hw.SavedChanged() = false;
            }
        }
    };
    Hw::StartCpu(*board);

#ifdef __ANDROID__
    // (a game made for a monitor on its side may be played with the phone upright too, its controls under the picture:
    // port/touch.h; the app's own hint, AndroidManifest.xml, is sideways only)
    SDL_SetHintWithPriority(SDL_HINT_ORIENTATIONS, rotate % 180 ? "Portrait LandscapeLeft LandscapeRight" : "LandscapeLeft LandscapeRight",
                            SDL_HINT_OVERRIDE);
#endif
    // (the sound device asked for small buffers, as cps2recomp asks: little waits there; docs/LAG.md's Sound)
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
#if defined(__linux__) && !defined(__ANDROID__)
    // (started by Steam, as on a Steam Deck in Game Mode: the Deck's own controller through Steam Input's pad, not SDL's
    // HIDAPI driver for it, which every 200 updates feeds the controller's watchdog with feature reports that wait on
    // the device: a frame of about 8 ms in every 50 or 100, issue #18. Steam Input has the controls, Guide and the
    // paddles as it is set up; the port uses no gyro or rumble. SDL_JOYSTICK_HIDAPI_STEAMDECK set by hand wins)
    if ((getenv("SteamGameId") || getenv("SteamAppId")) && !getenv(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK)) {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK, "0");
    }
    if (const char *deck = SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK)) {
        printf("input: SDL_JOYSTICK_HIDAPI_STEAMDECK=%s%s\n", deck, getenv(SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK) ? " (set by hand)" : " (started by Steam)");
    }
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    // The menu's language (port/i18n.h): RECOMP_LANGUAGE, which the launcher sets to the one chosen there, else the
    // system's
    port::AddTranslations();
    {
        std::vector<std::string> preferred = recomp::i18n::FromEnvironment();
        int count = 0;
        if (SDL_Locale **locales = SDL_GetPreferredLocales(&count)) {
            for (int i = 0; i < count; i++) {
                preferred.push_back(std::string(locales[i]->language ? locales[i]->language : "") +
                                    (locales[i]->country ? std::string("-") + locales[i]->country : std::string()));
            }
            SDL_free(locales);
        }
        recomp::i18n::SetLanguage(recomp::i18n::Resolve("", preferred));
    }
    const std::string title = config.count("title") ? config["title"] : "spelunkomp";
    // The window; an OpenGL window for a shader (port/shading.h: nothing else draws to it), where it was and as large
    auto makeWindow = [&](bool opengl, int x, int y, int w, int h, bool full) {
        const SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, w);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, h);
        // (an arcade resolution: made a window, given its mode, then made full screen)
        const recomp::sdl::ArcadeChoices choices = arcadeChoices();
        const bool fullNow = full && choices.mode == recomp::sdl::ArcadeMode::Off;
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (fullNow ? SDL_WINDOW_FULLSCREEN : 0) |
                                  (opengl ? SDL_WINDOW_OPENGL : 0) | (getenv("SPELUNK_HIDDEN_WINDOW") ? SDL_WINDOW_HIDDEN : 0));
        SDL_Window *made = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);
#ifndef __ANDROID__
        if (made) {
            arcade.SetFullscreen(made, choices, full);
            if (full && !fullNow) {
                SDL_SetWindowFullscreen(made, true);
                SDL_SyncWindow(made);
            }
        }
#endif
        return made;
    };
    // The Shader feature's preset (or --shader's), "" for none or when librashader is not there to draw it
    const size_t shaderAt = FeatureIndex(ToolkitFeature::Shader), linesAt = FeatureIndex(ToolkitFeature::ScanlineDirection);
    const bool canShade = strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    // (Your own's preset, by name: settings.ini's [shader] own, which the launcher and the menu choose; and why a
    // preset could not be drawn, to be said over the picture and in the menu, as well as in the log)
    std::string ownShader;
    if (!settingsPath.empty()) {
        recomp::Ini ini(settingsPath);
        ini.load();
        ownShader = recomp::shaders::Chosen(ini);
    }
    std::string shaderProblem;
    auto presetNow = [&]() -> std::string {
        std::string why;
        std::string file = !canShade ? "" : !shaderFile.empty() ? shaderFile : ShaderPreset(features[shaderAt], settingsPath, ownShader, why);
        if (!why.empty()) {
            fprintf(stderr, "shader: Your own: %s; the picture is drawn without it\n", why.c_str());
            shaderProblem = recomp::i18n::TrFormat("Your own shader: {}", {why});
        }
        if (!file.empty() && !port::Shading::Available(why)) {
            fprintf(stderr, "shader: %s; the picture is drawn without it\n", why.c_str());
            shaderProblem = recomp::i18n::TrFormat("No shader: {}", {why});
            file.clear();
        }
        return file;
    };
    std::string preset = presetNow();
    int shaderValue = features[shaderAt];
    std::string shaderOwn = ownShader; // (the name the preset was found by)
    // The picture as the window is to show it: the Screen feature's turn. With an arcade monitor's resolution chosen,
    // never stood up: the picture as the board sends it, its lines the monitor's, for a monitor on its side as the
    // cabinet's was (As the cabinet, top to the right: the half turn, for one turned the other way)
    auto turnNow = [&]() {
        const int screen = features[FeatureIndex(ToolkitFeature::Screen)];
        if (arcadeChoices().mode != recomp::sdl::ArcadeMode::Off) {
            return screen == 2 ? 180 : 0;
        }
        return screen == 1 ? 0 : screen == 2 ? 180 : rotate;
    };
    // The Menu orientation feature's turn of the menu and the tools' writing on the window, in degrees clockwise
    // (recomp-common/menu_turn.h): Auto, as the game's picture stands, so with the picture shown as the board sends it
    // for a monitor on its side they turn with it; Upright, never turned (as before); or a turn chosen
    const size_t menuTurnAt = FeatureIndex("menu_orientation");
    auto menuTurnNow = [&]() { return recomp::turn::MenuTurn(menuTurnAt < features.size() ? features[menuTurnAt] : 0, turnNow(), rotate); };
    // ... and its shape, in board pixels on its long side (the other side as the shape makes it): the Aspect feature's.
    // As the cabinet: the 4:3 of the board's monitor (3:4 turned), whatever the board's own pixel count (a PGM's 448 x
    // 224 was drawn across a 4:3 monitor, its pixels narrower than tall); Square pixels: the board's pixels as they are
    const size_t aspectAt = FeatureIndex("aspect");
    auto shapeNow = [&](float &sw, float &sh) {
        const bool sideways = turnNow() % 180 != 0;
        const float pw = sideways ? Hw::kHeight : Hw::kWidth, ph = sideways ? Hw::kWidth : Hw::kHeight;
        if (aspectAt < features.size() && features[aspectAt] == 1) {
            sw = pw;
            sh = ph;
        } else if (pw >= ph) {
            sw = pw;
            sh = std::round(pw * (sideways ? 4.0f / 3 : 3.0f / 4));
        } else {
            sh = ph;
            sw = std::round(ph * (sideways ? 3.0f / 4 : 4.0f / 3));
        }
    };
    // (the tools' writing and the menu are drawn on a picture of that shape, three times the board's size: their words
    // keep their own shape, whatever the picture's)
    auto canvasSize = [&](int &w, int &h) {
        float sw = 0, sh = 0;
        shapeNow(sw, sh);
        w = static_cast<int>(sw) * port::Hud::kScale;
        h = static_cast<int>(sh) * port::Hud::kScale;
    };
    // The window's size for that shape (in the display's points): `height` tall if given and that fits, else the most
    // of the display it may take, kept to the shape
    auto windowSizeFor = [&](int height, int &w, int &h) {
        float sw = 0, sh = 0;
        shapeNow(sw, sh);
        SDL_Rect usable{0, 0, 1280, 800};
        SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable);
        const float most = std::min(usable.w * 0.9f / sw, usable.h * 0.9f / sh);
        const float scale = std::min(height > 0 ? height / sh : 960.0f / std::max(sw, sh), most);
        w = static_cast<int>(std::lround(sw * scale));
        h = static_cast<int>(std::lround(sh * scale));
    };
    int startW = 720, startH = 960;
    windowSizeFor(0, startW, startH);
#ifdef __ANDROID__
    fullscreen = true; // (a phone: its one screen, all of it, the system's bars hidden)
#endif
    // (on the Arcade monitor page's screen, where another than the main one is chosen)
    int startX = SDL_WINDOWPOS_UNDEFINED, startY = SDL_WINDOWPOS_UNDEFINED;
#ifndef __ANDROID__
    if (const int screen = arcadeChoices().screen; screen > 0) {
        startX = startY = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(recomp::sdl::ChosenDisplay(screen)));
    }
#endif
    SDL_Window *window = makeWindow(!preset.empty(), startX, startY, startW, startH, fullscreen);
    if (!window) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    // The controls on a touch screen (port/touch.h), drawn over the whole window
    port::TouchControls touch(config.count("buttons") ? config["buttons"] : "");
    std::vector<uint32_t> touchPicture;
    SDL_Texture *touchTexture = nullptr; // (with SDL's renderer)
    const size_t touchAt = FeatureIndex("touch_controls");
    // The picture: the native renderer, or (--software, or no GPU that will do) the software blitter's through SDL's
    // renderer; or either's through a shader preset (port/shading.h), the native renderer then with no window of its
    // own. SDL is shut down after the renderer has let go of its device: `hw` (and its renderer) was made before this
    // and so would be destroyed after it, once SDL_Quit had unloaded the Vulkan driver it calls (a crash at every
    // quit on Linux; Metal is never unloaded)
    struct QuitSdl {
        Hw &hw;
        ~QuitSdl() {
            hw.gpu.Shutdown();
            SDL_Quit();
        }
    } quitSdl{hw};
    const bool useGpu = hw.Init(preset.empty() ? window : nullptr, softwareOnly, checkVideo);
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    int textureW = 0, textureH = 0;
    std::unique_ptr<port::Shading> shading;
    bool overlayStale = false; // (the shader's overlay to be given the tools' writing again)
    bool rendererVsync = false; // (SDL's renderer waits for the screen's refresh: setVsync)
    // The window made again, for OpenGL or not, where it is and as large
    auto remakeWindow = [&](bool opengl) {
        int x = 0, y = 0, w = 0, h = 0;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_DestroyWindow(window);
        window = makeWindow(opengl, x, y, w, h, full);
        return window != nullptr;
    };
    // The picture's way into the window: through `preset` if there is one and it can be made (else as without one, in
    // a window that is not OpenGL's), or the GPU's own, or SDL's renderer
    auto openPresenter = [&]() {
        if (!preset.empty()) {
            auto made = std::make_unique<port::Shading>();
            std::string why;
            if (made->Open(window, why) && made->Load(preset, why)) {
                made->framesPerSecond = 1e9f / static_cast<float>(Hw::kFrameTime.count());
                shading = std::move(made);
                overlayStale = true;
                return true;
            }
            fprintf(stderr, "shader %s: %s; the picture is drawn without it\n", preset.c_str(), why.c_str());
            shaderProblem = recomp::i18n::TrFormat("The shader {} didn't load: {}", {recomp::shaders::Utf8(recomp::shaders::PathOf(preset).filename()), why});
            made.reset();
            preset.clear();
            if (!remakeWindow(false)) {
                return false;
            }
        }
        if (useGpu) {
            return hw.SetWindow(window);
        }
        renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer) {
            SDL_SetRenderVSync(renderer, 0); // (a frame is shown when it is done, not held for the display)
            rendererVsync = false;
        }
        return renderer != nullptr;
    };
    auto closePresenter = [&]() {
        shading.reset();
        touch.Stale(); // (the next presenter is given the controls' picture again)
        if (touchTexture) {
            SDL_DestroyTexture(touchTexture);
            touchTexture = nullptr;
        }
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        hw.SetWindow(nullptr);
    };
    if (!openPresenter()) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    // Whether each picture shown waits for the screen's refresh: only paced by the screen, in full screen at an arcade
    // monitor's mode (recomp-common/arcade_screen.h); otherwise shown the moment it is done (docs/LAG.md)
    auto setVsync = [&](bool on) {
        if (shading) {
            shading->SetVsync(on);
        } else if (useGpu) {
            hw.gpu.SetVsync(on);
        } else if (renderer && on != rendererVsync) {
            SDL_SetRenderVSync(renderer, on ? 1 : 0);
            rendererVsync = on;
        }
    };
    hw.Attach();
    hw.SelfTest();
    // The Shader feature changed in the menu: another preset in the same window, or the window made again for OpenGL
    // or without it
    bool running = true;
    auto followShader = [&]() {
        // (or Your own's preset chosen by another name, while it is chosen)
        if ((features[shaderAt] == shaderValue && (shaderValue != kYourOwnShader || ownShader == shaderOwn)) || !shaderFile.empty() ||
            !canShade) {
            return;
        }
        shaderValue = features[shaderAt];
        shaderOwn = ownShader;
        const std::string next = presetNow();
        if (next == preset) {
            return;
        }
        if (!next.empty() && shading) {
            std::string why;
            if (shading->Load(next, why)) {
                preset = next;
            } else {
                fprintf(stderr, "shader %s: %s; the one before is kept\n", next.c_str(), why.c_str());
                while (!why.empty() && why.back() == '.') {
                    why.pop_back(); // (librashader's ends with one)
                }
                shaderProblem = recomp::i18n::TrFormat("The shader {} didn't load: {}. The one before is kept.",
                                                       {recomp::shaders::Utf8(recomp::shaders::PathOf(next).filename()), why});
            }
            return;
        }
        closePresenter();
        preset = next;
        if (!remakeWindow(!preset.empty()) || !openPresenter()) {
            fprintf(stderr, "SDL: %s\n", SDL_GetError());
            running = false;
        }
    };
    // Where in the window the picture goes, in the shape shapeNow gives. The Scaling feature: 0 the largest whole
    // multiple that fits, 1 as large as fits, 2 up a fixed multiple. A multiple is of the shape's long side, which is
    // the board's own pixels (with the PGM's 4:3 the short side is then stretched to the shape, its pixels whole
    // where the multiple is even); a board whose pixels are square is a whole multiple both ways
    auto place = [&]() {
        // (full screen at an arcade monitor's mode: the picture fills it, the monitor giving it its shape; a mode of
        // more lines than the board's, a super resolution of 240 for the PGM's 224, has them one for one in its middle)
        if (arcade.OnArcade(window)) {
            int ow = 0, oh = 0;
            SDL_GetWindowSizeInPixels(window, &ow, &oh);
            return recomp::sdl::ArcadeLines(SDL_FRect{0, 0, static_cast<float>(ow), static_cast<float>(oh)}, oh, Hw::kHeight);
        }
        const int scaling = features[FeatureIndex(ToolkitFeature::Scaling)];
        float pw = 0, ph = 0;
        shapeNow(pw, ph);
        int ow = 0, oh = 0; // (the window in real pixels: a whole multiple is of those)
        SDL_GetWindowSizeInPixels(window, &ow, &oh);
        const float fit = std::min(ow / pw, oh / ph);
        const float whole = std::max(1.0f, std::floor(fit));
        const float scale = scaling == 1 ? fit : scaling >= 2 ? std::min(static_cast<float>(scaling - 1), whole) : whole;
        const float w = std::round(pw * scale), h = std::round(ph * scale);
        // (upright with the touch controls, at the top: they go under it)
        return SDL_FRect{std::floor((ow - w) / 2), touch.WantsPictureAtTop(ow, oh) ? 0.0f : std::floor((oh - h) / 2), w, h};
    };
    // Following the finger (port/touch.h): player 1's ship where the game's hit boxes put it, turned upright as the
    // cabinet stood it (the stick's up its up); the nearest to where it was last, player 2's being there too
    const size_t touchMovementAt = FeatureIndex("touch_movement");
    {
        // (a board pixel's place, or a step's, turned by `degrees` as Turn turns the picture: its middle kept if `step`)
        auto turned = [](int degrees, float x, float y, bool step, float &tx, float &ty) {
            const float w = step ? 0 : Hw::kWidth, h = step ? 0 : Hw::kHeight;
            switch (((degrees % 360) + 360) % 360) {
            case 90: tx = h - y; ty = x; break;
            case 180: tx = w - x; ty = h - y; break;
            case 270: tx = y; ty = w - x; break;
            default: tx = x; ty = y; break;
            }
        };
        port::TouchControls::Ship ship;
        ship.where = [&board, rotate, turned](float &x, float &y) {
            if (!ThisGame().hitBoxes) {
                return false;
            }
            static std::vector<HitBox> boxes;
            static float lastX = -1e9f, lastY = -1e9f;
            boxes.clear();
            ThisGame().hitBoxes(*board, boxes);
            bool found = false;
            float best = 0;
            for (const HitBox &b : boxes) {
                if (b.kind != HitBox::Ship) {
                    continue;
                }
                float bx = 0, by = 0;
                turned(rotate, (b.x0 + b.x1) / 2, (b.y0 + b.y1) / 2, false, bx, by);
                const float d = std::hypot(bx - lastX, by - lastY);
                if (!found || d < best) {
                    x = bx;
                    y = by;
                    best = d;
                }
                found = true;
                if (lastX < -1e8f) {
                    break; // (none before: player 1's, the first)
                }
            }
            lastX = found ? x : -1e9f;
            lastY = found ? y : -1e9f;
            return found;
        };
        // (a step across the window: in the picture's pixels as shown, back to the board's, then turned as the cabinet)
        ship.fromWindow = [&, turned](float dx, float dy, float &x, float &y) {
            const SDL_FRect at = place();
            const int shown = turnNow();
            const bool sideways = shown % 180 != 0;
            const float pw = sideways ? Hw::kHeight : Hw::kWidth, ph = sideways ? Hw::kWidth : Hw::kHeight;
            const float ux = at.w > 0 ? dx * pw / at.w : 0, uy = at.h > 0 ? dy * ph / at.h : 0;
            turned(rotate - shown, ux, uy, true, x, y);
        };
        touch.SetShip(std::move(ship));
    }
    // The touch controls laid out for the window and the picture's place in it, and their picture handed to whichever
    // draws the window, when it changed
    auto touchLayer = [&]() {
        touch.Allow(touchAt >= features.size() || features[touchAt] != 1);
        touch.Follow(touchMovementAt >= features.size() || features[touchMovementAt] == 0);
        int ow = 0, oh = 0;
        SDL_GetWindowSizeInPixels(window, &ow, &oh);
        touch.Layout(ow, oh, place());
        int tw = 0, th = 0;
        if (!touch.TakePicture(touchPicture, tw, th)) {
            return;
        }
        const uint32_t *pixels = touchPicture.empty() ? nullptr : touchPicture.data();
        if (shading) {
            shading->SetWindowLayer(pixels, tw, th);
        } else if (useGpu) {
            hw.gpu.SetWindowLayer(pixels, tw, th);
        } else if (renderer) {
            if (touchTexture) {
                SDL_DestroyTexture(touchTexture);
                touchTexture = nullptr;
            }
            if (pixels && (touchTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, tw, th))) {
                SDL_SetTextureBlendMode(touchTexture, SDL_BLENDMODE_BLEND);
                SDL_UpdateTexture(touchTexture, nullptr, pixels, tw * 4);
            }
        }
    };
    // The Screen or Aspect feature changed in the menu: a window (not full screen, not maximised) made the new shape,
    // as tall as it was (or as large as the display allows)
    int shapeTurn = turnNow(), shapeAspect = aspectAt < features.size() ? features[aspectAt] : 0;
    auto followShape = [&]() {
        const int turn = turnNow(), aspect = aspectAt < features.size() ? features[aspectAt] : 0;
        if (turn == shapeTurn && aspect == shapeAspect) {
            return;
        }
        shapeTurn = turn;
        shapeAspect = aspect;
        if (SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED)) {
            return; // (full screen: the picture placed in it anew, as large as Scaling allows)
        }
        int w = 0, h = 0, x = 0, y = 0;
        SDL_GetWindowSize(window, &w, &h);
        SDL_GetWindowPosition(window, &x, &y);
        int nw = w, nh = h;
        windowSizeFor(h, nw, nh);
        if (nw != w || nh != h) {
            SDL_SetWindowSize(window, nw, nh);
            SDL_SetWindowPosition(window, x + (w - nw) / 2, y);
        }
    };
    // (--window-shot with a shader: the window as it was shown, read back)
    auto shadedShot = [&]() {
        std::vector<uint32_t> pixels;
        int w = 0, h = 0;
        if (shading->ReadBack(pixels, w, h)) {
            for (uint32_t &p : pixels) {
                p |= 0xFF000000u;
            }
            WritePng(windowShotNow.c_str(), pixels.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        }
        shading->keepLast = false;
        windowShotNow.clear();
    };
    // A picture made here (the software blitter's frame, the menu) shown; with a shader, not through it (the menu)
    auto show = [&](const std::vector<uint32_t> &pixels, int w, int h) {
        touchLayer();
        const SDL_FRect to = place();
        if (shading) {
            shading->keepLast = !windowShotNow.empty();
            shading->Show(pixels.data(), w, h, to, false, 0, false);
            if (!windowShotNow.empty()) {
                shadedShot();
            }
            return;
        }
        if (useGpu) {
            hw.PresentPicture(pixels.data(), w, h, to);
            return;
        }
        if (!texture || w != textureW || h != textureH) {
            if (texture) {
                SDL_DestroyTexture(texture);
            }
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
            textureW = w;
            textureH = h;
        }
        SDL_UpdateTexture(texture, nullptr, pixels.data(), w * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, &to);
        if (touchTexture) {
            SDL_RenderTexture(renderer, touchTexture, nullptr, nullptr);
        }
        if (!windowShotNow.empty()) {
            if (SDL_Surface *got = SDL_RenderReadPixels(renderer, nullptr)) {
                if (SDL_Surface *rgba = SDL_ConvertSurface(got, SDL_PIXELFORMAT_ABGR8888)) {
                    std::vector<uint32_t> out(static_cast<size_t>(rgba->w) * rgba->h);
                    for (int y = 0; y < rgba->h; y++) {
                        std::memcpy(&out[static_cast<size_t>(y) * rgba->w], static_cast<const uint8_t *>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch,
                                    static_cast<size_t>(rgba->w) * 4);
                    }
                    WritePng(windowShotNow.c_str(), out.data(), static_cast<uint32_t>(rgba->w), static_cast<uint32_t>(rgba->h));
                    SDL_DestroySurface(rgba);
                }
                SDL_DestroySurface(got);
            }
            windowShotNow.clear();
        }
        SDL_RenderPresent(renderer);
    };
    auto readScreen = [&](uint32_t *out) { hw.ReadScreen(out); };

    // Sound: the chip's one channel at its own rate (the phrases' rate: SDL resamples for the device)
    SDL_AudioStream *audio = nullptr;
    uint32_t audioRate = 0;
    const bool audioLog = getenv("SPELUNK_AUDIO_LOG") != nullptr;
    // (the Sound page's Sound filter, port/sound_filter.h: on what is played only, as it is handed to SDL)
    port::SoundFilter soundFilter;
    const size_t soundFilterAt = FeatureIndex("sound_filter");
    auto openAudio = [&]() {
        if (mute || (stopAfter && !realTime)) {
            return;
        }
        audioRate = hw.SampleRate();
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 1, static_cast<int>(audioRate)};
        if (audio) {
            SDL_SetAudioStreamFormat(audio, &spec, nullptr);
        } else if ((audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr))) {
            SDL_ResumeAudioStreamDevice(audio);
            if (audioLog) {
                SDL_AudioSpec device{};
                int frames = 0;
                SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio), &device, &frames);
                fprintf(stderr, "audio: device %d Hz, %d frames a buffer (%.1f ms), %s; the game's sound %u Hz\n", device.freq, frames,
                        device.freq ? frames * 1000.0 / device.freq : 0.0, SDL_GetCurrentAudioDriver(), audioRate);
                SDL_SetAudioStreamGain(audio, 0.0f); // (measured, not heard)
            }
        }
    };
    // What waits to be played when a frame's sound is handed over (in the device's seconds), and the trim to the
    // stream's rate that keeps it at kAudioWaiting: docs/LAG.md's Sound
    struct AudioLog {
        double least = 1, most = 0, sum = 0;
        int frames = 0, dropped = 0, empty = 0;
    } audioSeen;
    int audioSteady = 0; // (frames in a row whose sound was handed over: an empty queue after many is a gap heard)
    openAudio();

    // ---- The training tools: the practice point and rewind (the board's own way of keeping a moment) ----
    hw.MakeTools();
    auto &point = hw.point;
    auto &timeline = hw.timeline;
    constexpr uint32_t kMomentFrames = 6; // (a moment of the timeline every tenth of a second)
    uint32_t toolsBefore = 0, rewindHeld = 0, rewoundMoments = 0;
    // The controls of each frame since the practice point was last set or gone back to, and of the attempt before
    // (replayed from the point: the controls are the player's again where it ends)
    struct FrameControls {
        uint32_t held[3];
    };
    std::vector<FrameControls> attempt, replay;
    size_t replayAt = 0;
    bool replaying = false;
    uint32_t unseenFrames = 0;
    uint8_t late[60] = {};
    uint32_t lateFrames = 0, lateAt = 0;
    FrameControls unseenControls{};
    port::Hud hud;
    const size_t hitBoxesAt = FeatureIndex("hit_boxes");

    std::string note;
    int noteFrames = 0;
    auto say = [&](const std::string &words) {
        note = words;
        noteFrames = 90;
    };
    int rendererNoted = rendererAtStart; // (the Renderer setting as last seen: a change in the menu is said once)
    const bool toolTimes = getenv("SPELUNK_TOOL_TIMES") != nullptr;
    auto timed = [&](const char *what, auto &&fn) {
        const auto t0 = std::chrono::steady_clock::now();
        fn();
        if (toolTimes) {
            fprintf(stderr, "tools: frame %llu: %s: %.2f ms (%zu moments, %.1f MB)\n", static_cast<unsigned long long>(frameCount), what,
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), timeline.Moments(),
                    timeline.Bytes() / 1048576.0);
        }
    };

    // ---- Run-ahead's frames kept (run-ahead 1 or 2, offline): the frames ahead are run as the game's own would be
    // (the sound chips running, the controls as held when they are run, their sound kept aside), each kept before it
    // is run (hw.NetKeep, in places 0 and 1 by turns), and the board left at the last. At the next frame, if the
    // controls are as the first of them was given and nothing else has a claim on the board, that frame is the game's
    // next and is taken as it is, with its sound; then only one frame more is run for the picture ahead: one frame run
    // for each shown, where run-ahead's frames were all run again each time. Otherwise the board is put back to the
    // game's own frame and run on as ever. The frames, the picture and the sound are the same either way
    // (SPELUNK_NO_AHEAD_KEEP=1: never kept). Online play has its own rollback, and run-ahead is off there
    struct AheadKept {
        struct Frame {
            int place;          // (the board as it was before this frame: hw.NetKeep's place)
            uint32_t held[3];   // (the controls it was given, as held)
            bool late;          // (it had no new picture: the speed readout's)
            std::vector<int16_t> sound;
        };
        std::deque<Frame> frames; // (the frames run ahead of the game's own, oldest first; the board after the last)
        bool taken = false;       // (the first was taken as the game's next frame: the frame part runs none)
        Frame took;               // (... that one)
        uint64_t kept = 0, again = 0;
    } aheadKept;
    // (the frames ahead let go, the board being made anew or given another state: nothing put back)
    auto aheadForget = [&]() {
        for (const AheadKept::Frame &f : aheadKept.frames) {
            hw.NetDrop(f.place);
        }
        aheadKept.frames.clear();
        aheadKept.taken = false;
    };
    // ---- Save states ----
    auto statePath = [&](int slot) { return (savesFolder.empty() ? std::string(".") : savesFolder) + "/state" + std::to_string(slot) + ".bin"; };
    const std::string stateHeader = Hw::kStateHeader + gameId;
    auto saveState = [&](int slot) {
        std::vector<uint8_t> data;
        Archive ar(data, true);
        uint64_t length = stateHeader.size();
        ar.Pod(length);
        std::string header = stateHeader;
        ar.Bytes(header.data(), header.size());
        hw.BeforeSave();
        board->Serialize(ar);
        const std::string path = statePath(slot), temp = path + ".new";
        FILE *f = fopen(temp.c_str(), "wb");
        if (!f) {
            return false;
        }
        const bool wrote = fwrite(data.data(), 1, data.size(), f) == data.size();
        fclose(f);
        return wrote && std::rename(temp.c_str(), path.c_str()) == 0;
    };
    auto loadState = [&](int slot) {
        aheadForget(); // (frames run ahead are not the state's)
        std::vector<uint8_t> data;
        if (!ReadWholeFile(statePath(slot), data)) {
            return false;
        }
        Archive ar(data, false);
        uint64_t length = 0;
        ar.Pod(length);
        std::string header(length < 256 ? length : 0, ' ');
        ar.Bytes(header.data(), header.size());
        if (!ar.Ok() || header != stateHeader) {
            return false;
        }
        // (kept as it is first, so a state that does not read leaves the game as it was)
        std::vector<uint8_t> before;
        Archive keep(before, true);
        hw.BeforeSave();
        board->Serialize(keep);
        board->Serialize(ar);
        if (!ar.Ok()) {
            Archive back(before, false);
            board->Serialize(back);
            return false;
        }
        hw.AfterLoad();
        hw.ToolsLost();
        return true;
    };

    // ---- Attract mode (the Alternative modes' Attract mode page, as cps2recomp's): the board started again and left to
    // run its own show, over and over, as a cabinet nobody is playing: no coin in (free play off), the game given no
    // controls, and the game's own features that change play (its Game and Training mode pages': free play, a start at
    // another stage, the cheats) off for it. Start or a coin ends it (the coin goes in), and those features are the
    // player's again. Nothing else is touched: the show is the board's, frame for frame. Not in an online match.
    bool attract = false;
    std::vector<int> attractOwn; // (the player's own values of the features attract mode turns off, given back at its end)
    auto attractOff = [&](size_t i) {
        return i < ThisGame().features.size() && (AllFeatures()[i].category == "Game" || AllFeatures()[i].category == kTrainingPage);
    };
    auto beginAttract = [&]() {
        attractOwn = features;
        for (size_t i = 0; i < features.size(); i++) {
            features[i] = attractOff(i) ? 0 : features[i];
        }
        attract = true;
    };
    auto endAttract = [&]() {
        if (!attract) {
            return;
        }
        for (size_t i = 0; i < features.size() && i < attractOwn.size(); i++) {
            features[i] = attractOff(i) ? attractOwn[i] : features[i];
        }
        attract = false;
    };

    // ---- The in-game menu (recomp-common/menu.h) ----
    recomp::Menu menu;
    int speed = recomp::layout::kFull; // (recomp::layout::kSpeeds' place: 1/8 to 8x)
    bool muted = mute;
    // (the Online page's Host a match, as cps2recomp's: the row that hosts, and while hosting the one that stops; the
    // menu made again with the other when that changes)
    recomp::MenuConfig menuConfig;
    const recomp::MenuCommand hostRow{"Online", "Host a match",
                                      "Others can join you online while you play on as you like (the menu, the training tools, "
                                      "save states): the match's code is at the bottom of the picture to give them, and with your "
                                      "name from the launcher's Online page it's in the lobby too. When someone joins, the game "
                                      "starts over for the match, with your board settings and Game page. This row again stops hosting."};
    const recomp::MenuCommand stopRow{"Online", "Stop hosting",
                                      "Others can no longer join: the match's code stops working. Your game goes on as it is."};
    // (and the Training mode page's Begin at the stage, the second command, where the game has it)
    const recomp::MenuCommand beginRow{kTrainingPage, "Begin at the stage",
                                       "A new game now, at the stage chosen below (Start at stage; Off: the first), from wherever the "
                                       "game is: the board starts again and is run on unseen, through a coin and Start, to that stage's "
                                       "beginning."};
    // (the Training mode page's Frame advance, and Attract mode's Start: after the game's own commands)
    const recomp::MenuCommand stepRow{kTrainingPage, "Frame advance",
                                      "Stops the game, for studying it a frame at a time: then O, Tab or a gamepad's right trigger "
                                      "goes on by one frame (held: frame after frame), and the frame's number is at the bottom of "
                                      "the picture. P, or this row again, starts it again. The game runs exactly the frames it "
                                      "would; the Speed row above slows it instead."};
    const recomp::MenuCommand attractRow{kAttractPage, "Start", "Starts attract mode: the game's own show, over and over. Start (or a coin) ends it."};
    bool hostWanted = false, attractWanted = false;
    int firstGameCommand = 0, stepCommand = -1, attractCommand = -1;
    auto setHostRow = [&](bool hosting) {
        menuConfig.commands = {hosting ? stopRow : hostRow};
        if (ThisGame().beginNow) {
            menuConfig.commands.push_back(beginRow);
        }
        // (then the game's own: an alternative mode begun from its page)
        firstGameCommand = static_cast<int>(menuConfig.commands.size());
        menuConfig.commands.insert(menuConfig.commands.end(), ThisGame().commands.begin(), ThisGame().commands.end());
        stepCommand = static_cast<int>(menuConfig.commands.size());
        menuConfig.commands.push_back(stepRow);
        attractCommand = static_cast<int>(menuConfig.commands.size());
        menuConfig.commands.push_back(attractRow);
        menu.Configure(menuConfig);
    };
    // The Display page's Your own preset, under the Shader (recomp-common/own_shaders.h): the presets in the shaders
    // folder every game shares and this game's own, found again each time the menu opens (ownMenuRow), so one put
    // there while the game runs is in the list; chosen by name ([shader] own), which chooses Your own too. The menu's
    // features are the game's with this one after the Shader: menuFeature turns the menu's place into the game's
    const size_t ownAt = shaderAt + 1;
    recomp::shaders::Choices ownChoices;
    auto menuFeature = [ownAt](size_t i) { return i < ownAt ? i : i - 1; };
    auto ownMenuRow = [&]() {
        const OwnShaderFolders folders = OwnShaderFoldersOf(settingsPath);
        ownChoices = recomp::shaders::MakeChoices(recomp::shaders::Find(folders.game, folders.shared), ownShader);
        menuConfig.features[ownAt].choices = ownChoices.labels;
        menuConfig.features[ownAt].description = recomp::shaders::MenuDescription(folders.shared, folders.game);
    };
    {
        recomp::MenuConfig &mc = menuConfig;
        for (const recomp::FeatureInfo &f : AllFeatures()) {
            mc.features.push_back({f.category, f.title, f.description, f.choices});
        }
        mc.features.insert(mc.features.begin() + static_cast<std::ptrdiff_t>(ownAt),
                           recomp::MenuFeature{AllFeatures()[shaderAt].category, recomp::shaders::kTitle, "", {}});
        ownMenuRow();
        // (in attract mode, the features it turns off are shown and set as the player's, for after it)
        auto setFeature = [&](size_t i, int v) {
            (attract && attractOff(i) ? attractOwn[i] : features[i]) = v;
            if (!settingsPath.empty()) {
                recomp::Ini ini(settingsPath);
                ini.load();
                recomp::SetFeature(ini, AllFeatures(), i, v);
                ini.save();
            }
        };
        mc.value = [&](size_t at) {
            if (at == ownAt) {
                return ownChoices.IndexOf(ownShader);
            }
            const size_t i = menuFeature(at);
            return attract && attractOff(i) ? attractOwn[i] : features[i];
        };
        mc.setValue = [&, setFeature](size_t at, int v) {
            if (at == ownAt) {
                ownShader = ownChoices.NameAt(v);
                if (!settingsPath.empty()) {
                    recomp::Ini ini(settingsPath);
                    ini.load();
                    recomp::shaders::Choose(ini, ownShader);
                    ini.save();
                }
                if (features[shaderAt] != kYourOwnShader) {
                    setFeature(shaderAt, kYourOwnShader);
                }
            } else {
                setFeature(menuFeature(at), v);
            }
            // (the Shader, or Your own's preset: drawn with at once, and a preset that won't load said here, in the menu)
            if (at == ownAt || menuFeature(at) == shaderAt) {
                followShader();
                if (!shaderProblem.empty()) {
                    menu.Note(shaderProblem);
                }
            }
        };
        // (of a feature of many choices, those the launcher lets the menu offer: the Shader's; and of one set on the
        // launcher (kFeatureLauncherOnly: an arcade monitor's), none but the one chosen and off, which the menu always
        // offers: the row can only be put back to off)
        {
            recomp::Ini ini(settingsPath);
            if (!settingsPath.empty()) {
                ini.load();
            }
            std::vector<std::vector<bool>> offered;
            for (const recomp::FeatureInfo &f : AllFeatures()) {
                offered.push_back(settingsPath.empty() ? std::vector<bool>() : recomp::MenuChoices(ini, f));
                if (f.flags & kFeatureLauncherOnly) {
                    offered.back().assign(std::max<size_t>(f.choices.size(), 2), false);
                }
            }
            mc.offered = [offered, ownAt, menuFeature](size_t at, int choice) {
                const size_t feature = menuFeature(at);
                return at == ownAt || feature >= offered.size() || choice < 0 || choice >= static_cast<int>(offered[feature].size()) ||
                       offered[feature][static_cast<size_t>(choice)];
            };
        }
        // (Alternative modes: a game's own modes' pages first (Dodge training), then Training mode, the hit boxes' page
        // under that)
        const bool dodges = FeatureIndex("dodge_fires") < AllFeatures().size();
        mc.pages.push_back({kModesPage, "",
                            dodges ? "Training mode's tools for practising a stage, a boss or a pattern, Dodge training: the game's own "
                                     "enemy fire, a pattern at a time, and attract mode."
                                   : "Training mode's tools for practising a stage, a boss or a pattern: a practice point, rewind, a "
                                     "start at any stage or its boss, a ship nothing can hit, hit boxes; and attract mode.",
                            true});
        if (dodges) {
            mc.pages.push_back({dodge::kPage, kModesPage,
                                "Your ship alone against the game's own enemy fire, a pattern at a time: begin it here, and choose what is fired.", false});
        }
        // (the Training mode page has the Speed row too, at its top: slow motion for study, beside Frame advance)
        mc.pages.push_back({kTrainingPage, kModesPage,
                            "The training tools: a practice point, rewind, slow motion and frame advance, a start at any stage or its boss, "
                            "a ship nothing can hit, hit boxes.",
                            true, {recomp::MenuRow::Speed}});
        mc.pages.push_back({hitboxes::kPage, kTrainingPage, "Hit boxes drawn over the picture: each kind on or off, and its colour.", false});
        mc.pages.push_back({kAttractPage, kModesPage,
                            "The game's own show on a loop, with no coin in: the logos, the title, the demonstrations of play and the rankings.", false});
        // (the Board page under Game, where game.cfg declares the board's settings; the Controls page's autofire:
        // runtime/board_settings.h)
        if (!BoardSettings().empty()) {
            mc.pages.push_back({"Board", "Game",
                                "The game's own settings, as its test menu (F2) has them, kept in the board's EEPROM (the PGM's: its "
                                "battery RAM): each as the board is set unless chosen here. Put in as the board starts: Reset game to "
                                "play with a change."});
        }
        mc.pages.push_back({"Controls", "", "The controls: autofire for each of the game's buttons."});
        // (the Arcade monitor page under Display, as cps2recomp has it; none on a phone)
        if (FeatureIndex("crt_resolution") < AllFeatures().size()) {
            mc.pages.push_back({"Arcade monitor", "Display", "A CRT at the board's own resolution: set on the launcher."});
        }
        // (the standard speeds: these boards run faster than real time too, as fast forward does)
        recomp::layout::Speeds(mc, true);
        mc.speedDescription = recomp::layout::kSpeedDescription;
        mc.muted = [&]() { return muted; };
        mc.setMuted = [&](bool m) {
            muted = m;
            if (audio && m) {
                SDL_ClearAudioStream(audio); // (silent at once, not after what is queued)
            }
        };
        mc.confirmName = recomp::sdl::kConfirmName;
        mc.closeName = recomp::sdl::kCloseName;
        mc.slots = 99; // (save states: a tester asked for more than 9)
        // The standard layout (recomp-common/menu_layout.h): Online, Display, Game first; Alternative modes; the states;
        // the board's own rows; Reset all options, Reset game, Quit game, Resume. Speed and Mute on the Game page
        recomp::layout::Rows rows;
        rows.actions = {recomp::MenuRow(recomp::MenuRow::Action, 1, recomp::layout::kInsertCoin, "A coin into slot 1."),
                        recomp::MenuRow(recomp::MenuRow::Action, 2, recomp::layout::kTestMenu,
                                        "The board's test switch: the game's test menu: its settings, tests and bookkeeping.")};
        rows.resetDescription = "Starts the board again from power on.";
        recomp::layout::Standard(mc, rows);
        setHostRow(false);
        if (speedAtStart >= 0) {
            menu.SetSpeed(speedAtStart);
            speed = menu.Speed() < 0 ? recomp::layout::kFull : menu.Speed();
        }
    }

    const port::BoundTable &table = port::MakeBoundTable(2, config.count("buttons") ? config["buttons"] : "");
    const recomp::Bindings bindings = recomp::LoadBindings(settingsPath, table.table);
    Controls controls;
    controls.table = &table;
    controls.bindings = &bindings;
    controls.features = &features;
    // The gamepads: every one connected, each in a seat (a let-go one's seat stays empty for the next), and each
    // player's from them: those the launcher's Controllers page gave a player, then the rest in their seats' order
    // (recomp-common bindings.h: [controllers]); given out again when one comes or goes, not every frame
    const recomp::PadChoices padChoices = settingsPath.empty() ? recomp::PadChoices{} : recomp::LoadPadChoices(settingsPath);
    SDL_Gamepad *seats[8] = {};
    auto seated = [&]() {
        int count = 0;
        for (int i = 0; i < 8; i++) {
            count = seats[i] ? i + 1 : count;
        }
        recomp::AssignPads(padChoices, seats, count, 2, controls.pads);
    };
    // A key a control has been given (the launcher's Controllers page): it does only that, not a hotkey's work
    auto keyBound = [&](SDL_Scancode key) {
        for (int p = 0; p < 2; p++) {
            for (int a = 0; a < table.table.count; a++) {
                if (bindings.key[p][a] == key) {
                    return true;
                }
            }
        }
        return false;
    };
    const bool tabFree = !keyBound(SDL_SCANCODE_TAB); // (Tab opens the menu too, unless fast forward or another has it)
    controls.nineService = !keyBound(SDL_SCANCODE_9);
    controls.touch = &touch;
    auto attach = [&]() {
        controls.board = board.get();
        board->onInputRead = [](void *context) { static_cast<Controls *>(context)->Read(); };
        board->onInputReadContext = &controls;
    };
    attach();

    // ---- The input lag readout (the feature input_lag), in three parts. The game's: how many frames after a press
    // the picture first shows it, found in play by running the next frames twice from one moment (the stick as held,
    // and pushed the other way), unseen as run-ahead's frames are, every few seconds: the first frame whose picture is
    // not the same; the value most tries give. Run-ahead's frames off that. The port's: from a key or button going down
    // (the event's own time) to the frame that has it handed to the screen. What the screen adds is not known here.
    // A look is spread over the frames shown (issue #17: all of it in one frame made that frame late on a slower
    // machine, a stutter every few seconds): the moment is set aside twice (hw.LookKeep), and then each frame shown
    // runs one frame of one of the two runs, by turns, before the game's own (hw.LookIn, LookFrame, LookOut), so no
    // frame has more than one frame's work more. The runs' pictures are the same as when they were run in one go, so
    // the readout's numbers are too. Once the last tries all agree it looks less often; again soon when play resumes
    // (a new game, a continue).
    constexpr int kLagFrames = 10, kLagTries = 5;
    constexpr uint64_t kLagEvery = 240, kLagSteadyEvery = 3600, kLagAway = 120;
    struct Lag {
        int found[kLagTries] = {}, tries = 0, game = 0; // (game: 0 until found)
        uint64_t nextAt = 0;
        bool wasOn = false;
        uint64_t pressedAt = 0; // (ns; 0: no press waiting to be shown)
        double portMs = 0;
        int presses = 0;
        // (the look under way: its frames run so far, both runs together, by turns; 0: none)
        int look = 0;
        uint64_t lookAt = 0, plain[kLagFrames] = {};
        uint32_t held[3] = {};
        uint64_t away = 0; // (frames out of play since play was last seen)
    } lag;
    const size_t inputLagAt = FeatureIndex(ToolkitFeature::InputLag);
    auto lagOn = [&]() { return inputLagAt < features.size() && features[inputLagAt] != 0; };
    auto lagSteady = [&]() {
        if (lag.tries < kLagTries) {
            return false;
        }
        for (int i = 1; i < kLagTries; i++) {
            if (lag.found[i] != lag.found[0]) {
                return false;
            }
        }
        return true;
    };
    auto lagFound = [&](int found) {
        lag.found[lag.tries++ % kLagTries] = found;
        // (the value most tries gave; of two as common, the smaller)
        int best = 0, bestCount = 0;
        for (int i = 0; i < std::min(lag.tries, kLagTries); i++) {
            int count = 0;
            for (int j = 0; j < std::min(lag.tries, kLagTries); j++) {
                count += lag.found[j] == lag.found[i];
            }
            if (count > bestCount || (count == bestCount && lag.found[i] < best)) {
                best = lag.found[i];
                bestCount = count;
            }
        }
        lag.game = best;
        if (getenv("SPELUNK_REPORT_LAG")) {
            fprintf(stderr, "input lag: frame %llu: the game shows a press %d frames on (try %d; most often %d)\n",
                    static_cast<unsigned long long>(lag.lookAt), found, lag.tries, lag.game);
        }
    };
    // (a look begun: the moment set aside, and the controls held then, which both runs start from)
    auto lagBegin = [&]() {
        lag.held[0] = board->inputs[0];
        lag.held[1] = board->inputs[1];
        lag.held[2] = board->system;
        lag.lookAt = frameCount;
        hw.LookKeep();
        lag.look = 1; // (the next frame shown runs its first frame)
    };
    std::vector<uint32_t> lagScreen(static_cast<size_t>(Hw::kWidth) * Hw::kHeight);
    // (a look's frame's picture, read a frame after it was run (the GPU not waited for): to the run with the stick
    // as held, or compared with it)
    auto lagPicture = [&](int step) {
        const int way = step % 2, n = step / 2;
        if (!hw.LookScreen(step % 2, lagScreen.data())) {
            lag.look = 0;
            return;
        }
        uint64_t h = 0xCBF29CE484222325ull;
        for (uint32_t p : lagScreen) {
            h = (h ^ p) * 0x100000001B3ull;
        }
        if (!way) {
            lag.plain[n] = h;
        } else if (h != lag.plain[n]) {
            lag.look = 0;
            lagFound(n + 1);
        } else if (n + 1 == kLagFrames) {
            lag.look = 0; // (the stick changed nothing in the picture: no try)
        }
    };
    // (one frame of the look: the run with the stick as held, or the one with it pushed, by turns; after the
    // picture of the one before is looked at)
    auto lagStep = [&]() {
        const int step = lag.look - 1;
        if (step > 0 && !hw.LookReady((step - 1) % 2)) {
            return; // (the GPU not yet done with the frame before: the next frame shown, then, not this one held up)
        }
        if (step > 0) {
            lagPicture(step - 1);
            if (!lag.look) {
                return;
            }
        }
        const int way = step % 2;
        const uint32_t still[3] = {lag.held[0], lag.held[1], 0}, pushed[3] = {lag.held[0] ^ kInRight, lag.held[1], 0};
        const uint32_t *these = way ? pushed : still;
        const uint32_t heldBefore[3] = {board->inputs[0], board->inputs[1], board->system};
        const uint32_t *forcedBefore = controls.forced;
        controls.forced = these;
        hw.LookIn(way);
        board->inputs[0] = these[0];
        board->inputs[1] = these[1];
        board->system = these[2];
        controls.readThisFrame = false;
        if (ThisGame().beforeFrame) {
            ThisGame().beforeFrame(*board, features);
        }
        hw.LookFrame(step % 2);
        hw.LookOut(way);
        controls.forced = forcedBefore;
        controls.readThisFrame = false;
        board->inputs[0] = heldBefore[0];
        board->inputs[1] = heldBefore[1];
        board->system = heldBefore[2];
        lag.look++;
    };
    auto lagLine = [&](int ahead) {
        char text[96];
        const double frameMs = std::chrono::duration<double, std::milli>(Hw::kFrameTime).count();
        if (!lag.game) {
            snprintf(text, sizeof text, "INPUT LAG  GAME ? (IN PLAY)  PORT %.0f MS", lag.portMs);
            return std::string(text);
        }
        // (the frame the press is first read in is the port's time; the game's frames after it, less run-ahead's)
        const int after = std::max(0, lag.game - 1 - ahead);
        const double port = lag.presses ? lag.portMs : frameMs;
        if (ahead) {
            snprintf(text, sizeof text, "INPUT LAG  GAME %dF  RUN-AHEAD -%d  PORT %.0f MS  = %.0f MS", lag.game, ahead, port, after * frameMs + port);
        } else {
            snprintf(text, sizeof text, "INPUT LAG  GAME %dF  PORT %.0f MS  = %.0f MS", lag.game, port, after * frameMs + port);
        }
        return std::string(text);
    };


    std::vector<uint32_t> picture(Hw::kWidth * Hw::kHeight), turned, frozen, shown, standing;
    int turnedW = 0, turnedH = 0, frozenW = 0, frozenH = 0, standingW = 0, standingH = 0;
    // The menu's buttons as they are held now (recomp-common/menu_sdl.h: the arrows, the d-pad and the left stick,
    // Enter, Space and A to choose, Backspace and B to go back; the A key confirms too, as it did). Held arrows go on
    // through the rows and the choices (recomp-common's MenuRepeat). Asked only while the menu is open: closed, the
    // frame has nothing of the menu's but Start and Select's check (as before)
    recomp::MenuRepeat menuRepeat;
    auto menuHeld = [&]() {
        SDL_PumpEvents();
        const bool *keys = SDL_GetKeyboardState(nullptr);
        // (and the touch controls' stick and first two buttons: port/touch.h)
        touch.Feed();
        touch.Read(true);
        return recomp::sdl::MenuHeld(keys, controls.pads, 2) | (keys[SDL_SCANCODE_A] ? recomp::kMenuConfirm : 0u) | touch.Menu();
    };
    // (Start and Select of one gamepad held together: they open the menu, and close it)
    recomp::sdl::MenuChord chord;
    // (compact as the Arcade monitor page asks, or by itself on an arcade monitor: recomp-common/arcade_sdl.h)
    const size_t compactAt = FeatureIndex("crt_compact_menu");
    auto compactMenu = [&]() { return (compactAt < features.size() && features[compactAt]) || arcade.OnArcade(window); };
    // The menu's own picture: the picture under it (frozen) turned back by the menu's turn, so the menu lays itself out
    // for the screen as the player has it (a monitor turned on its side: taller than wide); drawn there, then turned
    // onto the picture as shown (shown, frozenW x frozenH). Not turned: drawn on frozen as it is
    std::vector<uint32_t> menuOwn, menuUnder;
    int menuTurn = 0, menuW = 0, menuH = 0;
    auto standMenu = [&]() {
        menuTurn = menuTurnNow();
        recomp::turn::TurnedSize(menuTurn, frozenW, frozenH, menuW, menuH);
        if (menuTurn) {
            menuUnder.resize(frozen.size());
            recomp::turn::TurnPixels(frozen.data(), frozenW, frozenH, 360 - menuTurn, menuUnder.data());
        }
    };
    auto drawMenu = [&]() {
        if (menuTurnNow() != menuTurn) {
            standMenu(); // (the Menu orientation changed in the menu: turned at once)
        }
        if (!menuTurn) {
            shown = frozen;
            menu.Draw(shown.data(), frozenW, frozenH);
            return;
        }
        menuOwn = menuUnder;
        menu.Draw(menuOwn.data(), menuW, menuH);
        shown.resize(frozen.size());
        recomp::turn::TurnPixels(menuOwn.data(), menuW, menuH, menuTurn, shown.data());
    };
    auto openMenu = [&]() {
        readScreen(picture.data());
        canvasSize(frozenW, frozenH); // (the picture's shape as shown, three times the board's size: room for the text)
        if (arcade.OnArcade(window)) {
            // (on an arcade monitor, on the board's own pixels and lines, which a super resolution stretches across as it
            // does the game's: the compact menu's letters as the monitor shows the game's, not shrunk to a third)
            frozenW = Hw::kWidth;
            frozenH = Hw::kHeight;
        }
        menu.SetCompact(compactMenu());
        TurnTo(picture.data(), turnNow(), frozenW, frozenH, frozen);
        standMenu();
        ownMenuRow(); // (the presets there now)
        menu.Configure(menuConfig);
        menu.Open();
        menuRepeat.Step(menuHeld()); // (what opened it is not also a press in it)
    };
    // Where the menu's picture (menuW x menuH, turned by menuTurn onto the picture as shown, frozenW x frozenH) is in
    // the window: place(), in the window's pixels, as show() puts it there on every path (SDL's renderer, the GPU's, a
    // shader's); the mouse and the fingers mapped back through the turn
    auto menuPlace = [&]() {
        const SDL_FRect to = place();
        return recomp::sdl::MenuPlace{to.x, to.y, to.w, to.h, menuW, menuH, menuTurn};
    };
    // F12: the board's picture as it is shown (turned and shaped, three times the board's size; no shader, no tools'
    // writing) into the saves folder's screenshots/, named by the game and the time
    auto screenshot = [&]() {
        namespace fs = std::filesystem;
        const std::string folder = (savesFolder.empty() ? std::string(".") : savesFolder) + "/screenshots/";
        std::error_code error;
        fs::create_directories(fs::path(folder), error);
        char stamp[32] = "";
        const std::time_t now = std::time(nullptr);
        if (const std::tm *local = std::localtime(&now)) {
            std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H.%M.%S", local);
        }
        std::string name = gameId + " " + stamp;
        for (int n = 2; fs::exists(fs::path(folder + name + ".png"), error); n++) {
            name = gameId + " " + stamp + " " + std::to_string(n);
        }
        std::vector<uint32_t> board(static_cast<size_t>(Hw::kWidth) * Hw::kHeight), out;
        readScreen(board.data());
        int w = 0, h = 0;
        canvasSize(w, h);
        TurnTo(board.data(), turnNow(), w, h, out);
        const std::string path = folder + name + ".png";
        if (WritePng(path.c_str(), out.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h))) {
            fprintf(stderr, "screenshot: %s\n", path.c_str());
            say("SCREENSHOT SAVED  screenshots/" + name + ".png");
        } else {
            fprintf(stderr, "screenshot: could not write %s\n", path.c_str());
            say("SCREENSHOT NOT SAVED");
        }
    };
    // (Esc, F1, the gamepad's menu buttons, Start and Select: the menu opened, or closed at once from any page)
    auto toggleMenu = [&]() {
        if (menu.IsOpen()) {
            menu.Close();
        } else {
            openMenu();
        }
    };
    // Reset all options: every feature as the launcher's Enhanced preset has it (settings.ini's own values taken out;
    // the menu puts the speed back to full itself). The Screen, Aspect and Shader follow at the next frame
    // (followShape, followShader)
    auto resetOptions = [&]() {
        if (!settingsPath.empty()) {
            recomp::Ini ini(settingsPath);
            ini.load();
            recomp::SetPreset(ini, AllFeatures(), "enhanced");
            ini.save();
            features = recomp::FeatureValues(ini, AllFeatures());
        } else {
            for (size_t i = 0; i < features.size(); i++) {
                features[i] = (AllFeatures()[i].flags & recomp::kFeatureEnhanced) ? 1 : 0;
            }
        }
        if (attract) {
            attract = false;
            beginAttract(); // (the player's own values are the reset ones; attract mode goes on)
        }
    };
    bool resetWanted = false, beginWanted = false, paused = false;
    int gameCommandWanted = -1;
    // Stopped (P, the Training mode page's Frame advance): `steps` frames still to go on by one at a time (O; Tab or a
    // gamepad's right trigger, fast forward's controls, held: frame after frame after a moment); the picture shown
    // again with the frame's number once stopped (pauseShown)
    int steps = 0, stepHeld = 0;
    bool pauseShown = true;
    auto stop = [&](bool on) {
        paused = on;
        steps = 0;
        pauseShown = !on;
    };
    auto handleChosen = [&](const recomp::Menu::Chosen &chosen) {
        switch (chosen.kind) {
        case recomp::Menu::Chosen::Save:
            if (saveState(menu.Slot())) {
                menu.Close();
            } else {
                menu.Note("The state could not be saved.");
            }
            break;
        case recomp::Menu::Chosen::Load:
            if (loadState(menu.Slot())) {
                menu.Close();
            } else {
                menu.Note("No state in that slot (or one from another version).");
            }
            break;
        case recomp::Menu::Chosen::Quit: running = false; break;
        case recomp::Menu::Chosen::Command:
            // (the first command: Host a match, or Stop hosting; the second: Begin at the stage; then the game's own,
            // each of which begins a new game its own way: Game::command; then Frame advance and Attract mode's Start)
            if (chosen.id == stepCommand) {
                stop(!paused);
            } else if (chosen.id == attractCommand) {
                attractWanted = true;
            } else if (chosen.id >= firstGameCommand) {
                gameCommandWanted = chosen.id - firstGameCommand;
            } else {
                (chosen.id == 1 ? beginWanted : hostWanted) = true;
            }
            menu.Close();
            break;
        case recomp::Menu::Chosen::Action:
            if (chosen.id == 1) {
                controls.coinFrames = 4;
            } else if (chosen.id == 2) {
                controls.testFrames = controls.kTestPressFrames;
            }
            menu.Close();
            break;
        case recomp::Menu::Chosen::Reset:
            resetWanted = true;
            if (ThisGame().command) {
                ThisGame().command(-1); // (a plain reset: no mode)
            }
            menu.Close();
            break;
        case recomp::Menu::Chosen::ResetOptions:
            resetOptions();
            menu.Note("Every option is back as it first was.");
            break;
        default: break;
        }
        speed = menu.Speed() < 0 ? recomp::layout::kFull : menu.Speed();
    };

    // ---- Straight to the title (the Game page's Start feature, on unless the player says the whole boot): from the
    // board's power-on to game.cfg's title_frame the game runs unseen and unheard, as fast as the machine goes, with no
    // controls but a check's scripted ones; the picture comes with the title. The frames are the board's own, run as
    // they always are: the game is where a shown boot leaves it, frame for frame, so the checks' runs (whose presses
    // come later) are the same either way. In an online match both sides have the board at power-on (the host's,
    // sent whole: small, VRAM still empty) and the host's choice of this (a Game page feature); each runs the boot
    // itself (bootUnseen) with no controls before the match's first frame, so the two are alike at the title. (The
    // host's state taken at the title instead would be some 11 MB to send on a CV1000, its VRAM full)
    const uint64_t titleFrame = config.count("title_frame") ? strtoull(config["title_frame"].c_str(), nullptr, 0) : 0;
    const size_t bootAt = FeatureIndex("boot");
    auto skipBoot = [&]() { return titleFrame > 0 && bootAt < features.size() && features[bootAt] == 0; };
    bool booting = skipBoot();
    if (!startState.empty()) {
        std::vector<uint8_t> data;
        if (!ReadWholeFile(startState, data)) {
            fprintf(stderr, "cannot read %s\n", startState.c_str());
            return 1;
        }
        Archive ar(data, false);
        board->Serialize(ar);
        if (!ar.Ok()) {
            fprintf(stderr, "%s is not a state of this game and build\n", startState.c_str());
            return 1;
        }
        hw.AfterLoad();
        hw.ToolsLost();
        booting = false;
    }
    auto resetBoard = [&]() {
        aheadForget(); // (frames run ahead are not the new board's)
        std::string why;
        auto fresh = std::make_unique<Board>();
        if (LoadSet(romFolder, config, *fresh, why)) {
            hw.CarryOver(*fresh);
            Hw::StartCpu(*fresh);
            board = std::move(fresh);
            {
                SettingsStore store = hw.Store(); // (the Board page's settings, as they are chosen now)
                boardSettings.Apply(store, features);
            }
            attach();
            hw.Attach();
            hw.ToolsLost();
            booting = skipBoot();
            lag.look = 0; // (a look under way: let go)
        }
    };
    // (an online match: the boot run here and now on both sides, from the power-on both have, before the match's first
    // frame; the other side kept told this one is still here)
    auto bootUnseen = [&](port::Netplay *keepAlive) {
        if (!skipBoot()) {
            return;
        }
        const uint32_t none[3] = {0, 0, 0};
        const uint32_t *forcedBefore = controls.forced;
        controls.forced = none;
        while (board->frame < titleFrame) {
            controls.readThisFrame = false;
            if (ThisGame().beforeFrame) {
                ThisGame().beforeFrame(*board, features);
            }
            hw.NetFrame();
            hw.Sound().clear();
            if (!controls.readThisFrame) {
                controls.Read();
            }
            if (ThisGame().afterFrame) {
                ThisGame().afterFrame(*board, features);
            }
            if (keepAlive && board->frame % 30 == 0) {
                keepAlive->KeepAlive();
            }
        }
        controls.forced = forcedBefore;
        booting = false;
    };

    // ---- Online play (port/netplay.h, port/online.h): Netplay runs the match's frames through netGame ----
    std::unique_ptr<port::Netplay> net;
    // (hosting from the in-game menu: the match waited for while the game is the player's own, offline; when someone
    // comes it becomes `net`, and after the match the player's game goes on alone, their features as they were)
    std::unique_ptr<port::Netplay> waitingHost;
    port::Netplay::Settings hostSettings;
    bool hostedFromMenu = false;
    std::vector<int> ownFeatures;
    std::string hostNote;
    uint64_t hostNoteUntil = 0;
    bool netOn = false, localIn = false, netLate = false;
    std::string netEnded;
    port::OnlineScreen netScreen;
    uint64_t tick = 0;
    auto next = std::chrono::steady_clock::now(); // (when the next frame is due)
    bool netCoinHeld = false;
    int netCoinFrames = 0;
    port::FnGame netGame;
    netGame.id = gameId;
    netGame.title = title;
    {
        // (what both sides must have the same of: the game's config, which names the set's files by their checksums, and the
        // board's state's layout)
        const char *cfg = BuiltInGameConfigText();
        uLong h = crc32(0, reinterpret_cast<const Bytef *>(cfg), static_cast<uInt>(strlen(cfg)));
        h = crc32(h, reinterpret_cast<const Bytef *>(stateHeader.data()), static_cast<uInt>(stateHeader.size()));
        netGame.build = static_cast<uint32_t>(h);
    }
    netGame.frameSeconds = std::chrono::duration<double>(Hw::kFrameTime).count();
    netGame.mostAhead = Hw::kNetMostAhead;
    netGame.features = &features;
    netGame.keep = [&](std::vector<uint8_t> &out) {
        out.clear();
        Archive ar(out, true);
        hw.BeforeSave();
        board->Serialize(ar);
    };
    netGame.putBack = [&](const std::vector<uint8_t> &in) {
        std::vector<uint8_t> data(in);
        Archive ar(data, false);
        board->Serialize(ar);
        if (!ar.Ok()) {
            return false;
        }
        hw.AfterLoad();
        hw.ToolsLost();
        return true;
    };
    // (someone has come to a match hosted from the menu: it begins from the board's reset, as one hosted from the
    // launcher does)
    netGame.coming = [&]() {
        if (waitingHost) {
            fprintf(stderr, "online: someone joined: the match starts from the board's reset\n");
            endAttract();
            ownFeatures = features;
            menu.Close();
            stop(false);
            speed = recomp::layout::kFull;
            menu.SetSpeed(recomp::layout::kFull);
            replaying = false;
            attempt.clear();
            net = std::move(waitingHost); // (Netplay::Waiting goes on, as the match's)
            hostedFromMenu = true;
            setHostRow(false);
        }
        resetBoard();
    };
    netGame.keepAt = [&](int which) { hw.NetKeep(which); };
    netGame.back = [&](int which) { hw.NetBack(which); };
    netGame.drop = [&](int which) { hw.NetDrop(which); };
    netGame.hash = [&]() {
        const uint64_t h = board->StateHash();
        return static_cast<uint32_t>(h ^ (h >> 32));
    };
    // A frame of the match: player 1's and 2's controls (their stick and buttons, Start and the coin), shown and heard
    // or run unseen and unheard (its sound dropped)
    netGame.step = [&](const uint16_t *players, bool shown) {
        // (autofire, each player's own, put in here on both sides alike by the board's frame: the controls sent are as
        // held, so the other side's guesses of them stay right; Netplay::TheirFeatures)
        const int me = net ? net->Local() : 0;
        static const std::vector<int> none;
        const std::vector<int> &theirs = net ? net->TheirFeatures() : none;
        const uint32_t forced[3] = {Autofire(players[0] & 0xFFu, board->frame, me == 0 ? features : theirs),
                                    Autofire(players[1] & 0xFFu, board->frame, me == 1 ? features : theirs),
                                    ((players[0] & port::kStartBit) ? kSysStart1 : 0u) | ((players[1] & port::kStartBit) ? kSysStart2 : 0u) |
                                        ((players[0] & port::kCoinBit) ? kSysCoin1 : 0u) | ((players[1] & port::kCoinBit) ? kSysCoin2 : 0u)};
        controls.forced = forced;
        controls.readThisFrame = false;
        if (ThisGame().beforeFrame) {
            ThisGame().beforeFrame(*board, features);
        }
        if (shown) {
            hw.BeforeFrame();
            board->RunFrame();
            netLate = hw.AfterFrame(false);
        } else {
            const size_t sound = hw.Sound().size();
            hw.NetFrame();
            hw.Sound().resize(sound);
        }
        if (!controls.readThisFrame) {
            controls.Read();
        }
        if (ThisGame().afterFrame) {
            ThisGame().afterFrame(*board, features);
        }
        controls.forced = nullptr;
    };
    // This side's player's controls for the match: player 1's (keys and player 1's gamepad), whichever side it plays
    auto netControls = [&]() -> uint16_t {
        SDL_PumpEvents();
        static const bool none[SDL_SCANCODE_COUNT] = {};
        const bool *keys = netScreen.Typing() || netScreen.leaving ? none : SDL_GetKeyboardState(nullptr);
        uint32_t bits = port::BoundBits(table, bindings, 0, keys, controls.pads[0]) | controls.scripted[0];
        bits |= (controls.scriptedSystem & (kSysStart1 | kSysStart2)) ? port::kStartBit : 0;
        bits |= (controls.scriptedSystem & (kSysCoin1 | kSysCoin2)) ? port::kCoinBit : 0;
        const bool coin = (bits & port::kCoinBit) != 0;
        if (coin && !netCoinHeld) {
            netCoinFrames = 4; // (a coin is a pulse a few frames long)
        }
        netCoinHeld = coin;
        bits &= ~port::kCoinBit;
        if (netCoinFrames > 0) {
            netCoinFrames--;
            bits |= port::kCoinBit;
        }
        localIn = localIn || (bits & port::kStartBit);
        return static_cast<uint16_t>(bits & 0x3FF);
    };
    FILE *netHashes = online.hashes.empty() ? nullptr : fopen(online.hashes.c_str(), "w");
    FILE *netLog = online.log.empty() ? nullptr : fopen(online.log.c_str(), "w");
    auto netBegin = [&](const port::Netplay::Settings &settings) {
        net = std::make_unique<port::Netplay>(settings);
        net->hashFile = netHashes;
        net->controlsLog = netLog;
        std::string why;
        if (!net->Begin(netGame, why)) {
            netEnded = why;
            fprintf(stderr, "online: %s\n", why.c_str());
        }
        netOn = false;
    };
    if (online.wanted) {
        netBegin(online.net);
    }
    // The player's own name for the lobby: --net-name's, else the launcher's Online page's (its launcher.ini, beside
    // settings.ini or a folder up, in a collection)
    auto launcherName = [&]() -> std::string {
        if (!online.net.name.empty() || settingsPath.empty()) {
            return online.net.name;
        }
        namespace fs = std::filesystem;
        fs::path at = fs::path(settingsPath).parent_path();
        for (int up = 0; up < 3 && !at.empty(); up++, at = at.parent_path()) {
            std::error_code error;
            if (fs::exists(at / "launcher.ini", error)) {
                recomp::Ini prefs((at / "launcher.ini").string());
                prefs.load();
                if (!prefs.get("online", "name").empty()) {
                    return prefs.get("online", "name");
                }
            }
        }
        return "";
    };
    auto stopHosting = [&]() {
        waitingHost.reset(); // (the socket closed: the code stops working)
        setHostRow(false);
        fprintf(stderr, "online: stopped hosting\n");
    };
    auto startHosting = [&](const port::Netplay::Settings &settings) {
        waitingHost = std::make_unique<port::Netplay>(settings);
        waitingHost->hashFile = netHashes;
        waitingHost->controlsLog = netLog;
        std::string why;
        if (!waitingHost->Begin(netGame, why)) {
            waitingHost.reset();
            fprintf(stderr, "online: %s\n", why.c_str());
            hostNote = "COULDN'T HOST: " + why;
            hostNoteUntil = tick + 300;
            setHostRow(false);
            return;
        }
        hostSettings = settings;
        setHostRow(true);
    };
    // The menu's row: hosting begun (the game plays on), or stopped
    auto hostFromMenu = [&]() {
        if (waitingHost) {
            stopHosting();
            return;
        }
        endAttract(); // (the match's features are the player's own)
        if (net) {
            return; // (online already)
        }
        port::Netplay::Settings settings = online.net;
        settings.hosting = true;
        settings.solo = true;
        settings.address.clear();
        settings.name = launcherName();
        settings.listed = !settings.name.empty();
        settings.renewCode.clear();
        settings.renewSecret.clear();
        settings.invite.clear();
        settings.inviteName.clear();
        startHosting(settings);
        if (waitingHost) {
            fprintf(stderr, "online: hosting from the menu; playing alone until someone joins\n");
        }
    };
    // After a match hosted from the menu (or the wait for its player given up): the player's game alone again, as the
    // match left it, with their own features; still hosted with the same code if the other player left or dropped
    auto backToSolo = [&](bool stillHosting) {
        port::Netplay::Settings settings = hostSettings;
        if (net) {
            if (!net->code.empty()) {
                settings.renewCode = net->code.substr(0, 3) + net->code.substr(4);
                settings.renewSecret = net->Secret();
            }
            if (netOn && net->ended.empty()) {
                net->Leave();
                net->Report(true);
            }
        }
        net.reset(); // (the socket closed: the port is free for hosting again)
        netOn = false;
        netEnded.clear();
        netScreen.leaving = false;
        netGuest = false;
        if (!ownFeatures.empty()) {
            features = ownFeatures;
            ownFeatures.clear();
        }
        hostedFromMenu = false;
        next = std::chrono::steady_clock::now();
        fprintf(stderr, "online: playing alone again%s\n", stillHosting ? ", still hosting" : "");
        if (stillHosting) {
            startHosting(settings);
        } else {
            setHostRow(false);
        }
    };
    // While no match is on (waiting, or after one has ended): the wait, the code, or why it ended, on black
    auto netWaitScreen = [&]() {
        int w = 0, h = 0;
        canvasSize(w, h);
        hud.Begin(w, h, menuTurnNow());
        netScreen.Wait(hud, net.get(), netEnded, tick);
        hud.Changed();
        shown.assign(static_cast<size_t>(w) * h, 0xFF000000u);
        hud.Over(shown.data());
        show(shown, w, h);
    };

    // The board's picture made ready to show (read back, and turned with the tools' writing over it, where it is shown
    // by the CPU's way: wantPixels), and shown: by the frame's run, and again while stopped (pauseShown)
    auto turnPicture = [&](bool wantPixels) {
        if (wantPixels || shading) {
            readScreen(picture.data());
        }
        if (wantPixels) {
            if (hud.Any()) {
                // (with words over it, the picture is made their size, in the shape shown)
                turnedW = hud.width;
                turnedH = hud.height;
                TurnTo(picture.data(), turnNow(), turnedW, turnedH, turned);
                hud.Over(turned.data());
            } else {
                Turn(picture.data(), turnNow(), 1, turned, turnedW, turnedH);
            }
        }
    };
    auto presentPicture = [&]() {
        if (shading) {
            // Through the shader: the board's picture as it sends it, the shader's drawing then stood up (the lines
            // along the game's own, as on the cabinet's turned monitor); or (Scanline direction: Horizontal, or Auto on
            // a screen wider than tall) stood up first, so the lines run across the screen. The tools' writing over it, not through the shader
            const int turn = turnNow();
            touchLayer();
            shading->keepLast = !windowShotNow.empty();
            // (Auto: across on a screen wider than tall, the cabinet's way on one shaped like the cabinet's)
            int ow = 0, oh = 0;
            SDL_GetWindowSizeInPixels(window, &ow, &oh);
            const int lines = features[linesAt];
            const bool across = lines == 2 || (lines == 0 && ow > oh);
            if (across && turn) {
                Turn(picture.data(), turn, 1, standing, standingW, standingH);
                shading->Show(standing.data(), standingW, standingH, place(), true, 0, true);
            } else {
                shading->Show(picture.data(), Hw::kWidth, Hw::kHeight, place(), true, turn / 90, true);
            }
            if (!windowShotNow.empty()) {
                shadedShot();
            }
        } else if (useGpu) {
            touchLayer();
            hw.PresentScreen(turnNow(), place());
        } else {
            show(turned, turnedW, turnedH);
        }
    };
    FILE *pictures = pictureLog.empty() ? nullptr : fopen(pictureLog.c_str(), "w");
    FILE *inputs = inputLog.empty() ? nullptr : fopen(inputLog.c_str(), "w");
    const auto frameTime = Hw::kFrameTime;
    // (the host's own time a frame, port/host_time.h: game.log a minute at a time, and the speed readout's second number)
    port::HostTime hostTime(std::chrono::duration<double, std::milli>(frameTime).count());
    double hostShown = 0;
    next = std::chrono::steady_clock::now();
    // The FPS counter (Display): frames handed to the screen, counted over the last half second (as Soulcalibur II's)
    const size_t fpsAt = FeatureIndex("fps_counter");
    uint64_t fpsFrom = SDL_GetTicksNS();
    unsigned fpsFrames = 0;
    double fpsRate = 0;
    // The open menu answers a finger itself, the touch controls on or off (with them off and no gamepad, it is the only
    // way back to them): a finger put down on it chooses the row under it, lifted where it went down does what a click
    // does there, and slid up or down moves through the rows a row at a time. A finger on one of the touch controls'
    // buttons is still theirs (Menu closes it, the first two choose and go back). What it chose is done in the loop,
    // not where the finger was read (which can be in the game's read of its controls)
    std::vector<recomp::Menu::Chosen> menuTaps;
    struct MenuFinger {
        SDL_FingerID id = 0;
        bool down = false, slid = false;
        float x0 = 0, y0 = 0, stepY = 0;
    } menuFinger;
    touch.before = [&](const SDL_Event &e, float windowX, float windowY) {
        const recomp::sdl::MenuPlace at = menuPlace();
        // (where the finger is in the menu's own picture, through its turn: a slide is up or down the menu as it stands)
        float x = 0, y = 0;
        at.ToMenu(windowX, windowY, x, y);
        auto tell = [&](recomp::MenuMouse what) {
            if (at.w > 0 && at.h > 0) {
                menuTaps.push_back(menu.Mouse(what, x, y));
            }
        };
        if (e.type == SDL_EVENT_FINGER_DOWN) {
            if (net || !menu.IsOpen() || touch.OnButton(windowX, windowY)) {
                return false;
            }
            menuFinger = {e.tfinger.fingerID, true, false, x, y, y};
            tell(recomp::MenuMouse::Move);
            return true;
        }
        if (!menuFinger.down || e.tfinger.fingerID != menuFinger.id) {
            return false;
        }
        if (e.type == SDL_EVENT_FINGER_MOTION) {
            // (a row's worth of sliding, about a twentieth of the menu's height, is a row)
            const float row = std::max(1.0f, at.height / 20.0f);
            menuFinger.slid = menuFinger.slid || std::hypot(x - menuFinger.x0, y - menuFinger.y0) > row * 0.5f;
            for (; menu.IsOpen() && menuFinger.slid && y - menuFinger.stepY <= -row; menuFinger.stepY -= row) {
                tell(recomp::MenuMouse::WheelDown);
            }
            for (; menu.IsOpen() && menuFinger.slid && y - menuFinger.stepY >= row; menuFinger.stepY += row) {
                tell(recomp::MenuMouse::WheelUp);
            }
            return true;
        }
        menuFinger.down = false;
        if (e.type == SDL_EVENT_FINGER_UP && !menuFinger.slid && menu.IsOpen()) {
            tell(recomp::MenuMouse::Click);
        }
        return true;
    };
    const bool aheadKeep = !getenv("SPELUNK_NO_AHEAD_KEEP");
    // (the scripted controls, --press and --system, as they are at frame `f`: checks)
    auto scriptAt = [&](uint64_t f) {
        controls.scripted[0] = controls.scripted[1] = controls.scriptedSystem = 0;
        for (const Held &h : held) {
            if (f >= h.from && f < h.from + h.count) {
                (h.player < 0 ? controls.scriptedSystem : controls.scripted[h.player]) |= h.bits;
            }
        }
    };
    // (whether the board may run on kept: run-ahead 1 and nothing else wanting the game's own frames, or the board)
    auto aheadKeepable = [&]() {
        const bool toolsOn = features[FeatureIndex(ToolkitFeature::PracticePoint)] || features[FeatureIndex(ToolkitFeature::Rewind)] ||
                             features[FeatureIndex(ToolkitFeature::FastForward)];
        return aheadKeep && (features[FeatureIndex(ToolkitFeature::RunAhead)] == 1 || features[FeatureIndex(ToolkitFeature::RunAhead)] == 2) && !checkVideo && !paused && !menu.IsOpen() &&
               speed == recomp::layout::kFull && !toolsOn && !lagOn() && !rollbackTest && !replaying && !net && !waitingHost &&
               !attract && !booting && !(ThisGame().unseen && ThisGame().unseen(*board, features) >= 0);
    };
    while (running) {
        const uint64_t workFrom = SDL_GetTicksNS();
        // (run-ahead's kept frame: taken as the game's next, or the board put back, before anything else of the loop
        // can touch the board)
        if (!aheadKept.frames.empty()) {
            scriptAt(frameCount);
            uint32_t now[3] = {};
            const bool quiet = controls.Sample(now);
            AheadKept::Frame &first = aheadKept.frames.front();
            if (quiet && aheadKeepable() && std::equal(now, now + 3, first.held)) {
                hw.NetDrop(first.place); // (the game's own frame is now the one after it)
                aheadKept.took = std::move(first);
                aheadKept.frames.pop_front();
                aheadKept.taken = true;
                aheadKept.kept++;
            } else {
                hw.NetBack(first.place); // (the game's own frame, as it was before the first frame ahead)
                for (const AheadKept::Frame &f : aheadKept.frames) {
                    hw.NetDrop(f.place);
                }
                aheadKept.frames.clear();
                aheadKept.again++;
            }
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (net && e.type != SDL_EVENT_QUIT && e.type != SDL_EVENT_GAMEPAD_ADDED && e.type != SDL_EVENT_GAMEPAD_REMOVED) {
                // (online: the chat and Esc's "Leave the match?" in a match; Esc gives up the wait, or closes after it)
                if (netOn) {
                    bool leave = false;
                    netScreen.Event(e, *net, window, tick, leave);
                    if (leave && hostedFromMenu) {
                        backToSolo(false); // (a match hosted from the menu: the player's game goes on alone)
                    } else if (leave) {
                        running = false;
                    }
                } else if ((e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_ESCAPE) ||
                           (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && e.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)) {
                    if (hostedFromMenu) {
                        backToSolo(false);
                    } else {
                        running = false;
                    }
                }
                continue;
            }
            if (touch.Event(e)) {
                continue; // (a finger: the touch controls')
            }
            recomp::Menu::Chosen clicked;
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (menu.IsOpen() && recomp::sdl::MouseEvent(e, window, menuPlace(), menu, clicked)) {
                handleChosen(clicked); // (the mouse over the menu: the menu's picture shown into place())
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && recomp::sdl::MenuKey(e.key.scancode, tabFree)) {
                toggleMenu();
            } else if (e.type == SDL_EVENT_KEY_DOWN && menu.IsOpen()) {
                // (the menu's buttons are asked as held, below)
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
                       (e.key.scancode == SDL_SCANCODE_F11 || (e.key.scancode == SDL_SCANCODE_RETURN && (e.key.mod & SDL_KMOD_ALT)))) {
                SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
            } else if (e.type == SDL_EVENT_KEY_DOWN && keyBound(e.key.scancode)) {
                // (a key the player has given to the game does only that)
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_P) {
                stop(!paused);
            } else if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_O) {
                if (paused) {
                    steps++; // (held: frame after frame)
                } else {
                    stop(true);
                }
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F5) {
                say(saveState(menu.Slot()) ? "STATE SAVED  SLOT " + std::to_string(menu.Slot()) : "STATE NOT SAVED");
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F9) {
                say(loadState(menu.Slot()) ? "STATE LOADED  SLOT " + std::to_string(menu.Slot()) : "NO STATE IN SLOT " + std::to_string(menu.Slot()));
            } else if (e.type == SDL_EVENT_KEY_DOWN && (e.key.scancode == SDL_SCANCODE_F6 || e.key.scancode == SDL_SCANCODE_F7)) {
                menu.SetSlot(menu.Slot() + (e.key.scancode == SDL_SCANCODE_F7 ? 1 : -1));
                say("SLOT " + std::to_string(menu.Slot()));
            } else if (e.type == SDL_EVENT_KEY_DOWN && (e.key.scancode == SDL_SCANCODE_MINUS || e.key.scancode == SDL_SCANCODE_EQUALS)) {
                // (slower, faster: the menu's Speed row's speeds)
                const int now = menu.Speed() < 0 ? recomp::layout::kFull : menu.Speed();
                menu.SetSpeed(std::clamp(now + (e.key.scancode == SDL_SCANCODE_EQUALS ? 1 : -1), 0, recomp::layout::kSpeedCount - 1));
                speed = menu.Speed() < 0 ? recomp::layout::kFull : menu.Speed();
                say(std::string("SPEED ") + recomp::layout::SpeedOf(speed).name);
            } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat && e.key.scancode == SDL_SCANCODE_F12) {
                screenshot();
            } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                if (recomp::sdl::MenuButton(static_cast<SDL_GamepadButton>(e.gbutton.button))) {
                    toggleMenu();
                }
            } else if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
                for (auto &p : seats) {
                    if (!p) {
                        p = SDL_OpenGamepad(e.gdevice.which);
                        if (p) {
                            // (which pads the game has, in game.log: Steam Input's, or SDL's own driver's)
                            const char *name = SDL_GetGamepadName(p);
                            printf("gamepad: %s\n", name ? name : "(no name)");
                        }
                        break;
                    }
                }
                seated();
            } else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) {
                for (auto &p : seats) {
                    if (p && SDL_GetGamepadID(p) == e.gdevice.which) {
                        SDL_CloseGamepad(p);
                        p = nullptr;
                    }
                }
                seated();
            }
            if (((e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) || e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) && !menu.IsOpen() && !lag.pressedAt &&
                lagOn()) {
                lag.pressedAt = e.common.timestamp; // (the input lag readout: the port's part)
            }
        }
        // (the menu's taps, taken with the fingers: menuFinger)
        for (const recomp::Menu::Chosen &chosen : menuTaps) {
            handleChosen(chosen);
        }
        menuTaps.clear();
        // The menu's buttons (not in an online match's screens, which have the events above): Start and Select together
        // open it and close it; in it, what is newly pressed, and the arrows again and again while held
        if (!net) {
            // (the touch controls' Menu opens it, and closes it)
            if (touch.TakeMenuPress()) {
                toggleMenu();
            } else if (chord.Step(controls.pads, 2)) {
                toggleMenu();
            } else if (menu.IsOpen() && !chord.held) {
                if (const unsigned pressed = menuRepeat.Step(menuHeld())) {
                    handleChosen(menu.Buttons(pressed));
                }
            }
        } else if (netOn && !netScreen.Typing() && chord.Step(controls.pads, 2)) {
            // (in a match, Start and Select together ask "Leave the match?" as Esc does, and take it back: a gamepad whose
            // Guide Steam has, with no other menu button, can still leave)
            netScreen.leaving = !netScreen.leaving;
        }
        followShader();
        if (!shaderProblem.empty()) {
            // (a preset that couldn't be drawn: why, over the picture for ten seconds, and in the menu if it is open)
            say(shaderProblem);
            noteFrames = 600;
            if (menu.IsOpen()) {
                menu.Note(shaderProblem);
            }
            shaderProblem.clear();
        }
        followShape();
        // (the Arcade monitor page's screen or resolution changed: the menu can put them back to off)
        arcade.Follow(window, arcadeChoices());
        if (!running) {
            break;
        }
        if (rendererAt < features.size() && features[rendererAt] != rendererNoted) {
            rendererNoted = features[rendererAt];
            if (rendererNoted != rendererAtStart) {
                say("RENDERER: WHEN THE GAME STARTS AGAIN"); // (the menu's change, used at the next start)
            }
        }
        if (beginNowAt && frameCount == beginNowAt) {
            beginNowAt = 0;
            beginWanted = true;
        }
        if (beginWanted && !netOn) {
            if (ThisGame().command) {
                ThisGame().command(-1); // (the game itself at the stage: no mode)
            }
            ThisGame().beginNow(); // (the board made anew is run on unseen to the stage: Game::unseen)
            resetWanted = true;
        }
        beginWanted = false;
        if (gameCommandWanted >= 0 && !netOn) {
            ThisGame().command(gameCommandWanted); // (an alternative mode begun: the board made anew is run to it)
            resetWanted = true;
        }
        gameCommandWanted = -1;
        if (resetWanted) {
            resetWanted = false;
            endAttract();
            resetBoard();
        }
        if ((attractWanted || (attractAt && frameCount == attractAt)) && !netOn) {
            // (attract mode: no other mode, the board started again with no coin in)
            if (ThisGame().command) {
                ThisGame().command(-1);
            }
            endAttract();
            beginAttract();
            resetBoard();
            say("ATTRACT MODE  START OR A COIN ENDS IT");
        }
        attractWanted = false;
        tick++;
        // Hosting from the menu: the row chosen (or --net-menu-host's frame), and the wait for a player kept going while
        // the game is played (when one comes, netGame.coming makes the wait the match's: `net`)
        if (hostWanted || (netMenuHostAt && frameCount == netMenuHostAt && !net && !waitingHost)) {
            hostWanted = false;
            hostFromMenu();
        }
        if (waitingHost) {
            std::string why;
            port::Netplay *host = waitingHost.get();
            if (host->Waiting(netGame, why) < 0 && waitingHost.get() == host) {
                fprintf(stderr, "online: stopped hosting: %s\n", why.c_str());
                hostNote = "STOPPED HOSTING: " + why;
                hostNoteUntil = tick + 300;
                stopHosting();
            }
        }
        if (net && !netOn && hostedFromMenu && !net->Found() && netEnded.empty()) {
            // (the one who came went quiet before the match began: the player's own game again, still hosted)
            fprintf(stderr, "online: the other player went quiet before the match began; still hosting\n");
            waitingHost = std::move(net);
            hostedFromMenu = false;
            if (!ownFeatures.empty()) {
                features = ownFeatures;
                ownFeatures.clear();
            }
            setHostRow(true);
        }
        if (net && !netOn) {
            // (the wait for the other player; the match begins when Waiting says so)
            std::string why;
            const int state = netEnded.empty() ? net->Waiting(netGame, why) : 0;
            if (state < 0 && hostedFromMenu) {
                fprintf(stderr, "online: %s\n", why.c_str());
                hostNote = "STOPPED HOSTING: " + why;
                hostNoteUntil = tick + 300;
                backToSolo(false);
                continue;
            } else if (state < 0) {
                netEnded = why;
                fprintf(stderr, "online: %s\n", why.c_str());
            } else if (state > 0) {
                netOn = true;
                netGuest = !net->Hosting();
                bootUnseen(net.get()); // (straight to the title, as the host has it: both sides alike)
                localIn = false;
                frameCount = 0;
                fprintf(stderr, "online: player 1 %s, player 2 %s\n", net->Name(0).empty() ? "(no name)" : net->Name(0).c_str(),
                        net->Name(1).empty() ? "(no name)" : net->Name(1).c_str());
            }
            if (!netOn) {
                if (stopAfter && !netEnded.empty()) {
                    running = false;
                }
                netWaitScreen();
                SDL_Delay(16);
                next = std::chrono::steady_clock::now();
                continue;
            }
            next = std::chrono::steady_clock::now();
        }
        if (netOn) {
            netScreen.Hear(*net, tick);
            if (!online.say.empty() && frameCount == online.sayAt) {
                netScreen.Said(*net, online.say, tick);
            }
            if (!net->Ready()) {
                if (!net->ended.empty()) {
                    // (the match is over: a host waits for the other player again with the same code, and a joiner whose
                    // connection dropped goes back to the host; the two then begin again from the board's reset)
                    fprintf(stderr, "online: %s\n", net->ended.c_str());
                    net->Report(true);
                    if (hostedFromMenu) {
                        // (a match hosted from the menu: the player's game goes on alone, still hosted if the other
                        // player left or dropped)
                        hostNote = net->ended;
                        hostNoteUntil = tick + 300;
                        backToSolo(net->left || net->lost);
                        continue;
                    }
                    const bool again = !stopAfter && (net->Hosting() ? net->left || net->lost : net->lost);
                    if (again) {
                        port::Netplay::Settings settings = online.net;
                        if (net->Hosting() && !net->code.empty()) {
                            settings.renewCode = net->code.substr(0, 3) + net->code.substr(4);
                            settings.renewSecret = net->Secret();
                        }
                        net.reset(); // (the socket closed: the port is free again)
                        if (settings.hosting) {
                            resetBoard();
                        }
                        netBegin(settings);
                    } else {
                        netEnded = net->ended;
                        netOn = false;
                    }
                    continue;
                }
                // (waiting a tick: the other side's controls are too far behind, or this side is ahead of it in time)
                next += frameTime;
                std::this_thread::sleep_until(next);
                continue;
            }
        }
        if (menu.IsOpen()) {
            drawMenu();
            show(shown, frozenW, frozenH);
            SDL_Delay(16);
            next = std::chrono::steady_clock::now();
            arcade.Menu(); // (the menu's time is not the game's: the rates measured, and the frame delay, start again)
            pauseShown = !paused; // (stopped, the picture comes back from under the menu)
            continue;
        }
        if (stepTestAt && frameCount == stepTestAt && !paused) {
            fprintf(stderr, "step test: stopped at frame %llu, %llu frames on one at a time\n", static_cast<unsigned long long>(frameCount),
                    static_cast<unsigned long long>(stepTestLeft));
            stop(true);
        }
        if (paused) {
            // (--step-test: a frame on at a time, then on as before)
            if (stepTestAt && !steps) {
                if (stepTestLeft) {
                    stepTestLeft--;
                    steps = 1;
                } else {
                    fprintf(stderr, "step test: on again at frame %llu\n", static_cast<unsigned long long>(frameCount));
                    stop(false);
                    stepTestAt = 0;
                }
            }
            // (fast forward's controls go on by a frame while stopped: held, frame after frame after a moment)
            SDL_PumpEvents();
            const bool *keys = SDL_GetKeyboardState(nullptr);
            const bool stepping = ((port::BoundBits(table, bindings, 0, keys, controls.pads[0]) |
                                    port::BoundBits(table, bindings, 1, keys, controls.pads[1])) & port::kFastBit) != 0;
            stepHeld = stepping ? stepHeld + 1 : 0;
            if (stepHeld == 1 || (stepHeld > 24 && stepHeld % 4 == 0)) {
                steps++;
            }
        }
        if (paused && !steps && (!stopAfter || stepTestAt)) {
            if (!pauseShown) {
                // (the frame shown again, with its number: the game's own frame, as run-ahead's are not shown stopped)
                pauseShown = true;
                int w = 0, h = 0;
                canvasSize(w, h);
                hud.Begin(w, h, menuTurnNow());
                hud.Note(("STOPPED  FRAME " + std::to_string(frameCount)).c_str());
                hud.Changed();
                hw.SetOverlay(hud);
                if (shading) {
                    shading->SetOverlay(hud.pixels.data(), hud.width, hud.height);
                }
                turnPicture(!useGpu && !shading);
                presentPicture();
            }
            SDL_Delay(16);
            next = std::chrono::steady_clock::now();
            arcade.Menu(); // (stopped: as the menu's time)
            continue;
        }
        steps -= steps > 0;

        if (!drags.empty()) {
            int ow = 0, oh = 0;
            SDL_GetWindowSizeInPixels(window, &ow, &oh);
            for (const Drag &d : drags) {
                const uint64_t end = d.from + d.count + d.hold;
                if (frameCount >= d.from && frameCount <= end) {
                    const float t = std::min(1.0f, static_cast<float>(frameCount - d.from) / static_cast<float>(d.count));
                    const float x = (d.x0 + (d.x1 - d.x0) * t) * ow, y = (d.y0 + (d.y1 - d.y0) * t) * oh;
                    touch.Scripted(frameCount == d.from ? SDL_EVENT_FINGER_DOWN : frameCount == end ? SDL_EVENT_FINGER_UP : SDL_EVENT_FINGER_MOTION, x,
                                   y);
                }
            }
            if (const std::string steering = touch.Steering(); !steering.empty()) {
                printf("drag %llu: %s\n", static_cast<unsigned long long>(frameCount), steering.c_str());
            }
        }
        controls.scripted[0] = controls.scripted[1] = controls.scriptedSystem = 0;
        for (const Held &h : held) {
            if (frameCount >= h.from && frameCount < h.from + h.count) {
                (h.player < 0 ? controls.scriptedSystem : controls.scripted[h.player]) |= h.bits;
            }
        }
        // (attract mode: ended by Start or a coin, before the frame, so that the frame reads them as the game's)
        if (attract) {
            SDL_PumpEvents();
            const bool *keys = SDL_GetKeyboardState(nullptr);
            const uint32_t bits = port::BoundBits(table, bindings, 0, keys, controls.pads[0]) | port::BoundBits(table, bindings, 1, keys, controls.pads[1]) |
                                  (controls.touch ? controls.touch->Read(false) : 0u);
            if ((bits & (port::kStartBit | port::kCoinBit)) || controls.coinFrames > 0 ||
                (controls.scriptedSystem & (kSysStart1 | kSysStart2 | kSysCoin1))) {
                endAttract();
                say("ATTRACT MODE ENDED");
            }
        }
        controls.attract = attract;
        // The training tools' buttons (either player's), each only with its feature on
        const bool pointOn = features[FeatureIndex(ToolkitFeature::PracticePoint)] != 0;
        const bool rewindOn = features[FeatureIndex(ToolkitFeature::Rewind)] != 0;
        const bool fastOn = features[FeatureIndex(ToolkitFeature::FastForward)] != 0;
        uint32_t tools = controls.scripted[0] | controls.scripted[1];
        if (pointOn || rewindOn || fastOn) {
            SDL_PumpEvents();
            const bool *keys = SDL_GetKeyboardState(nullptr);
            tools |= port::BoundBits(table, bindings, 0, keys, controls.pads[0]) | port::BoundBits(table, bindings, 1, keys, controls.pads[1]);
        }
        const uint32_t toolsAll = tools;
        tools &= (pointOn ? port::kPointBackBit | port::kPointSetBit : 0) | (rewindOn ? port::kRewindBit : 0) | (fastOn ? port::kFastBit : 0);
        tools |= pointOn ? toolsAll & port::kReplayBit : 0;
        // A game being run unseen to where a mode begins (the game says so, and with what controls): no picture but
        // a few a second, no sound, as fast as the machine goes
        const int unseenBits = ThisGame().unseen ? ThisGame().unseen(*board, features) : -1;
        const bool unseen = unseenBits >= 0;
        if (unseen) {
            unseenControls = {{static_cast<uint32_t>(unseenBits) & 0xFFFFu, 0,
                                (unseenBits & kUnseenStart ? kSysStart1 : 0u) | (unseenBits & kUnseenCoin ? kSysCoin1 : 0u)}};
            if (unseenFrames++ == 0 && audio) {
                SDL_ClearAudioStream(audio);
            }
            note = "ON THE WAY THERE...";
            noteFrames = 2;
        } else if (unseenFrames) {
            // (there: this is where practice begins)
            unseenFrames = 0;
            if (pointOn) {
                hw.PointSet();
                attempt.clear();
                say("PRACTICE POINT SET HERE");
            }
            next = std::chrono::steady_clock::now();
        }
        // (straight to the title: until the board's frame there; turned off meanwhile, the picture comes back at once)
        if (booting && (netOn || !skipBoot() || board->frame >= titleFrame)) {
            booting = false;
            next = std::chrono::steady_clock::now();
        }
        const uint32_t bootControls[3] = {controls.scripted[0], controls.scripted[1], controls.scriptedSystem};
        if (paused) {
            tools &= ~port::kFastBit; // (stopped, fast forward's controls go on by a frame: not faster)
        }
        const bool fast = (tools & port::kFastBit) != 0;
        if (fast && !unseen) {
            note = "FAST FORWARD";
            noteFrames = 2;
        }
        const uint32_t toolsPressed = tools & ~toolsBefore;
        toolsBefore = tools;
        if (!rewindOn && timeline.Begun()) {
            timeline.Clear();
        }
        if (!pointOn && point.IsSet()) {
            point.Clear();
        }
        if (toolsPressed & port::kPointSetBit) {
            timed("point set", [&] { hw.PointSet(); });
            attempt.clear();
            replaying = false;
            say("PRACTICE POINT SET");
        }
        if ((toolsPressed & port::kReplayBit) && point.IsSet()) {
            // (the attempt just made, or the one being replayed again)
            if (!replaying) {
                replay.swap(attempt);
            }
            hw.PointBack();
            attempt.clear();
            replayAt = 0;
            replaying = !replay.empty();
            if (audio) {
                SDL_ClearAudioStream(audio);
            }
        }
        if (toolsPressed & port::kPointBackBit) {
            if (point.IsSet()) {
                timed("point back", [&] { hw.PointBack(); });
                if (audio) {
                    SDL_ClearAudioStream(audio);
                }
                if (!replaying && !attempt.empty()) {
                    replay.swap(attempt);
                }
                attempt.clear();
                replaying = false;
                say("BACK TO THE PRACTICE POINT");
            } else {
                say("NO PRACTICE POINT SET YET");
            }
        }
        // Rewind: while held, a moment further back every other frame (three times the speed of play, backwards)
        const bool rewinding = (tools & port::kRewindBit) != 0;
        if (rewinding) {
            if (rewindHeld++ % 2 == 0) {
                bool went = false;
                timed("step back", [&] { went = hw.TimelineStepBack(); });
                rewoundMoments += went && rewindHeld > 1;
            }
            char words[48];
            snprintf(words, sizeof words, "REWIND  -%.1f s", rewoundMoments * kMomentFrames / 60.0);
            note = words;
            noteFrames = 2;
            if (audio) {
                SDL_ClearAudioStream(audio);
            }
            hw.Sound().clear();
        } else {
            rewindHeld = rewoundMoments = 0;
        }

        hw.SetTiming(FeatureIndex(ToolkitFeature::Slowdown) < features.size() && features[FeatureIndex(ToolkitFeature::Slowdown)] == 0); // (0: on)
        if (!rewinding) {
        if (replaying && replayAt >= replay.size()) {
            replaying = false;
            say("YOURS FROM HERE");
        }
        // (the input lag readout's look at the game, in play, now and then)
        if (lagOn() != lag.wasOn) {
            lag = {};
            lag.wasOn = lagOn();
            hw.LookDrop();
        }
        if (lag.wasOn && !unseen && !booting && !replaying && !fast && !checkVideo) {
            const bool inPlay = !ThisGame().playing || ThisGame().playing(*board);
            if (inPlay && lag.away >= kLagAway) {
                lag.nextAt = std::min(lag.nextAt, frameCount); // (back in play after a while out of it: a look soon)
            }
            lag.away = inPlay ? 0 : lag.away + 1;
            if (lag.look) {
                lagStep();
            } else if (frameCount >= lag.nextAt && inPlay) {
                lag.nextAt = frameCount + (lag.tries < kLagTries || !lagSteady() ? kLagEvery : kLagSteadyEvery);
                lagBegin();
            }
        }
        bool lateFrame = false;
        const bool frameTaken = aheadKept.taken && !netOn;
        if (frameTaken) {
            // (the frame ahead, run last time, is this one: its sound now)
            aheadKept.taken = false;
            lateFrame = aheadKept.took.late;
            hw.Sound().insert(hw.Sound().end(), aheadKept.took.sound.begin(), aheadKept.took.sound.end());
        } else if (netOn) {
            net->Frame(netGame, netControls()); // (online: rollback first if a guess was wrong, then the frame: netGame.step)
            lateFrame = netLate;
        } else {
        controls.forced = unseen ? unseenControls.held : replaying ? replay[replayAt++].held : booting ? bootControls : nullptr;
        if (replaying) {
            note = "REPLAY";
            noteFrames = 2;
        }
        if (rollbackTest) {
            // (--rollback-test: the frame run first unseen with the controls changed, as a wrong guess of online play's,
            // and the game put back: what follows must be as if it had never been)
            const uint32_t *before = controls.forced;
            const uint32_t wrong[3] = {board->inputs[0] ^ 0x1D, board->inputs[1] ^ 0x3B, board->system ^ kSysStart2};
            hw.NetKeep(0);
            controls.forced = wrong;
            controls.readThisFrame = false;
            if (ThisGame().beforeFrame) {
                ThisGame().beforeFrame(*board, features);
            }
            const size_t sound = hw.Sound().size();
            hw.NetFrame();
            hw.Sound().resize(sound);
            hw.NetBack(0);
            hw.NetDrop(0);
            controls.forced = before;
        }
        controls.readThisFrame = false;
        if (ThisGame().beforeFrame) {
            ThisGame().beforeFrame(*board, features);
        }
        hw.BeforeFrame();
        board->RunFrame();
        lateFrame = hw.AfterFrame(rewindOn);
        }
        // (the game's own speed: whether it had a new picture this frame, over the last second)
        lateFrames -= late[lateAt];
        late[lateAt] = lateFrame;
        lateFrames += late[lateAt];
        lateAt = (lateAt + 1) % 60;
        if (inputs) {
            fprintf(inputs, "%llu %02X %02X %03X\n", static_cast<unsigned long long>(board->frame), board->inputs[0], board->inputs[1], board->system);
        }
        if (point.IsSet() && attempt.size() < 60 * 60 * 30) {
            attempt.push_back({{board->inputs[0], board->inputs[1], board->system}});
        }
        hw.CheckFrame(frameCount);
        if (!netOn && !frameTaken && !controls.readThisFrame) {
            controls.Read(); // (a frame in which the game read nothing: the switches still follow the host)
        }
        if (!netOn && !frameTaken && ThisGame().afterFrame) {
            ThisGame().afterFrame(*board, features);
        }
        } // (not rewinding)
        frameCount++;
        port::TestCrashAt(frameCount); // (only with SPELUNKOMP_TEST_CRASH_AT_FRAME set: port/crash_log.h)
        if (frameCount % 300 == 0) {
            saveKept();
        }
#ifdef __ANDROID__
        // (a phone's speed, in game.log every ten seconds: the frames the port ran a second, and the longest frame)
        {
            static uint64_t speedFrom = SDL_GetTicksNS(), speedFrames = 0, longest = 0, before = SDL_GetTicksNS();
            const uint64_t now = SDL_GetTicksNS();
            longest = std::max(longest, now - before);
            before = now;
            if (++speedFrames >= 600) {
                printf("speed: %.1f frames a second, longest frame %.1f ms\n", speedFrames * 1e9 / static_cast<double>(now - speedFrom),
                       longest / 1e6);
                speedFrom = now;
                speedFrames = longest = 0;
            }
        }
#endif

        // Run-ahead: the game run on unseen for the picture a few frames from now (the controls as they are), then put
        // back as it was
        if (unseen && unseenFrames % 20 != 1) {
            hw.Sound().clear();
            continue; // (not shown)
        }
        if (booting && !pictures && !shots.count(frameCount) && !windowShots.count(frameCount) && !ramDumps.count(frameCount) &&
            std::none_of(menuTests.begin(), menuTests.end(), [&](const MenuTest &t) { return t.frame == frameCount; })) {
            hw.Sound().clear();
            if (stopAfter && frameCount >= stopAfter && !waitingHost) {
                running = false;
            }
            if (frameCount % 15 == 1) {
                // (a CV1000's boot takes a few seconds even so: said, on black, a few times a second)
                int w = 0, h = 0;
                canvasSize(w, h);
                hud.Begin(w, h, menuTurnNow());
                hud.Text(0.5f, 0.5f, 0, "STRAIGHT TO THE TITLE...", 0xFFB0B0B0u);
                hud.Changed();
                shown.assign(static_cast<size_t>(w) * h, 0xFF000000u);
                hud.Over(shown.data());
                show(shown, w, h);
            }
            continue; // (straight to the title: the boot not shown; a check's picture of a frame of it still is)
        }
        // (nor stopped, a frame on at a time, slowed or faster: there the frame shown is the game's own, as cps2recomp's)
        const int ahead = checkVideo || rewinding || unseen || booting || paused || speed != recomp::layout::kFull
                              ? 0
                              : features[FeatureIndex(ToolkitFeature::RunAhead)];
        const bool wantPixels = (!useGpu && !shading) || pictures || shots.count(frameCount);
        uint32_t aheadHeld[3] = {};
        const bool keepAhead = (ahead == 1 || ahead == 2) && !netOn && controls.Sample(aheadHeld) && aheadKeepable() &&
                               static_cast<int>(aheadKept.frames.size()) <= ahead;
        if (!keepAhead && !aheadKept.frames.empty()) {
            // (run-ahead 2's second frame still ahead, and now not to be kept: the board back to the game's own)
            hw.NetBack(aheadKept.frames.front().place);
            aheadForget();
        }
        if (keepAhead) {
            // (the frames ahead run as the game's own, each kept before it is run, and the board left at the last:
            // aheadKept. Those still ahead from the last frame are kept as they are: their controls are these)
            while (static_cast<int>(aheadKept.frames.size()) < ahead) {
                AheadKept::Frame f;
                f.place = aheadKept.frames.empty() ? 0 : 1 - aheadKept.frames.back().place;
                std::copy(aheadHeld, aheadHeld + 3, f.held);
                hw.NetKeep(f.place);
                controls.forced = aheadHeld;
                controls.forcedAutofire = true;
                controls.readThisFrame = false;
                if (ThisGame().beforeFrame) {
                    ThisGame().beforeFrame(*board, features);
                }
                const size_t soundBefore = hw.Sound().size();
                hw.BeforeFrame();
                board->RunFrame();
                f.late = hw.AfterFrame(false);
                if (!controls.readThisFrame) {
                    controls.Read();
                }
                if (ThisGame().afterFrame) {
                    ThisGame().afterFrame(*board, features);
                }
                controls.forced = nullptr;
                controls.forcedAutofire = false;
                f.sound.assign(hw.Sound().begin() + static_cast<std::ptrdiff_t>(soundBefore), hw.Sound().end());
                hw.Sound().resize(soundBefore);
                aheadKept.frames.push_back(std::move(f));
            }
        } else if (ahead > 0) {
            hw.BeginAhead();
            for (int i = 0; i < ahead; i++) {
                if (ThisGame().beforeFrame) {
                    ThisGame().beforeFrame(*board, features);
                }
                hw.AheadFrame();
            }
        }
        // What is written over the picture (the tools' notes), from the frame shown (with run-ahead, the frame ahead)
        {
            int canvasW = 0, canvasH = 0;
            canvasSize(canvasW, canvasH);
            hud.Begin(canvasW, canvasH, menuTurnNow());
            if (paused) {
                hud.Note(("STOPPED  FRAME " + std::to_string(frameCount)).c_str()); // (a frame on: which it is)
            } else if (noteFrames > 0) {
                noteFrames--;
                hud.Note(note.c_str());
            }
            if (features[FeatureIndex(ToolkitFeature::InputDisplay)]) {
                hud.Inputs(board->inputs[0]);
            }
            if (const int readout = features[FeatureIndex(ToolkitFeature::SpeedReadout)]) {
                char words[40];
                if (readout == 2) {
                    // (the host's time as the last second's mean, changed once a second as the percentage is read)
                    if (frameCount % 60 == 0 || hostShown == 0) {
                        hostShown = hostTime.RecentMs();
                    }
                    snprintf(words, sizeof words, "%3u%%  HOST %.1f MS", (60 - lateFrames) * 100 / 60, hostShown);
                } else {
                    snprintf(words, sizeof words, "%3u%%", (60 - lateFrames) * 100 / 60);
                }
                hud.Corner(words);
            }
            if (lagOn() && !replaying) {
                hud.Line(lagLine(ahead).c_str());
            }
            if (fpsAt < features.size() && features[fpsAt]) {
                char words[16];
                snprintf(words, sizeof words, "%.0f FPS", fpsRate);
                hud.Text(0.985f, 0.015f, 1, words);
            }
            if (hitBoxesAt < features.size() && features[hitBoxesAt] && ThisGame().hitBoxes) {
                MarkHitBoxes(hud, *board, turnNow(), features);
            }
            // (the scoring readouts: the game's hidden counters, down the left side (1) or the right (2), under the
            // tools' notes and the FPS)
            if (const size_t at = FeatureIndex("readouts"); at < features.size() && features[at]) {
                const bool right = features[at] == 2;
                float y = 0.10f;
                for (const Readout &r : ThisGame().readouts) {
                    if (const std::string value = r.value(*board); !value.empty()) {
                        hud.Text(right ? 0.985f : 0.015f, y, right ? 1 : -1, std::string(r.name) + " " + value, 0xFFFFFFFFu);
                        y += 0.045f;
                    }
                }
            }
            // (an alternative mode's readout: Dodge training's pattern and hits, over the boss's life bar, clear of the tools' notes)
            if (ThisGame().readout) {
                if (const std::string words = ThisGame().readout(*board); !words.empty()) {
                    hud.Text(0.5f, 0.036f, 0, words, 0xFFFFFFFFu);
                }
            }
            // (hosting from the menu, the game the player's own meanwhile: the match's code to give; or why it stopped)
            if (waitingHost) {
                const std::string &code = waitingHost->code;
                hud.Text(0.5f, 0.955f, 0,
                         code.empty() ? "HOSTING  UDP PORT " + std::to_string(hostSettings.port)
                                      : "HOSTING  CODE " + code + (waitingHost->Listed() ? "  IN THE LOBBY" : ""),
                         0xFF78E660u);
            } else if (!netOn && tick < hostNoteUntil) {
                hud.Text(0.5f, 0.955f, 0, hostNote, 0xFF40FFFFu);
            }
            if (netOn) {
                auto on = [&](const char *name) { return FeatureIndex(name) < features.size() && features[FeatureIndex(name)] != 0; };
                netScreen.Match(hud, *net, localIn, on("online_ping"), on("online_delay"), on("online_hide_names"), tick, frameCount);
            }
            const bool changed = hud.Changed();
            if (changed) {
                hw.SetOverlay(hud);
            }
            if (shading && (changed || overlayStale)) {
                shading->SetOverlay(hud.Any() ? hud.pixels.data() : nullptr, hud.width, hud.height);
                overlayStale = false;
            }
        }

        turnPicture(wantPixels);
        if (pictures) {
            uint64_t h = 0xCBF29CE484222325ull;
            for (uint32_t p : picture) {
                h = (h ^ p) * 0x100000001B3ull;
            }
            fprintf(pictures, "%llu %016llx\n", static_cast<unsigned long long>(frameCount), static_cast<unsigned long long>(h));
        }
        if (const auto it = windowShots.find(frameCount); it != windowShots.end()) {
            windowShotNow = it->second;
        }
        if (useGpu && !shading && !windowShotNow.empty()) {
            int shotW = 0, shotH = 0;
            canvasSize(shotW, shotH);
            hw.WindowShot(windowShotNow, turnNow(), shotW, shotH);
            windowShotNow.clear();
        }
        // Paced by the screen (full screen at an arcade monitor's mode, Keep the board's pace off, not online; at the
        // board's speed, not a check's run): the picture waits for the screen's refresh, and the frame delay then holds
        // the next frame, its controls read where the game reads them, as late before its refresh as the work allows
        const bool paced = arcade.Paced(window, arcadeChoices(), net != nullptr) && !fast && speed == recomp::layout::kFull &&
                           !(stopAfter && !realTime);
        setVsync(paced);
        arcade.WorkDone();
        if (!paused && !menu.IsOpen()) {
            const uint64_t workTo = SDL_GetTicksNS();
            hostTime.Add(workTo - workFrom, workTo);
        }
        presentPicture();
        fpsFrames++;
        if (const uint64_t now = SDL_GetTicksNS(); now - fpsFrom >= 500000000ull) {
            fpsRate = fpsFrames * 1e9 / static_cast<double>(now - fpsFrom);
            fpsFrom = now;
            fpsFrames = 0;
        }
        if (lag.pressedAt && !rewinding) {
            // (the frame that read the press is on its way to the screen)
            const double ms = static_cast<double>(SDL_GetTicksNS() - lag.pressedAt) / 1e6;
            lag.pressedAt = 0;
            if (ms >= 0 && ms < 250) {
                lag.portMs = lag.presses ? lag.portMs * 0.9 + ms * 0.1 : ms;
                lag.presses++;
            }
        }
        if (ahead > 0 && aheadKept.frames.empty()) {
            hw.EndAhead();
        }
        // (a moment of the timeline is kept once the frame is on show, so keeping it never holds the picture up)
        if (rewindOn && !rewinding && (!timeline.Begun() || timeline.Frames() >= kMomentFrames)) {
            timed("keep", [&] { hw.TimelineKeep(); });
        }

        if (audio && hw.SampleRate() != audioRate) {
            openAudio();
        }
        // Sound (docs/LAG.md's Sound): the frame's, handed to SDL the moment the frame is run. The game makes it at the
        // board's pace and the device takes it at its own, and the two clocks never quite agree: so the stream is played
        // a touch faster when sound piles up and a touch slower when it runs short (half a percent at most: not heard),
        // which keeps kAudioWaiting of it waiting as each frame's comes, no gaps and no growing delay. With more than
        // kAudioMost waiting (the game catching up after the host held it up) the frame's sound is left out. Slowed or
        // faster (the Speed row) it is played slower or faster by as much, as cps2recomp does; stopped, a frame on plays
        // its own frame's.
        const recomp::layout::Speed rate = recomp::layout::SpeedOf(speed);
        if (audio && !muted && !fast && !unseen && !booting) {
            constexpr double kAudioWaiting = 0.010, kAudioMost = 0.045;
            const double pace = paused ? 1.0 : static_cast<double>(rate.frames) / rate.ticks;
            const double waiting = SDL_GetAudioStreamQueued(audio) / (2.0 * audioRate) / pace;
            audioSeen.empty += audioSteady > 60 && waiting <= 0;
            if (waiting > kAudioMost) {
                audioSeen.dropped++;
            } else {
                const double trim = std::clamp((waiting - kAudioWaiting) * 0.5, -0.005, 0.005);
                SDL_SetAudioStreamFrequencyRatio(audio, static_cast<float>((1.0 + trim) * pace));
                soundFilter.Apply(hw.Sound().data(), hw.Sound().size(), features[soundFilterAt], audioRate);
                SDL_PutAudioStreamData(audio, hw.Sound().data(), static_cast<int>(hw.Sound().size() * 2));
            }
            audioSeen.least = std::min(audioSeen.least, waiting);
            audioSeen.most = std::max(audioSeen.most, waiting);
            audioSeen.sum += waiting;
            audioSeen.frames++;
            audioSteady = paused ? 0 : audioSteady + 1;
        } else {
            audioSteady = 0;
        }
        if (!soundLog.empty()) {
            static FILE *soundFile = fopen(soundLog.c_str(), "wb");
            if (soundFile && !hw.Sound().empty()) {
                fwrite(hw.Sound().data(), 2, hw.Sound().size(), soundFile);
                fflush(soundFile);
            }
        }
        if (audioLog && audioSeen.frames >= 60) {
            fprintf(stderr, "audio: frame %llu, waiting %.1f ms (least %.1f, most %.1f), %d left out, %d ran dry\n",
                    static_cast<unsigned long long>(frameCount), audioSeen.sum * 1000 / audioSeen.frames, audioSeen.least * 1000,
                    audioSeen.most * 1000, audioSeen.dropped, audioSeen.empty);
            audioSeen = {};
        }
        hw.Sound().clear();

        if (const auto it = ramDumps.find(frameCount); it != ramDumps.end()) {
            if (FILE *f = fopen(it->second.c_str(), "wb")) {
                fwrite(board->ram.data(), 1, board->ram.size(), f);
                fclose(f);
            }
        }
        if (const auto it = shots.find(frameCount); it != shots.end()) {
            WritePng(it->second.c_str(), turned.data(), static_cast<uint32_t>(turnedW), static_cast<uint32_t>(turnedH));
        }
        for (const MenuTest &test : menuTests) {
            if (test.frame != frameCount) {
                continue;
            }
            openMenu();
            for (char k : test.keys) {
                if (k >= '0' && k <= '9') {
                    // (a finger's tap, half way across and at that tenth of the way down the window: the menu's taps)
                    int ow = 0, oh = 0;
                    SDL_GetWindowSizeInPixels(window, &ow, &oh);
                    drawMenu();
                    touch.Layout(ow, oh, place());
                    const float y = (static_cast<float>(k - '0') + 0.5f) / 10 * static_cast<float>(oh);
                    touch.Scripted(SDL_EVENT_FINGER_DOWN, ow / 2.0f, y);
                    touch.Scripted(SDL_EVENT_FINGER_UP, ow / 2.0f, y);
                    for (const recomp::Menu::Chosen &chosen : menuTaps) {
                        handleChosen(chosen);
                    }
                    menuTaps.clear();
                    if (!menu.IsOpen()) {
                        break;
                    }
                    continue;
                }
                const unsigned b = k == 'u' ? recomp::kMenuUp : k == 'd' ? recomp::kMenuDown : k == 'l' ? recomp::kMenuLeft
                                 : k == 'r' ? recomp::kMenuRight : k == 'c' ? recomp::kMenuConfirm : recomp::kMenuBack;
                drawMenu();
                handleChosen(menu.Buttons(b));
                if (!menu.IsOpen()) {
                    break;
                }
            }
            if (menu.IsOpen()) {
                drawMenu();
            }
            WritePng(test.shot.c_str(), shown.data(), static_cast<uint32_t>(frozenW), static_cast<uint32_t>(frozenH));
            menu.Close();
        }
        if (stopAfter && frameCount >= stopAfter && !unseen && (!waitingHost || frameCount >= stopAfter + 3600)) {
            running = false; // (a check's run ends once a game run unseen is where it was going; hosting from the menu,
                             // once the match has had its frames, or a minute on if nobody came)
        }
        if ((stopAfter && !realTime && !netOn && !waitingHost) || unseen) {
            continue; // (checks run as fast as the machine goes, but --real-time's; an online match's at the board's speed)
        }
        if (paced) {
            arcade.Shown(); // (the frame delay's wait)
            next = std::chrono::steady_clock::now();
            continue;
        }
        if (fast || speed != recomp::layout::kFull) {
            arcade.Menu(); // (not the board's pace: nothing measured)
        } else {
            arcade.BoardPace(arcade.OnArcade(window)); // (in an arcade mode, the rate as measured goes in the log)
        }
        next += fast ? frameTime / 6 : frameTime * rate.ticks / rate.frames;
        const auto now = std::chrono::steady_clock::now();
        if (next > now) {
            std::this_thread::sleep_until(next);
        } else if (now - next > frameTime * 4) {
            next = now; // (fell behind: start again from now)
        }
    }
    arcade.Close(); // (the desktop's mode as it was)
    if (net && netOn) {
        net->Leave();
        net->Report(true);
    }
    if (netHashes) {
        fclose(netHashes);
    }
    if (netLog) {
        fclose(netLog);
    }
    saveKept();
    if (inputs) {
        fclose(inputs);
    }
    if (pictures) {
        fclose(pictures);
    }
    hostTime.Report(SDL_GetTicksNS());
#ifdef SPELUNK_SHADOW_TIMING
    // (the shadow check's runs of the game program, as the headless runner says it: port/headless.cpp)
    fprintf(stderr, "shadow: %llu events compared with the reference model, all the same; %llu poll-loop skips (%llu turns) the same as the turns run, "
            "%llu of them worked out all at once and the same turn by turn\n",
            static_cast<unsigned long long>(board->cpu.shadowEvents), static_cast<unsigned long long>(board->cpu.shadowSkips),
            static_cast<unsigned long long>(board->cpu.shadowSkipTurns), static_cast<unsigned long long>(board->cpu.shadowAtOnce));
#endif
    if (!aheadKept.frames.empty()) {
        hw.NetBack(aheadKept.frames.front().place); // (the game's own frame, for what is said and kept at the end)
        aheadForget();
    }
    if (aheadKept.kept + aheadKept.again) {
        printf("run-ahead: %llu frames kept, %llu run again\n", static_cast<unsigned long long>(aheadKept.kept),
               static_cast<unsigned long long>(aheadKept.again));
    }
    const int result = hw.Report();
    if (getenv("SPELUNK_REPORT_INPUT")) {
        fprintf(stderr, "input: %llu reads in %llu frames\n", static_cast<unsigned long long>(controls.reads), static_cast<unsigned long long>(frameCount));
    }
    return result;
}
