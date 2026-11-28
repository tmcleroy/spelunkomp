#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# Boss fire as volleys: the bullets one task fires in one frame. Per volley: its frame, the boss's attack byte
# (task +0x26), count, kinds, speeds, the angle of each relative to the volley's first, and the first's angle
# relative to the game's aim at the ship (coarse and fine, from the volley's place).
#   volleys.py PROGRAM RECORDING [loop-stage]
import collections, sys
sys.path.insert(0, __import__("os").path.dirname(__file__))
from extract import Aim, s16

aim = Aim(open(sys.argv[1], "rb").read())
want = sys.argv[3] if len(sys.argv) > 3 else None
vol = collections.OrderedDict()
for line in open(sys.argv[2]):
    head, _, obj = line.partition("#")
    r = head.split()
    if len(r) < 43 or r[1] == "002C84" or int(r[0]) < 4500:
        continue
    v = [int(x, 16) for x in r[2:]]
    stage, loop, boss = v[37] >> 16, v[37] & 0xFFFF, v[40] >> 16
    if not (boss or stage == 6) or (want and want != "%d-%d" % (loop, stage)):
        continue
    o = [int(x, 16) for x in obj.split()]
    pc = int(r[1], 16)
    fine = pc in (0x5101A, 0x54F74)
    d = v[0:8]
    ang = d[1] & 0xFF if fine else (d[1] & 0x3F) * 4
    x, y = s16(d[2] >> 16), s16(d[2] & 0xFFFF)
    if d[3]:
        a6 = v[35]
        x, y = s16((s16(a6 >> 16) + s16(d[4] >> 16)) & 0xFFFF), s16((s16(a6 & 0xFFFF) + s16(d[4] & 0xFFFF)) & 0xFFFF)
    ship = v[36]
    task = o[:16]
    attack = (task[0x26 // 4] >> (8 * (3 - 0x26 % 4))) & 0xFF if task else -1
    key = (int(r[0]), v[13], loop, stage)
    vol.setdefault(key, []).append(dict(type=d[0] & 0x3F, speed=s16(d[0] >> 16), ang=ang, fine=fine, x=x, y=y,
                                        sx=s16(ship >> 16), sy=s16(ship & 0xFFFF), kind=v[33] >> 24, attack=attack))
last = {}
for (f, a5, loop, stage), bs in vol.items():
    b0 = bs[0]
    rel = [((b["ang"] - b0["ang"] + 128) & 0xFF) - 128 for b in bs]
    ac = ((b0["ang"] - aim.coarse(b0["x"], b0["y"], b0["sx"], b0["sy"]) + 128) & 0xFF) - 128
    af = ((b0["ang"] - aim.fine(b0["x"], b0["y"], b0["sx"], b0["sy"]) + 128) & 0xFF) - 128
    gap = f - last.get(a5, f)
    last[a5] = f
    kinds = sorted({(b["type"], b["speed"]) for b in bs})
    print("%d-%d %6d +%4d task %06X kind %3d atk %3d  n %2d  %s first %3d aimC %+4d aimF %+4d  ks %s  rel %s" % (
        loop, stage, f, gap, a5, b0["kind"], b0["attack"], len(bs), "F" if b0["fine"] else "c", b0["ang"], ac, af,
        kinds, rel if len(rel) <= 12 else rel[:12] + ["..."]))
