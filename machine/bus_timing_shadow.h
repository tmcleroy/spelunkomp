// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The shadow build's comparison (-DSHADOW_TIMING=ON; tools/shadow/): the timing model in use and the reference kept
// as it was (machine/bus_timing_ref.h) are run side by side at every event, and their states compared field by field.
// Both are the same plain data (the state a save holds), so a model is copied into the reference whole when a state
// is loaded.
#pragma once

#include "bus_timing.h"
#include "bus_timing_ref.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

namespace spelunk {

static_assert(sizeof(Sh3Timing) == sizeof(ref::Sh3Timing), "the model's state must stay as the reference's (save states)");
static_assert(std::is_trivially_copyable_v<Sh3Timing> && std::is_trivially_copyable_v<ref::Sh3Timing>);

// The fields that differ, as "name new reference" lines; empty when the two are the same
inline std::string TimingDiff(const Sh3Timing &a, const ref::Sh3Timing &b) {
    std::string out;
    char line[160];
#define SHADOW_FIELD(f)                                                                                                     \
    if (a.f != b.f) {                                                                                                       \
        snprintf(line, sizeof line, "  %s: %llu, reference %llu\n", #f, static_cast<unsigned long long>(a.f),              \
                 static_cast<unsigned long long>(b.f));                                                                     \
        out += line;                                                                                                        \
    }
    SHADOW_FIELD(bcr2)
    SHADOW_FIELD(wcr1)
    SHADOW_FIELD(wcr2)
    SHADOW_FIELD(mcr)
    SHADOW_FIELD(ccr)
    SHADOW_FIELD(sdramFree)
    SHADOW_FIELD(postedDone)
    SHADOW_FIELD(otherFree)
    SHADOW_FIELD(lastInsn)
    SHADOW_FIELD(lastTime)
    SHADOW_FIELD(from)
    SHADOW_FIELD(lastFetchLine)
    SHADOW_FIELD(nextDecode)
    SHADOW_FIELD(mmEnd)
    SHADOW_FIELD(lastLoad)
    SHADOW_FIELD(nextLoad)
    SHADOW_FIELD(inSlot)
    SHADOW_FIELD(own)
    SHADOW_FIELD(portBase)
    SHADOW_FIELD(portMask)
#undef SHADOW_FIELD
    for (uint32_t s = 0; s < Sh3Timing::kSets; s++) {
        for (uint32_t w = 0; w < Sh3Timing::kWays; w++) {
            if (a.ways[s][w].line != b.ways[s][w].line || a.ways[s][w].dirty != b.ways[s][w].dirty ||
                a.order[s][w] != b.order[s][w]) {
                snprintf(line, sizeof line, "  cache set %u way %u: line %08X dirty %u order %u, reference line %08X dirty %u order %u\n", s, w,
                         a.ways[s][w].line, a.ways[s][w].dirty, a.order[s][w], b.ways[s][w].line, b.ways[s][w].dirty, b.order[s][w]);
                out += line;
            }
        }
    }
    return out;
}

// The scalar part alone (each event); the cache's arrays are compared with these at each frame's end and on a mismatch
inline bool TimingScalarsEqual(const Sh3Timing &a, const ref::Sh3Timing &b) {
    return a.sdramFree == b.sdramFree && a.postedDone == b.postedDone && a.otherFree == b.otherFree && a.lastInsn == b.lastInsn &&
           a.lastTime == b.lastTime && a.from == b.from && a.lastFetchLine == b.lastFetchLine && a.nextDecode == b.nextDecode &&
           a.mmEnd == b.mmEnd && a.lastLoad == b.lastLoad && a.nextLoad == b.nextLoad && a.inSlot == b.inSlot && a.own == b.own &&
           a.portBase == b.portBase && a.portMask == b.portMask && a.ccr == b.ccr && a.bcr2 == b.bcr2 && a.wcr2 == b.wcr2;
}
inline bool TimingCacheEqual(const Sh3Timing &a, const ref::Sh3Timing &b) {
    for (uint32_t s = 0; s < Sh3Timing::kSets; s++) {
        for (uint32_t w = 0; w < Sh3Timing::kWays; w++) {
            if (a.ways[s][w].line != b.ways[s][w].line || a.ways[s][w].dirty != b.ways[s][w].dirty) {
                return false;
            }
        }
    }
    return std::memcmp(a.order, b.order, sizeof a.order) == 0;
}

// The reference given the model's state (a state loaded: the same plain data)
inline void TimingCopy(const Sh3Timing &from, ref::Sh3Timing &to) {
    std::memcpy(static_cast<void *>(&to), static_cast<const void *>(&from), sizeof to);
}

} // namespace spelunk
