// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The Hit boxes page (a page of its own under Training mode, in the in-game menu and the launcher): the boxes drawn at all
// ("hit_boxes"), and for each kind of box the game gives (HitBox::Kind) its colour, or hidden ("hit_box_<kind>"). A
// kind's first choice is its own colour, so a settings file that says nothing draws each kind as it always was.
#pragma once

#include "../recomp-common/settings.h"
#include "game.h"

#include <cstdint>
#include <string>
#include <vector>

namespace spelunk::hitboxes {

constexpr const char *kPage = "Hit boxes";

// The colours a kind can be drawn in (red in the low byte, no alpha)
struct Colour {
    const char *name;
    uint32_t rgb;
};
inline const std::vector<Colour> &Palette() {
    static const std::vector<Colour> palette = {
        {"Yellow", 0x00FFFF}, {"Red", 0x2020FF}, {"Pink", 0xFF40FF}, {"Orange", 0x0090FF}, {"Green", 0x40FF40},
        {"Cyan", 0xFFFF20},   {"Blue", 0xFF6030}, {"Purple", 0xFF40A0}, {"White", 0xFFFFFF},
    };
    return palette;
}

struct KindInfo {
    const char *name, *title, *description;
    int colour; // (its own: a place in Palette)
};
inline const KindInfo &Kind(HitBox::Kind kind) {
    static const KindInfo kinds[HitBox::kKinds] = {
        {"hit_box_ship", "Player", "The ship's true hit box: what a bullet's point or an enemy's body must touch.", 0},
        {"hit_box_bullet", "Enemy bullets", "The point of each bullet: it hits when it is inside the ship's box.", 1},
        {"hit_box_body", "Enemies", "The bodies of the enemies that hit the ship when they touch it.", 2},
        {"hit_box_shot", "Player shots", "The player's shots, as the game tests them against the enemies' boxes.", 5},
        {"hit_box_ground", "Ground enemies",
         "Enemies the shots can hurt whose bodies never touch the ship (tanks, turrets, things on the ground): where to "
         "shoot them.",
         4},
        {"hit_box_hidden", "Hidden items",
         "Where each thing the game hides is (a secret bonus), before it is found: faint lines across the picture "
         "meeting there.",
         3},
    };
    return kinds[kind];
}

// A kind's choices: its own colour, the others, and Hidden (the last)
inline std::vector<std::string> Choices(HitBox::Kind kind) {
    const int own = Kind(kind).colour;
    std::vector<std::string> choices{Palette()[static_cast<size_t>(own)].name};
    for (size_t i = 0; i < Palette().size(); i++) {
        if (static_cast<int>(i) != own) {
            choices.push_back(Palette()[i].name);
        }
    }
    choices.push_back("Hidden");
    return choices;
}

// The features of the page for a game whose boxes are of the kinds `kinds` (bits 1 << HitBox::Kind), the master switch
// first
inline std::vector<recomp::FeatureInfo> Features(unsigned kinds) {
    std::vector<recomp::FeatureInfo> features;
    features.push_back({"hit_boxes", kPage, "Show hit boxes",
                        "What can hit what, as the game itself tests it, drawn over the picture. Each kind of box below can be "
                        "given a colour of its own, or hidden.",
                        0, {}});
    for (int k = 0; k < HitBox::kKinds; k++) {
        if (kinds & (1u << k)) {
            const KindInfo &info = Kind(static_cast<HitBox::Kind>(k));
            features.push_back({info.name, kPage, info.title, info.description, 0, Choices(static_cast<HitBox::Kind>(k))});
        }
    }
    return features;
}

// How a box of `kind` is drawn, given its feature's value (choice): false if hidden. fill and edge as port::Hud::Mark
// takes them (red in the low byte, alpha in the high)
inline bool Style(HitBox::Kind kind, int choice, uint32_t &fill, uint32_t &edge) {
    const std::vector<std::string> choices = Choices(kind);
    if (choice < 0 || choice + 1 >= static_cast<int>(choices.size())) {
        return choice < 0 ? Style(kind, 0, fill, edge) : false;
    }
    uint32_t rgb = 0;
    for (const Colour &c : Palette()) {
        if (choices[static_cast<size_t>(choice)] == c.name) {
            rgb = c.rgb;
        }
    }
    switch (kind) {
    case HitBox::Ship: fill = 0xC0000000u | rgb, edge = 0xFF000000u; break;     // (solid, a black edge)
    case HitBox::Bullet: fill = 0xFF000000u | rgb, edge = 0xFFFFFFFFu; break;   // (solid, a white edge)
    case HitBox::Shot: fill = 0x60000000u | rgb, edge = 0xD0000000u | rgb; break;
    case HitBox::Hidden: fill = 0x60000000u | rgb, edge = 0x90000000u | rgb; break; // (faint: lines across the picture)
    default: fill = 0x50000000u | rgb, edge = 0xE0000000u | rgb; break;           // (the bodies: see-through)
    }
    return true;
}

} // namespace spelunk::hitboxes
