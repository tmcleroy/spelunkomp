// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; its shape is cps2recomp's.
//
// What spelunkomp tells the launcher the recomp ports share (recomp-common/launcher/src/launcher.h): for each game,
// the words for its ROM set, the check of the player's (read and checked by checksum, as the game does), and what the
// game is started with; and, built as the collection (LAUNCHER_COLLECTION), every game in games/ under one launcher.
#include "launcher.h"

#include "../../port/bindings.h"
#include "../../port/i18n.h"
#include "../../port/netplay.h"
#include "../../runtime/own_shader.h"
#include "../../runtime/romset.h"

#include <cctype>
#include <deque>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace {

// A game's description, from its game.cfg
launcher::Description describe_game(const char *config) {
    port::AddTranslations(); // (the port's words in the player's language, beside recomp-common's)
    auto get = [config](const char *key) { return spelunk::GameConfigValueIn(config, key); };
    // (the words are kept for as long as the launcher runs)
    static std::deque<std::string> words;
    auto keep = [](std::string s) { return words.emplace_back(std::move(s)).c_str(); };
    const std::string set = get("rom_set"), name = get("rom_name"), parents = get("rom_parent");
    // (a split set: its parents' zips beside it, which the game reads too)
    std::string beside;
    {
        std::istringstream names(parents);
        std::string one, list;
        while (names >> one) {
            list += (list.empty() ? "" : " and ") + one + ".zip";
        }
        if (!list.empty()) {
            beside = ", with " + list + " beside it if it is a split set";
        }
    }
    launcher::Description d;
    d.original = "in the arcade";
    // (the launcher shows only what is set before a game starts: port/main.cpp's --list-features marks them; the board's
    // own settings are a card on each game's page, the rest is the in-game menu's)
    d.launcher_features_only = true;
    d.board_category = "Board";
    d.features_name = "Settings";
    d.layout = launcher::Description::Layout::Tabs;
    d.file_heading = "ROM SET";
#ifdef __ANDROID__
    // (Android: the set is copied into the app's folder; the screen's controls have a Menu button, and Back opens it)
    d.file_intro = keep("Choose your own copy of the game: the arcade ROM set " + set + ".zip (" + name + ")" + beside +
                        ". Only this version works. It is copied into the app's folder.\n\nWhile playing, Back or the screen's "
                        "Menu button, or Guide (or Start and Select together) on a gamepad, opens the in-game menu.");
    d.file_empty = keep("None chosen. Choose " + set + ".zip.");
#else
    d.file_intro = keep("Choose your own copy of the game: the arcade ROM set " + set + ".zip (" + name +
                        ")" + beside +
                        ". Only this version works. The files stay where they are: the launcher only reads them.");
    d.file_empty = keep("None chosen. Drop " + set + ".zip here, or choose it.");
#endif
    d.file_not_set_up = "The game isn't set up yet. Open the launcher (without --play) once to choose your ROM set.";
    d.file_filter = {"ROM set (zip)", "zip"};
    d.file_key = "rom";
    // The set's zip (or a folder holding its files). Only ever read.
    d.check = [config, set, name, beside](const std::string &path) {
        launcher::FileCheck check;
        std::error_code error;
        if (path.empty() || !std::filesystem::exists(path, error)) {
            check.message = "It isn't there any more.";
            return check;
        }
        std::string message;
        if (spelunk::CheckSetFor(config, path, message)) {
            check.result = launcher::FileCheck::Result::Good;
            check.message = name;
        } else {
            check.message = "Not usable: " + message + ". This port needs the set " + set + " (" + name +
                            ")" + beside + ". Other versions of the game will not work.";
        }
        return check;
    };
    d.check_on_thread = true; // (reading and checking the set takes a moment)
    // (the set's zip or folder; a merged set: the clone's files in its parent's zip)
    d.candidates = [set, parents](const std::filesystem::path &folder) {
        std::vector<std::filesystem::path> places = {folder / (set + ".zip"), folder / set};
        std::istringstream names(parents);
        std::string one;
        while (names >> one) {
            places.push_back(folder / (one + ".zip"));
        }
        return places;
    };
    d.arguments = [](const launcher::Launch &launch) {
        std::vector<std::string> args = {"--roms", launch.file, "--settings", (launch.folder / "settings.ini").string(),
                                         "--saves", (launch.folder / "saves").string()};
        if (launch.on("fullscreen")) {
            args.push_back("--fullscreen");
        }
        return args;
    };
    d.saves_folder = true;
    // (online play: port/netplay.h; the introduction service and its lobby are cps2recomp's, shared: online/ there)
    d.online = {"--host", "--join", port::Netplay::kDefaultPort, true,
                "Two players, each on their own machine with the game, play its two-player game together: one hosts and gives "
                "the other the code their game shows; the other joins with it. Each presses Start to come in. The match has "
                "the host's board settings and the Game page's features; the training tools are off."};
    d.online.lobby = keep(std::string(port::Netplay::kService) + "/v1/lobby/" + get("id") + "?protocol=" +
                          std::to_string(port::Netplay::kLobbyProtocol));
    d.online.list_argument = "--net-list";
    d.online.name_argument = "--net-name";
    d.online.invite_argument = "--net-invite"; // (a player in the waiting room invited: Collection::room)
    d.online.invite_name_argument = "--net-invite-name";
    d.online.room = keep(std::string(port::Netplay::kService) + "/v1/room");
    d.online.room_protocol = port::Netplay::kLobbyProtocol;
    d.online.game = keep(get("id"));
    d.online.delay_argument = "--net-delay"; // (the host's choice, told to the one joining in the welcome: port/netplay.cpp)
#ifndef __ANDROID__ // (a phone: its one screen, full)
    d.display = {{"fullscreen", "Full screen", "F11 switches while playing.", false, nullptr}};
    // (the Shader's Your own: the player's presets, in the shaders folder every game shares, beside launcher.ini)
    d.own_shaders = {"shader", spelunk::kYourOwnShader};
#endif
    d.bindings = port::MakeBoundTable(2, get("buttons")).table;
    d.controls_heading = "WHILE PLAYING";
#ifdef __ANDROID__
    d.controls = {{"Touch", "The game's buttons on the screen, with Coin, Start and Menu (beside the picture held sideways, under it "
                            "held upright); a finger anywhere else moves the ship as it moves (or an on-screen stick: Touch movement). "
                            "A gamepad's use hides them. Back, a gamepad's Guide, or Start and Select "
                            "together: the in-game menu (save states, speed, features, coin, reset, quit; Menu button, above: "
                            "which gamepad buttons). "
                            "Gamepads: the Controllers page says which is which player's; the rest go to players 1 and 2 "
                            "in the order they were connected."}};
#else
    d.controls = {{"Keys", "Esc, F1, a gamepad's Guide, or Start and Select together: the in-game menu (save states, speed, "
                           "features, coin, reset, quit; Menu button, above: which gamepad buttons)   F5 / F9: save and load a state   P: pause   O: a frame on   "
                           "F2: the board's test switch   F3: service   F11: full screen. "
                           "Gamepads: the Controllers page says which is which player's; the rest go to players 1 and 2 "
                           "in the order they were connected."}};
#endif
    return d;
}

} // namespace

