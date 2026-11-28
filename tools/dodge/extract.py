#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
"""DoDonPachi's enemy fire, taken from recordings of play, as the patterns Dodge training fires.

  tools/dodge/extract.py PROGRAM OUT.inc RECORDING...

A recording (tools/dodge/record.sh) has a line for every bullet the game made in a run through both loops: the
frame, the registers at the game's bullet routine (the bullet's kind, speed, angle and place; a5 the object's task,
a6 its body) and the ship's place. From these:

  - an object's fire is split by the place in the program it came from, and those places that always fire together
    are one attack; an attack's fire is split where it pauses (45 frames) into bursts;
  - one burst of the usual size stands for the attack; the pause to the next burst is the usual one;
  - each bullet's angle is found to be fixed (the same in every burst), or the game's aim at the ship (its coarse aim
    of 64 directions or its fine one of 256, from the bullet's place or the object's, at that moment or as the burst
    began) plus a fixed turn, by which holds across the bursts of all the recordings, the ship being somewhere else
    in each. An angle that is none of these (a pattern that turns on from burst to burst, a random one) is fired as
    it was recorded.
"""
import collections
import math
import struct
import sys

GAP = 45        # frames without fire that end a burst
LONGEST = 600   # a burst is cut to this many frames
MOST = 400      # ... and to this many bullets

def s16(v):
    return v - 0x10000 if v & 0x8000 else v

