# PicoCalc Doom

PicoCalc Doom runs Doom on the
[ClockworkPi PicoCalc](https://www.clockworkpi.com/picocalc): a port of
[FRANK DOOM](https://github.com/rh1tech/frank-doom), Chocolate Doom by way
of doomgeneric for the RP2350, to the PicoCalc's screen, keyboard, speakers
and SD card.

Doom's data is not included. Use the IWAD from your own copy of Doom
(the DOS, Steam or GOG releases, or the 2024 re-release), the shareware
`doom1.wad`, or [Freedoom](https://freedoom.github.io), which is free.

## What you need

- A PicoCalc with a **Pimoroni Pico Plus 2** or **Pico Plus 2 W** installed.
  Doom needs the Plus 2's 8 MB of PSRAM, so it will not run on a Pico,
  Pico 2 or Pico 2 W. The one image works on both Plus 2 boards; it does
  not use the radio.
- An SD card, FAT32 or exFAT, with a `doom` folder holding your WADs.
- Optionally, [UF2 Loader](https://github.com/pelrun/uf2loader) and the
  [PicoCalc shell](https://github.com/BlairLeduc/picocalc-shell), to start it
  from the card, give it arguments, and go back to the shell afterwards.

## Installing

Download `picocalc-doom-v<x.y>-pico-plus2.uf2` from the
[releases page](https://github.com/BlairLeduc/picocalc-doom/releases).
Either:

- **With UF2 Loader**: copy the `.uf2` to the card's `pico2-apps` folder and
  choose it from the loader's menu or the shell. Quitting Doom then goes
  back to the shell. This is the way to install on a board that already has
  the loader.
- **From BOOTSEL, with no loader**: hold BOOTSEL on the Plus 2 while
  plugging it into a computer, and copy the `.uf2` to the drive that
  appears. The drive goes away by itself when the copy ends. Quitting Doom
  then switches the PicoCalc off.

With UF2 Loader installed, a BOOTSEL copy goes into the loader's program
area instead of replacing it. If you have trouble installing the UF2,
erase the flash first (Raspberry Pi's `flash_nuke.uf2`), which removes the
loader too. The shell's own loader build starts the shell at power-on; hold
Up, F1 or F5 to start the program instead.

## The card

```
/doom/*.wad                 your IWADs, and any PWADs
/doom/picocalc.ini          the port's settings (optional, below)
/doom/default.cfg           Doom's settings, written when you quit
/doom/doomgenericdoom.cfg   Chocolate Doom's extra settings, written when you quit
/doom/.savegame/<game>/     saved games, one folder for each game
```

The card layout is FRANK DOOM's, so one card works with both.

With one IWAD on the card, Doom starts straight away. With more, a list
names them by title: Up and Down choose, Enter starts, and Esc leaves (to
the shell, or switches the PicoCalc off). `iwad =` in `picocalc.ini` skips
the list.

Doom knows an IWAD by its file name, which must be one of these:

| File | Game |
|---|---|
| `doom1.wad` | Doom shareware |
| `doom.wad` | Doom, or The Ultimate Doom |
| `doom2.wad` | Doom II: Hell on Earth |
| `plutonia.wad`, `tnt.wad` | Final Doom: The Plutonia Experiment, TNT: Evilution |
| `freedoom1.wad`, `freedoom2.wad` | Freedoom: Phase 1, Phase 2 |
| `chex.wad`, `hacx.wad` | Chex Quest, HACX |

Saves are kept for each game by its kind, as in Chocolate Doom, so
`doom.wad` and `doom1.wad` share one folder, as do `doom2.wad` and
`freedoom2.wad`. macOS's `._` files are ignored.

## What runs

Played on the PicoCalc: The Ultimate Doom, Doom II, Plutonia and TNT, the
original 1.9 data and the 2024 re-release's. Every demo of those four plays
to its end. The other IWADs above should run but have not been tried on
the device.

The 2024 re-release's add-ons are PWADs, loaded with `-file` (see
[Arguments](#arguments)):

- **No Rest for the Living** (`nerve.wad`) and **the Master Levels**
  (`masterlevels.wad`) over Doom II play their maps, but with Doom II's
  level names and, for No Rest for the Living, no end after MAP08: this
  engine predates Chocolate Doom's support for them as mission packs.
- **SIGIL** and **SIGIL II** do not: they are episodes 5 and 6, which a
  vanilla engine cannot reach.
- **Legacy of Rust** (`id1.wad`) does not: it is built for the re-release's
  new engine features. Nor is there a reason to copy `iddm1.wad`
  (deathmatch) or `extras.wad` (625 MB of the launcher's own content).

There is no multiplayer, mouse or ENDOOM screen. Heretic, Hexen and
Strife are not in this engine.

With music and effects playing, The Ultimate Doom's four demos run under
`-timedemo` at 56, 60, 63 and 41 frames a second, at screen size 10 (the
full view above the status bar); a smaller view is faster. Doom's game
runs at 35 tics a second, so those rates are above what play needs.

## Keys

| Key | Does |
|---|---|
| Up, Down | Forward, back |
| Left, Right | Turn; with Alt held, strafe |
| `,` `.` | Strafe left, right |
| Ctrl | Fire |
| Space | Use: open doors, press switches |
| `1`–`7` | Weapons |
| Caps Lock | Running off and on (it starts on) |
| Tab | Automap |
| `-` `=` | Screen size; zoom in the automap |
| Esc | Menu |
| Enter, Backspace | Choose, back in the menus |
| `y`, `n` | Yes, no |
| F1–F5 | Help, save, load, volume, detail |
| Shift+F1–F5 | Quicksave, end game, messages, quickload, quit |
| Alt+F1 | Gamma |
| Break (Shift+Esc) | Pause |
| Letters | Cheats, and names for saves |

**Running is always on**, and Caps Lock turns it off and on again; the
line under the picture says which. Shift does nothing in play: the
PicoCalc's keyboard swallows Shift with Left or Right, so it cannot be
a binding. `picocalc.ini` can start with running off, or make it a key
you hold.

Quit from Doom's menu, or Shift+F5 then `y`: Doom saves its settings, and
the PicoCalc goes back to the shell or switches off. With the stock UF2
Loader, holding Up, F1 or F5 while switching on goes to its menu, for any
program.

## Settings

Volumes, screen size, gamma, messages and the keys are Doom's own, set in
its menus or `default.cfg`, and kept when you quit.

`/doom/picocalc.ini` is optional, for what Doom cannot set itself. Each
line is `key = value`; `#` or `;` starts a comment.

```
iwad    = doom2.wad          # start this IWAD, without the list
args    = -skill 4           # more of Doom's arguments
run     = always             # always, hold (on run_key), or off
run_key = z                  # the key held to run, when run = hold
```

`run_key` is one key that is typed without Shift and is not otherwise in
use: a letter such as `z`, `9`, or one of `;` `'` `[` `]` `\` `/` `` ` ``.
A line that cannot be used is named under the picture when the game starts,
and the rest of the file still applies.

## Arguments

Doom's command-line arguments come from `args` in `picocalc.ini` and from
the shell, the shell's last, so they win:

```
picocalc-doom -file nerve.wad -warp 1
picocalc-doom -iwad doom2.wad -skill 4 -warp 7
picocalc-doom -timedemo demo1
```

File names are found in the shell's current folder first, then in
`/doom`. In the shell's DOS flavour, `/` works as `-`:
`PICOCALC-DOOM /FILE NERVE.WAD /WARP 1`. `-timedemo` reports its frame rate on the
screen when the demo ends; any key then starts again.

## Coming from FRANK DOOM

PicoCalc Doom keeps FRANK DOOM's engine, game and card layout. What
differs is mostly the hardware:

| | FRANK DOOM | PicoCalc Doom |
|---|---|---|
| Hardware | FRANK or Murmulator boards; RP2350 with PSRAM | PicoCalc with a Pico Plus 2 or Plus 2 W |
| Picture | HDMI | The PicoCalc's 320×320 screen, the 320×240 picture centred, not scaled |
| Sound | I2S | PWM stereo, to the speakers or headphones |
| Input | PS/2 or USB keyboard and mouse | The PicoCalc's keyboard |
| Clock | 252 MHz at 1.50 V | 300 MHz at 1.20 V |
| Settings and saves | Saves on the card; settings not kept | Doom's settings kept on the card when you quit; saves in a folder for each game |
| Leaving | Quit restarts | Quit goes back to the shell, or switches off |
| Arguments | None | From `picocalc.ini` and the shell |

`patches/frank-doom.patch` holds every change to FRANK DOOM's code.

## Building

Needs the Pico SDK 2.x and the Arm GNU toolchain; the
[Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)'s
installs are found automatically.

```
git clone --recurse-submodules https://github.com/BlairLeduc/picocalc-doom.git
scripts/build.sh                   # development build, log on UART0 (GP2/GP3)
scripts/build_release.sh 1.0       # dist/picocalc-doom-v1.0-pico-plus2.uf2
```

`docs/design.md` describes how the port works, `docs/milestones.md` what
was measured on the device, and `docs/hardware-notes.md` what was learned
about the PicoCalc along the way.

## Credits

- **Doom**, by id Software, whose source was released in 1997.
- **[Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom)**, by
  Simon Howard and contributors: the engine.
- **[doomgeneric](https://github.com/ozkl/doomgeneric)**, by ozkl: Chocolate
  Doom cut down for any platform.
- **[rp2040-doom](https://github.com/kilograham/rp2040-doom)**, by Graham
  Sanderson: the OPL music and sound code, and EMU8950's optimisations.
- **[EMU8950](https://github.com/digital-sound-antiques/emu8950)**, by
  Mitsutaka Okazaki: the OPL2 emulator.
- **[FRANK DOOM](https://github.com/rh1tech/frank-doom)**, by Mikhail
  Matveev: all of these brought to the RP2350, with the PSRAM support this
  port runs on.
- From FRANK DOOM's tree: **FatFs** by ChaN, and the SD card driver from
  **pico_fatfs** by Elehobica.
- The **[Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)**.
- **[UF2 Loader](https://github.com/pelrun/uf2loader)**, by pelrun, and
  the PicoCalc shell's `picocalc_shell.h`, for starting from the card,
  arguments, the time, and going back to the shell.
- The PicoCalc layer from `picocalc-quest`; the 8×10 font from
  `picocalc-text-starter`.

Doom is a trademark of id Software. No game data is included.

## Licence

GPL-2.0-or-later; see `LICENSE`. Chocolate Doom and doomgeneric are
GPL-2.0-or-later, EMU8950 and the 8×10 font are MIT, and FatFs and the SD
card driver are BSD-style. FRANK DOOM's `LICENSE`, with its list of the
projects it contains, is in `modules/frank-doom/`, and in the release zip
under `upstream/` with the GPL's and the SD card driver's texts.
