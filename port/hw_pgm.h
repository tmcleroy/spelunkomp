// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the game program (port/main.cpp) does in the way of IGS's PGM board: the picture (the native renderer,
// port/gpu_pgm.h, drawing the video chip as the blank leaves it; or the software model's through SDL's renderer), the
// training tools' memory of the game (runtime/moments.h: the whole board, kept by the pages that changed), the
// battery-backed work RAM as the saved file, and the ICS2115's samples.
#pragma once

#include "../machine/pgm.h"
#include "../runtime/board_settings.h"
#include "../runtime/game.h"
#include "../runtime/moments.h"
#include "../runtime/state.h"
#include "gpu_pgm.h"
#include "hud.h"
#include "png_write.h"
#include "scene.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace port {

using namespace spelunk;

struct Hw {
    static constexpr int kWidth = Igs023::kWidth, kHeight = Igs023::kHeight;
    // How many frames after its code ran a frame's picture is shown (none: the boxes lined up with a moving ship as they
    // are read after the frame): hit boxes are drawn that late
    static constexpr uint64_t kPictureLag = 0;
    // The board's frame: 264 lines of 64 microseconds (59.18 Hz)
    static constexpr std::chrono::nanoseconds kFrameTime{Pgm::kFrameNanoseconds};
    // Online play: the most frames run on guessed controls before the game waits for the other side (port/netplay.h;
    // a frame run again is cheap: well under a millisecond)
    static constexpr int kNetMostAhead = 8;
    static constexpr const char *kStateHeader = "spelunkomp pgm state 1 ";

    std::unique_ptr<Pgm> &board; // (the program's: made anew at a reset)
    GpuPgm gpu;
    bool useGpu = false, checkVideo = false;
    uint64_t renderedFrame = ~0ull, checkedFrames = 0, wrongFrames = 0;
    uint32_t maxSprites = 0;
    std::string checkShots = getenv("SPELUNK_CHECK_SHOTS") ? getenv("SPELUNK_CHECK_SHOTS") : "";

    explicit Hw(std::unique_ptr<Pgm> &board)
        : board(board) {}

    static void StartCpu(Pgm &b) {
        b.mode = M68K::Mode::Native;
        if (ThisGame().start) {
            ThisGame().start(b);
        }
    }
    // Where the game keeps its settings (runtime/board_settings.h): the battery-backed work RAM, at 800000
    SettingsStore Store() {
        SettingsStore store;
        store.bytes = board->ram.data();
        store.byteCount = board->ram.size();
        store.ramBase = 0x800000;
        return store;
    }
    // The battery-backed work RAM (the game's settings and scores), kept in the saves folder; written when it has
    // changed since it was last written
    uint8_t *Saved(size_t &size) {
        size = board->ram.size();
        m_savedHash = Hash();
        return board->ram.data();
    }
    bool &SavedChanged() {
        m_changed = Hash() != m_savedHash;
        return m_changed;
    }
    void CarryOver(Pgm &fresh) {
        fresh.ram = board->ram;
    }
    uint32_t SampleRate() const {
        return Ics2115::sampleRate;
    }
    std::vector<int16_t> &Sound() {
        return board->sound.out;
    }
    void SetTiming(bool on) {
        board->SetBoardTiming(on);
    }

