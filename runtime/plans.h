// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The timing model's plans compiled into the translated code (a prototype, issue #18; docs: the compiled-timing
// design note). A site is a data access or a taken branch in a translated block whose path since the last event is
// that block's own code: its plan, as Sh3Timing::StreamNew makes it, is a constant here (work/<id>/gen/plans.cpp,
// written by tools/plans/make_plans.py from the counters' plan dump), so running it is Sh3Timing::Run with every
// fetch and step known: no lookup, the loops unrolled. A site's function is used only while its block runs (SH3's
// planSlots, set by RunBlocks), its code not written meanwhile (SH3::codeWritten), and the model where the plan was
// made from (the last event's `from` and the register it loaded); anything else is the generic path. Which path
// gives the count changes nothing: the plan is the same path's (the shadow build checks every event).
#pragma once

#include "../machine/sh3.h"

namespace spelunk {

constexpr uint32 kPlanFetches = 8, kPlanOps = 16;
struct PlanConst {
    uint32 from;
    int8_t lastLoad;
    uint32 fetches, ops, tail, span;
    Sh3Timing::PathInfo info;
    uint32 fetch[kPlanFetches];
    Sh3Timing::Plan::Op op[kPlanOps];
};
struct PlanBlock {
    uint32 start, bytes;
    const SH3::PlanSlot *slots;     // (one a halfword of the block: slots[(insn - start) / 2]; each plan's own code)
    const SH3::PlanSlot *dataSlots; // (the same plans run as data, SPELUNK_PLANS_DATA=1: PlannedAccessData)
};
// The game's (work/<id>/gen/plans.cpp; runtime/no_plans.cpp: none)
const PlanBlock *PlanBlocks(size_t &count);

// (one code reader for every site, so AccessDataOut is one function)
struct PlanCode {
    SH3 *c;
    uint16 operator()(uint32 p) const {
        return c->TimingCode(p);
    }
};
// (ID 0 is the planted fault's, SPELUNK_PLANT_PLAN=1: its tail one cycle longer, for the shadow to find)
template <const PlanConst &P, int ID>
bool PlannedAccess(SH3 &c, uint32 a, uint32 bytes, bool write, uint32 &got) {
    Sh3Timing &m = c.memoryTiming;
    if (m.from != P.from || m.lastLoad != P.lastLoad) {
        return false;
    }
    const bool streamed = c.at != m.lastInsn || c.cycles != m.lastTime;
    const uint32 extra = streamed ? m.Run(P.fetch, P.fetches, P.op, P.ops, P.tail + (ID == 0 ? c.plantPlan : 0u), P.span, P.info,
                                          false, c.inSlot, c.cycles)
                                  : 0;
    got = m.AccessDataOut(c.timingPaths, c.at, a, bytes, write, c.cycles, extra, streamed, PlanCode{&c});
    return true;
}
// The plan as data (SPELUNK_PLANS_DATA=1): the site's guards, then one shared function running the plan through the
// ordinary Run and Walk; no lookup, nothing unrolled, a few bytes a site (the lookup's share of the gain alone)
bool PlannedAccessDyn(SH3 &c, const PlanConst &p, uint32 plant, uint32 a, uint32 bytes, bool write, uint32 &got);
bool PlannedTakenDyn(SH3 &c, const PlanConst &p, uint32 plant, uint32 branch, uint32 target, uint32 &got);
template <const PlanConst &P, int ID>
bool PlannedAccessData(SH3 &c, uint32 a, uint32 bytes, bool write, uint32 &got) {
    return PlannedAccessDyn(c, P, ID == 0 ? c.plantPlan : 0u, a, bytes, write, got);
}
template <const PlanConst &P, int ID>
bool PlannedTakenData(SH3 &c, uint32 branch, uint32 target, uint32 &got) {
    return PlannedTakenDyn(c, P, ID == 0 ? c.plantPlan : 0u, branch, target, got);
}
template <const PlanConst &P, int ID>
bool PlannedTaken(SH3 &c, uint32 branch, uint32 target, uint32 &got) {
    Sh3Timing &m = c.memoryTiming;
    if (m.from != P.from || m.lastLoad != P.lastLoad) {
        return false;
    }
    got = m.TakenCore(branch, target, c.cycles, PlanCode{&c}, [&]() __attribute__((always_inline)) {
        return m.Run(P.fetch, P.fetches, P.op, P.ops, P.tail + (ID == 0 ? c.plantPlan : 0u), P.span, P.info, true, false, c.cycles);
    });
    return true;
}

} // namespace spelunk
