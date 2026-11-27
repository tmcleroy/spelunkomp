// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's port/shading.h.
//
// Shaders: the picture drawn through a RetroArch shader preset (a .slangp: CRT, scanlines, xBRZ, ScaleFX and the
// rest), by librashader (https://github.com/SnowflakePowered/librashader), through OpenGL. librashader is a library
// beside the program (librashader.dylib, .so, .dll), opened when a shader is first asked for: without it, or with
// a preset it cannot make, there is no shader and the picture goes to the window as it always did.
// librashader needs the window's own OpenGL, which SDL's GPU API keeps to itself, so a window that shows a shader
// is an OpenGL window (SDL_WINDOW_OPENGL) with nothing else drawing to it: the native renderer then draws the board's
// picture with no window of its own (gpu_base.h), and it is read back and handed here.
//
// A game made for a monitor on its side is shaded one of two ways (the Scanline direction feature): its picture as
// the board makes it, the preset's drawing then turned to stand up (`turn`), so the lines run down the screen as on
// the cabinet's turned monitor; or turned first (port/main.cpp) and shaded standing, so they run across.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace port {

class Shading {
public:
    ~Shading();
    // True if librashader is there to be used (it is opened the first time this is asked); false with the reason
    static bool Available(std::string &error);
    // Takes the window's OpenGL (an OpenGL window, SDL_WINDOW_OPENGL); false with the reason
    bool Open(SDL_Window *window, std::string &error);
    void Close();
    // The preset to draw with (a .slangp file); false with the reason, and the one before is kept
    bool Load(const std::string &preset, std::string &error);
    const std::string &Preset() const {
        return m_preset;
    }
    // The board's frames a second (the presets that move with time count them)
    float framesPerSecond = 60.0f;
    // Shows a picture (`width` x `height` pixels, each red, green, blue and a spare byte in memory) in the
    // rectangle `where` of the window, the rest black: through the preset, or (`shade` false: the menu, which is
    // drawn larger than the game's picture) only fitted, smoothly. `turn`: quarter turns clockwise, after the preset,
    // of what it drew (`where` is the turned picture's place). `overlay`: the overlay over it (SetOverlay)
    void Show(const uint32_t *pixels, int width, int height, const SDL_FRect &where, bool shade, int turn, bool overlay);
    // What is laid over the picture's rectangle after it, not through the preset (the training tools' writing and the
    // hit boxes, port/hud.h): `width` x `height` pixels, red in the low byte, alpha in the high as how much shows,
    // stretched over `where`, unturned. None (nullptr) lays nothing.
    void SetOverlay(const uint32_t *pixels, int width, int height);
    // What is laid over the whole window after that (the touch controls: port/touch.h), stretched to it smoothly; the
    // same kind of pixels. None (nullptr): nothing
    void SetWindowLayer(const uint32_t *pixels, int width, int height, bool sharp = false);
    // Each picture shown waits for the screen's refresh (on: full screen at an arcade monitor's mode, paced by the
    // screen, recomp-common/arcade_screen.h), or (off, as Open leaves it) is shown the moment it is done
    void SetVsync(bool on);
    // For the checks (--window-shot): each picture shown kept, and the last one, its first row its top
    bool keepLast = false;
    bool ReadBack(std::vector<uint32_t> &pixels, int &width, int &height);

private:
    SDL_Window *m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    bool m_vsync = false; // (SetVsync's)
    void *m_chain = nullptr; // (librashader's libra_gl_filter_chain_t)
    std::string m_preset;
    // (the picture's texture, and the one the preset draws into, of `where`'s size, unturned; a framebuffer for each)
    uint32_t m_picture = 0, m_drawn = 0, m_pictureFrame = 0, m_drawnFrame = 0;
    int m_width = 0, m_height = 0, m_drawnWidth = 0, m_drawnHeight = 0;
    size_t m_frame = 0;
    std::vector<uint32_t> m_last;
    int m_lastWidth = 0, m_lastHeight = 0;
    void Keep();
    // (the overlay's texture, and the program that draws a texture into a rectangle of the window, turned or blended)
    uint32_t m_overlay = 0, m_layer = 0, m_program = 0, m_array = 0;
    int m_overlayWidth = 0, m_overlayHeight = 0, m_layerWidth = 0, m_layerHeight = 0;
    // (a texture of these pixels, made again at another size; none deletes it)
    void Texture(uint32_t &texture, int &haveWidth, int &haveHeight, const uint32_t *pixels, int width, int height, int filter);
    // A texture drawn into the window's rectangle at (x, y), w x h of its pixels counted from its foot, turned `turn`
    // quarter turns clockwise; blended by its alpha (not premultiplied) or not
    void Lay(uint32_t texture, int x, int y, int w, int h, int turn, bool blend);
};

} // namespace port
