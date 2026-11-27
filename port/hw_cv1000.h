// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the game program (port/main.cpp) does in the CV1000's own way: the picture (the native renderer running the
// game's draw lists on the GPU, port/gpu.h, or the software blitter's), the training tools' memory of the game
// (runtime/timeline.h), the EEPROM and the sound chip's samples.
#pragma once

#include "../machine/cv1000.h"
#include "../runtime/board_settings.h"
#include "../runtime/game.h"
#include "../runtime/timeline.h"
#include "gpu.h"
#include "hud.h"
#include "png_write.h"

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
    static constexpr int kWidth = Blitter::kScreenWidth, kHeight = Blitter::kScreenHeight;
    // How many frames after its code ran a frame's picture is shown (a frame's draw list is drawn by the blitter in the next frame and shown in the one after): hit boxes are drawn that late
    static constexpr uint64_t kPictureLag = 2;
    // The board's frame: 262 lines of 407 pixels at 6.4 MHz (60.0184 Hz)
    static constexpr std::chrono::nanoseconds kFrameTime{16661563};
    // Online play: the most frames run on guessed controls before the game waits for the other side (port/netplay.h;
    // a frame run again is dear: about 3 ms on an Apple silicon Mac, perhaps twice that on a Steam Deck)
    static constexpr int kNetMostAhead = 6;
    static constexpr const char *kStateHeader = "spelunkomp state 5 ";

    std::unique_ptr<Cv1000> &board; // (the program's: made anew at a reset)
    Gpu gpu;
    NativeList drawList;
    bool useGpu = false, checkVideo = false;
    Point point;
    Timeline timeline;
    VramAccess vramAccess;
    uint32_t maxPasses = 0, maxTile = 0, maxDraws = 0, listsBefore = 0;
    uint64_t checkedFrames = 0, wrongFrames = 0;
    std::string checkShots = getenv("SPELUNK_CHECK_SHOTS") ? getenv("SPELUNK_CHECK_SHOTS") : "";

    explicit Hw(std::unique_ptr<Cv1000> &board)
        : board(board) {}

    static void StartCpu(Cv1000 &b) {
        b.cpu.mode = SH3::Mode::Native;
        b.cpu.InstallTranslations();
        if (ThisGame().start) {
            ThisGame().start(b);
        }
    }
    // Where the game keeps its settings (runtime/board_settings.h): the RTC-9701's EEPROM
    SettingsStore Store() {
        SettingsStore store;
        store.words = board->rtc.eeprom.data();
        store.wordCount = board->rtc.eeprom.size();
        return store;
    }
    // The EEPROM (the game's settings and scores), kept in the saves folder
    uint8_t *Saved(size_t &size) {
        size = sizeof(board->rtc.eeprom);
        return reinterpret_cast<uint8_t *>(board->rtc.eeprom.data());
    }
    bool &SavedChanged() {
        return board->rtc.eepromChanged;
    }
    // A board made anew at a reset keeps what the old one's EEPROM held
    void CarryOver(Cv1000 &fresh) {
        fresh.rtc.eeprom = board->rtc.eeprom;
        fresh.rtc.eepromChanged = board->rtc.eepromChanged;
    }
    uint32_t SampleRate() const {
        return board->sound.sampleRate;
    }
    std::vector<int16_t> &Sound() {
        return board->sound.out;
    }
    void SetTiming(bool on) {
        if (on && !board->cpu.timing) {
            board->cpu.timingPaths.Forget(); // (the code may have been written while it was off)
#ifdef SPELUNK_SHADOW_TIMING
            board->cpu.refPaths.Forget();
#endif
        }
        board->cpu.timing = on;
    }

    // The picture: the native renderer, or (softwareOnly, or no GPU that will do) the software blitter's through
    // SDL's renderer
    bool Init(SDL_Window *window, bool softwareOnly, bool check) {
        checkVideo = check;
        useGpu = !softwareOnly && gpu.Init(window);
        return useGpu;
    }
    void Attach() {
        board->blitter.native = &drawList; // (the training tools read what was written from it, the GPU or not)
        board->blitter.rasterize = !useGpu || checkVideo;
        if (useGpu) {
            gpu.SetVram(board->blitter.vram);
        }
    }
    void SelfTest() {
        if (!useGpu || !getenv("SPELUNK_GPU_SELFTEST")) {
            return;
        }
        // (VRAM there and back, then one draw of a known kind)
        std::vector<uint16_t> pattern(board->blitter.vram.size()), back;
        for (size_t i = 0; i < pattern.size(); i++) {
            pattern[i] = static_cast<uint16_t>(i * 7 + (i >> 13));
        }
        gpu.SetVram(pattern);
        gpu.GetVram(back);
        size_t wrong = 0;
        for (size_t i = 0; i < pattern.size(); i++) {
            wrong += pattern[i] != back[i];
        }
        fprintf(stderr, "selftest: there and back: %zu wrong (first words %04X %04X %04X, back %04X %04X %04X)\n", wrong, pattern[0], pattern[1], pattern[8192],
                back[0], back[1], back[8192]);
        NativeList one;
        one.draws.push_back({100, 50, 131, 81, 1000 - 100, 2000 - 50, 64, 0});
        one.steps.push_back({false, 0, 1});
        gpu.Run(one);
        gpu.GetVram(back);
        wrong = 0;
        for (int y = 50; y <= 81; y++) {
            for (int x = 100; x <= 131; x++) {
                wrong += back[static_cast<size_t>(y) * 8192 + x] != pattern[static_cast<size_t>(y + 1950) * 8192 + x + 900];
            }
        }
        size_t outside = 0;
        for (size_t i = 0; i < pattern.size(); i++) {
            const int x = static_cast<int>(i % 8192), y = static_cast<int>(i / 8192);
            if (x < 100 || x > 131 || y < 50 || y > 81) {
                outside += pattern[i] != back[i];
            }
        }
        fprintf(stderr, "selftest: a plain draw: %zu of 1024 wrong, %zu outside it changed; got %04X want %04X\n", wrong, outside, back[50 * 8192 + 100],
                pattern[2000 * 8192 + 1000]);
        gpu.SetVram(board->blitter.vram);
    }
    // (GPU only) the window in real pixels; and a picture made by the program (the menu) shown
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
    // The screen as the board has it now, as pixels here (from the GPU's VRAM when that is where the picture is)
    void ReadScreen(uint32_t *out) {
        if (useGpu) {
            gpu.ReadScreen(board->blitter.scrollX, board->blitter.scrollY, out);
        } else {
            board->Picture(out);
        }
    }

    // ---- The training tools (runtime/timeline.h): the practice point and rewind ----
    void MakeTools() {
        constexpr int kW = Blitter::kVramWidth;
        auto softRead = [this](const std::vector<VramArea> &areas, uint16_t *out) {
            for (const VramArea &a : areas) {
                for (int row = 0; row < a.h; row++, out += a.w) {
                    std::memcpy(out, &board->blitter.vram[static_cast<size_t>(a.y + row) * kW + a.x], static_cast<size_t>(a.w) * 2);
                }
            }
        };
        auto softWrite = [this](const std::vector<VramArea> &areas, const uint16_t *in) {
            for (const VramArea &a : areas) {
                for (int row = 0; row < a.h; row++, in += a.w) {
                    std::memcpy(&board->blitter.vram[static_cast<size_t>(a.y + row) * kW + a.x], in, static_cast<size_t>(a.w) * 2);
                }
            }
        };
        if (useGpu) {
            vramAccess.read = [this](const std::vector<VramArea> &areas, uint16_t *out) { gpu.ReadAreas(areas, out); };
            vramAccess.write = [this, softWrite](const std::vector<VramArea> &areas, const uint16_t *in) {
                gpu.WriteAreas(areas, in);
                if (board->blitter.rasterize) {
                    softWrite(areas, in); // (the check's software blitter keeps up)
                }
            };
            vramAccess.readAll = [this](std::vector<uint16_t> &all) { gpu.GetVram(all); };
            vramAccess.kept = [this] { return gpu.DownloadBytes(); };
        } else {
            vramAccess.read = softRead;
            vramAccess.write = softWrite;
            vramAccess.readAll = [this](std::vector<uint16_t> &all) { all = board->blitter.vram; };
        }
    }
    void PointSet() {
        point.Set(*board, vramAccess);
    }
    void PointBack() {
        point.Back(*board, vramAccess);
        timeline.Jumped(*board, point);
    }
    // The game is no longer where the tools' notes say (a state loaded, the board reset)
    void ToolsLost() {
        timeline.Clear();
        point.Lost();
    }
    void TimelineKeep() {
        timeline.Keep(*board, vramAccess);
    }
    bool TimelineStepBack() {
        return timeline.StepBack(*board, vramAccess);
    }

    // ---- Save states: VRAM is the GPU's, brought back for the state and sent again after one is read ----
    void BeforeSave() {
        if (useGpu) {
            gpu.GetVram(board->blitter.vram);
        }
    }
    void AfterLoad() {
        if (useGpu) {
            gpu.SetVram(board->blitter.vram);
        }
    }

    // ---- A frame ----
    void BeforeFrame() {
        drawList.Clear();
        listsBefore = board->blitter.lists;
    }
    // After the board's frame: its lists run on the GPU, and noted for the tools. True: the game had no new picture
    // this frame (it was late)
    bool AfterFrame(bool rewindOn) {
        const bool late = board->blitter.lists == listsBefore;
        if (useGpu) {
            gpu.Run(drawList);
            maxPasses = std::max(maxPasses, gpu.passes);
            maxTile = std::max(maxTile, gpu.longestTile);
            maxDraws = std::max(maxDraws, gpu.drawsRun);
        }
        point.Note(drawList);
        if (rewindOn) {
            timeline.Note(drawList);
        }
        return late;
    }
    // --check-video: the GPU's screen against the software blitter's, pixel for pixel
    void CheckFrame(uint64_t frameCount) {
        if (!checkVideo || !useGpu) {
            return;
        }
        std::vector<uint32_t> soft(static_cast<size_t>(kWidth) * kHeight), hard(soft.size());
        board->Picture(soft.data());
        gpu.ReadScreen(board->blitter.scrollX, board->blitter.scrollY, hard.data());
        size_t wrong = 0;
        for (size_t i = 0; i < soft.size(); i++) {
            wrong += soft[i] != hard[i];
        }
        checkedFrames++;
        if (wrong) {
            wrongFrames++;
            if (wrongFrames == 1) {
                for (const NativeList::Step &step : drawList.steps) {
                    if (step.upload) {
                        const NativeUpload &u = drawList.uploads[step.first];
                        fprintf(stderr, "  upload %d,%d %dx%d first %04X\n", u.x, u.y, u.width, u.height, drawList.pixels[u.pixels]);
                    } else {
                        const NativeDraw &d = drawList.draws[step.first];
                        fprintf(stderr, "  %u draws; first: %d,%d-%d,%d src %d,%d flags %X colours %X\n", step.count, d.x0, d.y0, d.x1, d.y1, d.sx, d.sy,
                                d.flags, d.colours);
                    }
                }
                fprintf(stderr, "  scroll %u,%u clip %u,%u\n", board->blitter.scrollX, board->blitter.scrollY, board->blitter.clipX, board->blitter.clipY);
            }
            if (wrongFrames <= 5) {
                fprintf(stderr, "check-video: frame %llu: %zu pixels differ\n", static_cast<unsigned long long>(frameCount + 1), wrong);
                if (!checkShots.empty()) {
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-soft.png").c_str(), soft.data(), 320, 240);
                    WritePng((checkShots + "-" + std::to_string(frameCount + 1) + "-gpu.png").c_str(), hard.data(), 320, 240);
                }
            }
        }
    }
    void SetOverlay(const Hud &hud) {
        if (useGpu) {
            gpu.SetOverlay(hud.Any() ? hud.pixels.data() : nullptr, hud.width, hud.height);
        }
    }
    // Run-ahead: the board kept, frames run on for their picture only, and the board put back
    void BeginAhead() {
        board->KeepNow();
        if (useGpu) {
            gpu.BeginJournal();
            gpu.AheadWork(true);
        }
    }
    void AheadFrame() {
        drawList.Clear();
        board->RunFrame();
        if (useGpu) {
            gpu.Run(drawList);
        }
    }
    void EndAhead() {
        if (useGpu) {
            gpu.AheadWork(false);
            gpu.Rollback();
        }
        board->PutBack(0, false);
    }
    // The input lag readout's looks (port/main.cpp), spread over the frames shown: the board as it is now set aside
    // twice (LookKeep: the moment both of a look's runs start from), then, a frame at a time between the game's own,
    // the board and one of them changing places (LookIn: VRAM too, the GPU's by its texture), a frame run unseen with
    // the sound chip held and its picture read (LookFrame), and the two changing back (LookOut). Nothing of a look's
    // frames is left in the game's state, nor in what is shown. (Never with run-ahead's or online play's journals open:
    // the readout's frames come before the game's frame, and online play turns the readout off)
    void LookKeep() {
        board->SetAside(m_looks[0]);
        m_lookCopy = true; // (the second copy at the first frame of the look: the two copies not both in one frame)
        if (useGpu) {
            gpu.SetVramAside(0);
            gpu.SetVramAside(1);
        }
    }
    void LookIn(int which) {
        if (m_lookCopy) {
            m_looks[1] = m_looks[0];
            m_lookCopy = false;
        }
        board->Trade(m_looks[which], true);
        if (useGpu) {
            gpu.TradeVram(which);
        }
    }
    void LookFrame(int read) {
        AheadFrame();
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
            gpu.StartScreenRead(read, board->blitter.scrollX, board->blitter.scrollY);
        } else {
            m_lookScreens[read].resize(static_cast<size_t>(kWidth) * kHeight);
            board->Picture(m_lookScreens[read].data());
        }
    }
    void LookOut(int which) {
        board->Trade(m_looks[which], false);
        if (useGpu) {
            gpu.TradeVram(which);
        }
    }
    void LookDrop() {
        m_looks[0] = {};
        m_looks[1] = {};
        if (useGpu) {
            gpu.DropVramAside();
        }
    }
    Cv1000::Aside m_looks[2];
    bool m_lookCopy = false;
    // Online play's rollback (port/netplay.h): the game kept exactly (the sound chip with it) and VRAM by the areas
    // written since, put back (and kept on from there: the same moment can be gone back to again), and a frame run
    // unseen (its draw lists still run: VRAM is the frames' sum)
    // Two moments at once (`which`), each with VRAM's journal from it: the anchor and the next one
    void NetKeep(int which) {
        board->KeepNow(true, which);
        if (useGpu) {
            gpu.BeginJournal(which);
        }
    }
    void NetBack(int which) {
        if (useGpu) {
            gpu.Rollback(which);
            gpu.BeginJournal(which);
        }
        board->PutBack(which);
        board->blitter.BeginJournal(which);
    }
    void NetDrop(int which) {
        if (useGpu) {
            gpu.EndJournal(which);
        }
        board->Drop(which);
    }
    void NetFrame() {
        drawList.Clear();
        board->RunFrame();
        if (useGpu) {
            gpu.Run(drawList);
        }
    }
    // (GPU only) the window's own drawing of the screen, at three times the picture's size, into a file
    // (`w` x `h`: the picture's shape as shown, port/main.cpp's canvasSize)
    void WindowShot(const std::string &path, int degrees, int w, int h) {
        std::vector<uint32_t> out;
        gpu.ScreenToPixels(board->blitter.scrollX, board->blitter.scrollY, degrees / 90, w, h, out);
        WritePng(path.c_str(), out.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    }
    void PresentScreen(int degrees, const SDL_FRect &to) {
        gpu.PresentScreen(board->blitter.scrollX, board->blitter.scrollY, degrees / 90, to);
    }
    // At the end of a run (checks)
    int Report() {
        if (board->cpu.plannedEvents + board->cpu.genericEvents) {
            printf("planned: %llu events served, %llu generic\n", static_cast<unsigned long long>(board->cpu.plannedEvents),
                   static_cast<unsigned long long>(board->cpu.genericEvents));
        }
        if (checkVideo && useGpu) {
            // (and all of VRAM at the end: what the screen did not show)
            std::vector<uint16_t> hard;
            gpu.GetVram(hard);
            size_t wrong = 0;
            for (size_t i = 0; i < hard.size(); i++) {
                wrong += hard[i] != board->blitter.vram[i];
            }
            fprintf(stderr, "check-video: %llu of %llu frames differed; VRAM at the end: %zu pixels differ; most in a frame: %u draws, %u passes, %u draws on a tile\n",
                    static_cast<unsigned long long>(wrongFrames), static_cast<unsigned long long>(checkedFrames), wrong, maxDraws, maxPasses, maxTile);
        }
        if (getenv("SPELUNK_REPORT_STATE")) {
            // (the game's state, and all of VRAM's pixels)
            std::vector<uint16_t> all;
            vramAccess.readAll(all);
            uint64_t h = 0xCBF29CE484222325ull;
            for (uint16_t p : all) {
                h = (h ^ p) * 0x100000001B3ull;
            }
            fprintf(stderr, "state: frame %llu hash %016llx vram %016llx\n", static_cast<unsigned long long>(board->frame),
                    static_cast<unsigned long long>(board->StateHash()), static_cast<unsigned long long>(h));
        }
        return checkVideo && wrongFrames ? 1 : 0;
    }
};

} // namespace port
