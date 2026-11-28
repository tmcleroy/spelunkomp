// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The feature "Gadgets": framed panels in the space the picture leaves at its sides in a wide window: the game's
// hidden counters as numbers, meters and a graph over time (the rank graph), the score each stage made, the controls
// held. Drawn on a picture of its own a whole fraction of the window's size (about 360 lines, so a dot is a few of the
// window's pixels and sharp, as the board's own picture is), laid over the whole window by whichever draws it, in the
// place of the touch controls' layer (port/touch.h: those and the gadgets want the same sides; the touch controls,
// while shown, come first). Only read from the board (runtime/game.h's Gadget), never written to it.
#pragma once

#include "../recomp-common/text.h"
#include "../runtime/game.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace port {

class Gadgets {
public:
    std::vector<uint32_t> pixels; // red in the low byte, alpha in the high; width x height, each dot `scale` window pixels
    int width = 0, height = 0, scale = 1;

    // This frame's panels, for a window of ow x oh pixels with the picture at `picture` (its pixels too). (Whether a
    // game is played is each gadget's own: its words empty, its level false, the score -1 otherwise.) `inputs`:
    // player 1's controls held (machine/cv1000.h's kIn bits). True if the picture differs from the last (to be handed
    // on again); none to show: pixels empty
    bool Frame(const spelunk::Game &game, const spelunk::Board &board, uint32_t inputs, const SDL_FRect &picture, int ow,
               int oh) {
        m_words.clear();
        m_frame++;
        // (the layer's size: a whole fraction of the window, about 360 lines)
        const int s = std::max(1, (oh + 180) / 360);
        const int lw = (ow + s - 1) / s, lh = (oh + s - 1) / s;
        const int px0 = static_cast<int>(std::floor(picture.x / s)), px1 = static_cast<int>(std::ceil((picture.x + picture.w) / s));
        const int leftRoom = px0 - 2 * kMargin, rightRoom = lw - px1 - 2 * kMargin;
        const int leftW = std::min(kMostWidth, leftRoom), rightW = std::min(kMostWidth, rightRoom);
        const bool left = leftW >= kLeastWidth, right = rightW >= kLeastWidth;

        // (what each gadget shows now: kept, for the graphs and the score by stage, whether or not there is room)
        m_items.clear();
        TakeScores(game, board);
        m_history.resize(game.gadgets.size());
        for (size_t i = 0; i < game.gadgets.size(); i++) {
            const spelunk::Gadget &g = game.gadgets[i];
            Item item;
            item.title = g.title;
            item.kind = g.kind;
            item.colour = g.colour;
            item.right = g.right;
            item.history = &m_history[i];
            if (g.words) {
                item.words = g.words(board);
            }
            if (g.kind == spelunk::Gadget::Kind::Words) {
                if (item.words.empty()) {
                    continue;
                }
            } else {
                int value = 0, most = 0;
                if (!g.level || !g.level(board, value, most) || most <= 0) {
                    m_history[i].clear();
                    continue;
                }
                item.value = std::clamp(value, 0, most);
                item.most = most;
                if (g.kind == spelunk::Gadget::Kind::Graph && m_frame % kGraphEvery == 0) {
                    std::vector<float> &h = m_history[i];
                    h.push_back(static_cast<float>(item.value) / most);
                    if (h.size() > kGraphSamples) {
                        h.erase(h.begin(), h.begin() + static_cast<long>(h.size() - kGraphSamples));
                    }
                }
                if (item.words.empty()) {
                    item.words = std::to_string(item.value) + " / " + std::to_string(most);
                }
            }
            m_items.push_back(std::move(item));
        }
        // (the game's scoring readouts, the feature "readouts"'s counters, as one panel: for a game with no gadgets of
        // its own, whose gadgets show the same)
        if (game.gadgets.empty()) {
            std::string lines;
            for (const spelunk::Readout &r : game.readouts) {
                if (const std::string value = r.value(board); !value.empty()) {
                    lines += (lines.empty() ? "" : "\n") + std::string(r.name) + " " + value;
                }
            }
            if (!lines.empty()) {
                Item item;
                item.title = "COUNTERS";
                item.words = lines;
                m_items.push_back(std::move(item));
            }
        }
        if (!m_scoreLines.empty()) {
            Item item;
            item.title = "SCORE BY STAGE";
            item.words = m_scoreLines;
            item.right = true;
            m_items.push_back(std::move(item));
        }
        {
            Item item;
            item.title = "CONTROLS";
            item.kind = kControls;
            item.value = static_cast<int>(inputs & 0xFF);
            item.right = true;
            m_items.push_back(std::move(item));
        }

        // (the signature of what is drawn: the same as the last frame's, nothing drawn again)
        m_words = std::to_string(lw) + "x" + std::to_string(lh) + "@" + std::to_string(px0) + "," + std::to_string(px1) + (left ? "L" : "") +
                  (right ? "R" : "") + "\n";
        if (left || right) {
            for (const Item &item : m_items) {
                m_words += item.title + "|" + std::to_string(static_cast<int>(item.kind)) + "|" + item.words + "|" + std::to_string(item.value) +
                           "/" + std::to_string(item.most) + "|" + std::to_string(item.history ? item.history->size() : 0) + "|" +
                           std::to_string(item.history && !item.history->empty() ? item.history->back() : 0.0f) + "\n";
            }
        }
        if (m_words == m_wordsBefore) {
            return false;
        }
        m_wordsBefore = m_words;
        if (!left && !right) {
            const bool had = !pixels.empty();
            pixels.clear();
            width = height = 0;
            return had;
        }
        width = lw;
        height = lh;
        scale = s;
        pixels.assign(static_cast<size_t>(lw) * lh, 0u);
        // (a side too narrow: its gadgets go to the other)
        int y[2] = {kMargin, kMargin};
        for (const Item &item : m_items) {
            const int side = (item.right && right) || !left ? 1 : 0;
            // (no room left on its side: the other side's, if it has some)
            for (const int at : {side, 1 - side}) {
                if (!(at ? right : left)) {
                    continue;
                }
                const int w = at ? rightW : leftW;
                const int x = at ? px1 + kMargin : px0 - kMargin - w;
                if (const int h = Draw(item, x, y[at], w, lh)) {
                    y[at] += h + kGap;
                    break;
                }
            }
        }
        return true;
    }
    // The feature off: nothing shown (true if something was)
    bool Off() {
        m_wordsBefore.clear();
        m_history.clear();
        m_entries.clear();
        m_scoreLines.clear();
        const bool had = !pixels.empty();
        pixels.clear();
        width = height = 0;
        return had;
    }

private:
    static constexpr int kMargin = 6, kGap = 5, kMostWidth = 156, kLeastWidth = 84;
    static constexpr int kHeader = 11, kPad = 4;
    static constexpr size_t kGraphSamples = 240; // (one every kGraphEvery frames shown: the last 32 seconds)
    static constexpr uint64_t kGraphEvery = 8;
    static constexpr auto kControls = static_cast<spelunk::Gadget::Kind>(100);

