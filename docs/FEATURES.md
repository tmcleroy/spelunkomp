# Features

Everything a player can switch on, game by game. Every feature is off unless chosen (free play and "Straight to the
title" are on to begin with, and can be turned off): with them all off a game plays as the board. They are chosen on
the launcher's **Features** page, or while playing in the in-game menu (Esc, F1, a gamepad's Guide, or Start and
Select together; the Controllers page's Menu button can add the left stick's click, or leave only Start and
Select). A game's program lists its own with `--list-features`.

## The games

| Board | Games |
| --- | --- |
| CV1000 (SH-3) | Mushihimesama (and its 2011 Cave Matsuri Ver 1.5), Ibara, Ibara Kuro Black Label, Espgaluda II, Pink Sweets, Mushihimesama Futari (Ver 1.0, Ver 1.5, Black Label and its 2009 Another Ver), Muchi Muchi Pork!, Deathsmiles and MegaBlack Label, DoDonPachi Dai-Fukkatsu (Ver 1.0, Ver 1.5 and Black Label), Akai Katana, DoDonPachi SaiDaiOuJou |
| Cave's 68000 board | DonPachi (Japan Ver 1.01 and US Ver 1.12), DoDonPachi, Dangun Feveron, ESP Ra.De., Guwange, Mazinger Z, Air Gallet |
| IGS PGM | DoDonPachi Dai-Ou-Jou and Black Label, Ketsui, Espgaluda, DoDonPachi II: Bee Storm |

Each needs its own arcade ROM set, one version of it: the launcher's page for the game names the set.

## Every game

