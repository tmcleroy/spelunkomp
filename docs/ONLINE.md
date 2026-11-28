# Online play

Two players, each on their own machine, play a game's two-player mode together, with **rollback** netcode. It is
cps2recomp's online play (its `port/netplay.*`, and a copy of its introduction service, `online/`), made for these games
and their boards.

## Starting a match

Each player needs the game and its ROM set, and the same version of Spelunkomp (the launcher shows it at the top of
the sidebar). The launcher's **Online** page has it all, as in the user's other ports:

- **Open matches**: the matches other players are hosting in the lobby, in every game, each with **Join**. A match in a
  game whose ROM set you haven't added is greyed, with **Add…** to add it.
- **Join with a code**: type the code a host gave you and press **Join**. The launcher starts the game the match is in.
- **Host a match**: choose a game and press **Host**. The game shows the match's code to give the other player. If
  you've set **Your name**, the match is also listed in the lobby under it until someone joins; without a name, only
  those you give the code to can join.
- **Waiting room**: the other players whose Online page is open now, under their names. **Invite** one to a match in
  any game: your game starts and hosts, and they see the invitation on their Online page, with **Accept** and
  **Decline**.

Each game's own page has the same in its **Online** card. Nothing needs setting up on either router. The host is player
1.

**Hosting from the game**: the in-game menu's **Online** page has **Host a match**. The game goes on as you play it (the
menu, the training tools, save states), with "HOSTING  CODE K7Q-M2X" at the bottom of the picture: the code to give the
other player (and "IN THE LOBBY" when you have a name on the launcher's Online page, so it's listed there too). When
someone joins, the game starts over for the match, as a match hosted from the launcher does: from the board's power-on
(straight to the title if you have that), with your EEPROM and Game page; your training tools are off for it. The same
row, now **Stop hosting**, takes the code back. After the match (Esc's **Leave the match?**, or the other player
leaving), your game goes on alone from where the match was, with your own features again; if the other player left or
dropped, you're still hosting with the same code.

From a terminal:

```sh
mushisam --roms mushisam.zip --host                 # the host: shows a code, waits for the other player
mushisam --roms mushisam.zip --join CODE            # the other player: the host's code (or address)
mushisam --roms mushisam.zip --host 7900            # ... on another UDP port
mushisam --roms mushisam.zip --join 192.0.2.20:7900          # ... or by address and port
```

Other options: `--net-name NAME` (shown to the other player), `--net-list` (the match in the lobby), `--net-delay N`
(the host's input delay, 0 to 8), `--net-service URL|off` (another introduction service, or none: addresses only),
`--net-port N` (the UDP port a host uses), `--net-menu-host FRAME` (the menu's Host a match chosen at that frame: tests).

## How it connects

The two games are introduced by Spelunkomp's own small service (`online/`, a copy of cps2recomp's: a Cloudflare
Worker, spelunkomp-online.tmcleroy.workers.dev), which tells each where the other can be reached; they then connect
straight to each other through their routers (UDP hole punching), and none of the match goes through it. A code lasts
30 minutes and is good for one match. The service keeps each toolkit's lobby and waiting room apart by the protocol
number a game says (`Netplay::kLobbyProtocol`: 4000 and up for spelunkomp), so Spelunkomp's lobby lists only
Spelunkomp's matches; a game says its id (`mushisam`, from game.cfg), its title and its version with it. Nothing
needed registering with the service.

**Joining by address** works without the service: on a home network, the host's own address (the Online card lists
it); over the internet, the host's public address with UDP port 7845 forwarded to it; or a VPN such as Tailscale.

## In a match

- **Each player presses Start to come in**, as at the arcade: the first to press plays alone until the other presses
  theirs. "PRESS START TO JOIN" shows until you have.