    struct Item {
        std::string title, words;
        spelunk::Gadget::Kind kind = spelunk::Gadget::Kind::Words;
        int value = 0, most = 0;
        uint32_t colour = 0xFFFFC040u;
        bool right = false;
        const std::vector<float> *history = nullptr;
    };
    std::vector<Item> m_items;
    std::vector<std::vector<float>> m_history;
    std::string m_words, m_wordsBefore;
    uint64_t m_frame = 0;

    // The score by stage: the score as each stage was entered (in order of entry), and the lines made of them
    struct Entry {
        int stage;
        long long score;
    };
    std::vector<Entry> m_entries;
    long long m_lastScore = -1;
    std::string m_scoreLines;

    static std::string Grouped(long long n) {
        std::string digits = std::to_string(n < 0 ? 0 : n), out;
        for (size_t i = 0; i < digits.size(); i++) {
            if (i && (digits.size() - i) % 3 == 0) {
                out += ',';
            }
            out += digits[i];
        }
        return out;
    }
    void TakeScores(const spelunk::Game &game, const spelunk::Board &board) {
        if (!game.score || !game.stage) {
            return;
        }
        const long long score = game.score(board);
        const int stage = game.stage(board);
        if (score < 0 || stage < 0) {
            return; // (the lines kept as they were: the last game's, until the next begins)
        }
        // (a new game: the score gone down)
        if (score < m_lastScore) {
            m_entries.clear();
        }
        m_lastScore = score;
        if (m_entries.empty() || m_entries.back().stage != stage) {
            m_entries.push_back({stage, m_entries.empty() ? 0 : score});
        }
        std::string lines;
        const size_t first = m_entries.size() > 14 ? m_entries.size() - 14 : 0; // (the last 14 shown)
        for (size_t i = first; i < m_entries.size(); i++) {
            const long long made = (i + 1 < m_entries.size() ? m_entries[i + 1].score : score) - m_entries[i].score;
            const int st = m_entries[i].stage;
            std::string name = st < static_cast<int>(game.stageNames.size()) ? game.stageNames[st] : std::to_string(st + 1);
            // ("Stage 3" shown as 3, as the games' own names give them)
            if (name.rfind("Stage ", 0) == 0) {
                name = name.substr(6);
            }
            name.resize(std::max<size_t>(name.size() + 1, 4), ' ');
            lines += (lines.empty() ? "" : "\n") + std::string(i + 1 == m_entries.size() ? ">" : " ") + name + Grouped(made);
        }
        lines += "\n TOTAL " + Grouped(score);
        m_scoreLines = lines;
    }

