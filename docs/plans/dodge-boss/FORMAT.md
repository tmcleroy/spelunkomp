# Dodge patterns as rules: what the bosses' code does, and a file format for an editor

Made from the write-ups beside this file (stage1..stage6.md, hibachi.md: each boss's code, read and checked against
four recordings of play; bullets.md: the bullets themselves). This is the plan for the pattern editor: a static web
page that edits a pattern file, and Dodge training loading those files (owner's choice, 2026-11-28).

## How DoDonPachi builds a boss's fire (the common parts)

Every boss is made of the same few things; only the numbers and the way they are put together differ.

- **A bullet** is one call of the game's maker: a kind (0..39: table 5104E; 37 and 38 share a record), a speed, an
  angle, a place, and perhaps a parent it rides on. The speed it flies at is `kind's base (22 for most, 18 for kind
  39) + the call's speed`, in eighths of a pixel a frame; the angle is of 64 (maker 50DDC) or of 256 (50F94), 0 up,
  growing clockwise. The game's own sine tables are reproduced exactly in bullets.md, so a preview moves bullets as
  the game does. A bullet hits as a point (the ship's box is about 7 by 4 pixels), whatever its sprite.
- **Most kinds fly straight**; a few have their own motion, part of the kind and not of the pattern: 10, 14, 23 home
  in, 15 curves, 16 slows, spins and drops, 14/19/20/23 speed up, 13 is an orb that bursts into a ring of 32, 24..27
  ride their parent for 8..10 frames.
- **The second loop's extra bullets are not in the patterns**: the makers the bosses call turn each bullet into two
  (speed s and s+6), three (s, s+5, s+10) or a pair 8/256 apart, by flags of the firing object. No boss's own code
  reads the loop (Hibachi's calls the second loop's makers directly). So a pattern is written once; "twins" is a
  property of its bullets.
- **A volley** is the bullets fired in one frame: a base angle, then a shape around it (fans, rings, lines of speeds,
  unrolled lists of turns). The base is one of: fixed; the game's aim at the ship (of 64 or 256, from the boss or the
  muzzle); the aim as the attack began; the last volley's base plus a step (a spiral), bouncing between limits or
  when it has passed the ship's aim by so much (stage 6); a barrel that turns toward the ship a step at a time and
  fires where it points (stages 3, 6); a table stepped on its own timer (a triangle wave: stages 1, 6, Hibachi); the
  boss's own heading (Hibachi). Plus, often, a random turn.
- **The "random" is a table**, 256 bytes walked by one counter that every object shares (057C84, 057DBA, counter
  101273): uniform within the range asked for. For a dodge mode it can be treated as random.
- **An attack** is volleys every N frames, K times, from one or more muzzles together, then a pause. Numbers in it
  change per volley (a speed counting down, a spacing growing, an interval of 2 + n/4), and many depend on the
  game's rank (0..63: stage 4's and stage 6's pauses are 256 - 2 x rank and the like), on a level that grows with time
  and parts destroyed (stage 3: 0..16), or on the boss's life.
- **The phases** are chosen by a table and a rest timer (stages 1, 3 at random from weights; 2, 4, 5, 6, Hibachi in a
  fixed order), and switched by life: below a share of its life a boss cuts the running attack and goes to its last
  (stage 4 at 53%, stage 6 at a third, Hibachi's second form below 0x1C000 of 0x46000). Parts destroyed change some
  (stage 3's level, stage 4's cannons, stage 2's head).
- **Some bosses throw objects that fire for themselves**: stage 5's drones (aimed singles; a rotating cross),
  stage 2's ball (bursts into 16), Hibachi's needles, drones, wall poppers and pods. Each is a small pattern of its
  own on an object with a path.
- **Muzzles are parts of the boss**, up to about 135 pixels from its middle, and many move with it (wings swing,
  pods orbit, turrets on a circle of 16 places).

None of the seven bosses' attacks the write-ups cover (stages 1 to 6 and Hibachi) needs anything outside these; what is still unread is listed in
each write-up (stage 1's second phase was never reached in the recordings; a few dead routines; some paths).

## The file

One JSON file is a set of patterns; Dodge training reads every `*.json` in a `dodge` folder in the game's data
folder; the editor opens, previews and saves them. Player-made patterns are the player's own and can be shared. The
game's own attacks written in this form (from the write-ups) are the game's design, so, like the recorded patterns
now, they are not put in the downloads without the owner's say (see "Open" below).

Numbers may be expressions of `n` (the volley, from 0), `t` (frames since the attack began), `rank` (0..63),
`level` (a pattern's own 0..16, chosen in the menu), `rnd(a, b)` (uniform, a..b), `+ - * / %` and `min max abs`.

```json
{
  "format": "spelunkomp dodge 1",
  "game": "ddonpach",
  "patterns": [{
    "name": "Mouth fan and aimed arms",
    "author": "",
    "look": "boss 1",                     // what the emitter is drawn as: a game boss's look (made on the player's machine), or a shape
    "muzzles": {                          // pixels from the emitter's middle; "path" optional: points it moves through
      "left":  {"at": [-48, 10]},
      "right": {"at": [ 48, 10]},
      "mouth": {"at": [0, 24]}
    },
    "phases": [{                          // in turn, or "pick": weights; "until": frames, or "hits" taken by the emitter
      "name": "arms",
      "attacks": [{                       // all of a phase's attacks run together
        "muzzle": ["left", "right"],
        "start": 0, "every": 5, "times": 40, "then": 30,
        "base": {"aim": "fine", "from": "muzzle"},     // or {"fixed": 128} | {"aim": "coarse", "when": "start"} |
                                                       // {"spin": 7} | {"sweep": 4, "bounce": [-60, 60], "about": "aim"} |
                                                       // {"track": 4, "every": 8} | {"table": [0, 4, 8, 4], "every": 3}
        "turn": "48 - n",                              // added to the base: may grow per volley
        "random": [-7, 7],
        "shape": [{"fan": 2, "spacing": "2 * (48 - n)"}],  // fan / ring / line {"n", "speeds": [a, b]} / list of {"turn", "speed", "at"}
        "bullet": {"kind": 0, "speed": 12, "twins": "speed"}   // speed in eighths of a pixel a frame; twins: none | speed | three | pair
      }]
    }],
    "objects": {                          // thrown helpers: a path and an attack of their own
      "drone": {"life": 120, "path": [{"speed": 16, "angle": "aim"}, {"after": 30, "speed": 4}],
                "attacks": [{"start": 17, "every": 4, "times": 8, "base": {"aim": "coarse", "when": "start"},
                             "turn": "8 * n", "shape": [{"ring": 4}], "bullet": {"kind": 5, "speed": 30}}]}
    }
  }]
}
```

An attack can also throw an object (`"throw": "drone", "every": 8, "times": 16, "from": "left"`) instead of
firing bullets.

The game side: Dodge training runs a pattern's attacks once a frame from the arena it has now, calling the game's
maker with `speed - base(kind)` as the call's speed and the angle of 256 (or of 64 for the kinds that turn by 64ths:
10, 14, 15, 16, 19, 20, 23), so the bullets stay the game's own (moved, drawn, hit and cleared by it). The aims are
the game's, as now. Thrown objects need a small mover of the mode's own (the game's objects cannot be borrowed
without its stage spawner), and a place in the game's memory for their state (the mode's 44 bytes now: to grow).

The editor: one static page (no server), a 240 by 320 board drawn as the game's (portrait), the ship as a box the
mouse moves, bullets as dots by kind; the preview steps the same rules frame by frame (the sine table, kinds'
motions, the game's two aims, from bullets.md) so what is seen is what the game will fire; a frame slider, a rank
and level slider, and the JSON beside the form.

## Open (for the owner)

1. The game's own boss attacks as files: written by hand from the write-ups they are the game's design in numbers.
   Ship them (a "built-in" set), or only make them on the player's machine (a step like the recordings), or not at
   all (players start from a blank file and the recorded patterns stay as they are)?
2. Boss life: phases switched by life (most bosses' last attack) need a life the emitter can lose; shooting the
   emitter (the game's hit test on its object), or by time only?
3. The look of player-made patterns: one of the game's bosses' looks, or simple shapes?
