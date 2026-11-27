// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The main thread's waits between frames, and keeping its core awake through them (a prototype, issue #18). On a
// Steam Machine (amd-pstate-epp, powersave) a core idle for a good part of each frame is clocked down, and the next
// frame's work (run-ahead's put-back above all) then starts on a slow core. SPELUNK_KEEP_AWAKE:
//   0  (or unset) the waits as ever: the frame pacing sleeps, the GPU's waits block
//   1  the frame pacing sleeps until 3 ms before the next frame, then spins to it
//   2  every wait spins: the frame pacing, the GPU's fences (EndFrame, SubmitWork) and the swapchain's acquire
// The spin pauses the core (x86 PAUSE, ARM YIELD) and yields to the system every 50 microseconds. Nothing of the
// game's is touched: only when the main thread goes on. Each wait's time is counted, and said with
// SPELUNK_AHEAD_TIMES's lines (port/main.cpp): where a frame's idle time goes.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace spelunk::awake {

inline int Mode() {
    static const int mode = getenv("SPELUNK_KEEP_AWAKE") ? atoi(getenv("SPELUNK_KEEP_AWAKE")) : 0;
    return mode;
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
// (each wait's time, nanoseconds, over the run)
struct Waits {
    uint64_t pacing = 0, acquire = 0, fences = 0;
};
inline Waits g_waits;
// Spins until `done()` (or `until`, the steady clock's), the core kept busy
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
    const uint64_t from = NowNs();
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
    g_waits.pacing += NowNs() - from;
}

} // namespace spelunk::awake
