// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
// The block dispatcher: translations where there are any (and the RAM still holds what they were made from), the
// interpreter a block at a time where not. runtime/blocks.h says how blocks are cut.
#include "blocks.h"

#include "../machine/sh3.h"
#include "../machine/sh3_ops.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spelunk {

namespace {
    BlockEnd g_ends[0x10000];
    bool g_ready = false;
    void BuildEnds() {
        using namespace ops;
        for (uint32 op = 0; op < 0x10000; op++) {
            const OpFn f = SH3::s_ops[op];
            BlockEnd e = BlockEnd::No;
            if (f == BRA || f == BSR || f == BRAF || f == BSRF || f == JMP || f == JSR || f == RTS || f == RTE || f == BT ||
                f == BF || f == BTS || f == BFS) {
                e = BlockEnd::Branch;
            } else if (f == TRAPA || f == ILLEGAL || f == SLEEP || f == LDC_SR || f == LDCL_SR) {
                e = BlockEnd::Barrier;
            }
            g_ends[op] = e;
        }
        g_ready = true;
    }
    bool Delayed(uint16 op) {
        using namespace ops;
        const OpFn f = SH3::s_ops[op];
        return f != BT && f != BF && f != TRAPA && f != ILLEGAL && f != SLEEP && f != LDC_SR && f != LDCL_SR;
    }
} // namespace

BlockEnd EndsBlock(uint16_t opcode) {
    if (!g_ready) [[unlikely]] {
        BuildEnds();
    }
    return g_ends[opcode];
}

bool SH3::RecordBlock(uint32 pc, bool found) {
    const uint32 phys = pc & (kRamSize - 1);
    std::vector<uint32> &versions = m_recordedAt[pc];
    if (!IsRam(pc) && !versions.empty()) {
        // (the flash: never another version)
        if (!found) {
            m_recorded[versions[0]].found = false;
        }
        return false;
    }
    // (the same bytes as a version already recorded: the same block, since where it ends is read from them)
    for (uint32 i : versions) {
        Recorded &r = m_recorded[i];
        if (phys + r.bytes.size() <= kRamSize && std::memcmp(ram + phys, r.bytes.data(), r.bytes.size()) == 0) {
            if (!found) {
                r.found = false; // (found by following, and now run)
            }
            return false;
        }
    }
    // (the block's bytes as they are now, by the shared rule)
    std::vector<uint8> bytes;
    uint32 a = pc;
    for (unsigned n = 0; n < kMaxBlockInstructions; n++) {
        const uint16 op = Fetch(a);
        bytes.push_back(static_cast<uint8>(op >> 8)); // (as RAM holds them: big-endian)
        bytes.push_back(static_cast<uint8>(op));
        const BlockEnd end = EndsBlock(op);
        if (end != BlockEnd::No) {
            if (end == BlockEnd::Branch && Delayed(op)) {
                const uint16 slotOp = Fetch(a + 2);
                bytes.push_back(static_cast<uint8>(slotOp >> 8));
                bytes.push_back(static_cast<uint8>(slotOp));
            }
            break;
        }
        a += 2;
    }
    versions.push_back(static_cast<uint32>(m_recorded.size()));
    m_recorded.push_back({pc, found, std::move(bytes)});
    return true;
}

void SH3::FollowBlock(uint32 start, unsigned steps) {
    using namespace ops;
    std::vector<std::pair<uint32, unsigned>> work{{start, steps}};
    while (!work.empty()) {
        const auto [pc, left] = work.back();
        work.pop_back();
        uint32 a = pc;
        uint16 op = 0;
        BlockEnd end = BlockEnd::No;
        for (unsigned n = 0; n < kMaxBlockInstructions; n++, a += 2) {
            op = Fetch(a);
            end = EndsBlock(op);
            if (end != BlockEnd::No) {
                break;
            }
        }
        // Where the CPU can go next from this block's end, as far as the code says (not through a register)
        uint32 next[2];
        int count = 0;
        const OpFn f = SH3::s_ops[op];
        const uint32 near = a + 4 + static_cast<uint32>(static_cast<sint32>(static_cast<sint8>(op & 0xFF)) * 2);
        const uint32 far = a + 4 + static_cast<uint32>((static_cast<sint32>((op & 0xFFF) << 20) >> 20) * 2);
        if (end == BlockEnd::No) {
            next[count++] = a; // (128 instructions: the next block goes on from there)
        } else if (f == BT || f == BF) {
            next[count++] = near;
            next[count++] = a + 2;
        } else if (f == BTS || f == BFS) {
            next[count++] = near;
            next[count++] = a + 4;
        } else if (f == BRA) {
            next[count++] = far;
        } else if (f == BSR) {
            next[count++] = far;
            next[count++] = a + 4; // (where the call returns to)
        } else if (f == BSRF || f == JSR) {
            next[count++] = a + 4;
        } else if (f == TRAPA || f == SLEEP || f == LDC_SR || f == LDCL_SR) {
            next[count++] = a + 2;
        }
        for (int i = 0; i < count; i++) {
            if ((IsRam(next[i]) || IsFlash(next[i])) && !(next[i] & 1) && RecordBlock(next[i], true) && left > 1) {
                work.push_back({next[i], left - 1});
            }
        }
    }
}

