# Training

What the collection gives for learning these games, what each part rests on and how it is checked. The list of what
is still to come is in ROADMAP.md.

Everything here is a feature (the launcher's Features page, the in-game menu's Training mode page under Alternative modes), off unless chosen: with
all of them off the game plays exactly as the board. None of them adds a frame of lag: the practice point and rewind
do their keeping after a frame is on show.

## For every game (no facts about the game needed)

| Feature | Keys (gamepad) | What it does |
| --- | --- | --- |
| Practice point | `\` sets (right shoulder), Backspace goes back (left shoulder), `]` replays (right stick click) | The game as it is that instant kept in memory, and put back at once, as often as wanted. Replay shows the last attempt again from the point, with the controls as they were pressed; they are the player's again where it ends |
| Rewind | hold R (left trigger) | The game runs backwards, three times as fast as play, through its last 30 seconds or so; let go and play on |
| Fast forward | hold Tab (right trigger) | Up to six times as fast, without sound |
| Input display | | The stick and buttons as the game reads them, in the corner |
| Speed readout | | The game's own speed over the last second, in the corner: 100% or less where it slows down (docs/SLOWDOWN.md) |
| Slow motion | - and = | The Speed row, at the top of the Training mode page as on the Game page: 3/4 down to 1/8 (or faster, up to 8x). The sound is played slower by as much, as cps2recomp plays it |
| Frame advance | P stops, O a frame on (stopped: Tab, the right trigger) | The page's Frame advance row (or P) stops the game and shows the frame's number; then each press is one frame on, held frame after frame. P, or the row again, plays on |

The buttons can be changed on the launcher's Controllers page. Slow motion and frame advance are always there, not
features: they change only when the frames are shown, never what the game computes (the same frames and state hash as
at full speed: checked with `--real-time --speed` and `--step-test`, below). Run-ahead's picture ahead is for play at
full speed: slowed, faster or stopped, the frame shown is the game's own, as in cps2recomp. In an online match neither
is offered (the match is at the board's speed for both players).

### How a moment is kept (runtime/timeline.h)

A whole state of the board is 75 MB or more, most of it VRAM (8192 x 4096 pixels). Kept that way, a practice point
would stall the game and rewind could not exist. So:

- **Main RAM by its 4 KB pages.** A moment of the timeline holds only the pages that differ from the moment before,
  as they were before (an undo log), so stepping back is a few page copies.
- **VRAM by 64 x 64 blocks, and only those written.** The lists the game ran say exactly which pixels it wrote
  (machine/blitter.h's NativeList: every draw's rectangle after clipping, every upload). Only those blocks are brought
  from the GPU when a moment is kept, in one transfer, and only those are sent back when one is gone back to.
- **The practice point** holds all of RAM and all of VRAM once (brought when it is set: about 11 ms), and from then
  notes the blocks written; going back sends those blocks only (about 3 ms).
- **The timeline** keeps a moment every 6 frames (a tenth of a second), about 1 MB each in play, up to 384 MB in
  all: about 30 seconds. Keeping one takes about 1 ms and is done after the frame is shown. The 384 MB is what it takes
  of the machine's memory: its own copy of RAM and VRAM, the GPU's buffer a moment's VRAM is read back through, and its
  moments, kept one after another in one store used round and round (made at its start, the system giving it memory as
  it fills), not each in an allocation of its own, whose freed places the allocator could not always use again.

The sound chip's state is part of a moment, so the music is where it was.

### The check (tools/check_tools.sh)

For each game, with the GPU renderer: the tools on and never used leave the game's state and all of VRAM as a run
without them; a practice point set and gone back to 300 frames later, then played on, ends in the same state, the
same VRAM and the same picture as a run that simply played that many frames; the same for rewind held 300 frames
(5 s), which must also have gone back at least 888 frames of play; and for both together with run-ahead on. All five
games pass.

### Slow motion and frame advance, checked

`--step-test FRAME:N` stops the game at FRAME and goes on N frames one at a time, as the frame advance button does;
`--real-time --speed N` runs a check paced as play is, at the Speed row's place N (4: 1/2), with its sound. DoDonPachi,
3,400 frames into stage 1 with the stick and shot held: stopped at 3,050 and 40 frames on one at a time, the state hash
and every frame's picture as the run without (run-ahead off), and the same state with run-ahead at 2; 700 frames at
half speed with 20 of them stepped, the same state as at full speed.

## For each game (from its facts: runtime/shmup.h)

A game's names header (games/<id>/src/<id>.h) says where its stage, ships, bombs and the ship's safe time are, and
runtime/shmup.h makes the features from them:

| Feature | What it does |
| --- | --- |
| Start at stage | A new game begins at the stage chosen, through the game's own start of it |
| Begin at the stage | (a command at the top of the Training mode page) A new game now, at the stage chosen, from wherever the game is: the board starts again and is run unseen through its start-up, a coin, Start and the shot pressed over and over (the attract show, the press-start screen, any choice of ship or mode: its first), until the stage chosen begins (`Game::beginNow`, runtime/shmup.cpp's BeginDrive; the stage start's own hooks say when, `shmup::StageBegun`). Where the stage is set before the ship is chosen (DOJ, Black Label, Ketsui: `Facts::flownWord`) or a map of stages follows (Deathsmiles), the shot is pressed on until the game's word says the ship is flown. It gives up after 20,000 frames. Not in an online match. Tried with stage 3 in all 23 games, and with stage 5 in DoDonPachi II (the stage begins 1,784 frames of the board after the command) |
| Straight to the boss | A new game is run on unseen (10 to 30 seconds) to the boss of the stage it begins at, the ship firing and out of harm; the practice point is set there |
| Out of harm | Nothing hits the ship (the game's own safe time after a ship appears, held) |
| Bombs never run out | The bombs are filled again as they are used |
| Ships never run out | A ship lost is not counted |

| Game | Has them |
| --- | --- |
| Mushihimesama, Matsuri Ver 1.5 | all five |
| Mushihimesama Futari 1.0, 1.5, Black Label and its Another Ver | all five (a game begun at a later stage shows the stage tally first: the game's own way in) |
| Espgaluda II | all five (the guard barrier's gauge kept full stands for the bombs). Also the gems kept full (32 bits at 0x0C37D76C, 500 the most, as the game's own routine that adds gems, 0x0C068BD0, holds them: Kakusei for as long as wanted) |
| DoDonPachi Dai-Fukkatsu 1.0, 1.5, Black Label | all five (one stage-begin routine found by its code's shape; tried at stage 2 and its boss) |
| DoDonPachi SaiDaiOuJou | all five (a later stage reached through stage 1's launch and the game's own way on to the next stage, 0x0C1F27F0 called at the collision pass's entry, with stage 1's tally; tried at stage 3 and the bosses of stages 1 and 2) |
| Ibara, Ibara Kuro Black Label | all five (a stage begun through the game's own script, its pointer sent past the stages before it; out of harm holds the safe count at 59: above 60 it also kept a lost ship from coming back) |
| Pink Sweets | start at stage (seven stages), straight to the boss, out of harm, ships that never run out, and the Rose Cracker always charged in place of bombs, which the game has none of (its gauge, the player's record +0x286, held full at 128: every shot tap releases one) |
| Mushihimesama | COUNTER (Maniac and Ultra: the counter's value, a graph, its bar cut off at 500), SCORE ITEMS (the 100, 200 and 500 items taken), NEXT EXTEND; score by stage. No rank and no power level were found |
| Muchi Muchi Pork! | all five. The bombs are a byte at 0x0C510D3C, kept at 2; the bomb is button 3 (button 2 is the lard attack). Also the lard meter kept full (a word at 0x0C510D36, 104 full) |
| Deathsmiles, Deathsmiles MegaBlack Label | all five. A stage is a place on the map, begun by the new-game routine's count of stages played (0x0C06F040 / 0x0C1EFE60, r5); "Off" is the map. The life is a word, 130 a ship (kept at two ships' worth); out of harm holds the safe count in the player's record (+0xAC / +0xD8) at 64. The castles' boss tunes are not known, so the trip to the boss times out there |
| Akai Katana | all five (player 1's record at 0x0C7E8628: ships 32 bits, 1,600 a ship). A later stage (2-6) is begun through the game's own way on to a stage, 0x0C1F48F0, given the stage chosen where a new game calls it from 0x0C275DE2; the game plays a stage's end first. Stage 5's boss tune is not known, so that trip times out. Also the energy kept full (record +0x14, 32 bits), which Phantom form spends (button 4, held): it shares a bar of 8,000 with the life, and is held at what the life's ceiling leaves of it, as the game's own routine that adds energy (0x0C1F9470) holds it: 3,200 with three ships, more as ships are lost |
| DonPachi (Japan 1.01, US 1.12) | all five (stages 2 to 5 through the carrier's launch and the game's own stage end, with stage 1's clear bonus; the bombs are not filled while the stage's tally counts them into points) |
| DoDonPachi | all five (stages 2 to 6 of the first loop; a later stage is reached through stage 1's launch and the game's own stage end, about 15 seconds, with stage 1's clear bonus; tried at stages 3, 4 and 6 and the bosses of stages 1 and 3) |
| Dangun Feveron | all five (stages 2 to 5, sent on as the ship select ends) |
| ESP Ra.De. | start at stage (2 to 6: the stage order's pointer moved on), straight to the boss, out of harm, characters that never run out, and the guard barrier never runs out in place of bombs (button 3; its gauge, a word at +0x46 of the character's record, 0x102750, kept full at 0xFA0) |
| Guwange | all five. A later stage (2-7) is begun through the game's own stage end (070E78, as a boss dying calls it), after stage 1's tally; the boss is the life bar's word 0x20249C. Lives are a ceiling (0x2026DE) and the life under it (0x2026DA), 0xC00 a ship; both are raised together (`Facts::shipsAlso`) |
| DoDonPachi Dai-Ou-Jou, Black Label | all five (stages 2 to 5 begun from the new game task's own stage) |
| Ketsui | all five, at all five stages. Stage 4 is one the game only ever begins from stage 3's end (begun from a new game its scroll stands still and the picture stays black), so a game begun there is sent on into it as stage 3's end sends it (`Facts::sendOnStages` / `sendOn`): once the scroll task has had its first run, its speed, standing place and timer are set as stage 3's end leaves them, and the game's own timer begins the stage (0CCE86: 0C98CA, then 0CADE6), the scroll running in as from stage 3 while the ship is brought in. Tried: the stage's scenery and enemies against the same stage reached by play, and played on through its boss to stage 5; the trip to each stage's boss (all five arrive: 6,907, 8,104, 10,400, 12,373 and 23,671 frames of the board on the way); Begin at the stage at stage 4 |
| Espgaluda | all five (the guard barrier's gauge kept full stands for the bombs; a later stage is reached through the game's own stage end, with its tally). Also the gems kept full (a word at 80EF04, 500 the most: Kakusei for as long as wanted) |
| Mazinger Z | all five, the stage start at all eight stages and the second loop's eight (the new game's fade, 004624, entered with the stage and the loop chosen, as the program's own stage select, which this set never shows, leaves them). The game has no ships: its three bars of energy stand for them (the low four bits of a byte, kept at two: `Facts::shipsMask`, "Energy never runs out"). The bomb is button 3 (button 2 is the robot's special attack, which costs nothing). No word says a boss is on: the trip to the boss ends when the stage's script has passed the entry that starts the boss's tune, or in stage 3, which has none, the entry its boss comes in after (`Facts::bossOn`) |
| Air Gallet | all five (stages 2 to 6: the stage put in the game's stage word as a new game clears it, 003DC2, as the attract show's demonstration begins at its stages; a later stage begins with the ship flying, without stage 1's launch from the carrier. The boss is where the stage asks for the bosses' tune: the game has no word for it, so its own table of places is read, 00B110) |
| DoDonPachi II: Bee Storm | all five, at each of the seven stages (the game keeps its place in the mode's order of stages in a word, set as a game begins: 14BC06). Its own Practice mode has only the first four, so a later stage chosen there is its fourth; the features work in Practice, Combat and Internet Rank alike. The trip to the boss ends at the boss's warning (a hook at 16AD8C: no word of memory says a boss is on), 6 seconds in at stage 1 and up to 4 minutes of the game's time at stage 5. A ship of the Energy kind has its gauge kept full in place of bombs. Out of harm is the ship's own shield count (the shield is drawn) |

Futari Ver 1.0 and Black Label (2007), and Dai-Fukkatsu Ver 1.0, are other builds of the games beside them: each place
was re-found in their programs from the sibling's, its routines by their instruction bytes and its places in RAM through
the literal pools of the same code (each names header says where it came from), and checked in headless runs (a game at
stage 3, the trip to stage 1's boss, the bomb held with out of harm and both endless features on). Mushihimesama
Matsuri Ver 1.5 is Mushihimesama's program reworked, most of it moved: its places were found by watching the player's
record and breaking on the routines the same features use in Mushihimesama, and checked the same way.

The three games that have them share an engine: the two routines that begin a game were found in Mushihimesama by
watching its stage number being written, and in the other two by the shape of their code. The checks so far are by
eye (a game begun at each stage, shot after 5 and 20 seconds; a game with all four on played with the bomb button
held for 40 seconds, which ends in game over without them). Nobody has played a later stage through this way yet,
and a stage begun this way has not had its graphics compared with the same stage reached by play.

On Cave's 68000 board the same features come from the same facts by the board's own means: the new-game routine is
hooked by the translated code (`AddHook`, the places named in the game's names header) rather than the SH-3's
native calls, a stage is begun by the game's own way to the next stage (a stage number put straight in place gives
the stage's map without what its first stage brings: in DoDonPachi the ship and the bombs' row), the boss is a word
of memory (DoDonPachi's is a fixed word, not an object's), and what must survive run-ahead and rewind is kept in the
board's `featureWords`.

A boss is on when the sound chip is told to play its tune (the sequences are listed in each game's names header):
a word of memory that marks the boss was tried first and is not in the same place in every game played. The
attract show's demonstration is told from a real game by where the new-game routine is called from, and is left
alone. A replay is checked exact (the state and VRAM after it are those of the attempt). The trip to the boss was
tried at Mushihimesama's stages 1, 3 and 5, Espgaluda II's stage 4 and Futari's stages 1 and 2; the true last
bosses' tunes are not checked.

## Dodge training

An alternative mode, in DoDonPachi so far: the ship alone against an emitter that fires the game's own patterns
(docs/DODGE.md).

## Hit boxes

The Hit boxes page (under Training mode) draws what can hit what over the picture, read from the game's memory as its own
hit test reads it (`Game::hitBoxes`, from a game's facts). "Show hit boxes" turns them on; each kind of box the game
gives has a row of its own with a colour (nine to choose from) or Hidden (runtime/hit_boxes.h; a kind's first choice
is its own colour, so a settings file that says nothing draws them as before):

| Kind | Colour unless chosen | What |
| --- | --- | --- |
| Player | yellow | the ship's true box |
| Enemy bullets | red | each bullet's point (a bullet hits when its point is inside the ship's box) |
| Enemies | pink | the bodies of enemies that hit the ship on touching it |
| Player shots | cyan | the shots, as the game tests them against the enemies |
| Ground enemies | green | enemies the shots can hurt whose bodies never hit the ship (tanks, turrets) |

A game offers the rows of the kinds it gives (`Facts::hitKinds`). The boxes are taken from the frame shown (with run-ahead, the frame ahead), turned with the picture and drawn on
the tools' layer over it, so the GPU and software paths both show them; the game is not touched.

| Game | Has them |
| --- | --- |
| DoDonPachi | the ship (its box of about 7 x 4 pixels), the bullets, the enemies' bodies: from the game's test at 0057EE |
| DonPachi (Japan 1.01, US 1.12) | the ship (about 13 x 10), the bullets, the bodies: from the test at 0055DA |
| Dangun Feveron | the ship, the bullets, the bodies (not while a bomb is on, when the game does not test them): from 009FB6 / 009F36 |
| ESP Ra.De., Guwange | the character, the bullets, the bodies: from 005E72 / 007D4C |
| Espgaluda II | the ship (5 x 5 pixels), the bullets, the enemies whose bodies hit: from the game's collision kinds (0x0C0631E0 tests each kind's members; the ship is kind 0 of 0x0C37D2A0, the bullets kind 6 and the bodies kind 5 of 0x0C37D300), read after the frame |
| Mushihimesama | the ship (about 5 x 5 pixels), the bullets, the enemies whose bodies hit: from the game's test at 0x0C05B110, read as it is entered (the game makes these boxes anew each frame and clears them by its end). While the ship is out of harm the game makes no box for it; it is drawn where it would be |
| Mushihimesama Matsuri Ver 1.5 | the same, from its test at 0x0C05B3E0: its lists are Mushihimesama's with entries of 8 bytes (the record at +4) |
| Mushihimesama Futari 1.5, Black Label Another Ver, 1.0, Black Label | the ship (5 x 5), the bullets, the bodies that hit: Espgaluda II's engine of collision kinds (tables 0x0C38A2C4 / 0x0C383548 / 0x0C387BD8 / 0x0C383120), read after the frame |
| DoDonPachi Dai-Fukkatsu 1.5, Black Label, SaiDaiOuJou, Dai-Fukkatsu 1.0 | the ship (5 x 5; 7 x 7 in SaiDaiOuJou), the bullets, the bodies: the same kinds in a later build (0x0C5C0568 / 0x0C713AAC / 0x0C5F5E0C / 0x0C5BF230). Out of harm, the ship's box is drawn at its hittable size from the game's own table |
| Ibara, Ibara Kuro Black Label | the ship's box (its graze box left out), the bullets, the bodies: from the tests at 0x0C05F460 / 0x0C05F1E0 (0x0C06EF50 / 0x0C06ECE0), placed by the field's camera |
| Deathsmiles | the character (about 4 x 4), the bullets, the bodies that hit: collision kinds at 0x0C3869C0 |
| Deathsmiles MegaBlack Label | the character, the bullets (0x0C65E548) and the bodies that hit: Deathsmiles' body test (0x0C1E6D00, from the ship's pass 0x0C1E61E4) |
| Pink Sweets, Muchi Muchi Pork! | the ship (its graze box left out), the bullets, the bodies: from the collision pass's tests (0x0C076AE0 / 0x0C076E50; 0x0C074250 / 0x0C0745C0), read as the first is entered; the owners' flags turn the boxes, placed by the field's camera |
| Akai Katana | the ship, the bullets, the bodies: SaiDaiOuJou's engine of collision kinds (table 0x0C7E45F8), not turned for a horizontal game but flipped top to bottom (its y counts up from the bottom). Out of harm, the game's own 24-pixel safe box is drawn as it is |
| DoDonPachi Dai-Ou-Jou, Black Label, Ketsui, Espgaluda | the ship, the bullets, the bodies: from the tests at 1440A2 / 244D62 / 0B02FC / 11B1D0 (Ketsui and Espgaluda test bodies every other frame; they are drawn every frame) |
| Mazinger Z | the robot (8 x 14 pixels, the same for all three), the bullets (boxes here, not points: 11 x 11 for the common one, drawn see-through), the bodies: from the game's test at 00608E, one table of everything but the robots, read as the test is entered (it marks what it hits). A bullet is what hits the robot and the shots pass through; most flying enemies do not hit the robot at all and are drawn as ground enemies |
| Air Gallet | the ship (its box against the bullets' boxes; against the bodies only its middle point counts), the bullets (each a small box), the bodies: from the test at 01005E (bodies are tested every other record a frame; all are drawn) |
| DoDonPachi II: Bee Storm | the ship (9 x 7 pixels), the bullets, the bodies: from the tests at 14EBD2, 14FC42 and 150104, all the 68000's (the ARM only moves the bullets, whose records are in the RAM the two share; the game tests one player a frame in turn, and the boxes are drawn every frame) |

Every game also gives the **player shots** and the **ground enemies** (the enemies the shots are tested against
that are not among those that hit the ship), from the game's own shot test, read the same way as its other boxes:

| Game | Shots, and ground enemies |
| --- | --- |
| DoDonPachi, DonPachi, Dangun Feveron, ESP Ra.De., Guwange | an enemy the shots test has bit 0x20 of its first byte (the ship's test wants 0xC0), so a ground one is 0x80 + 0x20 without 0x40: tanks, turrets, the blimp's parts, Guwange's turret house. Shots from the shot tests at 0059E8 / 00576C / 009B60 / 00606A / 00836C: the shots, and the lasers, options, Feveron's formation and Guwange's shikigami where the test takes them (bombs, and boxes that only cancel bullets, left out) |
| Air Gallet | the shot test follows the ship's (0101F4): an enemy the shots hurt has bit 13 of its first word, one that hits the ship bit 14; the first 57 of the 156 records are the ground's and are not tested against the ship (unless the word at 109270 is set). Some large aircraft are drawn as ground: the game does not test them against the ship. Shots: each player's 32 records (the bombs' blasts and the pickups left out) |
| DoDonPachi Dai-Ou-Jou, Black Label, Ketsui, Espgaluda | the shot test follows the ship's in the same routine. Enemies are two groups, the second the ground's; one the shots can hurt has bit 0x2000 of its first word. Shots as the test reads them, with DOJ's laser (its beam cut where it stops, its aura) and Espgaluda's lasers, awakened or not |
| DoDonPachi II: Bee Storm | an enemy has up to ten boxes, each with a bit for "the shots hurt it" and one for "it hits the ship": a box with the first and not the second is ground. The shots' boxes from their records as 150104 tests them; while the laser is on, its aura, beam and head (14FC42) in their place, and while the bee storm is out, its two boxes (14FEFC) |
| Mushihimesama, Matsuri Ver 1.5 | the body lists' records with +0 set and +4 clear are tested by the shot pass (0x0C05A9C0; Matsuri's 0x0C05ACF0) and never hit the ship; the shots and lasers' lists, read at the hit test's entry |
| Ibara, Ibara Kuro Black Label, Pink Sweets, Muchi Muchi Pork! | the shot tests (0x0C05E6E0 / 0x0C05EAA0; 0x0C06E1D0 / 0x0C06E540; 0x0C075BA0 / 0x0C076150; 0x0C073490 / 0x0C0737F0): an enemy's shootable bit without its bit that hits the ship: turrets, tanks, buildings that can be destroyed. The shots' boxes from the game's own table of them, the options' shots too |
| Espgaluda II | enemy kinds 0 and 1 of 0x0C37D288 are what the shots hit (tested by 0x0C062BC0 against shot kinds 4 to 9, 12, 13: kind 12 the laser); one with no body (kind 15) of the same owner is drawn as ground (here also harmless enemies in the air, as most never hit the ship) |
| Mushihimesama Futari (every version), Deathsmiles, MegaBlack Label | the later engine: the enemies' pass tests every member of body kinds 0, 1 and 21 whose +12 is set against shot kinds 4 to 15 (bit 1 of +27: shots pass through it); a member whose bit 0 is clear and whose owner has no body is ground (walking beetles; in Deathsmiles few: houses, as nearly every enemy also has a body box) |
| DoDonPachi Dai-Fukkatsu 1.5, Black Label, SaiDaiOuJou, Akai Katana, Dai-Fukkatsu 1.0 | the same, in a later build: enemies' passes 0x0C1E6860 / 0x0C1E67B0 / 0x0C1E78F0 / 0x0C1E8100 / 0x0C1E67C0 (in Akai Katana kind 4 is the main shot). The fortresses' turrets, the tanks |
| Mazinger Z | the same test and table: an enemy's flag for the shots (bit 1) or for the attacks that also clear bullets (bit 2) without the flag that hits the robot (bit 0): tanks, turrets, and here most enemies in the air too. Shots: the players' records of both kinds (the shots; the special attack's flames and the bomb's own parts), each with its box |

The boxes are drawn with the frame the picture is from: a CV1000 shows a frame's draw list two frames after the code
that made it (the blitter draws it the next frame, and it is shown the one after), the 68000 board a frame after (the
sprite chip draws the list at the next frame's interrupt), the PGM at once (`Hw::kPictureLag`; checked with moving
bullets in Mushihimesama, Dai-Fukkatsu and DoDonPachi, and a moving ship on the PGM). DoDonPachi II is a frame behind
its own boxes on the PGM (it makes its sprite list from the places its tests use in the same frame, and the board
shows a list a frame after it was made), so its own reading hands on the frame before's boxes (games/ddp2/src/game.cpp;
checked with single volleys of shots, which move 14 pixels a frame: each crescent in its box).

The CV1000 games build their boxes each frame for the test and clear them after, so for them the boxes are read at the
entry of the game's hit test (`Facts::hitTest`, a native call that reads and lets the routine run) rather than after
the frame. Each game's test is its own (Espgaluda II's is a general engine of collision kinds, whose lists last and can
be read after the frame), and no other game shares Mushihimesama's or Espgaluda II's code, so each needs its own
reading.

Checked by eye: the ground boxes on the tanks and turrets of each game's early stages, the shots' over its shots; the ship's box on its cockpit (with and without run-ahead, GPU and software), a dot on each bullet,
boxes on the bosses' bodies. The game's own test was read, not run against the drawing: a hit has not been compared
with the boxes frame by frame.

### DoDonPachi II: Bee Storm's (2026-11-15)

Checked by eye in headless runs and the game program's pictures: a game begun at each of the seven stages (the stage's
own title card, then its scenery and enemies); the trip to each stage's boss (all seven arrive at the boss's warning:
336, 3,404, 9,246, 11,081, 14,048, 13,564 and 2,332 frames after the stage begins); a game with ships that never run
out still going where the same play without them is over; the bombs kept at five through a bomb every 150 frames, and
the Energy ship's gauge at full; the boxes over stages 1, 2, 4 and 5 and stage 1's boss (the ship's on its canopy,
still and moving; a dot on each bullet; the enemies' and the ground's on their sprites; each shot's crescent in its
box; the laser's three boxes with the laser on). The game's own Practice mode (four stages, chosen from a menu of
its own, with the game's rules) is left as it is: the features are the port's and work in any mode. With no ship
flown (the selects, the ranking) the game tests nothing and no box is drawn.

Not there: the boxes of the bee storm (14FEFC) are read as the laser's are but were not looked at in a picture; the
shots' and enemies' boxes of player 2 are read (the ship, shots and laser of both players) but were only seen with
player 1; the trip to the boss was not timed against a board.

## Scoring readouts (runtime/game.h: Game::readouts)

The counters a game keeps and does not show, read from its memory each frame and written down the left of the picture
(the feature "Scoring readouts", offered only by a game that has some). A game lists them as names and a routine that
reads each one's value now ("" hides it).

| Game | Readouts |
| --- | --- |
| Muchi Muchi Pork! | LARD: the lard meter as a number, out of 104 (the game draws only its bar) |

## Gadgets (runtime/game.h: Game::gadgets; port/gadgets.h)

The feature "Gadgets" (Training mode page, every game): framed panels in the space a stood-up picture leaves
at its sides in a wide window.
Drawn on a picture of their own a whole fraction of the window's size (about 360 lines, so each of its dots is a few
of the window's pixels, sharp), laid over the window as the touch controls' layer is (those, while they show, come
first: they want the same sides). Read from the board after each frame shown, never written to it: the game runs the
same frames with them on, online too (they are each player's own).

Every game has the toolkit's: CONTROLS (the stick and buttons held), SPEED (the game's speed as a graph: the Speed
readout's number, the share of the last 60 refreshes the board gave a new picture in, so where it slows down shows as
a dip; docs/plans/GADGETS-RESEARCH.md says why it is that and not the CPU's load) and, where its facts give player 1's
score and the stage (Game::score, Game::stage), SCORE BY STAGE (what each stage made, the stage now marked, and the
total; begun again when the score goes down, a new game). A game with scoring readouts and no gadgets of its own shows
them as one panel, COUNTERS. A game's own are Words (lines), a Meter (a bar, with words over it) or a Graph (the value
kept every 8 frames over the last half minute, the rank graph). A side too narrow for a panel (84 of its dots) gives
its gadgets to the other; none are drawn when neither has room (the picture across the window: As the cabinet, or a
tall window).

| Game | Its own gadgets |
| --- | --- |
| Dai-Ou-Jou Black Label | RANK (the game rank, a graph), BULLET SPEED (with the hyper rank), CHAIN (the gauge, hits, best), HYPER (the gauge or the time left, items held), BEES, TO THE 2ND LOOP (the game's own test); score by stage |
| Dai-Ou-Jou | The same as Black Label's, at the white label's own places (its bullet speed 10 at most); its readouts too |
| DoDonPachi | RANK (0 to 63, the byte the enemies' fire reads, a graph), CHAIN (the timer, hits, best), BEES (this stage's, the sets of 13), TO THE 2ND LOOP (the game's own test: no continue, and misses, the best chain for the ship, the score or the bees); score by stage, 1-1 to 2-7 |
| Ketsui | CHIPS (the stock and the total, the one counted now first), LOOP; score by stage. No rank was found (the enemies' patterns are by stage, loop and difficulty), nor the second loop's test |
| Muchi Muchi Pork! | RANK (a graph), LARD, NEXT MEDAL, BOSS (the life of the part that counts, and the phase). From wnka's MAME layout, its places checked here |
| Pink Sweets | RANK (a graph), ZAN, NEXT DROPS (the round of eight, and the shot and option power-ups dropped), NEXT MEDAL, BOSS (life and phase). From Olifante's MAME layout, its places checked here |
| Ibara | RANK (a graph), NEXT DROPS (the round of 25), NEXT MEDAL. From the MAME layout linked from epozzobon.it/re (its places for this set's later program), checked here |
| ESP Ra.De. | SCORE ITEMS (the counter under the lives, how far it goes before it is full, 200 then 100 more each time, and FULL while it is), MULTIPLIER (the power shot's, and the frames it lasts), BARRIER (the gauge as a share); score by stage |
| Espgaluda | KAKUSEI (the bullet multiplier, to 100), OVERMODE (its level, 1 to 4, and the way to the next), GOLD (the count, to 1,000, and the step it is on, one more each 100); score by stage |
| Espgaluda II | OVER LEVEL (Overmode's level, 0 to 3, kept when a ship is lost, and its way to the next in Overmode: 250 frames), GOLD (to 1,000); score by stage |
| Mushihimesama Futari (every set) | RANK (a graph, to the mode's most), RANK FROM (what it is made of: the mode's base, the ship's part from the bombs held, the time played); score by stage. The rank's routine read from the game's code in each set |
| Dangun Feveron | RANK (0 to 63, the byte the bullets' speed reads, a graph; 63 through the second round), CYBORGS TO 1UP (the cyborgs taken this game, the extend at 2,000); score by stage, 1-1 to 2-5 |

## Not yet

The other games' scoring readouts (Mushihimesama's rank, Espgaluda II's Kakusei multiplier, Futari's counters and
multiplier: none found yet), the gadgets of Deathsmiles, Akai Katana, DoDonPachi II, DonPachi and Guwange (docs/plans/GADGETS-RESEARCH.md: what the official ports show), score attack and the other modes (ROADMAP.md).