    // The picture: the native renderer, or (softwareOnly, or no GPU that will do) the software model's through SDL's
    // renderer
    bool Init(SDL_Window *window, bool softwareOnly, bool check) {
        checkVideo = check;
        useGpu = !softwareOnly && gpu.Init(window, board->video);
        return useGpu;
    }
    void Attach() {
        board->rasterize = !useGpu || checkVideo;
        renderedFrame = ~0ull;
        if (useGpu) {
            board->Redraw();
            Render(true);
        }
    }
    void SelfTest() {}
    // (GPU only) the window drawn into from now on; none while the picture goes through a shader (port/shading.h)
    bool SetWindow(SDL_Window *window) {
        return !useGpu || gpu.SetWindow(window);
    }
    void OutputSize(int &width, int &height) const {
        gpu.OutputSize(width, height);
    }
    void PresentPicture(const uint32_t *pixels, int width, int height, const SDL_FRect &to) {
        gpu.PresentPicture(pixels, width, height, to);
    }
    void ReadScreen(uint32_t *out) {
        if (useGpu) {
            gpu.ReadScreen(out);
        } else {
            board->Picture(out);
        }
    }
    // The chip as the last blank left it, drawn by the GPU (once a frame; not for frames run unseen)
    void Render(bool always = false) {
        if (!useGpu || (!always && (board->noPicture || board->frame == renderedFrame))) {
            return;
        }
        renderedFrame = board->frame;
        gpu.Render(board->video);
        maxSprites = std::max(maxSprites, gpu.spritesDrawn);
    }

    // ---- The scene export (port/scene.h): the chip's picture taken apart, from the layers as they are and the sprites
    // on show (as the GPU draws it), back to front: the sprites that go behind the background, the background, the
    // sprites in front of it, the text ----
    bool TakeScene(Scene &scene, int entries) {
        Igs023 &video = board->video;
        Igs023::Split split;
        video.SplitFrame(split);
        const size_t size = static_cast<size_t>(kWidth) * kHeight;
        scene.width = kWidth;
        scene.height = kHeight;
        scene.frame = board->frame;
        scene.backdrop = split.backdrop;
        scene.picture.resize(size);
        ReadScreen(scene.picture.data());
        scene.layers.clear();
        auto layer = [&](const char *name, const char *kind, int tile, const std::vector<uint32_t> &pixels, int behind) {
            SceneLayer one;
            one.name = name;
            one.kind = kind;
            one.tile = tile;
            one.Clear(size);
            for (size_t at = 0; at < size; at++) {
                if (pixels[at] && (behind < 0 || split.spriteBehind[at] == behind)) {
                    one.Set(at, pixels[at] & 0xFFFFFFu);
                }
            }
            scene.layers.push_back(std::move(one));
        };
        layer("sprites-behind", "objects", 16, split.sprites, 1);
        layer("background", "scroll", 32, split.background, -1);
        layer("sprites", "objects", 16, split.sprites, 0);
        layer("text", "scroll", 8, split.text, -1);
        // The sprites each by itself, front to back (the chip draws the list from its end, and no sprite over a pixel
        // one drew before it)
        scene.entries.clear();
        scene.entryMap.clear();
        if (entries != kSceneNoEntries) {
            const size_t count = video.shown.size();
            std::vector<int> place(count, -1);
            std::vector<int> x0(count, kWidth), y0(count, kHeight), x1(count, -1), y1(count, -1);
            for (size_t at = 0; at < size; at++) {
                if (const int32_t e = split.spriteEntry[at]; e >= 0 && static_cast<size_t>(e) < count) {
                    const int x = static_cast<int>(at % kWidth), y = static_cast<int>(at / kWidth);
                    x0[e] = std::min(x0[e], x);
                    x1[e] = std::max(x1[e], x);
                    y0[e] = std::min(y0[e], y);
                    y1[e] = std::max(y1[e], y);
                }
            }
            for (size_t n = count; n-- > 0;) {
                const Igs023::Sprite &s = video.shown[n];
                if (x1[n] < 0) {
                    continue; // (none of it drawn: off the screen, or the chip's switch leaves it out)
                }
                SceneEntry e;
                e.entry = static_cast<int>(n);
                e.tile = s.offs;
                e.colour = static_cast<int>(s.color);
                e.level = s.pri;
                e.attributes = s.flip;
                e.layer = s.pri ? "sprites-behind" : "sprites";
                if (entries == kSceneEntryPictures) {
                    if (!video.SpriteAlone(n, e.x, e.y, e.w, e.h, e.pixels)) {
                        continue;
                    }
                } else {
                    e.x = x0[n], e.y = y0[n], e.w = x1[n] - x0[n] + 1, e.h = y1[n] - y0[n] + 1;
                }
                place[n] = static_cast<int>(scene.entries.size());
                scene.entries.push_back(std::move(e));
            }
            scene.entryMap.assign(size, 0);
            for (size_t at = 0; at < size; at++) {
                if (const int32_t e = split.spriteEntry[at]; e >= 0 && static_cast<size_t>(e) < count && place[e] >= 0) {
                    scene.entryMap[at] = static_cast<uint32_t>(place[e] + 1);
                }
            }
        }
        return true;
    }