// ---- The plans the translated sites keep (SitePlans, sh3.h) ----
namespace {
    // (one code reader for every site, so AccessDataOut is one function)
    struct PlanCode {
        SH3 *c;
        uint16 operator()(uint32 p) const {
            return c->TimingCode(p);
        }
    };
}
// A site's plan, for an event that took the generic path: made as StreamNew makes it (Sh3Timing::DerivePlan) and kept
// for the site if it may be: known, its key the block's (its start reached by a branch, or the last event in it), the
// bytes it read the block's alone. A site keeps two keys; a third is not kept
static void KeepPlan(SH3 &c, SH3::Translation &t, uint32 site, bool taken, uint32 from, sint8 lastLoad) {
    const uint32 start = c.planStart, end = c.planStart + c.planBytes;
    if (!(from == (start | 1u) || (!(from & 1u) && from > start && from <= site))) {
        return;
    }
    Sh3Timing::Made made;
    c.memoryTiming.DerivePlan(from, lastLoad, site, taken, PlanCode{&c}, made);
    const bool keep = made.info.known && made.lo != Sh3Timing::kNoLine && made.lo >= start && made.lo + made.bytes <= end &&
                      made.plan.tail <= 0xFFFF && made.plan.span <= 0xFFFF;
    if (!t.plans) {
        t.plans = std::make_unique<SitePlans>();
        t.plans->index.assign(c.planBytes / 2, 0); // (a block's slots made at its first plan, or its first path refused)
    }
    SitePlans &sp = *t.plans;
    uint8 &at = sp.index[(site - start) / 2];
    if (!at) {
        if (sp.sites.size() >= 255) {
            return; // (a byte names a site: a block's first 255 sites)
        }
        sp.sites.emplace_back();
        at = static_cast<uint8>(sp.sites.size());
    }
    if (!keep) {
        sp.sites[at - 1].refused[taken ? 1 : 0]++; // (not made again for this site from the second on)
        return;
    }
    sint32 *slot = taken ? sp.sites[at - 1].taken : sp.sites[at - 1].access;
    const int free = slot[0] < 0 ? 0 : slot[1] < 0 ? 1 : -1;
    if (free < 0) {
        return;
    }
    SitePlans::Plan p{from, lastLoad, static_cast<uint8>(made.plan.fetches), static_cast<uint8>(made.plan.ops),
                      static_cast<uint16>(made.plan.tail), static_cast<uint16>(made.plan.span), made.info,
                      static_cast<uint32>(sp.fetch.size()), static_cast<uint32>(sp.op.size())};
    if (c.plantPlan == 1) {
        p.tail++; // (SPELUNK_PLANT_PLAN=1: the first plan kept a cycle longer, for the shadow to find)
        c.plantPlan = 2;
    }
    sp.fetch.insert(sp.fetch.end(), made.plan.fetch, made.plan.fetch + made.plan.fetches);
    sp.op.insert(sp.op.end(), made.plan.op, made.plan.op + made.plan.ops);
    slot[free] = static_cast<sint32>(sp.plans.size());
    sp.plans.push_back(p);
}
__attribute__((noinline)) uint32 SH3::SiteAccess(uint32 a, uint32 bytes, bool write) {
    Translation &t = *planBlock;
    const uint32 site = at;
    const sint32 *slot = nullptr;
    bool refused = false;
    if (t.plans) {
        if (const uint8 i = t.plans->index[(site - planStart) / 2]) {
            const SitePlans &sp = *t.plans;
            slot = sp.sites[i - 1].access;
            refused = sp.sites[i - 1].refused[0] >= 2;
            for (int k = 0; k < 2 && slot[k] >= 0; k++) {
                const SitePlans::Plan &p = sp.plans[static_cast<size_t>(slot[k])];
                if (memoryTiming.from == p.from && memoryTiming.lastLoad == p.lastLoad) {
                    const bool streamed = at != memoryTiming.lastInsn || cycles != memoryTiming.lastTime;
                    const uint32 extra = streamed ? memoryTiming.Run(sp.fetch.data() + p.fetchAt, p.fetches, sp.op.data() + p.opAt, p.ops, p.tail,
                                                                     p.span, p.info, false, inSlot, cycles)
                                                  : 0;
                    plannedEvents++;
                    return memoryTiming.AccessDataOut(timingPaths, at, a, bytes, write, cycles, extra, streamed, PlanCode{this});
                }
            }
        }
    }
    genericEvents++;
    const uint32 from = memoryTiming.from;
    const sint8 lastLoad = memoryTiming.lastLoad;
    const bool streamed = at != memoryTiming.lastInsn || cycles != memoryTiming.lastTime;
    const uint32 got = memoryTiming.Access(timingPaths, at, a, bytes, write, inSlot, cycles, [this](uint32 p) -> uint16 { return TimingCode(p); });
    if (streamed && !refused && !(slot && slot[1] >= 0)) {
        KeepPlan(*this, t, site, false, from, lastLoad); // (a site with room: its plan made for the next time)
    }
    return got;
}
__attribute__((noinline)) uint32 SH3::SiteTaken(uint32 branch, uint32 target) {
    Translation &t = *planBlock;
    const sint32 *slot = nullptr;
    bool refused = false;
    if (t.plans) {
        if (const uint8 i = t.plans->index[(branch - planStart) / 2]) {
            const SitePlans &sp = *t.plans;
            slot = sp.sites[i - 1].taken;
            refused = sp.sites[i - 1].refused[1] >= 2;
            for (int k = 0; k < 2 && slot[k] >= 0; k++) {
                const SitePlans::Plan &p = sp.plans[static_cast<size_t>(slot[k])];
                if (memoryTiming.from == p.from && memoryTiming.lastLoad == p.lastLoad) {
                    plannedEvents++;
                    return memoryTiming.TakenCore(branch, target, cycles, PlanCode{this}, [&]() __attribute__((always_inline)) {
                        return memoryTiming.Run(sp.fetch.data() + p.fetchAt, p.fetches, sp.op.data() + p.opAt, p.ops, p.tail, p.span, p.info,
                                                true, false, cycles);
                    });
                }
            }
        }
    }
    genericEvents++;
    const uint32 from = memoryTiming.from;
    const sint8 lastLoad = memoryTiming.lastLoad;
    // (the branch whose slot made the last access is stepped without a path: Sh3Timing::Taken; nothing to keep for it)
    const bool streamed = !(memoryTiming.lastInsn == branch + 2 && memoryTiming.from == branch + 4);
    const uint32 got = memoryTiming.Taken(timingPaths, branch, target, cycles, [this](uint32 p) -> uint16 { return TimingCode(p); });
    if (streamed && !refused && !(slot && slot[1] >= 0)) {
        KeepPlan(*this, t, branch, true, from, lastLoad);
    }
    return got;
}