- **Each player plays with their own player 1 controls** (keys and player 1's gamepad: the launcher's Controllers page says which), and sees the game as their own
  settings have it (the Display page: the screen's turn, the scaling), and hears it as theirs have it (the Sound
  page's Sound filter).
- **The host's game is the match's**: the match begins at the board's power-on with the host's EEPROM (the game's own
  settings: difficulty, lives, its test menu's choices, with the host's Board page put in over them) and the host's Game
  page (free play, the CV1000's and the PGM's slowdown).
  With the Game page's **Start** at **Straight to the title** (as it is unless the host changed it), both sides run
  the boot unseen, with no controls, from that same power-on before the match's first frame, so the match begins at
  the title. The two are alike because they run the same frames from the same state with the same (no) controls; the
  game is sent at power-on, while a CV1000's VRAM is still empty (74 KB), not at the title (some 11 MB). Each side's
  controls go on being sent while it runs the boot, so a slower machine's few seconds of it aren't taken for a drop.
  The other player's own EEPROM is left as it was: nothing of the match is written into it.
- **Autofire is each player's own** (the Controls page): each side tells the other its own in the hello or the
  welcome, sends its controls as held, and both sides put each player's autofire in the same way, by the board's frame
  number, as the frame is run (rollback's frames too). Held buttons stay held in what is sent, so the other side's
  guesses of them stay right and autofire costs no rollbacks.
- **Off in a match, on both sides**: the training tools that change the game (out of harm, endless bombs and ships, a
  start at a stage or the boss, the practice point and its replay, rewind, fast forward), run-ahead and the input lag
  readout, the in-game menu, save states, pause and the speed. What is only drawn over the picture stays each player's
  own: the input display, the speed readout and the hit boxes (a co-op match isn't made unfair by them; cps2recomp's
  shooting games have them so too).
- **On screen**: both players' names at the bottom, each on their side (**Hide names** hides them), the ping between
  them ("PING 42 MS"), and the input delay too if you like ("DELAY 2"). Each is a switch on the Online tab of the
  in-game menu. "WAITING FOR THE OTHER PLAYER" shows if their game goes quiet for a moment.
- **Chat**: press T (or a gamepad's right stick click), type a line and press Enter (Esc puts it away). The last few
  lines show at the bottom left for a few seconds. It goes straight from one game to the other. While you type, the
  keyboard is the chat's; a gamepad still plays.
- **Leaving**: Esc (or a gamepad's menu button, as the launcher's Menu button has it; Start and Select together
  always) asks **Leave the match?** over the game, which goes on; Enter (or A) leaves, Esc (or B) stays.
- **Coming back after a drop**: if the other player's game closes or their connection drops, the host's game waits for
  them again with the same code; a joining player whose connection dropped is taken back to the host by itself. The
  two then begin again from the board's power-on.

## How rollback works here

Each side runs the whole game with both players' controls: its own player's held back 2 frames (`--net-delay`), the
other's guessed (as they last were) until they come. When a guess was wrong, the game is put back and run on again
with the real ones, unseen and unheard, in the time of one frame. Up to 8 frames are guessed; past that the game waits
for the other side (6 on the CV1000, whose frames are dearer).

The game is kept the way run-ahead keeps it (`port/hw_*.h`), made exact for this:

- **The board**: a CV1000's main RAM and its CPU, blitter registers, NAND, clock and EEPROM, and the sound chip's state;
  a 68000 board's or a PGM's whole state. Unlike run-ahead's frames, the frames run again run the sound chips too, so a
  game that reads them sees what it saw the first time; their samples are dropped, and only the newest frame's are
  played. The sound already heard of a frame later found wrong is not taken back.
- **The picture**: a CV1000's VRAM is on the GPU (or in the software blitter, `--software`), and is the sum of every
  frame's drawing, the graphics uploaded into it among them. It is kept by the areas written since the moment kept (a
  journal, on the GPU), and the frames run again run their draw lists too, so the picture after a rollback is the one
  the right controls make; only the newest frame is shown. The 68000 boards' and the PGM's pictures are made afresh
  each frame from their state, so the frames run again make none.
- **Two moments are kept, never more**: the **anchor**, the game before the first frame whose other player's
  controls are still guessed, and the **next anchor**, kept at the newest frame while its guesses are still to be found
  right or wrong. When they are found right it becomes the anchor; when one is found wrong, the game goes back to the
  anchor and runs the frames since again. Keeping a moment costs about 0.4 ms on a CV1000 (16 MB of RAM copied), and
  20 microseconds on the other boards. With every frame confirmed (a home network: the other player's controls come
  within the 2 frames of delay) nothing is run again.

What it costs (this Mac, an Apple silicon Mac mini, with many other jobs running: the times are on the high side):

| | a frame shown | a frame run again (going back included) | keeping the game |
|---|---|---|---|
| CV1000 (Mushihimesama) | 2.3 to 2.6 ms | 3.0 to 3.7 ms | 0.41 to 0.44 ms |
| Cave 68000 (DoDonPachi) | 0.4 ms | 0.14 ms | 0.02 ms |
| PGM (Dai-Ou-Jou) | 0.5 ms | 0.3 ms | 0.02 ms |

A 60 Hz frame is 16.7 ms. A Steam Deck's CPU is roughly half as fast a core for this work (not measured there yet), so a
CV1000 frame run again would take some 6 to 8 ms there: two or three a frame without a dropped frame. A rollback goes
back at most about twice the frames guessed, so with 2 frames of delay a CV1000 game keeps its speed on a Deck up to a
ping of about 60 to 80 ms; past that a wrong guess can cost a frame now and then. The 68000 boards and the PGM have room
for any rollback.

## Checked against each other

Every second each side sends the other a fingerprint of the game as it was after a frame both have the real controls
for (the board's state hash: main RAM, the CPU's registers, the EEPROM; on the 68000 boards the video's memory too).
If they ever differ, the match stops and says so (a desync, which should not happen).

`tools/check_online.sh ROMS [GAME...]` plays two copies on one machine against each other with made-up controls (each
presses Start now and then and moves about at random with the shot held), packets sent late and lost if asked
(`FAKE=LAG:JITTER:LOSS`), and checks every second's hashes are alike, line for line (`LOG=1`: every frame's controls
and hash, each side's, to find the first frame two games part at). `--rollback-test` checks the keeping alone, in one
copy: every frame is run first with other controls and put back, and the game must end as a run without it does.

`HOSTMENU=1` has the host choose the menu's Host a match a few frames in and play alone until the other copy joins
it (the menu's way into a match, the game made anew when they come).

The game as a match begins (sent from the host to the other player, compressed) is the board's whole state at
power-on: 75 MB for a CV1000 (its VRAM, all of it, and its RAM), 74 KB as sent; a few hundred bytes for the others.
