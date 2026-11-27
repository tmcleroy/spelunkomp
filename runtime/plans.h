// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The timing model's plans given to the translated code as data (a prototype, issue #18, Option B). A site is a data
// access or a taken branch in a translated block whose path since the last event is that block's own code; its plan,
// as Sh3Timing::StreamNew makes it, is a constant here (work/<id>/gen/plans/plans.cpp, written by
// tools/plans/make_plans.py from the counters' plan dumps), so the event needs no lookup (Sh3Paths::Find): the
// site's plan is run by the ordinary Sh3Timing::Run. A site's plan is used only while its block runs (SH3's
// planIndex, set by RunBlocks), its code not written meanwhile (SH3::codeWritten), and the model where the plan was
// made from (the last event's `from` and the register it loaded); anything else is the generic path. Which path gives
// the count changes nothing: the plan is the same path's (the shadow build checks every event).
#pragma once

#include "../machine/sh3.h"

namespace spelunk {

// A plan: its key's rest, its shape, and where its fetches and steps are in the game's pools
struct PlanData {
    uint32 from;
    int8_t lastLoad;
    uint8 fetches, ops;
    uint16 tail, span;
    Sh3Timing::PathInfo info;
    uint32 fetchAt, opAt;
};
struct PlanBlock {
    uint32 start, bytes;
    const uint8 *index;          // (a byte a halfword of the block: its site's number in `sites`, one on; 0 none)
    const SH3::PlanSite *sites;
};
struct PlanTables {
    const PlanData *plans = nullptr;
    const uint32 *fetch = nullptr;
    const Sh3Timing::Plan::Op *op = nullptr;
    const PlanBlock *blocks = nullptr;
    size_t planCount = 0, blockCount = 0;
};
// The game's (work/<id>/gen/plans/plans.cpp; runtime/no_plans.cpp: none)
const PlanTables &Plans();

} // namespace spelunk
