#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The timing model's plans for the translated code (runtime/plans.h; a prototype, issue #18), from the counters'
# plan dump (a -DTIMING_STATS=ON headless run with SPELUNK_PLAN_DUMP=file: every path streamed in a translated block,
# its key and its plan as the model made it). The BLOCKS translated blocks with the most paths streamed get their
# sites' plans: a site's path known (not the fallback), read in its own block's bytes alone, and short enough for
# PlanConst; at most two keys (the last event's `from` and the register it loaded) a site, the commonest first.
#   tools/plans/make_plans.py DUMP work/<id>/gen/plans/plans.cpp [BLOCKS]   (BLOCKS: 40)
import sys

dump, out = sys.argv[1], sys.argv[2]
want = int(sys.argv[3]) if len(sys.argv) > 3 else 40
FETCHES, OPS = 8, 16
rows = []
for line in open(dump):
    head, fetch, ops = line.rstrip('\n').split('|')
    f = head.split()
    r = dict(block=int(f[0], 16), end=int(f[1], 16), frm=int(f[2], 16), to=int(f[3], 16), last=int(f[4]), count=int(f[5]),
             lo=int(f[6], 16), bytes=int(f[7]), tail=int(f[8]), span=int(f[9]), icount=int(f[10]), known=int(f[11]),
             load=int(f[12]), stall=int(f[13]), slot=int(f[14]), own=int(f[15]), ilast=int(f[16]),
             fetch=[int(x, 16) for x in fetch.split()], ops=[tuple(int(y) for y in x.split(':')) for x in ops.split()])
    rows.append(r)
by_block = {}
for r in rows:
    by_block.setdefault((r['block'], r['end']), []).append(r)
top = sorted(by_block.items(), key=lambda kv: -sum(r['count'] for r in kv[1]))[:want]
lines = ['// The timing model\'s plans for this game\'s translated code, made by tools/plans/make_plans.py from a plan dump:',
         '// generated, not edited (runtime/plans.h)', '#include "plans.h"', '', 'namespace spelunk {', 'namespace {', '']
blocks, plan_id, served, all_count = [], 0, 0, sum(r['count'] for r in rows)
for (block, end), rs in top:
    sites = {}
    for r in rs:
        ok = (r['known'] and r['lo'] != 0xFFFFFFFF and r['bytes'] > 0 and r['lo'] >= block and r['lo'] + r['bytes'] <= end and
              len(r['fetch']) <= FETCHES and len(r['ops']) <= OPS and block <= (r['to'] & ~1) < end)
        if ok:
            sites.setdefault(r['to'], []).append(r)
    slots = {}
    for to, variants in sites.items():
        variants.sort(key=lambda r: -r['count'])
        names = []
        for r in variants[:2]:
            name = 'kP%d' % plan_id
            fetch = ', '.join('0x%08Xu' % x for x in r['fetch']) or '0'
            ops = ', '.join('{%d, %d, %d}' % o for o in r['ops']) or '{0, 0, 0}'
            lines.append('constexpr PlanConst %s = {0x%08Xu, %d, %d, %d, %du, %du, {%d, %d, %d, %d, %d, %d, %d}, {%s}, {%s}};' % (
                name, r['frm'], r['last'], len(r['fetch']), len(r['ops']), r['tail'], r['span'], r['icount'], r['known'],
                r['ilast'], r['load'], r['stall'], r['slot'], r['own'], fetch, ops))
            names.append((name, plan_id))
            plan_id += 1
            served += r['count']
        slots.setdefault(((to & ~1) - block) // 2, {})['t' if to & 1 else 'a'] = names
    n = (end - block) // 2
    for table, suffix in (('kS', ''), ('kD', 'Data')):
        entries = []
        for i in range(n):
            if i not in slots:
                entries.append('{}')
                continue
            def refs(kind, fn):
                names = slots[i].get(kind, [])
                return ', '.join(['&%s<%s, %d>' % (fn, nm, pid) for nm, pid in names] + ['nullptr'] * (2 - len(names)))
            entries.append('{{%s}, {%s}}' % (refs('a', 'PlannedAccess' + suffix), refs('t', 'PlannedTaken' + suffix)))
        lines.append('const SH3::PlanSlot %s%08X[%d] = {%s};' % (table, block, n, ', '.join(entries)))
    lines.append('')
    blocks.append((block, end - block))
lines += ['const PlanBlock kBlocks[] = {%s};' % ', '.join('{0x%08Xu, %du, kS%08X, kD%08X}' % (b, n, b, b) for b, n in blocks), '', '} // namespace', '',
          'const PlanBlock *PlanBlocks(size_t &count) {', '    count = sizeof kBlocks / sizeof kBlocks[0];', '    return kBlocks;', '}', '',
          '} // namespace spelunk', '']
open(out, 'w').write('\n'.join(lines))
print('%d blocks, %d plans; their streams %d of %d in the dump (%.1f%%)' % (len(blocks), plan_id, served, all_count, 100.0 * served / max(all_count, 1)))
