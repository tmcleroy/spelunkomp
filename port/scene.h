// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/scene.h.
//
// The scene export: the picture shown, and each of the board's layers by itself as a PNG with nothing where the layer
// has nothing, so that the picture can be taken apart and put together again; and a scene.json that says what each
// file is and the layers' order, back to front. The files are cps2recomp's, so its scene diorama
// (tools/diorama/diorama.html) opens them. What a layer is depends on the board (port/hw_*.h, TakeScene):
//
//  - Cave's 68000 boards: each tile layer at each of its tiles' priorities ("layer1", or "layer1-p2" where the layer
//    has tiles of more than one priority), and the sprites of each priority ("sprites-p0" to "sprites-p3"), back to
//    front as the chips put them (a sprite goes below the tiles of its own priority);
//  - the PGM: the sprites behind the background, the background, the sprites in front of it, the text layer;
//  - the CV1000, whose blitter draws every frame into one picture: the frame's draws in the order the game made them,
//    a run of draws from the same part of VRAM in the same way a layer ("draws-1"...), the picture's own pixels where
//    a draw shows, and what each draw painted by itself where a later one covers it.
//
// Every layer of a scene is the picture's size, turned as the player sees it (a game made for a monitor on its side
// stood up); scene.json says the shape of a pixel as the screen shows it (pixelAspect: wide over high). What the port
// draws over the picture (the training tools' hit boxes and writing) is in it too, as layers of its own, and the hit
// boxes are listed in scene.json. The sprites of a 68000 board or a PGM are each saved by themselves too (entries.png,
// entry-map.png), so that the diorama can pick one out of its layer.
//
// A clip is the same for each frame of a few seconds, in a folder per layer, written by threads beside the game.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace port {

struct SceneLayer {
    std::string name;
    std::string board = "game"; // ("game"; "overlay": what the port draws over the picture)
    std::string kind;           // ("scroll": a layer of tiles; "objects": sprites or draws)
    int tile = 0;               // (a layer of tiles: its tiles' size in pixels)
    std::string extra;          // (more for scene.json, as `"key": value` pairs after a comma: a priority, a VRAM page)
    int width = 0, height = 0;  // (0: the scene's; an overlay may be larger, its writing finer)
    std::vector<uint32_t> pixels; // (red in the low byte, alpha in the top: 0xFF where the layer has something)
    bool empty = true;
    // (pixels made the scene's size, all nothing)
    void Clear(size_t size) {
        pixels.assign(size, 0);
        empty = true;
    }
    void Set(size_t at, uint32_t colour) {
        pixels[at] = colour | 0xFF000000u;
        empty = false;
    }
};

// A sprite by itself (the diorama picks one out of its layer): drawn alone in the box of the picture it covers
struct SceneEntry {
    int entry = 0;           // (its place in the board's list)
    uint32_t tile = 0;       // (its first pixels' place in the sprite ROMs)
    int colour = 0, level = 0; // (its colour set, and its priority)
    uint32_t attributes = 0;
    int x = 0, y = 0, w = 0, h = 0;
    std::string layer;       // (the layer it is in)
    std::vector<uint32_t> pixels; // (w x h; a still's only)
};

// A hit box the port shows, in the picture's pixels
struct SceneBox {
    int player = -1; // (0: player 1's ship; -1 not a player's)
    std::string kind;
    int x = 0, y = 0, width = 0, height = 0;
    uint32_t colour = 0; // (red in the low byte)
    uint8_t fill = 0;    // (how solid its inside is, of 255)
};

struct Scene {
    int width = 0, height = 0;
    uint64_t frame = 0;
    uint32_t backdrop = 0;
    double pixelAspect = 1; // (a pixel's width over its height as the screen shows it)
    std::vector<uint32_t> picture; // (as shown, solid)
    std::vector<SceneLayer> layers; // (back to front)
    std::vector<SceneEntry> entries; // (front to back)
    std::vector<uint32_t> entryMap;  // (each pixel: 1 + the front entry's place in `entries`; 0 none)
    std::vector<SceneLayer> overlays;
    std::vector<SceneBox> boxes;
};

// How much of a scene a board takes (TakeScene): its sprites each by itself as well, drawn alone (a still's), or only
// the map of which is in front where and their boxes there (a clip's frame)
constexpr int kSceneNoEntries = 0, kSceneEntryMap = 1, kSceneEntryPictures = 2;

// The scene turned `degrees` clockwise (the board's picture to the picture as shown: port/main.cpp's TurnTo)
void TurnScene(Scene &scene, int degrees);

class SceneExport {
public:
    ~SceneExport();
    // A scene, in a folder of its own under `under` named `name`; what to say of it
    std::string Save(const Scene &scene, const std::string &under, const std::string &name);
    // A clip of the next `frames` frames at `fps`, in a folder of its own; what to say of it (it is begun, or why not)
    std::string StartClip(const std::string &under, const std::string &name, int frames, double fps);
    // Ended before its length, the frames so far kept
    void StopClip(bool cutShort = false);
    bool Recording() const {
        return m_clip.on;
    }
    // Each frame the game ran and showed, while a clip is recorded: its scene; or none (no picture to take apart yet)
    void Frame(Scene &&scene);
    void Missed();
    // What is to be said now (a clip finished writing, or could not be): true once for each
    bool TakeSaid(std::string &said);

private:
    struct Clip {
        std::atomic<bool> on{false};
        std::string folder;
        int wanted = 0, taken = 0, missed = 0;
        double fps = 60;
        std::vector<std::string> names; // (the layers' names, as first seen: the folders)
        std::string json;               // (their lines of scene.json)
        bool entries = false;           // (a frame had sprites: entry-map/ and entries/)
        std::vector<std::string> overlayNames;
        std::string overlaysJson;
    } m_clip;
    struct Job {
        std::string folder;
        int number = 0;
        Scene scene;
    };
    std::mutex m_lock;
    std::condition_variable m_wake;
    std::deque<Job> m_jobs;
    std::vector<std::thread> m_writers;
    int m_writing = 0; // (jobs taken and not yet written)
    bool m_failed = false, m_stopping = false, m_finishing = false, m_cutShort = false;
    std::string m_frameJson;
    int m_width = 0, m_height = 0;
    double m_pixelAspect = 1;
    std::string m_finalJson, m_finalFolder;
    int m_finalFrames = 0;
    std::string m_said;
    bool m_saying = false;
    void Writer();
    void Finish();
};

} // namespace port
