// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// What the port writes over the game's picture (the training tools' notes and readouts): a picture of its own, three
// times the game's size so its text is fine, see-through where nothing is written. The GPU lays it over the screen
// (Gpu::SetOverlay); the software path blends it into the picture.
#pragma once

#include "../recomp-common/menu_turn.h"
#include "../recomp-common/text.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace port {

class Hud {
public:
    static constexpr int kScale = 3;
    int width = 0, height = 0;
    std::vector<uint32_t> pixels; // red in the low byte, alpha in the high

    // A frame's writing begins: nothing written yet. `turn`: the words' turn on this picture, in degrees clockwise
    // (the Menu orientation: recomp-common/menu_turn.h), so they stand upright on a monitor turned on its side; the
    // marks (the hit boxes) are the picture's own, never turned
    // `large`: the words' letters a board pixel a dot (8 of the board's lines tall, not 16 of the 3x picture's 24): an
    // arcade monitor at the board's own lines (the compact menu's screen), where the small ones are a few lines and blur
    void Begin(int w, int h, int turn = 0, bool large = false) {
        m_wordsBefore.swap(m_words);
        m_words.clear();
        if (w != width || h != height || recomp::turn::Normal(turn) != m_turn || large != m_large) {
            m_large = large;
            width = w;
            height = h;
            m_turn = recomp::turn::Normal(turn);
            pixels.assign(static_cast<size_t>(w) * h, 0);
            m_wordsBefore = "\x01"; // (drawn again)
        }
        m_drawn = false;
        m_marks.clear();
        m_showInputs = false;
        m_showCorner = false;
        m_showLine = false;
        m_texts.clear();
    }
    // A line of words in a box near the top, in the middle
    void Note(const char *words) {
        m_words += "N:";
        m_words += words;
        m_words += '\n';
        m_note = words;
    }
    // The stick and buttons held (machine/cv1000.h's kIn bits), in the bottom left corner
    void Inputs(uint32_t bits) {
        m_words += "I:" + std::to_string(bits & 0xFF) + "\n";
        m_inputs = bits & 0xFF;
        m_showInputs = true;
    }
    // A box over the picture (the hit boxes), in this picture's pixels: from x0, y0 up to x1, y1; filled with `fill`
    // (its alpha as how much shows) inside an outline of `edge` (none if its alpha is 0)
    void Mark(int x0, int y0, int x1, int y1, uint32_t fill, uint32_t edge) {
        m_marks.push_back({x0, y0, x1, y1, fill, edge});
        m_words += "M:" + std::to_string(x0) + "," + std::to_string(y0) + "," + std::to_string(x1) + "," + std::to_string(y1) + "," +
                   std::to_string(fill) + "\n";
    }
    // A short line in the bottom right corner (the game's speed)
    void Corner(const char *words) {
        m_words += "C:";
        m_words += words;
        m_words += '\n';
        m_corner = words;
        m_showCorner = true;
    }
    // A line along the bottom, in the middle (the input lag readout), above the corner's
    void Line(const char *words) {
        m_words += "L:";
        m_words += words;
        m_words += '\n';
        m_line = words;
        m_showLine = true;
    }
    // Words anywhere (online play's names, ping, chat and notes): at x, y (fractions of the picture's width and height:
    // its left, middle or right at x as `align` is -1, 0 or 1; its top at y), in a colour (red in the low byte), on a
    // dark box
    void Text(float x, float y, int align, const std::string &words, uint32_t colour = 0xFFFFFFFFu) {
        if (words.empty()) {
            return;
        }
        m_texts.push_back({x, y, align, words, colour});
        m_words += "T:" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(align) + "," + std::to_string(colour) + ":" + words + "\n";
    }
    // Something is written this frame
    bool Any() const {
        return !m_words.empty();
    }
    // What is written differs from the frame before: the picture is drawn again (call once, after the frame's writing)
    bool Changed() {
        if (m_words == m_wordsBefore) {
            return false;
        }
        std::fill(pixels.begin(), pixels.end(), 0u);
        if (m_words.empty()) {
            return true;
        }
        m_to = pixels.data();
        m_toW = width;
        m_toH = height;
        for (const MarkBox &m : m_marks) {
            Box(m.x0, m.y0, m.x1 - m.x0, m.y1 - m.y0, m.fill);
            if (m.edge >> 24) {
                const int t = 2; // (the outline's thickness)
                Box(m.x0 - t, m.y0 - t, m.x1 - m.x0 + 2 * t, t, m.edge);
                Box(m.x0 - t, m.y1, m.x1 - m.x0 + 2 * t, t, m.edge);
                Box(m.x0 - t, m.y0, t, m.y1 - m.y0, m.edge);
                Box(m.x1, m.y0, t, m.y1 - m.y0, m.edge);
            }
        }
        if (m_turn == 0) {
            Words(width, height);
            return true;
        }
        // (turned: the words drawn upright on a picture of the turned screen's shape, then turned onto this one over
        // the marks)
        int uw = 0, uh = 0;
        recomp::turn::TurnedSize(m_turn, width, height, uw, uh);
        m_upright.assign(static_cast<size_t>(uw) * uh, 0);
        m_to = m_upright.data();
        m_toW = uw;
        m_toH = uh;
        Words(uw, uh);
        m_turned.resize(pixels.size());
        recomp::turn::TurnPixels(m_upright.data(), uw, uh, m_turn, m_turned.data());
        for (size_t i = 0; i < pixels.size(); i++) {
            if (m_turned[i] >> 24) {
                pixels[i] = m_turned[i];
            }
        }
        m_to = pixels.data();
        m_toW = width;
        m_toH = height;
        return true;
    }
    // Blended into a picture of the same size (the software path)
    void Over(uint32_t *picture) const {
        for (size_t i = 0; i < pixels.size(); i++) {
            const uint32_t o = pixels[i], a = o >> 24;
            if (!a) {
                continue;
            }
            const uint32_t p = picture[i];
            const uint32_t r = ((p & 0xFF) * (255 - a) + (o & 0xFF) * a) / 255;
            const uint32_t g = (((p >> 8) & 0xFF) * (255 - a) + ((o >> 8) & 0xFF) * a) / 255;
            const uint32_t b = (((p >> 16) & 0xFF) * (255 - a) + ((o >> 16) & 0xFF) * a) / 255;
            picture[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
        }
    }

private:
    // The words (all but the marks) on the picture m_to is (width x height: this one's, or the turned screen's)
    void Words(int width, int height) {
        const recomp::text::Canvas canvas{m_to, width, height};
        if (!m_note.empty() && m_words.find("N:") != std::string::npos) {
            // (a long one, a shader's error say, in lines across the picture)
            const float dot = Dot(), most = static_cast<float>(width) - 48;
            const float w = std::min(recomp::text::TextWidth(m_note.c_str(), dot), most);
            const int lines = recomp::text::TextLines(m_note.c_str(), dot, most);
            const float x = (width - w) / 2, y = height * 0.12f;
            Box(static_cast<int>(x - 8), static_cast<int>(y - 6), static_cast<int>(w + 16),
                static_cast<int>((lines - 1) * recomp::text::kLineHeight * dot + 8 * dot + 12), 0xC0000000u);
            canvas.Text(x, y, dot, most, {255, 255, 255, 255}, m_note.c_str());
        }
        for (const TextAt &t : m_texts) {
            const float dot = Dot();
            const float w = recomp::text::TextWidth(t.words.c_str(), dot);
            float x = t.x * width - (t.align == 0 ? w / 2 : t.align > 0 ? w : 0);
            x = std::max(8.0f, std::min(x, width - w - 8.0f));
            const float y = std::max(6.0f, std::min(t.y * height, height - 8 * dot - 6.0f));
            Box(static_cast<int>(x - 6), static_cast<int>(y - 5), static_cast<int>(w + 12), static_cast<int>(8 * dot + 10), 0xA0000000u);
            canvas.Text(x, y, dot, 0,
                        {static_cast<uint8_t>(t.colour), static_cast<uint8_t>(t.colour >> 8), static_cast<uint8_t>(t.colour >> 16), 255},
                        t.words.c_str());
        }
        if (m_showCorner) {
            const float dot = Dot();
            const float w = recomp::text::TextWidth(m_corner.c_str(), dot);
            const float x = width - w - 14, y = height - 8 * dot - 14;
            Box(static_cast<int>(x - 6), static_cast<int>(y - 5), static_cast<int>(w + 12), static_cast<int>(8 * dot + 10), 0x90000000u);
            canvas.Text(x, y, dot, 0, {255, 255, 255, 255}, m_corner.c_str());
        }
        if (m_showLine) {
            const float dot = Dot();
            const float w = recomp::text::TextWidth(m_line.c_str(), dot);
            const float x = (width - w) / 2, y = height - 130.0f - (dot - 2) * 8; // (above the input display and the corner)
            Box(static_cast<int>(x - 6), static_cast<int>(y - 5), static_cast<int>(w + 12), static_cast<int>(8 * dot + 10), 0x90000000u);
            canvas.Text(x, y, dot, 0, {255, 255, 255, 255}, m_line.c_str());
        }
        if (m_showInputs) {
            // (the stick as a 3 x 3 grid with the way it leans lit, then the four buttons)
            const int cell = 20, gap = 4, x0 = 14, y0 = height - 12 - 3 * (cell + gap);
            Box(x0 - 6, y0 - 6, 7 * (cell + gap) + 16, 3 * (cell + gap) + 9, 0x90000000u);
            const int sx = (m_inputs & 0x08 ? 1 : 0) - (m_inputs & 0x04 ? 1 : 0), sy = (m_inputs & 0x02 ? 1 : 0) - (m_inputs & 0x01 ? 1 : 0);
            for (int gy = -1; gy <= 1; gy++) {
                for (int gx = -1; gx <= 1; gx++) {
                    const bool lit = gx == sx && gy == sy;
                    Box(x0 + (gx + 1) * (cell + gap), y0 + (gy + 1) * (cell + gap), cell, cell, lit ? 0xFF40E0FFu : 0xA0404040u);
                }
            }
            for (int b = 0; b < 4; b++) {
                const bool held = m_inputs & (0x10u << b);
                const int bx = x0 + 3 * (cell + gap) + 8 + b * (cell + gap), by = y0 + (cell + gap);
                Box(bx, by, cell, cell, held ? 0xFF60FF60u : 0xA0404040u);
                const char digit[2] = {static_cast<char>('1' + b), 0};
                canvas.Text(static_cast<float>(bx + 5), static_cast<float>(by - cell + 2), 2, 0, {220, 220, 220, 255}, digit);
            }
        }
    }

    struct MarkBox {
        int x0, y0, x1, y1;
        uint32_t fill, edge;
    };
    std::vector<MarkBox> m_marks;
    struct TextAt {
        float x, y;
        int align;
        std::string words;
        uint32_t colour;
    };
    std::vector<TextAt> m_texts;
    std::string m_words, m_wordsBefore, m_note;
    bool m_large = false;
    float Dot() const {
        return m_large ? static_cast<float>(kScale) : 2.0f;
    }
    bool m_drawn = false, m_showInputs = false, m_showCorner = false, m_showLine = false;
    std::string m_corner, m_line;
    uint32_t m_inputs = 0;
    int m_turn = 0;
    std::vector<uint32_t> m_upright, m_turned; // (the words upright, and turned: m_turn)
    uint32_t *m_to = nullptr;                  // (where Box draws, m_toW x m_toH)
    int m_toW = 0, m_toH = 0;
    void Box(int x, int y, int w, int h, uint32_t colour) {
        for (int py = std::max(y, 0); py < std::min(y + h, m_toH); py++) {
            for (int px = std::max(x, 0); px < std::min(x + w, m_toW); px++) {
                m_to[static_cast<size_t>(py) * m_toW + px] = colour;
            }
        }
    }
};

} // namespace port