    // A box of colour (red in the low byte) put in, not blended
    void Box(int x, int y, int w, int h, uint32_t colour) {
        for (int py = std::max(y, 0); py < std::min(y + h, height); py++) {
            for (int px = std::max(x, 0); px < std::min(x + w, width); px++) {
                pixels[static_cast<size_t>(py) * width + px] = colour;
            }
        }
    }
    void Words(int x, int y, const std::string &words, uint32_t colour) {
        const recomp::text::Canvas canvas{pixels.data(), width, height};
        const recomp::text::Color c{static_cast<uint8_t>(colour), static_cast<uint8_t>(colour >> 8), static_cast<uint8_t>(colour >> 16), 255};
        canvas.Chars(static_cast<float>(x), static_cast<float>(y), 1, words.c_str(), words.size(), c);
    }
    // Lines broken at '\n', and those longer than `fits` characters at their last space before it (or cut), the rest
    // of one indented a character
    static std::vector<std::string> Wrapped(const std::string &s, size_t fits) {
        std::vector<std::string> out;
        fits = std::max<size_t>(fits, 4);
        size_t at = 0;
        while (at <= s.size()) {
            const size_t end = std::min(s.find('\n', at), s.size());
            std::string line = s.substr(at, end - at);
            while (line.size() > fits) {
                size_t cut = line.rfind(' ', fits);
                if (cut == std::string::npos || cut < 2) {
                    cut = fits;
                }
                out.push_back(line.substr(0, cut));
                line = " " + line.substr(line[cut] == ' ' ? cut + 1 : cut);
            }
            out.push_back(line);
            at = end + 1;
        }
        return out;
    }
    // One gadget at x, y, w wide; its height
    int Draw(const Item &item, int x, int y, int w, int most) {
        using Kind = spelunk::Gadget::Kind;
        int body = 0;
        const int bx = x + kPad, by = y + kHeader + kPad, bw = w - 2 * kPad;
        // (a meter's or a graph's one line, cut to the panel)
        auto fit = [&](std::string s) {
            s.resize(std::min(s.size(), static_cast<size_t>(bw / recomp::text::kCharWidth)));
            return s;
        };
        const std::vector<std::string> lines = item.kind == Kind::Words ? Wrapped(item.words, static_cast<size_t>(bw / recomp::text::kCharWidth))
                                                                        : std::vector<std::string>();
        switch (item.kind) {
        case Kind::Words: body = static_cast<int>(lines.size()) * recomp::text::kLineHeight; break;
        case Kind::Meter: body = recomp::text::kLineHeight + 9; break;
        case Kind::Graph: body = kGraphHeight + 2 + recomp::text::kLineHeight; break;
        default: body = 3 * 8 + 2; break; // (the controls)
        }
        const int h = kHeader + 2 * kPad + body;
        if (y + h > most) {
            return 0; // (no room left on this side)
        }
        // (the frame: a dark panel, its title on a band)
        Box(x - 1, y - 1, w + 2, h + 2, 0xFF6A5A48u);
        Box(x, y, w, h, 0xD8180F0Au);
        Box(x, y, w, kHeader, 0xF0583C24u);
        Words(x + 3, y + 2, item.title, 0xFFE8E0D8u);
        switch (item.kind) {
        case Kind::Words: {
            for (size_t line = 0; line < lines.size(); line++) {
                // (">" marks the line now: the stage being played)
                const bool now = !lines[line].empty() && lines[line][0] == '>';
                Words(bx, by + static_cast<int>(line) * recomp::text::kLineHeight, lines[line], now ? 0xFF60E8FFu : 0xFFFFFFFFu);
            }
            break;
        }
        case Kind::Meter: {
            Words(bx, by, fit(item.words), 0xFFFFFFFFu);
            const int my = by + recomp::text::kLineHeight + 1;
            Box(bx, my, bw, 7, 0xFF402820u);
            Box(bx + 1, my + 1, (bw - 2) * item.value / item.most, 5, item.colour);
            break;
        }
        case Kind::Graph: {
            // (the line over the last half minute, filled under, newest at the right; a grid at the quarters)
            Box(bx, by, bw, kGraphHeight, 0xFF301E14u);
            for (int q = 1; q < 4; q++) {
                Box(bx, by + kGraphHeight * q / 4, bw, 1, 0xFF4A3426u);
            }
            const std::vector<float> &hist = *item.history;
            const uint32_t under = (item.colour & 0x00FFFFFFu) | 0x70000000u;
            const int n = static_cast<int>(hist.size());
            for (int col = 0; col < bw; col++) {
                const int i = n - bw + col;
                if (i < 0) {
                    continue;
                }
                const int top = by + kGraphHeight - 1 - static_cast<int>(std::lround(hist[i] * (kGraphHeight - 1)));
                Box(bx + col, top + 1, 1, by + kGraphHeight - top - 1, under);
                Box(bx + col, top, 1, 1, item.colour);
            }
            char pct[16];
            snprintf(pct, sizeof pct, "  %d%%", item.value * 100 / item.most);
            // (the share of its most, unless the words give one)
            Words(bx, by + kGraphHeight + 2, fit(item.words.find('%') == std::string::npos ? item.words + pct : item.words), 0xFFFFFFFFu);
            break;
        }
        default: {
            // (the stick as a 3 x 3 grid with the way it leans lit, then the four buttons)
            const int cell = 7, gap = 1;
            const int bits = item.value;
            const int sx = (bits & 0x08 ? 1 : 0) - (bits & 0x04 ? 1 : 0), sy = (bits & 0x02 ? 1 : 0) - (bits & 0x01 ? 1 : 0);
            for (int gy = -1; gy <= 1; gy++) {
                for (int gx = -1; gx <= 1; gx++) {
                    Box(bx + (gx + 1) * (cell + gap), by + (gy + 1) * (cell + gap), cell, cell, gx == sx && gy == sy ? 0xFF40E0FFu : 0xFF403020u);
                }
            }
            for (int b = 0; b < 4; b++) {
                const bool held = bits & (0x10 << b);
                const int x0 = bx + 3 * (cell + gap) + 8 + b * 14, y0 = by + 6;
                Box(x0, y0, 11, 11, held ? 0xFF60FF60u : 0xFF403020u);
                const char digit[2] = {static_cast<char>('1' + b), 0};
                Words(x0 + 3, y0 + 2, digit, held ? 0xFF000000u : 0xFFB0B0B0u);
            }
            break;
        }
        }
        return h;
    }
    static constexpr int kGraphHeight = 36;
};

} // namespace port
