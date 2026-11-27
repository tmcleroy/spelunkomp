# Input lag

Input lag is never acceptable in these ports. The rule, and how it is kept:

- **Never more than the board.** The game reads its inputs from the CPU's ports once a frame, in its vertical sync
  interrupt. The port's frontend polls the host's devices at that read (`Cv1000::onInputRead`), not at the start of
  the host's frame, so the newest state the host has is what the game gets.
- **Nothing queued on the way to the screen.** The presenter shows a frame as soon as the game has finished it; no
  frames wait in a queue (SDL_GPU with at most one frame in flight, immediate or mailbox presentation where the
  display allows, VRR where it has it). Each present waits for the GPU to have finished the frame two before it,
  whatever the presentation (port/gpu_base.h): when the GPU keeps up that frame is long done and nothing waits; when
  it does not, the game is held back instead of frames piling up. The one exception is chosen: full screen at an arcade monitor's mode (the
  Arcade monitor page) is synced to the screen, each picture waiting for its refresh so that none is shown twice or
  torn; there the frame delay (Windows, where the system says when the screen refreshes) holds each frame back until
  just before its refresh, so its controls are read late and the wait is taken out of the lag again, as GroovyMAME's
  frame delay does. *Keep the board's pace* turns it off.
- **Less than the board, as a choice.** A CV1000 game draws a frame into VRAM beside the one on show and moves the
  screen's window to it afterwards, and the blitter takes time to finish; the picture a player sees is frames behind
  the input that caused it. The port knows the moment a frame's draw list is complete and can show it then. That
  option changes nothing the game computes (the game runs the same frames) and is offered as "Reduce the board's
  display lag".
- **No feature may add any**: the menu, overlays, readouts, filters, recording and online play all sit after the
  game's read and before nothing.

## Run-ahead

The games themselves answer late: Mushihimesama shows a press in the picture two frames after the frame that reads
it, on the board as here. The Run-ahead feature (Game page; Off, 1 to 4 frames) takes that out: each frame, after
the real frame is run and its sound taken, the board is kept (`Cv1000::KeepNow`: main RAM copied, the small state
archived, VRAM kept by the rows written, the sound chip held still), run on that many frames with the controls as
they are, its picture shown, and put back (`PutBack`). The game itself is untouched: the same frames, scores and
slowdown, and the same state hash whatever the setting (the check below). The right setting is the game's own lag;
more shows a jump when a press changes what the frames run ahead had assumed. It is off by default, as every change
from the board is.

**Its frames kept** (run-ahead 1 and 2, offline). Run as above, each frame shown cost one frame and N more, all run
again at the next frame. Now the frames ahead are run as the game's own would be (`hw.NetKeep`, online play's exact
moment: the sound chips run, the sound decoders' history kept too, and each frame's sound put aside), with the
controls as held at the frame's start, and the board left at the last of them. At the next frame the controls are
read as the game would read them; when they are the same as the first frame ahead was given, that frame is the game's
next one and is taken as it is, with its sound, and only one frame more is run for the picture ahead. When they are
not (or a coin, Start, the test switch, the menu, a training tool or a check wants the board), the board is put back
(`hw.NetBack`) and the frame run as ever. So with the controls held steady a frame shown costs one frame and a kept
moment, whatever the setting; a change costs what it did. The frames, the picture shown and the sound are the same
as without it, byte for byte (the state hash, the picture log and `--sound-log` compared, below);
`SPELUNK_NO_AHEAD_KEEP=1` turns it off. The program says at its end how many frames were kept and how many run
again.

**The processor between frames** (Game: Processor between frames). A frame's work done, the program waits for the
next, and a processor idle for a good part of each frame has its clock turned down by its power management: a Steam
Machine (amd-pstate-epp, balance_performance) ran SaiDaiOuJou's boss at 3.68 GHz in play against 4.78 headless, and
run-ahead's frames run again then start on a slow core. Light, the default, spends the last 3 ms of each wait busy
(the core paused in a loop, port/keep_awake.h); Full the whole wait, the GPU's waits too; with run-ahead at 2 frames
or more, Light is Full. On that Steam Machine, at the boss: run-ahead off, 7.8 ms a frame (median) with it off, 6.0
Light, 5.45 Full; run-ahead 2 with the stick moving, the frames over the board's 16.7 ms from 540 to 270 (Full) and
a frame run again from 18.3 to 15.4 ms. The frames, state and pictures are the same in every mode (the state and
VRAM hashes compared).

## Measured, not assumed

`tools/lag.sh ROMS` (GAME=<id>): into play, then the stick pushed at a known frame; the first frame whose picture
differs from the same run without the press is found, for run-ahead 0 to 3, and the game's state after the run is
compared across them. It opens hidden windows, two at a time.