void SH3::InterpretBlock() {
    planBlock = nullptr; // (no plans for the interpreter: the generic path)
    if (recording && (IsRam(PC) || IsFlash(PC))) [[unlikely]] {
        if (RecordBlock(PC, false) && recordStatic) {
            FollowBlock(PC, recordStatic);
        }
    }
    for (unsigned n = 0; n < kMaxBlockInstructions; n++) {
        const uint32 pc = PC;
        const uint16 op = Fetch(pc);
        branched = false;
        at = pc;
        s_ops[op](*this, op);
        cycles += s_cycles[op];
        if (!branched) {
            PC += 2;
        } else if (timing) [[unlikely]] {
            TimeBranch(pc, PC);
        }
        if (EndsBlock(op) != BlockEnd::No) {
            return;
        }
    }
}

SH3::Translation *SH3::Lookup(uint32 pc) {
    if (!IsRam(pc)) [[unlikely]] {
        if (m_flash.empty()) {
            return nullptr;
        }
        const auto it = m_flash.find(pc);
        if (it == m_flash.end()) {
            return nullptr;
        }
        Translation *t = it->second;
        if (t->epoch == UINT32_MAX) {
            // (looked at once: the flash is the game's own, as the translation was made from it)
            t->epoch = 0;
            t->valid = true;
            for (uint32 i = 0; i < t->block->length; i += 2) {
                const uint16 op = Fetch(pc + i);
                t->valid = t->valid && t->block->bytes[i] == static_cast<uint8>(op >> 8) && t->block->bytes[i + 1] == static_cast<uint8>(op);
            }
        }
        return t->valid ? t : nullptr;
    }
    const uint32 phys = pc & (kRamSize - 1), page = phys >> 12;
    std::vector<Translation *> &slots = m_pages[page];
    if (slots.empty()) {
        return nullptr;
    }
    // (an address can have more than one translation: code loaded over other code, each version recorded)
    for (Translation *t = slots[(phys & 0xFFF) >> 1]; t; t = t->next) {
        if (t->block->address != pc) {
            continue;
        }
        const uint32 last = (phys + t->block->length - 1) >> 12;
        const uint32 lastEpoch = last < kPages ? pageEpoch[last] : 0;
        if (t->epoch != pageEpoch[page] || t->epoch2 != lastEpoch) [[unlikely]] {
            t->epoch = pageEpoch[page];
            t->epoch2 = lastEpoch;
            t->valid = phys + t->block->length <= kRamSize && std::memcmp(ram + phys, t->block->bytes, t->block->length) == 0;
        }
        if (t->valid) [[likely]] {
            return t;
        }
    }
    return nullptr;
}

