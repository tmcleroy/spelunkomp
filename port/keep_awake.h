// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The main thread's waits between frames, and keeping its core awake through them (the Game page's "Processor
// between frames", runtime/features.cpp). A processor's power management clocks a core down when it is idle for a
// good part of each frame, waiting for the next: on a Steam Machine (amd-pstate-epp) from 4.8 GHz to 3.7 in play, so
// that the next frame's work, and run-ahead's frames run again above all, starts on a slow core. The modes:
//   0  Off: the waits as ever (the frame pacing sleeps, the GPU's waits block)
//   1  Light: the frame pacing sleeps until 3 ms before the next frame, then spins to it (the default)
//   2  Full: every wait spins: the frame pacing, the GPU's fences (EndFrame, SubmitWork), the swapchain's acquire
// The spin pauses the core (x86 PAUSE, ARM YIELD) and yields to the system every 50 microseconds. Nothing of the
// game's is touched: only when the main thread goes on (the same frames, state and pictures in every mode). On
// Windows, SDL sets the timer's resolution to 1 ms (SDL_HINT_TIMER_RESOLUTION's default), so the sleep before
// Light's 3 ms is short of them. SPELUNK_KEEP_AWAKE=0, 1 or 2 overrides the setting (for measuring).
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace spelunk::awake {

// (the mode in use: port/main.cpp sets it from the setting each frame, Use; SPELUNK_KEEP_AWAKE over it)
inline int g_mode = 1;
inline int Mode() {
    return g_mode;
}
inline void Use(int mode) {
    static const int over = getenv("SPELUNK_KEEP_AWAKE") ? atoi(getenv("SPELUNK_KEEP_AWAKE")) : -1;
    g_mode = over >= 0 ? over : mode;
}
inline void Pause() {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}
inline uint64_t NowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
// Spins until `done()`, the core kept busy
template <typename Done>
void SpinWhile(Done done) {
    uint64_t yieldAt = NowNs() + 50000;
    while (!done()) {
        for (int i = 0; i < 32; i++) {
            Pause();
        }
        if (const uint64_t now = NowNs(); now >= yieldAt) {
            std::this_thread::yield();
            yieldAt = now + 50000;
        }
    }
}
// The frame pacing's wait until `next`, as the mode has it
inline void PaceUntil(std::chrono::steady_clock::time_point next) {
    const int mode = Mode();
    if (mode == 1) {
        const auto early = next - std::chrono::milliseconds(3);
        if (early > std::chrono::steady_clock::now()) {
            std::this_thread::sleep_until(early);
        }
    }
    if (mode >= 1) {
        SpinWhile([&]() { return std::chrono::steady_clock::now() >= next; });
    } else {
        std::this_thread::sleep_until(next);
    }
}

} // namespace spelunk::awake