| Page | Feature | What it does |
| --- | --- | --- |
| Game | Free play | No coins needed: Start begins a game or continues one. Off: a coin (5, or a gamepad's Select) for each credit |
| Game | Start | Straight to the title: the board's checks, warning and logos run unseen. Or the whole boot, as the board shows it |
| Game | Slowdown as the board | On unless turned off: the board's CPU waiting on its memory worked out, so the game slows where a board does (CV1000: docs/SLOWDOWN.md; PGM: its waits). Off: busy scenes at full speed, for a slow device. Not yet compared with a board. Not on Cave's 68000 board, which has none to add |
| Graphics | Screen | Stood up for an ordinary screen, or as the cabinet sends it, for a screen turned on its side (top to the left or right) |
| Graphics | Scaling | Whole pixels (the largest whole multiple that fits), a fixed multiple, or fill the window |
| Graphics | Aspect | As the cabinet's 4:3 monitor, or square pixels |
| Graphics | Shader | RetroArch's shaders through librashader: CRT, CRT Lottes, CRT Guest, CRT Royale, CRT Light, CRT Newpixie (a light CRT look with chromatic aberration, ghosting, a little curvature and scanlines), scanlines, xBRZ and more, or your own preset |
| Graphics | Your own preset | The preset Your own draws with, chosen by name from every `.slangp` in the shaders folder all the games share (folders in it too, so a RetroArch shader pack goes in whole with its passes): `%APPDATA%\Spelunkomp\shaders\`, `~/Library/Application Support/Spelunkomp/shaders/`, `~/.local/share/Spelunkomp/shaders/`, or `user/Spelunkomp/shaders/` in portable mode. The launcher makes it, its **Open shaders folder** button opens it, and updates leave it alone. A game's own `shaders` folder, beside its settings, still works and comes first. A preset that fails to load says why, over the picture, in the menu and in the log. On Android the folder is in the app's own storage, which a computer over USB can usually reach but the phone's file manager usually can't: there is no import there yet |
| Graphics | Scanline direction | A shader's lines down the picture, as on the cabinet's turned monitor, or across it |
| Graphics | Renderer | GPU (the default): the board's drawing done by the graphics chip (docs/NATIVE.md). Software: by the processor, the reference the GPU is checked against; the same picture, for small integrated graphics (Intel UHD or Iris, a low-end phone's GPU) where it can be faster. Used from the next start; `--software` is Software whatever it says. The log (logs/game.log) suggests it when the GPU is Intel's integrated graphics or a low-end Mali or Adreno |
| Arcade monitor | Full screen resolution | For a CRT at arcade rates (CRT Emudriver, a custom resolution): full screen in the board's own resolution, pixel for pixel (CV1000 and Cave's 68000 board 320 x 240, Mazinger Z 384 x 240, the PGM 448 x 224), or a super resolution of its lines and 1920 columns or more (the PGM's 224 lines one for one in a mode of 240 where the card has none of 224); only a mode near the board's rate (60.02, 57.44, 59.19 Hz). A game made for a monitor on its side is shown as the board sends it, for a monitor turned as the cabinet's was. Set on the launcher; not on Android |
| Arcade monitor | Screen | Which screen the game opens and goes full screen on. Set on the launcher |
| Arcade monitor | Make the mode at the board's rate, Monitor | Windows: Switchres (GroovyMAME's) makes that mode at exactly the board's rate, within the chosen monitor's ranges, and the desktop's mode comes back at the end. Set on the launcher |
| Arcade monitor | Keep the board's pace | Off (the default): in full screen at an arcade mode one frame for each of the screen's refreshes, with a frame delay where the system says when it refreshes (Windows): each frame's controls read and its picture made as late before its refresh as the work allows, no added lag. On: the board's clock, as in a window. Online always the board's clock |
| Arcade monitor | Compact menu | The in-game menu in small letters, its rows close together; on by itself on an arcade mode, where it is drawn on the board's own lines |
| Sound | Sound filter | Softens the treble of what is played: Soft (from about 8 kHz) or Softer (about 5 kHz), a second-order low-pass on the sound as it is handed to the system only; the game, recordings and online play are as before. Each player's own online (port/sound_filter.h) |
| Controls | Run-ahead | Takes out the frames of lag the game itself has: the game plays the same, only the picture is ahead (docs/LAG.md) |
| Controls | Input lag readout | The game's own frames from a press to the picture, what run-ahead takes off, and the port's own time |
| Training mode | Practice point | The game kept as it is and brought back at once, as often as you like; a replay of your last attempt from it |
| Training mode | Rewind | Hold to run back through the last 30 seconds or so, then play on |
| Training mode | Fast forward | Hold to run several times as fast, without sound |
| Training mode | Input display | The stick and buttons as the game reads them |
| Training mode | Speed readout | How fast the game is going: under 100% where it slows down |
| Training mode | Speed (slow motion) | The Speed row at the page's top (as on the Game page): 3/4 down to 1/8 for study, or faster; the sound slowed with it. The game computes exactly the same frames |
| Training mode | Frame advance | Stops the game with the frame's number shown; then O, Tab or a gamepad's right trigger goes on a frame at a time (held: frame after frame). P or the row again plays on |
| Online | Ping, Input delay, Hide names | What an online match shows at the bottom of the picture (docs/ONLINE.md) |

Always there: save states (F5 / F9, and the menu), pause (P) and a frame on (O; stopped, Tab or the right trigger too),
the speed from 1/8 to 8x (the menu, - and =; not in an online match),
the board's test switch (F2) and service (F3), full screen (F11).

## From each game's facts

These rest on where each game keeps its stage, ships, bombs and the ship's safe time, and on its own hit test
(docs/TRAINING.md has the table of which game has which, and how each was found):

| Feature | What it does | Games |
| --- | --- | --- |
| Start at stage | A new game begins at the stage chosen, through the game's own start of it | all |
| Begin at the stage | (a command) A new game now at the stage chosen, from wherever the game is | as Start at stage |
| Straight to the boss | A new game run on unseen to the boss of its stage, the practice point set there | all (Akai Katana's stage 5 and Deathsmiles' castles time out) |
| Out of harm | Nothing hits the ship | all |
| Bombs never run out | The bombs filled again as used (a gauge kept full where the game has one in their place) | all |
| Ships never run out | A ship lost is not counted (Mazinger Z: energy) | all |
| Hit boxes | The ship's box, enemy bullets, enemies, ground enemies and the player's shots, each kind in a colour of its own or hidden | all |

## Alternative modes

- **Attract mode** (every game): the board started again and left to run its own show over and over, as a cabinet
  nobody is playing: the logos, the title, the demonstrations of play and the rankings, with the board's own sound. No
  coin is in (free play is off for it, so the show is not held at the press-start screen), the game is given no
  controls, and its features that change play (free play, a start at another stage, the cheats) are off while it runs.
  Start or a coin ends it, and the game is yours again as you had it. As cps2recomp's attract mode, with the whole show
  (cps2recomp's shooting games can also show only the demonstrations or only the story; not here yet). Not in an
  online match.

- **Dodge training** (DoDonPachi only so far): your ship alone against an emitter where a boss would be, firing the
  game's own enemy and boss patterns, one over and over, all in turn, or in scenarios from a warm-up to a gauntlet,
  with your hits counted (docs/DODGE.md). Not in the downloads yet: it is unfinished, and its patterns are data
  recorded from the game, made on the builder's machine (work/ddonpach/dodge_patterns.inc) like the translated code.

## Online play

Two players on two machines play a game's two-player mode together, with rollback, in every game: host from the
launcher's Online page or the in-game menu and give the other player the code, or list the match in the lobby.
The match has the host's board settings and free play; the training tools are off (docs/ONLINE.md).

## The launcher

One launcher for every game: finds each game's ROM set in a folder of them (or a set dropped on its window), lists
the games by name, release date or board, and has the Features, Controllers (keys and two gamepads, remapped) and
Online pages. In English, Spanish and Brazilian Portuguese.