const launcher::Description &launcher::describe() {
    static const Description about = describe_game(spelunk::BuiltInGameConfigText());
    return about;
}

#ifdef LAUNCHER_COLLECTION
const launcher::Collection &launcher::describe_collection() {
    static const Collection about = [] {
        port::AddTranslations();
        Collection c;
        c.subtitle = "Cave arcade shooting games, native";
#ifdef __ANDROID__
        // (Android: the sets are chosen with the system's picker and copied into the app's folder: recomp-common's
        // launcher, App::choose_folder)
        c.intro = "Each game needs your own copy of it: its arcade ROM set. Add the sets from your phone or a drive: they are "
                  "copied into the app's folder.\n\n"
                  "While playing, Back or the screen's Menu button, or Guide (or Start and Select together) on a gamepad, opens "
                  "the in-game menu.";
        c.find_title = "Add your sets";
        c.find_detail = "Choose the ROM sets, as many as you like at once, by the set's name (mushisam.zip, say). Each goes to its game.";
#else
        c.intro = "Each game needs your own copy of it: its arcade ROM set. Show the launcher the folder you keep them in, or "
                  "drop a set (or the folder) on this window. The sets stay where they are: the launcher only reads them.\n\n"
                  "While playing, Esc or F1 on the keyboard, or Guide (or Start and Select together) on a gamepad, opens the "
                  "in-game menu.";
        c.find_title = "Look in a folder";
        c.find_detail = "Finds each game's ROM set in a folder of them, by the set's name (mushisam.zip, say).";
#endif
        static std::deque<std::string> words;
        for (const spelunk::GameEntry &game : spelunk::AllGames()) {
            const char *name = words.emplace_back(spelunk::GameConfigValueIn(game.config, "name")).c_str();
            std::string env = std::string(game.id) + "_HOME";
            for (char &ch : env) {
                ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
            }
            const char *home = words.emplace_back(env).c_str();
            launcher::CollectionGame one{game.id, name, game.id, home, describe_game(game.config), nullptr};
            // (for the Games page's sort and its choice of hardware: when it came out, and the board it ran on)
            const std::string released = spelunk::GameConfigValueIn(game.config, "released");
            one.released = released.empty() ? nullptr : words.emplace_back(released).c_str();
            const std::string hardware = spelunk::GameConfigValueIn(game.config, "hardware");
            one.hardware = hardware == "cave68k" ? "Cave 68000" : hardware == "pgm" ? "IGS PGM" : "CV1000";
            c.games.push_back(std::move(one));
        }
        c.bindings = c.games.front().description.bindings;
        c.file_name = "ROM set";
#ifndef __ANDROID__
        // (the scene diorama, tools/diorama/diorama.html, cps2recomp's: shipped beside the games, opened in the browser)
        c.tools = {{"Scene diorama",
                    "Opens the scenes and clips you save in the games (Game > Scene export) in your browser as 3D cut-outs, to turn, "
                    "pull apart and photograph.",
                    "Open diorama", "diorama.html"}};
#endif
        // (online play's lobby, every game's, and the waiting room: the service cps2recomp's ports use too, its lobby
        // and room kept apart by the protocol said, Netplay::kLobbyProtocol)
        static const std::string lobby = std::string(port::Netplay::kService) + "/v1/lobby?protocol=" + std::to_string(port::Netplay::kLobbyProtocol);
        c.lobby = lobby.c_str();
        static const std::string lookup = std::string(port::Netplay::kService) + "/v1/code/";
        c.code_lookup = lookup.c_str();
        static const std::string room = std::string(port::Netplay::kService) + "/v1/room";
        c.room = room.c_str();
        c.room_protocol = port::Netplay::kLobbyProtocol;
        c.online_features = "Online"; // (the ping, the input delay and the names on the screen: port/online.h)
        c.online_intro = "Play a game's two-player mode with someone on another machine: join a match a player is hosting, or host one in "
                         "any of your games for others to join, or invite someone from the waiting room. A game's own page has its Online "
                         "card too, for a match with a code only the people you give it to know.";
        // (where the players talk: the Discord, at the top of the Online page)
        c.community_url = "https://discord.gg/FSC9GZ6nm";
        c.community_title = "Join us on Discord";
        c.community_detail = "Find players for a match, tell us what to fix or add, and hear about new versions first.";
        c.community_button = "Open Discord";
        return c;
    }();
    return about;
}
#endif
