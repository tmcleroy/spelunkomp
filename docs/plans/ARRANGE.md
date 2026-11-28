# Plan: arrange modes

An arrange mode is the arcade game played to other rules: the same stages, art and program, with the lives, bombs,
rank, loops, scoring or bullets changed. Cave made them for its home ports (Mushihimesama's and Futari's on the PS2 and
the Xbox 360, Espgaluda II's on the 360, DaiOuJou Re:Incarnation's three, one for each Element Doll); trap15 made two as
patches to the arcade programs (DoDonPachi Arrange, Ketsui Arrange); M2's Battle Garegga Rev.2016 has a Custom mode
where the player sets the rules, rank among them. None of Cave's home arranges are in an arcade program, so the
collection can't run them: it can run trap15's, and arranges of its own, and ones its players make.

The goal: an **arrange** is a small file of rules (and later, bullet patterns) that any game in the collection can be
played under. A few come with the collection; players make, share and post scores in their own.

## Rules that hold throughout

- **Arcade first.** With no arrange chosen nothing here touches the game: the state hash of a run with the feature
  built in and none chosen is the same as without it (a check, as tools/check_tools.sh does for training). An arrange
  is chosen, never on by default, and its name is shown on the picture while it is in play.
- **Only what the game's facts allow.** A rule is offered for a game only where its names header says where the thing
  it changes is kept, as Training mode's features are (runtime/shmup.h). An arrange whose rules a game can't honour is
  not offered for it, and the editor says which rule is missing.
- **Deterministic.** Rules act at the places the game's own code is hooked, or between frames on words kept in the
  board's featureWords, so run-ahead, rewind, save states and online rollback all keep working. An arrange is part of
  what both sides of an online match must have the same (its hash in the match's handshake) and part of a replay's
  header.
- **Ours, not copies.** Cave's home arranges can be the idea behind a rule (a shot always in hyper, no bombs, the second
  loop's patterns), but their patterns and layouts are not recreated, and our arranges are not named after theirs.
  trap15's patches are never shipped: the player supplies the patch, as they do the ROM set.

## The arrange file

The same `key = value` form as game.cfg. Shipped arranges in `games/<id>/arranges/*.arrange`; the player's in
`~/Library/Application Support/Spelunkomp/arranges/<id>/` (and the matching folder on Linux, Windows, Android). Its
id is the SHA-256 of its rules, written out in a fixed order, so the same rules are the same arrange whatever its name.

```
name = Two loops, no bombs
about = The second loop's fire from the first stage. No bombs; a ship for every boss.
game = ddonpach
loop = 2              # begin in this loop (where the game has a loop word)
bombs = 0             # the stock held at this (0: none); or `bombs = keep` for as the game
extend_on_boss = 1    # ships given at each boss beaten
continues = 0
rank = max            # as | min | max | locked:<n> | manual (where the game has a rank word)
bullet_speed = +4     # added to each bullet's speed as it is made (where the game's bullet maker is known)
auto_bomb = when_firing   # a hit while the shot is held spends a bomb instead (Garegga's)
stages = 1 3 2 4 5    # the order (where the game can be sent to a stage: Begin at the stage's way)
```

The first rules, by what they need:

| Rule | Needs (from the names header) | Have it now |
| --- | --- | --- |
| bombs, ships, continues | ships and bombs bytes; the continue's place | ships and bombs: all games |
| stages, start stage | the stage start | all games |
| loop | loopWord | 68000 games with one; CV1000/PGM to find |
| extend_on_boss | a stage's boss beaten (the trip to the boss's end) | to find |
| rank | the rank word the enemies' fire reads | Guwange (its gadget), others to find |
| bullet_speed | the bullet maker and its speed argument | DoDonPachi (docs/plans/dodge-boss/bullets.md) |
| auto_bomb | the hit test (Hit boxes) and the bomb routine | the hit test: all games; bomb to find |
| hyper/gauge rules | each game's gauge words | per game (the gadgets' words) |

## Steps

1. **The engine and DoDonPachi.** `runtime/arrange.h/.cpp`: read and hash an arrange, check it against the game's
   facts, apply its rules from the hooks above. The menu: Alternative modes > Arrange (choose one, begin, the rules it
   has shown under it). Find DoDonPachi's rank, loop and boss-beaten places. Ship three of ours:
   - *Second loop first*: the second loop from stage 1, with its extends.
   - *No bombs, a ship a boss*.
   - *Auto-bomb* (as Garegga's: only while the shot is held).

   Checks: none chosen = the state hash unchanged; each shipped arrange played headless through stage 1 with the same
   state hash every run, and through run-ahead 2 and a rewind; an online match with an arrange keeps in step.
2. **The other games.** The same rules found game by game (a table in docs/TRAINING.md's way of who has what). Two
   arranges of ours for each game where the rules make a different game, not just a harder one.
3. **trap15's patches.** A game.cfg that is a patch of another set (`patch_of = ddonpach`, the patch's SHA-256, the
   CRCs it gives). The launcher finds the player's patch file beside the ROM set and shows *DoDonPachi Arrange* and
   *Ketsui Arrange* as games of their own. Find first: which revision of each set each patch is made for (the
   collection's `ket` may not be it); whether the patched code can be translated as the game is built from the
   player's set, or must run in the interpreter (measured: it must hold full speed on the Deck); trap15's leave to
   name them in the launcher.
4. **The editor.** A web page (as the Dodge pattern editor will be) that makes an arrange: the rules each game has,
   checked as you set them, and the file to save into the arranges folder. Then **bullet patterns** in arranges:
   per stage, an enemy's or a boss phase's fire replaced or added to, fired through the game's own bullet makers so
   it moves, hits and is cleared by bombs as the game's own bullets are. This waits on the dodge-boss work's
   rules for boss attacks (docs/plans/dodge-boss/), which is the pattern vocabulary; BulletML (Kenta Cho's) can be
   read into it later. New enemies where a stage has none are not in this plan: they need each stage's spawn tables.
5. **Sharing and scores.** On the project's own worker (spelunkomp-online): `POST /v1/arrange` (the file; at most
   8 KiB, a name and description of 60 and 280 characters), `GET /v1/arrange?game=ID` (the list, by plays), `GET
   /v1/arrange/HASH`; the launcher's page to browse and fetch them. Scores per arrange hash with the replay
   attached; checked later by playing the replay headless (on the fleet's worker), shown as unchecked until then.
   The online dashboard gets the arranges' counts.

## Open questions for the owner

- The menu's place: under Alternative modes (as Dodge training), or a page of its own in the launcher too?
- Shipped arranges: which games first after DoDonPachi?
- trap15's patches: worth asking for leave to name them, or keep them as "a patch you supply" only?
- Sharing: in the first release of the feature, or after it has been played a while?

## Sources

- Mushihimesama's and Futari's arranges: shmups.wiki (Mushihimesama, Mushihimesama Futari), Wikipedia.
- DaiOuJou Re:Incarnation's Arrange S, L and EX: Gematsu (2023-11), TV Tropes (DonPachi).
- Battle Garegga Rev.2016's Premium, Blast Branch and Custom modes: m2stg.com, Famitsu (2018-11).
- trap15's DoDonPachi Arrange and Ketsui Arrange: shmups.system11.org threads (p=783638, p=811760).
- BulletML: Kenta Cho (ABA Games); its guide in BulletMLLib.
