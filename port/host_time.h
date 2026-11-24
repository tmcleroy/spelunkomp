// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
//
// The host's own time a frame: how long this machine takes to make a frame (the board's frame, run-ahead's and
// rollback's frames, the picture's drawing and handing to the GPU), from the top of the frame's loop to the present, so
// without the waits for the screen or the board's pace. The speed readout's main number is the game's own (its
// slowdown, which is the board's); this tells it from the machine's. A board's frame is 16.7 ms (the CV1000's; 17.4 on
// Cave's 68000 board, 16.9 on the PGM): a machine whose time is near or over that cannot keep up. Waits for the GPU
// outside the present (a GPU slower than the game) are in it too. In game.log a minute at a time (median, p95, the
// most), and on the readout as the last second's mean when the Speed readout is "On, with the host's time". Only frames
// run and shown are counted: not those run unseen (straight to the title, on the way to a stage), stopped or under the
// menu.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace port {

class HostTime {
public:
    explicit HostTime(double frameMs)
        : m_frameMs(static_cast<float>(frameMs)) {}
    // A frame's time, `ns` of work, at `now` (SDL_GetTicksNS)
    void Add(uint64_t ns, uint64_t now) {
        const float ms = static_cast<float>(ns / 1e6);
        if (m_minute.empty()) {
            m_from = now;
        }
        m_minute.push_back(ms);
        m_recentSum += ms - m_recent[m_recentAt];
        m_recent[m_recentAt] = ms;
        m_recentAt = (m_recentAt + 1) % kRecent;
        m_recentCount = std::min(m_recentCount + 1, kRecent);
        if (now - m_from >= 60000000000ull) {
            Report(now);
        }
    }
    // The last second's mean (the last 60 frames counted), in ms; 0 before any
    double RecentMs() const {
        return m_recentCount ? m_recentSum / m_recentCount : 0;
    }
    // What has been counted since the last line, into the output (game.log): at each minute, and at the end
    void Report(uint64_t now) {
        if (m_minute.empty()) {
            return;
        }
        std::vector<float> &v = m_minute;
        const size_t n = v.size();
        size_t over = 0;
        for (float ms : v) {
            over += ms > m_frameMs;
        }
        std::sort(v.begin(), v.end());
        printf("host: %zu frames in %.0f s: the port's time a frame %.1f ms median, %.1f p95, %.1f most (the board's frame: "
               "%.1f ms; %zu over it)\n",
               n, (now - m_from) / 1e9, v[n / 2], v[std::min(n - 1, n * 95 / 100)], v[n - 1], m_frameMs, over);
        fflush(stdout);
        v.clear();
    }

private:
    static constexpr int kRecent = 60;
    float m_frameMs;
    std::vector<float> m_minute;
    uint64_t m_from = 0;
    float m_recent[kRecent] = {};
    double m_recentSum = 0;
    int m_recentAt = 0, m_recentCount = 0;
};

} // namespace port