// The frame's wait with the timing model off: `sub Rm0,Rn0; cmp/hi Rm1,Rn1; bt/s back; mov.l @Rm3,Rn0` reading a word of
// main RAM (PollTurn's first shape), its translation entered at its first instruction. Without the timing model a
// turn costs its four instructions' cycles (s_cycles, as the translation and the interpreter count them) and nothing
// else, and nothing writes the word while the loop turns (the CPU writes nothing in it; an interrupt, or the board's
// event, is a stop the block's test sees after a turn). So once Rn0 holds the word, as each turn leaves it, every turn
// is alike: the block would turn until its stop, and the turns before its last are worked out here (the cycles; Rn0
// and T stay as they are), the block running the last itself. Native mode only, as the translations are
void SH3::WaitAhead(Translation &t) {
    const uint32 head = t.block->address;
    if (t.wait < 0) {
        const uint16 op0 = Fetch(head), op1 = Fetch(head + 2), op2 = Fetch(head + 4), op3 = Fetch(head + 6);
        t.wait = 0;
        if ((op0 & 0xF00F) == 0x3008 && (op1 & 0xF00F) == 0x3006 && op2 == 0x8DFC && (op3 & 0xF00F) == 0x6002 &&
            t.block->length >= 8) {
            t.n0 = (op0 >> 8) & 15;
            t.m0 = (op0 >> 4) & 15;
            t.n1 = (op1 >> 8) & 15;
            t.m1 = (op1 >> 4) & 15;
            t.m3 = (op3 >> 4) & 15;
            const uint8 n3 = (op3 >> 8) & 15;
            if (n3 == t.n0 && t.m3 != t.n0) {
                t.wait = 1;
                t.waitCost = s_cycles[op0] + s_cycles[op1] + s_cycles[op2] + s_cycles[op3];
            }
        }
        if (!t.wait || !t.waitCost) {
            t.wait = 0;
            return;
        }
    }
    if (!pollSkip || cycles >= blockStop || interruptReady) {
        return;
    }
    const uint32 address = R[t.m3];
    if (!IsRam(address) || (address & 3) != 0) {
        return;
    }
    uint32 value;
    std::memcpy(&value, ram + (address & (kRamSize - 4)), 4);
    value = __builtin_bswap32(value);
    if (R[t.n0] != value) {
        return; // (not yet as a turn leaves it)
    }
    const uint32 r0 = value - R[t.m0];
    const uint32 rn = t.n1 == t.n0 ? r0 : R[t.n1], rm = t.m1 == t.n0 ? r0 : R[t.m1];
    if (!(rn > rm)) {
        return; // (the loop ends at its first turn: the block runs it)
    }
    // (the turns the block would run: each begun under the stop; all but the last)
    const uint64 turns = (blockStop - cycles + t.waitCost - 1) / t.waitCost;
    if (turns < 2) {
        return;
    }
    cycles += (turns - 1) * t.waitCost;
    T = 1;
    waitSkips++;
    waitTurnsSkipped += turns - 1;
}