| Game | Board (run-ahead off) | 1 | 2 | 3 |
|---|---|---|---|---|
| Mushihimesama | 2 frames | 1 | 0 | 0 |
| Mushihimesama Futari 1.5 | 2 frames | 1 | 0 | 0 |
| Akai Katana | 2 frames | 1 | 0 | 0 |

Espgaluda II and DoDonPachi SaiDaiOuJou are not measured yet: the script's way into play does not have the ship
under control at its press frame in those two (their state was the same at every setting).

Still to come: the same figures through the real presenter to the display (the time from the game's frame being done
to its being on screen), in the gate.

## The readout in the game (Display: Input lag readout)

The feature input_lag writes a line at the bottom of the picture: the frames the game itself takes
to show a press (found in play: every few seconds the next ten frames are run twice from one moment, unseen as
run-ahead's are, once with the stick as held and once pushed the other way, and the first frame whose picture differs
is the answer; the value most of five tries give), what run-ahead takes off that, the port's own time from a key or
button going down (the event's time) to handing the frame that read it to the screen, and the sum. "In play" is the
game's facts' (the ship flown, or the game's mode); a game without them is measured whenever the stick changes the
picture. With it on, the game's state is the same as without (checked: DoDonPachi and Mushihimesama, the same state
hash after 6,000 frames). It reads 3 frames in both: the frame that reads the press and the 2 after it that
tools/lag.sh finds.

A look is spread over the frames shown (issue #17: run in one frame, its twenty unseen frames made that frame several
times as long, and on a slower machine late: a stutter every few seconds). The moment is set aside twice (main RAM and
the small state, the CV1000's VRAM as a texture of its own on the GPU; the whole state on the 68000 board and the
PGM), and then each frame shown runs one frame of one of the two runs, by turns, before the game's own: the board and
the moment change places, the frame is run with the sound chip held, and they change back, the game's own picture
drawn again. Its picture is read back from the GPU without waiting, and looked at a frame later; while the GPU is
still behind with it, the look waits a frame rather than holding this one up. A frame shown so has at most one
frame's work more. The runs' pictures are the ones they had in one go, so the numbers are the same (and the game's
state, frame for frame, the same with the readout on as off). Once five tries agree it looks once a minute instead
of every 4 seconds, and again soon when play resumes after two seconds out of it (a new game, a continue).

## Sound

How long after the game makes a sound the system's sound output has it. The board makes its sound as the frame goes;
here a frame is run in one go and its sound handed to SDL the moment it is done, so a sound is never later than the
board's by the frame's own time. Before it is played it waits behind (1) the sound already in the stream, and (2) the
sound device's buffer; the system's own output (the driver, the converter) comes after both, and is not counted here.

`tools/audio_lag.sh ROMS [GAME...]` measures (1) and (2): each game run for a minute as it is played (`--real-time`,
hidden, its sound handed over as in play but not heard: `SPELUNK_AUDIO_LOG`), the sound waiting as each frame's comes
logged once a second, the first ten seconds left out. A Mac mini (M4, macOS 26, Core Audio at 44.1 kHz), busy with
other work, one game a board:

| Game | Before: device + waiting | Now: device + waiting |
| --- | --- | --- |
| Mushihimesama (CV1000) | 23.2 + 20.3 = 43.5 ms | 5.8 + 9.3 = 15.1 ms |
| DoDonPachi (68000 board) | 23.2 + 20.2 = 43.4 ms | 5.8 + 9.1 = 14.9 ms |
| Ketsui (PGM) | 23.2 + 71.5 = 94.7 ms | 5.8 + 9.1 = 14.9 ms |

Before, the device had SDL's own buffer (1,024 frames: 23.2 ms at 44.1 kHz) and a frame's sound was handed over
whenever less than 200 ms waited, so what waited was whatever it had grown to: here 20 ms, on the PGM 71 ms, and more
after each time the host held the game up (the frames run to catch up put their sound behind), never coming down.
Now (port/main.cpp's Sound):

- **The device asks for 256 frames** (5.8 ms), as cps2recomp asks on every system it ships for (macOS, Linux, the
  Steam Deck, Windows, Android).
- **10 ms waits, no more.** The game makes its sound at the board's pace and the device takes it at its own, and the
  two clocks never agree exactly; so the stream is played a touch faster when sound piles up and a touch slower when
  it runs short (half a percent at most, not heard), which holds what waits as each frame's sound comes at 10 ms. With
  more than 45 ms waiting (the game catching up after the host held it up) that frame's sound is left out, rather than
  everything after it being late.
- **Slowed or faster** (the Speed row), the stream is played slower or faster by as much, as cps2recomp does (before,
  the sound was off away from full speed). Stopped, a frame on plays that frame's sound. Fast forward is silent.

In these runs the sound never ran dry (a gap heard: nothing waiting when a frame's sound came, after a second of
steady play) and no frame's sound was left out. About one frame (15 ms) is left: the device's buffer and the margin
that keeps it from running dry when the host is a few milliseconds late with a frame.