class Aim:
    """The game's two aims (059A6A: 64 directions; 059C8C: 256), from x, y to tx, ty (64ths of a pixel)."""
    def __init__(self, program):
        self.coarse_table = [struct.unpack(">H", program[0x59AE8 + 2 * i:0x59AEA + 2 * i])[0] for i in range(8 * 16)]
        self.fine_base = [struct.unpack(">H", program[0x59CFE + 2 * i:0x59D00 + 2 * i])[0] for i in range(8)]
    def coarse(self, x, y, tx, ty):
        d0, d1, d2, d3 = [(v + 0x1800) & 0xFFFF for v in (x, y, tx, ty)]
        d4 = 0x10
        if d1 < d3:
            d1, d4 = d3 - d1, 0
        else:
            d1 -= d3
        if d0 < d2:
            d0, d4 = d2 - d0, d4 + 8
        else:
            d0 -= d2
        if d1 < d0:
            d4 += 4
            d0, d1 = d1, d0
        if d1 == 0:
            return 0
        ratio = min(15, (16 * d0 // d1 + 1) // 2)
        return (self.coarse_table[d4 * 4 + ratio] * 4) & 0xFF
    def fine(self, x, y, tx, ty):
        d0, d1, d2, d3 = [(v + 0x1800) & 0xFFFF for v in (x, y, tx, ty)]
        d4 = 8
        if d1 < d3:
            d1, d4 = d3 - d1, 0
        else:
            d1 -= d3
        if d0 < d2:
            d0, d4 = d2 - d0, d4 + 4
        else:
            d0 -= d2
        if d1 < d0:
            d4 += 2
            d0, d1 = d1, d0
        if d1 == 0:
            return 0
        q, rem = divmod(d0 << 5, d1)
        half = d1 >> 1
        if half < rem:
            q += 2
        elif (half >> 1) < rem:
            q += 1
        base = self.fine_base[d4 // 2]
        return (base + q if d4 in (0, 6, 10, 12) else base - q) & 0xFF

def load(path, run):
    events = []
    for line in open(path):
        r = line.partition("#")[0].split()
        if len(r) < 43 or r[1] == "002C84" or int(r[0]) < 4500:
            continue
        v = [int(x, 16) for x in r[2:]]
        d, a, stack = v[0:8], v[8:16], v[16:32]
        a5c, a66, ship, stage, count, boss = v[33], v[35], v[36], v[37], v[39], v[40]
        site = next((x for x in stack[3:] if 0x400 <= x < 0x80000 and not x & 1 and
                     not (0x50D20 <= x < 0x51050 or 0x54E00 <= x < 0x55000)), 0)
        pc = int(r[1], 16)
        ex, ey = s16(a66 >> 16), s16(a66 & 0xFFFF)
        x, y = s16(d[2] >> 16), s16(d[2] & 0xFFFF)
        parent = d[3] != 0
        if parent:  # (the bullet is put at its parent's place plus d4, and may stay with it: 050E8C)
            ox, oy = s16(d[4] >> 16), s16(d[4] & 0xFFFF)
            x, y = s16((ex + ox) & 0xFFFF), s16((ey + oy) & 0xFFFF)
        # (the game's two bullet routines: the first takes one of 64 directions, the other, which also sets bit 19 of
        # the bullet's first long, one of 256: `fine`. Angles here are all of 256)
        fine = pc in (0x5101A, 0x54F74)
        if abs(ex - x) > 0x3000 or abs(ey - y) > 0x3000:
            ex, ey = x, y  # (a6 is not the firing object's body there)
        events.append(dict(run=run, f=int(r[0]), late=fine, type=d[0] & 0x3F, speed=s16(d[0] >> 16),
                           ang=d[1] & 0xFF if fine else (d[1] & 0x3F) * 4, x=x, y=y, parent=parent, a5=a[5], site=site, obj=a5c >> 24, ex=ex, ey=ey,
                           sx=s16(ship >> 16), sy=s16(ship & 0xFFFF), stage=stage >> 16, loop=stage & 0xFFFF, boss=boss >> 16))
    return events

def bursts_of(events):
    out, cur = [], []
    for e in events:
        if cur and e["f"] - cur[-1]["f"] > GAP:
            out.append(cur)
            cur = []
        cur.append(e)
    if cur:
        out.append(cur)
    return out

def mode_share(values):
    c = collections.Counter(values).most_common(1)[0]
    return c[0], c[1] / len(values)

def main():
    program = open(sys.argv[1], "rb").read()
    aim = Aim(program)
    events = []
    for run, path in enumerate(sys.argv[3:]):
        events += load(path, run)
    # An object's life: its task's fire with no gap of 600 frames
    lives = collections.defaultdict(list)
    for e in events:
        lives[(e["run"], e["a5"], e["obj"], e["loop"], e["stage"])].append(e)
    instances = collections.defaultdict(list)  # (stage, obj, loop) -> lives
    for key, es in lives.items():
        cur = []
        for e in es:
            if cur and e["f"] - cur[-1]["f"] > 600:
                instances[(key[4], key[2], key[3])].append(cur)
                cur = []
            cur.append(e)
        instances[(key[4], key[2], key[3])].append(cur)

    patterns = []
    stats = collections.Counter()
    # (an object's places fire together the same in both loops: found from both loops' lives, the first loop's
    # being more often seen)
    both = collections.defaultdict(list)
    for (stage, obj, loop), lifes in instances.items():
        both[(stage, obj)] += lifes
    for (stage, obj, loop), own in sorted(instances.items()):
        lifes = both[(stage, obj)]
        # The places that fire together: those whose fire is at the same times, mostly (the frames, by eights, of each
        # place's bursts in every life; two places are one attack when half of the frames either fires in both do)
        active = collections.defaultdict(set)
        for n, life in enumerate(lifes):
            by_site = collections.defaultdict(list)
            for e in life:
                by_site[e["site"]].append(e)
            for s, es in by_site.items():
                for burst in bursts_of(es):
                    for f in range(burst[0]["f"] // 8 - 1, burst[-1]["f"] // 8 + 2):
                        active[s].add((n, f))
        group = {}
        def find(s):
            while group.setdefault(s, s) != s:
                s = group[s]
            return s
        sites = sorted(active)
        for i, s in enumerate(sites):
            for t in sites[i + 1:]:
                if len(active[s] & active[t]) * 2 >= len(active[s] | active[t]):
                    group[find(t)] = find(s)
        groups = collections.defaultdict(set)
        for s in sites:
            groups[find(s)].add(s)
        for head in sorted(groups):
            sites = groups[head]
            all_bursts, gaps = [], []
            for life in own:
                bs = bursts_of([e for e in life if e["site"] in sites])
                all_bursts += bs
                gaps += [b[0]["f"] - a[0]["f"] for a, b in zip(bs, bs[1:])]
            if not all_bursts:
                continue
            cut = []
            for b in all_bursts:
                b = [e for e in b if e["f"] - b[0]["f"] < LONGEST][:MOST]
                cut.append(b)
            size, _ = mode_share([len(b) for b in cut])
            same = [b for b in cut if len(b) == size and [e["site"] for e in b] == [e["site"] for e in next(c for c in cut if len(c) == size)]]
            chosen = same[0]
            length = chosen[-1]["f"] - chosen[0]["f"]
            near = sorted(g for g in gaps if g < length + 400)
            pause = near[len(near) // 2] if near else length + 90
            pause = max(pause, length + 24, 40)
            # Each bullet's angle: fixed, an aim plus a turn, or as recorded
            out = []
            cx = sum(e["x"] for e in chosen) // len(chosen)
            cy = sum(e["y"] for e in chosen) // len(chosen)
            def near(values):
                # (the value most are within 6 of, and how many are)
                centre, _ = mode_share(values)
                for _ in range(2):
                    close = sorted(((v - centre + 128) & 0xFF) - 128 for v in values if abs(((v - centre + 128) & 0xFF) - 128) <= 6)
                    centre = (centre + close[len(close) // 2]) & 0xFF
                return centre, sum(abs(((v - centre + 128) & 0xFF) - 128) <= 6 for v in values) / len(values)
            def aims(i, measure):
                found = ("recorded", 0, 0, 0.0)
                for how in ("each", "first"):
                    for origin in ("bullet", "object"):
                        for kind in ("coarse", "fine"):
                            turns = []
                            for b in same:
                                at = b[i] if how == "each" else b[0]
                                ox, oy = (at["x"], at["y"]) if origin == "bullet" else (at["ex"], at["ey"])
                                turns.append((b[i]["ang"] - getattr(aim, kind)(ox, oy, at["sx"], at["sy"])) & 0xFF)
                            value, share = measure(turns)
                            if share > found[3] + 0.02:
                                found = ((how, origin, kind), 0, value, share)
                return found
            for i, e in enumerate(chosen):
                angles = [b[i]["ang"] for b in same]
                best = ("recorded", 0, e["ang"], 0.0)
                value, share = mode_share(angles)
                if share >= 0.9 or len(same) < 3:
                    best = ("fixed", 0, value, share)
                else:
                    exact = aims(i, mode_share)
                    turn, with_first = mode_share([(b[i]["ang"] - b[0]["ang"]) & 0xFF for b in same])
                    if exact[3] >= 0.6:
                        best = exact
                    elif i > 0 and with_first >= 0.8:
                        best = ("with first", 0, turn, with_first)
                    else:
                        about = aims(i, near)
                        if about[3] >= 0.8:
                            best = about
                stats[best[0] if isinstance(best[0], str) else "aim " + " ".join(best[0])] += 1
                out.append(dict(dt=e["f"] - chosen[0]["f"], late=e["late"], type=e["type"], speed=e["speed"], dx=e["x"] - cx, dy=e["y"] - cy,
                                ex=e["ex"] - cx, ey=e["ey"] - cy, parent=e["parent"], how=best[0], angle=best[2], share=best[3]))
            patterns.append(dict(run=chosen[0]["run"], frame=chosen[0]["f"], task=chosen[0]["a5"], x0=chosen[0]["x"] // 64, y0=chosen[0]["y"] // 64, stage=stage, obj=obj, loop=loop, boss=max(e["boss"] for e in chosen), sites=sorted(sites), bullets=out,
                                 length=length, pause=pause, seen=len(all_bursts), same=len(same), cx=cx, cy=cy))
    # The same fire from another place or another object is one pattern: of an object's own, those alike in when each
    # bullet comes, its kind and its speed (a turret's twin on the other side); of the whole game's, those alike in
    # everything (kept where it is first met, and how often it was seen added up)
    def loose(p):
        return (p["stage"], p["obj"], p["loop"], tuple(sorted((b["dt"], b["type"], b["speed"], b["late"]) for b in p["bullets"])))
    def strict(p):
        first = p["bullets"][0]["angle"]
        return (p["loop"], tuple(sorted((b["dt"], b["type"], b["speed"], b["late"], b["how"] if isinstance(b["how"], str) else "aim",
                                         (b["angle"] - (first if b["how"] in ("fixed", "recorded") else 0)) & 0xFF, b["dx"] // 256, b["dy"] // 256)
                                        for b in p["bullets"])))
    # (a burst seen once or twice with few bullets is a piece of something, cut by the object's end)
    patterns = [p for p in patterns if p["seen"] >= 3 or len(p["bullets"]) >= 24]
    for key in (loose, strict):
        kept = {}
        for p in sorted(patterns, key=lambda p: (p["loop"], p["stage"], -p["seen"])):
            k = key(p)
            if k in kept:
                kept[k]["seen"] += p["seen"]
            else:
                kept[k] = p
        patterns = sorted(kept.values(), key=lambda p: (p["loop"], p["stage"], p["boss"], p["obj"], p["sites"]))
    # An object's small bursts are mostly one thing fired from this place or that (the second loop's extra bullets most
    # of all): the three most seen stand for them; and of its large ones the eight most seen
    kept = []
    by_object = collections.defaultdict(list)
    for p in patterns:
        by_object[(p["loop"], p["stage"], p["obj"])].append(p)
    for group in by_object.values():
        small = sorted((p for p in group if len(p["bullets"]) < 24), key=lambda p: -p["seen"])[:3]
        large = sorted((p for p in group if len(p["bullets"]) >= 24), key=lambda p: -p["seen"])[:8]
        kept += small + large
    patterns = sorted(kept, key=lambda p: (p["loop"], p["stage"], p["boss"], p["obj"], len(p["bullets"]), p["sites"]))
    return patterns, stats

WORDS = 21  # a sprite in a .looks recording: its entry's five words, then its colour's sixteen palette entries
LOOK_FRAMES = (0, -2, 2, -4, 4, -6, 6)  # the frames about a burst's first whose sprites are looked at

def looks_of(patterns, recordings):
    """Each pattern's look: the sprites its firing object drew as its burst began (a boss's: every object's of the
    stage then), from the second kind of recording (RECORDING.looks: record.sh's second step, made for the frames
    this writes to RECORDING.frames). A sprite: across and down from the middle of the burst's fire in pixels, its
    look word, number and size.

    The object is the task that fired; where that task drew nothing (the firing is another task's than the drawing),
    the stage's object with a sprite nearest the burst's first bullet. An object being shot is drawn in other colours
    on some frames, with a flash over it: so seven frames are looked at, a sprite is kept if most of them have it, and
    its look word is the one most have. A look that would reach off the top of the picture moves the whole pattern
    down (`shift`, pixels across the board, 0 or less)."""
    def signed(v):
        return ((v + 0x200) & 0x3FF) - 0x200
    # (a sprite's colours are its look word's high byte: for each sprite number, the one most of all the frames looked at
    # give it. An object being shot all through a recording is in its flash's colours in most frames about one burst,
    # but not in most of all)
    colours = collections.defaultdict(collections.Counter)
    for path in recordings:
        try:
            for line in open(path + ".looks"):
                words = line.partition("|")[2].split()
                for i in range(0, len(words), WORDS):
                    colours[words[i + 1]][int(words[i], 16) >> 8] += 1
        except FileNotFoundError:
            pass
    def usual(look, number):
        return (colours["%04X" % number].most_common(1)[0][0] << 8) | (look & 0xFF)
    # (sprites that many stages' objects draw are not an object's own: explosions, smoke. Left out of a boss's look, which
    # is every object's)
    stage_of = {(p["run"], p["frame"] + d): p["stage"] for p in patterns for d in LOOK_FRAMES}
    stages = collections.defaultdict(set)
    for run, path in enumerate(recordings):
        try:
            for line in open(path + ".looks"):
                head, _, tail = line.partition("|")
                r = head.split()
                if 0x100142 <= int(r[2 + 8 + 5], 16) <= 0x100D82 and (run, int(r[0])) in stage_of:
                    words = tail.split()
                    for i in range(0, len(words), WORDS):
                        stages[int(words[i + 1], 16)].add(stage_of[(run, int(r[0]))])
        except FileNotFoundError:
            pass
    for run, path in enumerate(recordings):
        frames = sorted({p["frame"] + d for p in patterns if p["run"] == run for d in LOOK_FRAMES})
        open(path + ".frames", "w").write("".join("%d\n" % f for f in frames))
        drew = collections.defaultdict(dict)  # frame -> task -> sprites
        try:
            for line in open(path + ".looks"):
                head, _, tail = line.partition("|")
                r = head.split()
                words = [int(w, 16) for w in tail.split()]
                entries = [words[i:i + WORDS] for i in range(0, len(words), WORDS)]
                drew[int(r[0])].setdefault(int(r[2 + 8 + 5], 16), []).extend(e for e in entries if e[4])
        except FileNotFoundError:
            pass
        for p in patterns:
            if p["run"] != run:
                continue
            p["sprites"], p["shift"], p["colours"] = [], 0, []
            now = drew.get(p["frame"], {})
            objects = [t for t in now if 0x100142 <= t <= 0x100D82 and now[t]]  # (the stage's objects' tasks: 5 to 54)
            # (a boss's: every object's; one whose objects draw nothing, the hidden last boss among them, is drawn by task
            # 55, which draws nothing else then)
            boss = p["boss"] or p["stage"] == 6
            tasks = (objects or [0x100DC2]) if boss else [p["task"]] if now.get(p["task"]) else []
            for task in tasks:
                # (the frame whose list for the task is of the usual length: a flash over the object is one sprite more;
                # each sprite's look word the one most of those frames give its place in the list)
                lists = [drew.get(p["frame"] + d, {}).get(task, []) for d in LOOK_FRAMES]
                if not any(lists):
                    continue
                length = collections.Counter(len(l) for l in lists if l).most_common(1)[0][0]
                alike = [l for l in lists if len(l) == length]
                for i, (look, number, x, y, size) in enumerate(e[:5] for e in alike[0]):
                    if boss and len(stages[number]) >= 5:
                        continue
                    p["sprites"].append((signed(x) - p["cx"] // 64, signed(y) - p["cy"] // 64, look, number, size))
            if not tasks:
                # (drawn by another task than fires: the sprites about the burst's first bullet that touch one another,
                # of one of the objects' tasks or of the task that draws the objects on the ground, 55)
                def box(e):
                    return signed(e[2]), signed(e[3]), signed(e[2]) + (e[4] >> 8) * 16, signed(e[3]) + (e[4] & 0xFF) * 16
                def apart(a, b):
                    return max(a[0] - b[2], b[0] - a[2], a[1] - b[3], b[1] - a[3])
                best = []
                for task in objects + [0x100DC2]:
                    entries = now.get(task, [])
                    at = (p["x0"], p["y0"], p["x0"], p["y0"])
                    group = [e for e in entries if apart(box(e), at) <= 12]
                    grew = bool(group)
                    while grew:
                        grew = False
                        for e in entries:
                            if e not in group and any(apart(box(e), box(g)) <= 0 for g in group):
                                group.append(e)
                                grew = True
                    if group and len(group) <= 16 and len(group) > len(best):
                        best = group
                p["sprites"] = [(signed(x) - p["cx"] // 64, signed(y) - p["cy"] // 64, look, number, size) for look, number, x, y, size in (e[:5] for e in best)]
            p["sprites"] = [(dx, dy, usual(look, number), number, size) for dx, dy, look, number, size in p["sprites"]]
            # (each colour the look uses, as the palette had it in the nearest of the frames looked at that drew with it:
            # the game loads its enemies' colours stage by stage, and a boss's as it comes)
            for colour in sorted({look >> 8 & 0x3F for _, _, look, _, _ in p["sprites"]}):
                for d in LOOK_FRAMES:
                    had = [e[5:] for es in drew.get(p["frame"] + d, {}).values() for e in es if e[0] >> 8 & 0x3F == colour and len(e) == WORDS]
                    if had:
                        p["colours"].append((colour,) + tuple(had[0]))
                        break
            if p["sprites"]:
                # (the emitter is 256 pixels up the board's 320; the top 24 are the game's own rows)
                top = max(dx + (size >> 8) * 16 for dx, _, _, _, size in p["sprites"])
                p["shift"] = max(-112, min(0, 296 - 256 - top))

HOW = {"fixed": 0, "recorded": 0, "with first": 1, "each": 2, "first": 3}

def write(patterns, path):
    """The patterns as C++ (runtime/dodge.h's Bullet and Pattern), hardest last by `rank`."""
    for p in patterns:
        # How hard: the bullets a second, more for fast ones and for aimed ones
        seconds = max(p["pause"], 30) / 57.44
        speed = sum(b["speed"] for b in p["bullets"]) / len(p["bullets"])
        aimed = sum(not isinstance(b["how"], str) for b in p["bullets"]) / len(p["bullets"])
        p["score"] = len(p["bullets"]) / seconds * (1 + max(-0.6, speed / 24)) * (1 + 0.5 * aimed)
    for rank, p in enumerate(sorted(patterns, key=lambda p: p["score"])):
        p["rank"] = rank
    out = ["// SPDX-License-Identifier: GPL-3.0-only",
           "// Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.",
           "// Made by tools/dodge/extract.py from recordings of play (tools/dodge/record.sh): not written by hand.",
           "// A bullet: frames after the burst's first, across and down from the middle of the burst's fire, the firing object's",
           "// place likewise, the bullet's kind, speed, angle or turn (of 256), and how (runtime/dodge.h's kHow bits)",
           "static const spelunk::dodge::Bullet kDodgeBullets[] = {"]
    first = 0
    rows = []
    looks, look_rows = {}, []
    for p in patterns:
        p["sprites"] = [(dx + p["shift"], dy, look, number, size) for dx, dy, look, number, size in p.get("sprites", [])]
        key = tuple(p["sprites"])
        if key not in looks:
            looks[key] = len(look_rows)
            look_rows += ["    {%d, %d, 0x%04X, 0x%04X, 0x%04X}," % sprite for sprite in key]
        p["look"] = looks[key]
    palettes, palette_rows = {}, []
    for p in patterns:
        key = tuple(p.get("colours", []))
        if key not in palettes:
            palettes[key] = len(palette_rows)
            palette_rows += ["    {%d, {%s}}," % (row[0], ", ".join("0x%04X" % c for c in row[1:])) for row in key]
        p["palette"] = palettes[key]
    for p in patterns:
        for b in p["bullets"]:
            how = b["how"]
            bits = HOW[how] if isinstance(how, str) else HOW[how[0]] | (8 if how[1] == "object" else 0) | (16 if how[2] == "fine" else 0)
            bits |= (32 if b["late"] else 0) | (64 if b["parent"] else 0)
            out.append("    {%d, %d, %d, %d, %d, %d, %d, %d, %d}," % (b["dt"], b["dx"] + p.get("shift", 0) * 64, b["dy"], b["ex"] + p.get("shift", 0) * 64, b["ey"], b["type"], b["speed"], b["angle"], bits))
        rows.append("    {%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d}," % (
            p["stage"], p["loop"], 1 if p["boss"] else 0, p["obj"], first, len(p["bullets"]), p["length"], p["pause"], min(p["seen"], 65535),
            p["rank"], p["look"], len(p.get("sprites", [])), p["palette"], len(p.get("colours", []))))
        first += len(p["bullets"])
    out += ["};", "",
            "// A pattern: the stage (0 the first; 6 the hidden last boss's), the loop, whether a boss fires it, the object's kind, its",
            "// bullets (the first's place in kDodgeBullets, how many), the burst's length and the frames from one burst's beginning to",
            "// the next's, how many bursts the recordings had, its place among all from the easiest (0) to the hardest, and its",
            "// look (the first's place in kDodgeSprites, how many) and the look's colours (in kDodgeColours)",
            "static const spelunk::dodge::Pattern kDodgePatterns[] = {"] + rows + ["};", "",
            "// A sprite of a pattern's look: across and down from the emitter in pixels, its look word, number and size",
            "static const spelunk::dodge::Sprite kDodgeSprites[] = {"] + look_rows + ["    {0, 0, 0, 0, 0},", "};", "",
            "// A look's colours: which of the sprites' 64 colours, and its sixteen palette entries",
            "static const spelunk::dodge::Colours kDodgeColours[] = {"] + palette_rows + ["    {0, {0}},", "};", ""]
    open(path, "w").write("\n".join(out))

if __name__ == "__main__":
    patterns, stats = main()
    looks_of(patterns, sys.argv[3:])
    write(patterns, sys.argv[2])
    print(sum(1 for p in patterns if p.get("sprites")), "patterns with a look")
    print(len(patterns), "patterns;", sum(len(p["bullets"]) for p in patterns), "bullets;", dict(stats))
    for p in patterns:
        hows = collections.Counter(b["how"] if isinstance(b["how"], str) else "aim" for b in p["bullets"])
        print("stage %d loop %d obj %3d%s  %2d places  %4d bullets  %4d frames  pause %4d  seen %3d (%3d alike)  %s" % (
            p["stage"] + 1, p["loop"] + 1, p["obj"], " boss" if p["boss"] else "     ", len(p["sites"]), len(p["bullets"]), p["length"], p["pause"],
            p["seen"], p["same"], dict(hows)))