void SH3::RunBlocks(uint64 stop) {
    blockStop = stop;
    if (mode == Mode::Native) {
        while (cycles < blockStop && !interruptReady && !sleeping) {
            if (stopPc != 0xFFFFFFFFu) [[unlikely]] {
                if ((PC & 0x1FFFFFFFu) == (stopPc & 0x1FFFFFFFu)) {
                    stopPc = 0xFFFFFFFFu;
                    if (onStopPc) {
                        onStopPc();
                    }
                }
            }
            if (RunNativeCall()) [[unlikely]] {
                continue;
            }
            if (Translation *t = Lookup(PC)) [[likely]] {
                if (!timing && t->wait != 0) [[unlikely]] {
                    WaitAhead(*t);
                }
                planBlock = plansOn ? t : nullptr;
                planStart = t->block->address;
                planBytes = t->block->length;
                codeWritten = false;
                t->block->run(*this);
                planBlock = nullptr;
                nativeBlocks++;
            } else {
                if (countInterpreted) [[unlikely]] {
                    m_interpretedAt[PC]++;
                }
                InterpretBlock();
                interpretedBlocks++;
            }
        }
        return;
    }
    while (cycles < blockStop && !interruptReady && !sleeping) {
        if (stopPc != 0xFFFFFFFFu && (PC & 0x1FFFFFFFu) == (stopPc & 0x1FFFFFFFu)) [[unlikely]] {
            stopPc = 0xFFFFFFFFu;
            if (onStopPc) {
                onStopPc();
            }
        }
        if (RunNativeCall()) [[unlikely]] {
            continue;
        }
        InterpretBlock();
        interpretedBlocks++;
    }
}

void SH3::RunOneBlock(bool native) {
    if (native && RunNativeCall()) {
        return;
    }
    if (mode == Mode::Native) {
        if (Translation *t = Lookup(PC)) {
            planBlock = plansOn ? t : nullptr;
            planStart = t->block->address;
            planBytes = t->block->length;
            codeWritten = false;
            t->block->run(*this);
            planBlock = nullptr;
            nativeBlocks++;
            return;
        }
    }
    InterpretBlock();
    interpretedBlocks++;
}

void SH3::InstallTranslations() {
    size_t count = 0;
    const NativeBlock *blocks = NativeBlocks(count);
    m_translations.clear(); // (their sites' plans with them)
    m_translations.resize(count);
    plansOn = !getenv("SPELUNK_NO_PLANS");
    plantPlan = getenv("SPELUNK_PLANT_PLAN") ? 1u : 0u;
    m_flash.clear();
    for (size_t i = 0; i < count; i++) {
        Translation &t = m_translations[i];
        t.block = &blocks[i];
        if (!IsRam(blocks[i].address)) {
            m_flash[blocks[i].address] = &t;
            continue;
        }
        const uint32 phys = blocks[i].address & (kRamSize - 1);
        const uint32 first = phys >> 12, last = (phys + blocks[i].length - 1) >> 12;
        for (uint32 p = first; p <= last && p < kPages; p++) {
            codeMark[p] = 1;
        }
        std::vector<Translation *> &slots = m_pages[first];
        if (slots.empty()) {
            slots.assign(2048, nullptr);
        }
        Translation *&head = slots[(phys & 0xFFF) >> 1];
        t.next = head;
        head = &t;
    }
}

void SH3::NoteRamWrite(uint32 a, uint32 length) {
    if (!length) {
        return;
    }
    const uint32 first = (a & (kRamSize - 1)) >> 12, last = ((a & (kRamSize - 1)) + length - 1) >> 12;
    for (uint32 p = first; p <= last && p < kPages; p++) {
        if (codeMark[p]) {
            ++pageEpoch[p];
        }
    }
}

void SH3::SaveRecording(const char *path) const {
    FILE *out = fopen(path, "w");
    if (!out) {
        return;
    }
    for (const Recorded &r : m_recorded) {
        fprintf(out, "%08X ", r.pc);
        for (uint8 b : r.bytes) {
            fprintf(out, "%02X", b);
        }
        fputs(r.found ? " found\n" : "\n", out);
    }
    fclose(out);
}

void SH3::SaveInterpreted(const char *path) {
    FILE *out = fopen(path, "w");
    if (!out) {
        return;
    }
    std::vector<std::pair<uint64, uint32>> order;
    for (const auto &[pc, n] : m_interpretedAt) {
        order.push_back({n, pc});
    }
    std::sort(order.begin(), order.end(), [](const auto &x, const auto &y) { return x.first != y.first ? x.first > y.first : x.second < y.second; });
    for (const auto &[n, pc] : order) {
        // (stale: a translation of another version of the code there; none: never recorded)
        bool translated = false;
        if (IsRam(pc)) {
            const uint32 phys = pc & (kRamSize - 1);
            const std::vector<Translation *> &slots = m_pages[phys >> 12];
            for (Translation *t = slots.empty() ? nullptr : slots[(phys & 0xFFF) >> 1]; t; t = t->next) {
                translated |= t->block->address == pc;
            }
        }
        fprintf(out, "%08X %llu %s\n", pc, static_cast<unsigned long long>(n), translated ? "stale" : "none");
    }
    fclose(out);
}

} // namespace spelunk
