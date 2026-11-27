#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The timing model's plans for the translated code, as data (runtime/plans.h; a prototype, issue #18, Option B), from
# the counters' plan dumps (a -DTIMING_STATS=ON headless run with SPELUNK_PLAN_DUMP=file: every path streamed in a
# translated block, its key and its plan as the model made it; several runs' dumps together). Every translated block a
# dump saw gets its sites' plans: a site's path known (not the fallback), read in its own block's bytes alone; at most
# two keys (the last event's `from` and the register it loaded) a site, the commonest first. Written: the plans (a
# header each, their fetches and steps in two pools), and for each block a byte a halfword naming its site.
#   tools/plans/make_plans.py work/<id>/gen/plans/plans.cpp DUMP...
import sys

out, dumps = sys.argv[1], sys.argv[2:]
rows = {}
for dump in dumps:
    for line in open(dump):
        head, fetch, ops = line.rstrip('\n').split('|')
        f = head.split()
        key = (int(f[0], 16), int(f[2], 16), int(f[3], 16), int(f[4]))
        r = rows.get(key)
        if r:
            r['count'] += int(f[5])
            continue
        rows[key] = dict(block=int(f[0], 16), end=int(f[1], 16), frm=int(f[2], 16), to=int(f[3], 16), last=int(f[4]),
                         count=int(f[5]), lo=int(f[6], 16), bytes=int(f[7]), tail=int(f[8]), span=int(f[9]), icount=int(f[10]),
                         known=int(f[11]), load=int(f[12]), stall=int(f[13]), slot=int(f[14]), own=int(f[15]), ilast=int(f[16]),
                         fetch=[int(x, 16) for x in fetch.split()], ops=[tuple(int(y) for y in x.split(':')) for x in ops.split()])
by_block = {}
for r in rows.values():
    by_block.setdefault((r['block'], r['end']), []).append(r)
plans, fetch_pool, op_pool, blocks = [], [], [], []
served = 0
total = sum(r['count'] for r in rows.values())
for (block, end), rs in sorted(by_block.items()):
    sites = {}
    for r in rs:
        ok = (r['known'] and r['lo'] != 0xFFFFFFFF and r['bytes'] > 0 and r['lo'] >= block and r['lo'] + r['bytes'] <= end and
              block <= (r['to'] & ~1) < end and r['tail'] <= 0xFFFF and r['span'] <= 0xFFFF)
        if ok:
            sites.setdefault((r['to'] & ~1, r['to'] & 1), []).append(r)
    if not sites:
        continue
    by_insn = {}
    for (insn, taken), variants in sites.items():
        variants.sort(key=lambda r: -r['count'])
        ids = []
        for r in variants[:2]:
            ids.append(len(plans))
            plans.append((r, len(fetch_pool), len(op_pool)))
            fetch_pool.extend(r['fetch'])
            op_pool.extend(r['ops'])
            served += r['count']
        by_insn.setdefault(insn, {})['t' if taken else 'a'] = ids
    insns = sorted(by_insn)
    if len(insns) > 255:
        insns = insns[:255] # (a byte a halfword: a block of more than 255 sites keeps its first 255)
    blocks.append((block, end - block, insns, by_insn))

lines = ['// The timing model\'s plans for this game\'s translated code, made by tools/plans/make_plans.py from plan dumps:',
         '// generated, not edited (runtime/plans.h)', '#include "plans.h"', '', 'namespace spelunk {', 'namespace {', '']
lines.append('const uint32 kFetch[] = {%s};' % (', '.join('0x%08Xu' % x for x in fetch_pool) or '0'))
lines.append('const Sh3Timing::Plan::Op kOp[] = {%s};' % (', '.join('{%d, %d, %d}' % o for o in op_pool) or '{0, 0, 0}'))
lines.append('const PlanData kPlans[] = {')
for r, fa, oa in plans:
    lines.append('    {0x%08Xu, %d, %d, %d, %d, %d, {%d, %d, %d, %d, %d, %d, %d}, %du, %du},' % (
        r['frm'], r['last'], len(r['fetch']), len(r['ops']), r['tail'], r['span'], r['icount'], r['known'], r['ilast'],
        r['load'], r['stall'], r['slot'], r['own'], fa, oa))
lines.append('};')
for block, nbytes, insns, by_insn in blocks:
    n = nbytes // 2
    index = [0] * n
    sites = []
    for insn in insns:
        index[(insn - block) // 2] = len(sites) + 1
        a = by_insn[insn].get('a', []) + [-1, -1]
        t = by_insn[insn].get('t', []) + [-1, -1]
        sites.append('{{%d, %d}, {%d, %d}}' % (a[0], a[1], t[0], t[1]))
    lines.append('const uint8 kI%08X[%d] = {%s};' % (block, n, ', '.join(str(x) for x in index)))
    lines.append('const SH3::PlanSite kS%08X[%d] = {%s};' % (block, len(sites), ', '.join(sites)))
lines.append('const PlanBlock kBlocks[] = {%s};' % ', '.join('{0x%08Xu, %du, kI%08X, kS%08X}' % (b, n, b, b) for b, n, _, _ in blocks))
lines += ['', '} // namespace', '', 'const PlanTables &Plans() {',
          '    static const PlanTables tables{kPlans, kFetch, kOp, kBlocks, sizeof kPlans / sizeof kPlans[0], sizeof kBlocks / sizeof kBlocks[0]};',
          '    return tables;', '}', '', '} // namespace spelunk', '']
open(out, 'w').write('\n'.join(lines))
data = len(plans) * 24 + len(fetch_pool) * 4 + len(op_pool) * 4 + sum(nb // 2 + 16 * len(ins) for _, nb, ins, _ in blocks) + 16 * len(blocks)
print('%d blocks, %d sites, %d plans (%d fetches, %d steps); the dumps\' streams served by them %d of %d (%.1f%%); about %d KB of data, %d bytes a plan' % (
    len(blocks), sum(len(ins) for _, _, ins, _ in blocks), len(plans), len(fetch_pool), len(op_pool), served, total,
    100.0 * served / max(total, 1), data // 1024, data // max(len(plans), 1)))