    // ---- The training tools: the practice point and rewind, each moment the whole board ----
    void MakeTools() {}
    void PointSet() {
        Keep(m_point);
        m_pointSet = true;
    }
    void PointBack() {
        Put(m_point);
        moments.Clear();
    }
    void ToolsLost() {
        moments.Clear();
    }
    void TimelineKeep() {
        Keep(m_now);
        moments.Keep(m_now);
    }
    bool TimelineStepBack() {
        const std::vector<uint8_t> *then = moments.StepBack();
        if (then) {
            m_now = *then;
            Put(m_now);
        }
        return then != nullptr;
    }
    struct PointView {
        Hw &hw;
        bool IsSet() const { return hw.m_pointSet; }
        void Clear() { hw.m_pointSet = false; }
    } point{*this};
    struct TimelineView {
        Hw &hw;
        bool Begun() const { return hw.moments.Begun(); }
        void Clear() { hw.moments.Clear(); }
        uint32_t Frames() const { return hw.moments.Frames(); }
        size_t Moments() const { return hw.moments.Count(); }
        size_t Bytes() const { return hw.moments.Bytes(); }
    } timeline{*this};
    Moments moments;

    void BeforeSave() {}
    void AfterLoad() {
        board->Redraw();
        Render(true);
    }

    // ---- A frame ----
    void BeforeFrame() {
        picturesBefore = board->pictures;
    }
    // True: the game gave the sprite DMA no new list this frame (it was late)
    bool AfterFrame(bool rewindOn) {
        if (rewindOn) {
            moments.Note();
        }
        Render();
        return board->pictures == picturesBefore;
    }
    // --check-video: the GPU's picture against the software model's, pixel for pixel
    void CheckFrame(uint64_t frameCount) {
        if (!checkVideo || !useGpu) {
            return;
        }
        std::vector<uint32_t> soft(static_cast<size_t>(kWidth) * kHeight), hard(soft.size());
        board->Picture(soft.data());
        gpu.ReadScreen(hard.data());
        size_t wrong = 0;
        for (size_t i = 0; i < soft.size(); i++) {
            wrong += soft[i] != hard[i];
        }
        checkedFrames++;
        if (wrong) {
            wrongFrames++;
            if (wrongFrames <= 5) {
                fprintf(stderr, "check-video: frame %llu: %zu pixels differ (%u sprites)\n", static_cast<unsigned long long>(frameCount + 1), wrong,
                        gpu.spritesDrawn);
                if (!checkShots.empty()) {
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-soft.png").c_str(), soft.data(), kWidth, kHeight);
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-gpu.png").c_str(), hard.data(), kWidth, kHeight);
                }
            }
        }
    }
    void SetOverlay(const Hud &hud) {
        if (useGpu) {
            gpu.SetOverlay(hud.Any() ? hud.pixels.data() : nullptr, hud.width, hud.height);
        }
    }
    void BeginAhead() {
        board->KeepNow();
        if (useGpu) {
            gpu.AheadWork(true);
        }
    }
    void AheadFrame() {
        board->RunFrame();
        Render();
    }
    void EndAhead() {
        if (useGpu) {
            gpu.AheadWork(false);
        }
        board->PutBack();
    }
    // The input lag readout's looks (port/main.cpp), spread over the frames shown: the board as it is now set aside
    // twice (LookKeep: the moment both of a look's runs start from), then, a frame at a time between the game's own,
    // the board and one of them changing places (LookIn), a frame run unseen with the sound chip held and its picture
    // drawn and read (LookFrame), and the two changing back (LookOut), the game's own picture drawn again. Nothing of
    // a look's frames is left in the game's state, nor in what is shown.
    void LookKeep() {
        board->SetAside(m_looks[0]);
        m_lookCopy = true; // (the second copy at the first frame of the look: the two copies not both in one frame)
    }
    void LookIn(int which) {
        if (m_lookCopy) {
            m_looks[1] = m_looks[0];
            m_lookCopy = false;
        }
        board->Trade(m_looks[which], true);
    }
    void LookFrame(int read) {
        board->RunFrame();
        Render(true);
        LookRead(read);
    }
    // (the picture of a look's frame, read without waiting for the GPU: asked for a frame on, by then long done;
    // `read`: one of two at once)
    bool LookReady(int read) {
        return !useGpu || gpu.ReadDone(read);
    }
    bool LookScreen(int read, uint32_t *out) {
        if (useGpu) {
            return gpu.EndScreenRead(read, out);
        }
        std::memcpy(out, m_lookScreens[read].data(), m_lookScreens[read].size() * 4);
        return true;
    }
    std::vector<uint32_t> m_lookScreens[2];
    void LookRead(int read) {
        if (useGpu) {
            gpu.StartScreenRead(read);
        } else {
            m_lookScreens[read].resize(static_cast<size_t>(kWidth) * kHeight);
            board->Picture(m_lookScreens[read].data());
        }
    }
    void LookOut(int which) {
        board->Trade(m_looks[which], false);
        Render(true);
    }
    void LookDrop() {
        m_looks[0] = {};
        m_looks[1] = {};
    }
    Pgm::Aside m_looks[2];
    bool m_lookCopy = false;
    // Online play's rollback (port/netplay.h): the whole board kept exactly (the sound chips with it) and put back,
    // and a frame run unseen (no picture made: the next shown frame makes its own)
    // (two moments at once, `which`: the anchor and the next one)
    void NetKeep(int which) {
        board->KeepNow(true, which);
    }
    void NetDrop(int) {}
    void NetBack(int which) {
        board->PutBack(which);
    }
    void NetFrame() {
        board->noPicture = true;
        board->RunFrame();
        board->noPicture = false;
    }
    // (`w` x `h`: the picture's shape as shown, port/main.cpp's canvasSize)
    void WindowShot(const std::string &path, int degrees, int w, int h) {
        std::vector<uint32_t> out;
        gpu.ScreenToPixels(degrees / 90, w, h, out);
        WritePng(path.c_str(), out.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }
    void PresentScreen(int degrees, const SDL_FRect &to) {
        gpu.PresentScreen(degrees / 90, to);
    }
    int Report() {
        if (checkVideo && useGpu) {
            fprintf(stderr, "check-video: %llu of %llu frames differed; most sprites in a frame: %u\n", static_cast<unsigned long long>(wrongFrames),
                    static_cast<unsigned long long>(checkedFrames), maxSprites);
        }
        if (getenv("SPELUNK_REPORT_STATE")) {
            fprintf(stderr, "state: frame %llu hash %016llx vram %016llx\n", static_cast<unsigned long long>(board->frame),
                    static_cast<unsigned long long>(board->StateHash()), 0ull);
        }
        return checkVideo && wrongFrames ? 1 : 0;
    }

private:
    uint32_t picturesBefore = 0;
    bool m_pointSet = false, m_changed = false;
    uint64_t m_savedHash = 0;
    std::vector<uint8_t> m_point, m_now;
    uint64_t Hash() const {
        uint64_t h = 0xCBF29CE484222325ull;
        for (uint8_t b : board->ram) {
            h = (h ^ b) * 0x100000001B3ull;
        }
        return h;
    }
    void Keep(std::vector<uint8_t> &into) {
        into.clear();
        Archive ar(into, true);
        board->Serialize(ar);
    }
    void Put(std::vector<uint8_t> &from) {
        Archive ar(from, false);
        board->Serialize(ar);
        board->Redraw();
        Render(true);
        board->sound.out.clear();
    }
};

} // namespace port
