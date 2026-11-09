# Milestone log

What each milestone verified, on which board, on what date, and what was
not checked, newest first. `design.md` §16 holds each milestone's scope
and done-when criteria; this file keeps the full record.

## M10: release

**In progress.** **2026-11-08**, on a Pimoroni Pico Plus 2 W (RP2350B rev
2) in a PicoCalc, flashed over SWD with `scripts/flash.sh`; Arm GCC
15.2.Rel1, Pico SDK 2.3.1; 300 MHz at 1.20 V. The card as M9 left it
(`iwad = doom.wad`, `args = -skill 4`, screen size 10). Built, run and
read by Claude; the owner played the release soak.

### Done when

| Check | Result |
|---|---|
| The release UF2 boots from UF2 Loader | not yet (the owner) |
| The release UF2 boots from BOOTSEL | not yet (the owner) |
| An episode's first level plays with sound, on the release image | played with sound in the soak, flashed over SWD; from the loader and BOOTSEL not yet |
| The documents describe what shipped | README, LICENSE, `build_release.sh` written; the record to finish |

### Checked so far

| Check | Result |
|---|---|
| The release image runs, read over SWD (`scripts/counters.sh`) | **passed**: the counter block found by its symbol, 156 B in SRAM; 135 s into The Ultimate Doom's attract loop, 7,973 frames drawn and published, 7,972 presented; late refills, underruns, I²C errors and dropped OPL writes 0; core 0's stack 2,024 B, core 1's 1,092 B; image 501 KB; no partition (flashed over SWD) |
| The release image says nothing on UART0 | **passed**: the capture holds the development image's last lines before the flash, then nothing |
| `-timedemo demo1` frame hashes from the release configuration, read over SWD (`counters.sh --hashes`) | **same 1,702, different 0, skipped 7** (the 7 of M5), missing 0. The image is the release build with `PICOCALC_DOOM_ARGS="-iwad doom.wad -timedemo demo1 -framehashes"` built in; the shipped image with the same in `picocalc.ini` is still to run (the owner's card) |
| `counters.sh --watch` on the release image, 1 minute from the attract loop | the check runs and fails only for want of keys, as it should |
| **The shipped image's own frame hashes**, `-timedemo demo1 -framehashes` in the card's `picocalc.ini` (the owner), read over SWD | **same 1,702, different 0, skipped 7**, missing 0: the release build of `09352c1` from a clean tree, flashed over SWD; demo 1 to its end, 1,753 frames drawn |
| **The release soak**, `scripts/counters.sh --watch 31`, keys by hand, on battery | **passed, but for the owner's pauses**: one boot (the owner switched on 9 s before the first read); 186 reads over 30.8 minutes of play in The Ultimate Doom; 7,636 keys taken; late refills, I²C errors, keys lost, unknown and stray, dropped OPL writes **0**; 3 level loads (one through the intermission), with 36 underrun slots and 3 OPL late halves at them and none elsewhere; frames published, the game tic and the level tic rising in every read; drawn 66.0 and presented 57.0 a second; core 0's stack 2,016 B, core 1's 1,092 B, heap 4,544 B; 22 to 28 °C; **on battery throughout**, 91 %. The check failed minutes 9 and 14 for no keys: the owner paused in play (8.2–10.2 and 13.2–14.5 min), confirmed by the owner. The image said `5d8a6cb-dirty`: built before the first M10 commit, its firmware sources that commit's (`out/m10/release-soak.log`) |
| Host tests; `check_sources.sh` | 12 of 12; 95 compiled, 9 excluded, matching upstream |

### Found

- **One OPL late half at boot on the release image**, every boot (three of
  three): between 5 and 7 s of uptime, as Doom's first frames are
  published and the title music starts; none after, through the attract
  loop. About 10 ms of silence before the music has begun. M9's
  development captures have none; the development build's log slows core
  0's start, which is the likeliest difference. `counters.sh` counts the
  failure counters from the first read in play, as `soak_check.sh` does,
  and names any count from before.
- **OPL late halves come at level loads**, with the underruns: the release
  soak had one at each of its three level changes (a load at 2.3 min; into
  the intermission at 18.3 and the next level at 18.7), and none in play.
  Core 0 reading the card stalls the music's hand-over as it does the
  mix. M9's soaks never changed level. With the owner's word the soak
  allows them where it allows underruns: `counters.sh` and `soak_check.sh`
  judge both by their rise, within a read (or 10 s) of a level load or a
  change of game state; M9's soak at screen size 10 still passes.
- **Frame hashes are a run-time switch** (`-framehashes`), so the image
  that ships can be checked; the list is read over SWD from the PSRAM
  through the cached alias (hardware-notes.md §2.7).
- **The counter block can be read torn** (hardware-notes.md §2.7);
  `counters.sh --watch` takes each read twice until two agree.
- **M1's test pattern is gone**, at the owner's word: `build/pico-release`'s
  cache had kept `PICOCALC_DOOM_TEST_PATTERN=ON` from an earlier configure,
  so the image soaked drew it at boot until Doom's first frame (every
  other option at its default, checked against `CMakeLists.txt`).
  `build_release.sh` configures afresh and would not have; the pattern and
  its option are removed, so no build can.
- The root `LICENSE` was a copy of upstream's notice; it is now the GPL-2.0
  text, with upstream's notice in the release zip's `upstream/`.

### Not checked

- The release image from UF2 Loader and from BOOTSEL; the Plus 2.

## M9: performance pass and soak

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh`; development builds, Arm GCC
15.2.Rel1, Pico SDK 2.3.1; 300 MHz at 1.20 V; UART0 captured. The card
held the original 1.9 IWADs, M7's `picocalc.ini` (`iwad = doom.wad`,
`args = -skill 4`) and the `default.cfg` M8's persistence check left at
screen size 9 (Found). The levers were measured at 9, each against its
control at 9; the soak and the final gate were run again at 10, the
owner's size, on 2026-11-08 after `=` and F10 `y` put it back. Built, run
and read by Claude; the owner looked at the panel and listened on
2026-11-08 (Checks).

### Done when

| Check | Result |
|---|---|
| The 30-minute soak passes, on battery | **passed** at screen size 10, 2026-11-08, `scripts/soak.sh 30`, `soak_check.sh`: one boot; 1,905 heartbeats in play, every second; 4,852 keys taken, some in every minute; late refills, I²C errors, log lines dropped, keys lost, OPL late halves and dropped writes **0** throughout; **no underrun**; frames published, the game tic and the level tic rising in every heartbeat; **on battery throughout** (the charging bit clear in every heartbeat, the battery 100 % to 98 %) |
| The same at screen size 9 | **passed**, 2026-11-07, with the same counts (4,850 keys); battery for the first 1,180 s, then USB |
| Every lever recorded as kept or dropped with its number | below |

### The levers

Each against the last kept as its control, in one sitting.

| Lever | Control | Measured | Result |
|---|---|---|---|
| **Strategy C**, the copy into the pool by DMA (§7.3) | B | `-timedemo` demos 1–4: **57.2, 61.3, 65.6, 41.2** fps against 44.5, 48.0, 50.2, 33.8; demo 1 twice each, the same | **kept**, the default |
| **The drawers in SRAM** (M5's lever 3) | C | demo 1 57.2 → **58.9**, demo 4 41.2 → **43.1** | **kept**, on; no heap (the gap before `.bss`) |
| **Dirty rows** in the presenter (§7.4) | C + drawers | turning in E1M1 for 53 s: 98 rows a frame of 240; a present 20.4 → **10.0 ms**; no late halves or underruns | **kept**, on |
| The mixer and EMU8950 in SRAM (M6) | `MIX_SRAM` off | the OPL 23.7 % of core 1 against 46.6 %, and 6 late halves in 53 s without it; the heap 8.6 against 33.2 KB | **kept** |
| A smaller mix ring (§8.3) | 4 slots | 3 slots: heap +5.7 KB, effect latency 86 ms, 1 slot underrun in demo 1; 2 slots: +11.4 KB, 57 ms, 3 underruns | **not taken**: 4 stays the default, the owner hearing no problem with it (`PICOCALC_DOOM_MIX_RING`) |
| More of Doom in the gap | the profile | core 0 under C + drawers, demo 1, 20 s: `R_DrawColumn` 15.9 %, `R_DrawSpan` 9.2 % (both SRAM), then `P_MobjThinker` 6.0 %, `I_Pico_UpdateSound` 5.9 %, `R_RenderSegLoop` 5.5 %, the card's SPI 4.7 %; 61 % in flash. Nothing left worth the SRAM | **not built** |

The gate with every lever kept, audio on, at screen size 10 (2026-11-08,
against M6's at 10):

| | Demo 1 | Demo 2 | Demo 3 | Demo 4 |
|---|---:|---:|---:|---:|
| M6 | 41.6 | 43.8 | 45.8 | 32.1 |
| **M9** | **55.7** | **59.8** | **63.2** | **40.9** |

Every demo is above 35 again. At screen size 9 the same build gave demo
1 58.9 and demo 4 43.1.

| Strategy C | B | C |
|---|---:|---:|
| Core 0 per frame, copy | 6.18 ms | 0 (0.11 ms to start it) |
| The copy itself | | 4.4–4.7 ms by DMA, longest 6.4–8.0 |
| Core 0's waits for it | | 3–38 ms a second in all |
| Present, core 1, under `-timedemo` | 21.3 ms | 22.8 ms |
| Standing in E1M1 (150 s): drawn, presented a second | 51.4, 37.6 | 69.8, 37.5 |
| Core 0 busy, standing | 96.0 % | 87.2 % |

The "standing" runs were meant as Doom's attract loop at real speed, but
`-skill` in `picocalc.ini` starts a game at once, so they show the player
still at E1M1's start (Found).

### Checks

| Check | Result |
|---|---|
| Demo 1's frame hashes, C, against the host's at screen size 9 | **1,703 the same, 0 different**, 6 skipped as unstable |
| `PICOCALC_DOOM_COPY_CHECK`: Doom's frame unchanged between the copy's start and core 0's wait | **1,708 frames compared, 0 different** |
| Host tests | **12 of 12**, with `test_engine_screens` (sizes 8 and 9 failed before the fix) |
| `check_sources.sh` | passed |
| The panel and the audio, by the owner, 2026-11-08 | **no problems seen or heard**, with dirty rows on and the mix ring at 4 slots |
| The soak's check against a control | the menu held open for 10 s: **failed**, on those 10 heartbeats ("level tic did not rise") |

### The soak

`scripts/soak.sh 30`: E1M1 at skill 4 with `iddqd` and `idkfa`, a 12-second
round of keys (walk, turn, strafe, fire three weapons, use) over the UART
for 30 minutes.

| | Screen size 10, battery | Screen size 9 |
|---|---:|---:|
| Drawn, presented a second | 64.4, 58.9 | 66.2, 64.0 |
| Rows sent a frame (of 240) | 98.7 | 80.1 |
| Core 0 busy | 79.3 % | 75.4 % |
| Die | 21–24 °C | 23–25 °C |
| Stacks, core 0 and core 1 | 2,032, 1,148 B | 2,016, 1,148 B |
| The level's load | 662 ms | 661 ms |

The heap's high mark 4,512 of 8,608 B in both. Dirty rows send a
repeated frame in about 2 ms.

### Found

- **M8's persistence check left the card at screen size 9**: it pressed
  `-` to prove a change survives, and nothing put it back, so this
  milestone's lever runs were at 9 until the owner noticed. A test that
  changes saved state must restore it, and the record name the state
  each run met (emulator-lessons.md §12).
- **`-timedemo` had stopped since M8** with "Z_Free: freed a pointer
  without ZONEID": upstream's `V_DrawPatch()` moves patches at y ≥ 168
  down 40 rows for the status bar, also inside the view border's 208-row
  buffer, so at screen sizes 8 and 9 the border went past the buffer into
  the next zone block. M8's `default.cfg` held size 9. M7's
  build played; M8's with `-config none.cfg` played; the host engine,
  given each size and a heap check a frame, failed at 8 and 9. A
  `v_video.c` hunk moves patches on the screen only (design.md §7.1).
- **The "attract loop" was a still scene**: `args = -skill 4` makes Doom
  start a game (emulator-lessons.md §12).
- **The first dirty-rows build froze the panel**: the sniffer needs its
  own enable as well as the channel's bit (hardware-notes.md §4.7); the
  rows-sent figure gave it away.
- **The 8 KB-aligned PWM ring leaves 4,364 bytes before `.bss`** that
  nothing used; the drawers and dirty rows' table now sit there
  (hardware-notes.md §2.3, design.md §12.1).
- **Opening the probe's port again loses capture bytes**: a key script
  opened per round lost a heartbeat line and part of another in 3
  minutes; one opening for the run lost none in 33 (hardware-notes.md
  §2.7).
- `host_engine.sh compare`'s awk failed on a capture's reset noise outside
  the C locale.

**The code**: strategy C and its check in `video_shim.c`; dirty rows in
`core1.c`; Doom's clocks and state in the heartbeat; the level-load log;
`scripts/soak.sh`, `soak_check.sh`; `-hostscreenblocks` and
`-hostcheckzone` in the host engine, `HOST_ARGS` for `host_engine.sh
hashes`. **The patch** gains two hunks: `v_video.c`'s, and
`i_picosound.c`'s ring size. Switches: `PICOCALC_DOOM_VIDEO_BUF` (C),
`PICOCALC_DOOM_COPY_CHECK` (off), `PICOCALC_DOOM_DRAWERS_SRAM` (on),
`PICOCALC_DOOM_DIRTY_ROWS` (on), `PICOCALC_DOOM_MIX_RING` (4).

### Sizes

| | M8, development | M9, development | M9, release |
|---|---:|---:|---:|
| `.text` | 306,892 | 305,828 | 279,652 |
| `.rodata` | 144,932 | 145,188 | 136,556 |
| `.data` | 91,836 | 94,596 | 94,572 |
| `.bss` | 417,308 | 417,376 | 413,228 |
| SRAM left for the heap | 8,676 | **8,608** | 12,756 |

### Not checked

- The levers each against its control at screen size 10 (measured at 9;
  the final build's gate and soak at 10).
- The release build on the device, and the Plus 2 (M10).
- Demos 2 and 3 with the drawers; Doom II, Plutonia, TNT under `-timedemo`.
- The keys through the southbridge in a soak (UART keys bypass it; M10's
  release soak).

## M8: saves, configuration, exit, time, arguments

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh` (so no UF2 Loader: the exit's
power-off path); development build, Arm GCC 15.2.Rel1, Pico SDK 2.3.1;
300 MHz at 1.20 V; UART0 captured. The card held the original 1.9 IWADs
and M7's `picocalc.ini` (`iwad = doom.wad`, `args = -skill 4`). Before the
work the owner settled D10, D14 (Doom's Quit), D15 (the shell's arguments)
and a save directory for each game. Built, run over the UART and read by
Claude; the PicoCalc switched back on by the owner. The shell's checks were
run by the owner on a Pimoroni Pico Plus 2 (RP2350B rev 2) in a second
PicoCalc, started from the shell under UF2 Loader, with UART0 captured
there and no `picocalc.ini` on its card.

### Done when

| Check | Result |
|---|---|
| A save made in Doom loads after a power cycle | **passed**: a new game, F2 and slot 0 over the UART, saved to `/doom/.savegame/doom.wad/`; F10 `y` switched the PicoCalc off; the owner switched it on; F3 and Enter loaded it (`gameaction` 3, no error) |
| A change in Doom's options menu survives a quit and a power cycle | **passed**: the screen made one step smaller (`-`), F10 `y`, off, on: `screenblocks 9`, from 10, read back from `default.cfg` |
| F10 then `y` returns to the shell when started by it | **passed**, the owner, on the Plus 2, twice (Doom II and Doom; the at-exit list in 27 and 29 ms); Esc in the WAD list too, four times |
| F10 then `y` powers off when installed without the loader | **passed**: "exit: power off in 6 s, asked for 5 ms after the key"; the PicoCalc went off |
| A save after starting from the shell carries the shell's local date and time | **passed**: the owner saved in Doom started from the shell and read the file's date and time on the card, correct, and again in Doom II, in `doom2.wad/`, in TNT: Evilution, in `tnt.wad/`, and in Plutonia, in `plutonia.wad/`; the port had the shell's clock, 2026-11-07 18:29:45, UTC−5 |
| A save after a cold power-on without the shell carries the fallback | **not yet read**: the save above was made with "the shell's clock is not set"; its date on the card, FatFs's 2022-01-01, is to be read on a computer |
| `picocalc-doom -file <pwad> -warp 1 1` from the shell starts that map with that PWAD | **passed for the PWAD**, the owner: `-iwad doom.wad -file nerve.wad` from `/doom` gave `-iwad /doom/doom.wad -file /doom/nerve.wad`, and Doom added both (`nerve.wad` 117 lumps in 20 ms) and started. Then with `-warp`, the owner: it started the map asked for. Earlier, the words arrived (`-iwad /doom/doom2.wad` from `/doom` started Doom II); `-iwad doom2.wad` from `/home` failed ("IWAD file '/home/doom2.wad' not found!"), fixed since: a file not found from the user's directory goes as typed, for Doom to find in `/doom` (design.md §13.4); the same command from `/home` on the fixed build then worked, and a like command in the shell's DOS flavour (`/` switches), the owner |
| **Measured**: save write time | **85 ms** on the Plus 2 W, **59 ms** on the Plus 2, from the temporary file's name to the rename done |
| **Measured**: time from `y` to the shell's first screen | **about 3 s**, the owner by stopwatch, on the Plus 2: the at-exit list 27–29 ms, then the 2 s "Back to the shell." page, then UF2 Loader loading the shell, about 1 s. On the Plus 2 W without the loader, the power-off was asked 5 ms after the key |

### Found

- **Upstream never read or wrote `default.cfg`**: `m_config.c`'s
  `LoadDefaultCollection()` and `SaveDefaultCollection()` are under
  `#if ORIGCODE`. Three one-line hunks compile them back in; their
  `fprintf()`, `fscanf()` and `feof()` go to FatFs through
  `picocalc/config_stdio.h`, which also undoes the quiet build's
  `fprintf()`, under which the release would have written an empty file.
  Checked in the release binary: `SaveDefaultCollection()` calls the port's
  `fprintf` ten times (design.md §3.2, §5.3).
- **Four bindings do not survive `default.cfg`**: fire, use and the
  strafes are doomgeneric's codes 160–163, which the file holds as no scan
  code and reads back as unbound. `test_config_file` shows it; on the board
  each boot after the first quit logs the four put back by
  `I_InitJoystick()` (design.md §9.4).
- **The first new directory stopped the program**: FatFs's `f_mkdir()`
  asks for up to 32 KB and falls back on NULL, but the SDK's `malloc()`
  panics. Core 0 was found halted at `panic("Out of memory")` (DHCSR
  halted, CFSR and HFSR 0; PC and LR read through DCRSR, no halt sent).
  A wrap of `ff_memalloc()` returns NULL for a request that does not fit
  (design.md §10.1, hardware-notes.md §7.1).
- **A `-playdemo` could not end**: the owner's `/playdemo demo1` from the
  shell played, but quitting during it stopped with "W_GetNumForName:  not
  found". doomgeneric's `D_DoomMain()` returns, and the demo's name was a
  buffer local to it, so `G_CheckDemoStatus()` looked up stack since
  reused. Upstream's, and a demo's own end would hit it too. Wraps of
  `G_DeferedPlayDemo()` and `G_TimeDemo()` keep a copy (design.md §13.3).
  With them, the owner's demo returned to the shell at its end and when
  quit during it.
- **ENDOOM would have ended in `exit()`**: `D_Endoom()` is on the at-exit
  list and calls `exit(0)`; `I_Quit()` clears `show_endoom` first.
- `picocalc_shell.h` was the copy from before the shell's arguments; it is
  now the shell's of 2026-11-05.
- **A file given from the shell is not always the user's**: `-iwad
  doom2.wad` typed in `/home` means `/doom`'s IWAD, which Doom finds from
  its own directory. A file word is made a path from the user's directory
  only when it is there.
- **`-file` with an IWAD shows the WAD list**, rightly: it names an add-on,
  not the game. The owner had read it as choosing the game; `-iwad` does
  that. Kept as Doom's (the owner, 2026-11-07).
- Doom II's saves went to `/doom/.savegame/doom2.wad/`, Doom's to
  `doom.wad/`, each made on first use (with the `ff_memalloc()` wrap).

**The code**: `doom_files.c` (configuration stdio, the save directory, the
save timing, the FatFs allocation), `config_stdio.h`, `shell_args.c`,
`I_AtExit()` and `I_Quit()` in `dg_picocalc.c`, the "Doom has quit." page,
`PICOCALC_SHELL_TAKES_ARGS` in `main.c`. **The patch** gains `m_config.c`'s
three hunks. Host tests **11 of 11**, with `test_shell_args` and
`test_config_file`; `check_sources.sh` passed; the release build builds,
not run on the device.

### Sizes

| | M7, development | M8, development | M8, release |
|---|---:|---:|---:|
| `.text` | 302,824 | 306,892 | 280,780 |
| `.rodata` | 143,436 | 144,932 | 136,364 |
| `.data` | 91,836 | 91,836 | 91,812 |
| `.bss` | 416,832 | 417,308 | 413,156 |
| SRAM left for the heap | 9,152 | **8,676** | 12,828 |

In play after the load: heap 4,580 of 8,676 B; core 0's stack 1,960 of
3,072 B.

### Not checked

- The fallback date on the Plus 2 W's save, read on a computer.
- Saves in the IWADs not on the card (shareware, Freedoom, Chex Quest,
  HACX).
- The release build on the device.
- The "Doom has quit." page (no shell and no power-off).

## M7: controls and settings

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh`; development build, Arm GCC
15.2.Rel1, Pico SDK 2.3.1; 300 MHz at 1.20 V; UART0 captured. The card held
the original 1.9 IWADs (design.md §10.4). Before the work the owner
accepted D15 for `picocalc.ini`'s `args` and asked for Caps Lock to toggle
running. Built, run and read by Claude; played by the owner.

### Done when

| Check | Result |
|---|---|
| A full episode-1 level played comfortably on the keyboard | **passed**, the owner, The Ultimate Doom, 1,009 s over two boots |
| The bindings recorded as D12's settlement | **the bindings as they were** (design.md §9.2), always-run on with Caps Lock toggling it, the poll as it was: D12 **settled** |
| A settings file with a bad line boots with the line named and the rest applied | **passed**: `iwad = doom.wad`, `run = sometimes`, `args = -skill 4`; logged "line 2 not used, always, hold or off", Doom started at once from `iwad` with `-iwad doom.wad -skill 4`, running always; the owner saw the line named in the bottom band |
| Key-to-motion latency at the settled poll rate | a press to the next frame published a median of **30 ms** after its FIFO read (p90 46 ms, longest 93 ms; 575 presses), against M4's 39 / 52 / 109 ms; FIFO read to `DG_GetKey()` a median of 15 ms (p90 21 ms) |

### The settings and the run

| Check | Result |
|---|---|
| No `picocalc.ini` | "no /doom/picocalc.ini; the defaults", the WAD list, running always |
| The run setting | `I_InitJoystick()`, after `M_LoadDefaults()`: `joyb_speed` 31 and `key_speed` Shift (182), which the key map never sends. **No patch hunk** (design.md §9.4) |
| Caps Lock | over the UART and by the owner: each press toggles, releases ignored, "running off" / "running always" logged |
| Host tests | **9 of 9**, with `test_settings`: each setting in any case, eight bad lines named in order with why and the rest applied, comments, a BOM, CRLF, a last line with no end, the later line winning, `run = hold` without `run_key`, and the run key against every binding in `keymap_bindings`, Doom's own keys and every Shift-only character. Mutations to the parser (a Doom key dropped, the bindings check removed, error recording removed) each fail it |
| `check_sources.sh` | passed |
| Release build | builds; not run on the device |

### The owner's play

1,009 s over two boots. In play (902 s with 25 or more frames presented a
second): core 0 busy 93.6 % mean, 36.5 presented a second. The keyboard
polled 32–50 times a second in play, median 36, 4.88 ms a read; 32
releases held over to the next tic (§9.1); I²C errors 0; no unknown codes.
Stacks: core 0 2,016 of 3,072 B, core 1 1,084 of 2,048 B. **Heap 5,056 of
9,152 B.**

**The code**: `settings.c` (Quest's parser, Doom's keys, the file read
into the PSRAM and let go, the run variables and Caps Lock), `status.c`
(Quest's band, which waits for a page to close and the bands to clear
rather than dropping a message), `test_settings.c`; `DG_Init()` reads the
settings, `DG_StartScreen()` takes `iwad` and `args`, `I_InitJoystick()`
applies the run, `input_shim.c` passes Caps Lock on. `keys.sh` learns
`caps`. **The patch** is unchanged.

### Sizes

| | M6, development | M7, development | M7, release |
|---|---:|---:|---:|
| `.text` | 297,272 | 302,824 | 277,136 |
| `.rodata` | 141,508 | 143,436 | 135,436 |
| Image (`.bin`) | 531,696 | 539,176 | 505,464 |
| `.data` | 91,836 | 91,836 | 91,812 |
| `.bss` | 416,352 | 416,832 | 412,684 |
| SRAM left for the heap | 9,632 | **9,152** | about 13,300 |

### Found

- **The run setting needs no patch**: Doom calls the port's
  `I_InitJoystick()` after loading `default.cfg`, so the port's choice is
  set there and wins (design.md §9.4, §5.3).
- **The poll rate is fine as it is**: 36 a second in play, from one poll
  after each 20 ms present; the owner found the keys responsive, so
  polling between the present's rows is not needed (hardware-notes.md §6).
- **Key to frame is faster than M4's** (median 30 ms against 39 ms), with
  the OPL now on core 1 and the frames steadier at 35.
- **A 2 KB text buffer is too dear in SRAM** with 9.6 KB of heap left; the
  file is read into the PSRAM instead.

### Not checked

- `run = hold` with a `run_key`, and whether the MCU delivers a letter held
  with an arrow.
- Caps Lock's "Always running" and "Walking" in the band, on the panel (the
  toggle itself was logged).
- An `iwad =` naming an IWAD not on the card, and an `-iwad` inside `args`
  (in code, not run).
- The release build on a device; the Plus 2.
- Doom II, Final Doom, Freedoom with the settings file.

## M6: audio

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh`; development builds, Arm GCC
15.2.Rel1, Pico SDK 2.3.1; 300 MHz at 1.20 V; UART0 captured. The card held
the original 1.9 IWADs (design.md §10.4); scripted runs `-iwad doom.wad`
through `PICOCALC_DOOM_ARGS`. D11 settled by the owner before the work
(49,718 Hz, upstream's audio shape). Run and read by Claude; listened to
by the owner throughout, and the play test played by the owner.

### Done when

| Check | Result |
|---|---|
| Effects and OPL music by ear in E1M1 (The Ultimate Doom) and MAP01 (Doom II): pitch, speed, no carrier whine | **passed**, the owner, through the speakers and through the headphone jack |
| Underruns over ten minutes of play and across level loads | 1,050 s of play over two boots: **47 slots** underrun after the first was played (most at the level loads, each a slot of 1,421 frames), 2 late halves from core 1; no late refills |
| Mixing's share of core 0 with music | upstream unchanged: 22–28 %; as kept, with the OPL on core 1: **4–5 %** (the effects mixer); the OPL takes 16–35 % of core 1, in the LCD's waits (design.md §8.4) |
| The gate again, `-timedemo` with audio | demo 1 **41.6**, demo 2 **43.8**, demo 3 **45.8**, demo 4 **32.1** fps (M5 without audio: 46.4, 48.0, 50.2, 35.7). Demo 4 is below 35; **accepted by the owner**, 2026-11-07 (Found) |
| The late path forced (EL §6.4) | `PICOCALC_DOOM_AUDIO_STALL=30`: core 0's interrupts masked 30 ms every 5 s through music, 11 stalls: **2 late refills a stall**, no storm, no underrun, 96–99 refills a second throughout |

### The output

| Check | Result |
|---|---|
| PWM | GP26/27, slice 5, divider 1, **TOP 3016**, carrier 99,436 Hz, **49,718.26 Hz**, oversample 2; halves of 512 frames (10.3 ms), DMA 2 and 3, `DMA_IRQ_1` at `0x40` |
| The refill interrupt | 0.5–0.9 % of core 0, longest 68–112 µs, 96–99 refills a second, no late refills outside the forced runs |
| The refill path in SRAM | `cmake/check_sram.cmake` on every build: `audio_irq`, `gated_fill`, `audio_stage_fill`, upstream's `i2s_fill_audio`, and the SRAM `memcpy`/`memset` its object now calls; it fails the build on a flash target (checked against a function in flash) |
| Level | upstream's mix peaks at 900–1,600 of 32,768; gain ×8, saturated, "right" by the owner (×1 "very quiet") |
| Host tests | **8 of 8**, with `test_audio_stage`: the period, every 16-bit sample at four periods and three gains against a floating-point model, 4,000 halves across every slot boundary, underrun slots as silence (a one-word mutation fails it) |
| Host engine: demo 1 against the list | passed (the sound module is not in the host engine) |
| `check_sources.sh` | passed |

### The levers (design.md §8.3)

| demo 1 | `-timedemo` fps | Mix on core 0 | `-playdemo`: seconds below 35 drawn | presented a second |
|---|---:|---:|---:|---:|
| No audio (M5) | 46.4 | | | |
| Upstream's mix, renderer in flash | 35.9 | 25–28 % | | |
| EMU8950 and the mixer in SRAM | 37.3 | 19–25 % | 6 of 50 (31–34) | 37.6 |
| Upstream's Thumb renderer instead | 37.3 | 21–25 % | | |
| Vanilla's tic wait in `TryRunTics` | | 19 % | real drops to 27–32 | (frames capped at 35) |
| OPL on core 1, between presents | | 4 % | 0 of 50 | **29.9** |
| **OPL on core 1, in the LCD's waits (kept)** | **41.6** | **4–5 %** | **0 of 50** | **35.8** |

The profile (core 0, demo 1, 20 s, the renderer in SRAM): EMU8950's slot
functions about 18 %, `I_Pico_UpdateSound` 5.8 %, `audio_stage_fill`
0.8 %. Demo 4 at real speed (`-playdemo demo4`) drew 23–33 frames in 12 of
its 25 seconds with the OPL on core 0 *and* on core 1: its music is light
(mix 4–8 %), so those dips are the renderer's, M5's narrow pass.

### The owner's play

1,050 seconds over two boots (The Ultimate Doom, then Doom II), the
default development build. Core 0 busy 90.4 % mean, 50.5 frames drawn a
second, 12 seconds below 35; presented 35.6 a second, most seconds 34–38,
the few under 30 at loads and the WAD list. Stacks: core 0 2,004 of
3,072 B, core 1 1,084 of 2,048 B. **Heap 5,536 of 9,632 B.** I²C errors 0.
Log lines dropped 1,575.

**The code**: `audio_shim.c` (Quest's PWM ring, the conversion with the
gain, the counters, the gate on upstream's slots), `audio_stage.c` (the
portable staging, host-tested), `opl_core1.c` (the OPL on core 1 through
three link wraps), a heartbeat line each for the audio and the OPL, the
heap's extent in the heartbeat, `DG_SleepMs()` topping up the mixer, and
`cmake/check_sram.cmake`. `sram_sections.cmake` learns symbol
redefinition and `ALL`. **The patch** gains one hunk: `i_picosound.c`
read Doom's unsigned 8-bit sound lumps as signed (Found). Switches:
`PICOCALC_DOOM_OPL_CORE1` (on), `PICOCALC_DOOM_MIX_SRAM` (on),
`PICOCALC_DOOM_AUDIO_GAIN` (8), `PICOCALC_DOOM_OPL_ASM` (off),
`PICOCALC_DOOM_AUDIO_STALL` (0, scratch).

### Sizes

| | M5, development | M6, development | M6, release |
|---|---:|---:|---:|
| `.text` | 318,832 | 297,272 | 271,896 |
| `.rodata` | 142,732 | 141,508 | 134,052 |
| Image (`.bin`) | 528,872 | 531,696 | 498,840 |
| `.data` | 66,228 | 91,836 | 91,812 |
| `.bss` | 397,140 | 416,352 | 412,204 |
| SRAM left for the heap | 60,644 | **9,632** | about 13,800 |

`.data` grew by the mixer and EMU8950's renderer in SRAM (about 24 KB);
`.bss` by the PWM ring (8 KB), the staging buffer (5.7 KB), the OPL queue
(4 KB) and its chunk (1 KB).

### Found

- **Every effect played over a full-scale buzz**: upstream read Doom's DMX
  sound lumps, unsigned 8-bit, as signed. The owner heard it at once;
  three lumps read from the WAD showed it (emulator-lessons.md §6.5).
- **The mix is quiet**: peaks near −27 dBFS; ×8 in the conversion
  (hardware-notes.md §5.10).
- **EMU8950 costs 18 % of a core at 49,716 Hz on the M33**, from SRAM or
  flash, in C or its RP2040 assembly (hardware-notes.md §5.11).
- **The presenter waits for the LCD most of each row**, so core 1 hides
  the OPL there (hardware-notes.md §5.11). Between frames it did not fit.
- **The screen wipe never mixed**, so every level began with about 1.3 s
  of silence; mixing from `DG_SleepMs()`, which the wipe calls each frame,
  ended it (3 slots, then none).
- **Demo 4 falls below the gate with audio**, 32.1 fps: audio still
  costs core 0 about 6 % (the effects mixer and the interrupt), and
  demo 4's M5 margin was 2 %. The copy into the pool (6.2 ms a frame) is
  still the largest lever (M9, strategy C). **The owner accepted 32 fps for
  demo 4 with audio** (2026-11-07); the copy by DMA stays M9's.
- **The heap is down to 9.6 KB, with 5.5 KB used**: the SRAM code costs
  24 KB of it. A PWAD that needs more heap would fail; `MIX_SRAM` off
  gives back 24 KB, at a cost to core 0 (the effects mixer) and core 1
  (EMU8950) from flash not yet measured with the OPL on core 1.
- **A CMake option's new default again did not reach an existing build
  directory** (as in M3): `MIX_SRAM` stayed off in `build/pico` until set.
- **Captures begin with UART noise from the reset**, which makes `grep`
  treat a log as binary and print nothing; `grep -a` with `LC_ALL=C`.

### Not checked

- The release build on a device; the Plus 2.
- `doom1.wad`, Freedoom, Plutonia, TNT, a PWAD's music.
- Doom's sound and music volume settings in the menu, by ear (M8 saves them).
- The effect latency (up to 114 ms by construction, §8.3) by ear.
- A soak longer than 18 minutes (M9).
- Late halves from core 1 under the heaviest music with the heaviest
  scene: 2 in 18 minutes of play, 1 in demo 3.

## M5: first play, and the gate

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh`; development builds, Arm GCC
15.2.Rel1, Pico SDK 2.3.1; 300 MHz at 1.20 V, PSRAM 100 MHz, flash 75 MHz,
LCD SPI 75 MHz; UART0 captured. The card held the original 1.9 IWADs
(design.md §10.4); every run `-iwad doom.wad` (The Ultimate Doom) through
`PICOCALC_DOOM_ARGS`, one build directory each. Run and read by Claude.
The owner then played E1M1 to E1M3 on the keyboard, on the default build
(B and the colormaps), **without problems**, twice: the first session was
not captured, and its counters were read from the heartbeat at 1,850 s
since boot; the second, with `s_sound.c` quiet, was captured whole.

### The gate

`-timedemo` frames per second, Doom's own figure, core 1 presenting, no
audio. Each lever against the one before it as the control, in one
sitting:

| Demo (tics) | A: upstream | B: `I_VideoBuffer` in SRAM | B + COLORMAP in SRAM | B + COLORMAP + drawers in SRAM |
|---|---:|---:|---:|---:|
| demo1, E1M5 (1,710) | 28.0 | 44.2 | 46.4 | 48.2 |
| demo2, E2M2 (2,347) | | 47.2 | 48.0 | |
| demo3, E3M5 (3,863) | | 48.6 | 50.2 | |
| demo4, E4M2 (818) | | **34.4** | **35.7**, twice | 37.6 |

**Passed** with levers 1 and 2, against the proposed 35 fps for every demo
(design.md §16): B alone left demo 4 at 34.4, and the colormaps brought it
to 35.7, the same 802 realtics on a repeat run. Both are now on by
default. Lever 3 was measured for M9 (+4 % on demo 1, +5 % on demo 4)
and is a build switch, off; lever 4 (the copy by DMA) was not built. D7
stands: no higher clock. The pass is narrow: demo 4 clears 35 by 2 %.

| Per frame, demo 1 | A | B |
|---|---:|---:|
| Core 0's copy (`I_FinishUpdate`) | 9.71 ms | 6.08 ms |
| Core 0's publish (palette, margins, clean) | 0.45 ms | 0.68 ms |
| Present, core 1 | 19.6–19.9 ms, longest 21.2 | the same |

**The profile** (hardware-notes.md §2.7), core 0 under B, demo 1, 20 s,
78,775 PC samples: `memcpy` 27.8 % (the copy into the pool), `R_DrawColumn`
16.8 %, `R_DrawSpan` 10.2 %, `R_RenderSegLoop` 5.0 %, `P_MobjThinker` 2.9 %,
`xip_cache_maintain` 2.9 % (the publish's clean), the card's SPI 3.7 %;
93 % of samples in flash. The copy is the next cost: strategy C (§7.3) is
M9's first lever.

Core 0's busy share under `-timedemo` is 96–99 % by construction (Doom
never sleeps); its share in play is below.

### Frame hashes

| Build | Device's demo-1 list against the host's |
|---|---|
| A, twice | 1,702 the same, **0 different**, 7 skipped (with the remade list below) |
| B | 1,702 the same, 0 different, 7 skipped |
| B + COLORMAP | 1,702 the same, 0 different, 7 skipped |
| B + COLORMAP + drawers | 1,702 the same, 0 different, 7 skipped |

The first comparison found **three tics that differed, by one pixel each**
(475, 796, 1517), the same three with the same CRCs on two runs: not a
marginal PSRAM, which would vary. The tic 475 frame dumped over the UART
(`PICOCALC_DOOM_FRAME_DUMP_TIC`) differed from the host engine's at one
pixel, (296, 135); the host engine's column drawer, instrumented for that
pixel, showed an imp's column (`TROOB1`) whose first row maps to texture
row −1, which the drawer's `& 127` reads as row 127, 92 bytes past the
1,600-byte lump. That is vanilla Doom. Past the lump is the next zone
block, behind a 24-byte header on the device and a 40-byte one on the
host, and the list's fills and offsets never change it. The host engine
now pads every zone block with 128 bytes of the run's fill
(`-hostzonepad`); the remade list marks those three, tic 206, and three
more (334, 1275, 1376) as unstable, and no other hash changed. Not
`char`'s signedness, `-fwrapv`, the optimiser or floating point, each
ruled out on the host first; nor the sound module's lumps (a `-nosound`
device run differed the same way).

### The owner's play

| Check | First session (not captured) | Second session (captured, 1,192 s) |
|---|---|---|
| E1M1 to E1M3 played on the keyboard | **passed** (the owner) | **passed** (the owner) |
| Key events | 8,638 read; ring full 0, unknown 0, stray 0 | 5,379 read; ring full 0, unknown 0, stray 0 |
| I²C errors | 0 | 0 |
| Core 0's stack, painted | 2,004 of 3,072 B | 2,004 of 3,072 B |
| Present | 19.7 ms, longest 21.2 ms | 19.5 ms, longest 21.2 ms |
| Log lines dropped | **38,414**: upstream's sound traces | **18,766**, all in two seconds, at the two intermissions: upstream's `wi_stuff.c` (Found) |

Core 0's busy share in the second session, over the seconds of play
(frames drawn, the heartbeats of loads and intermissions left out):

| Level | Seconds | Busy, median | 90th percentile | Busiest second | Frames drawn a second |
|---|---:|---:|---:|---:|---:|
| E1M1 | 387 | 87.9 % | 94.2 % | 98.4 % | 57.7 |
| E1M2 | 537 | 85.4 % | 93.8 % | 98.3 % | 54.3 |
| E1M3 | 133 | **90.2 %** | 95.4 % | 98.3 % | 51.8 |

The share overstates the work: Chocolate Doom's `TryRunTics()` returns
when a tic boundary passes without a new tic, and `D_Display()` draws the
same tic again, so core 0 draws 52–58 frames a second instead of
sleeping. **Every second of play drew 35 frames or more**, but for one in
E1M1 (31, busy 98.4 %) and the seconds around the level loads. E1M3,
the heaviest by the median, still drew 51.8 a second.

### Other measurements

| Check | Result |
|---|---|
| Core 0's stack, painted | **2,116 of 3,072 B** over the four demos (M1–M4: 2,004, on Doom II's) |
| Core 1's stack | 900 of 2,048 B |
| Log lines dropped | 0 in every run |
| Frames dropped by core 1 | under `-timedemo` only, when core 0 publishes faster than a 19.7 ms present allows (224–756 a demo under B); none expected at Doom's 35 a second |
| Die temperature | 23–24 °C |
| Host tests | 7 of 7 |
| Host engine: demo 1 against the remade list | passed |
| `check_sources.sh` | passed |

**The code**: strategy B and lever 2 as shims, not patches:
`video_shim.c` wraps `I_InitGraphics()` and points `I_VideoBuffer` at a
static SRAM buffer (upstream's zone block is left allocated, so the zone's
layout stays the host's), and `render_sram.c` wraps `R_Init()`, copies
COLORMAP to SRAM and runs `R_InitLightTables()` again. Lever 3 renames
the drawers' sections to `.time_critical.*` after compiling
(`cmake/sram_sections.cmake`), so the SDK's linker script puts them in
SRAM. The frame hashes and the one-tic dump are in `video_shim.c`,
behind `PICOCALC_DOOM_FRAME_HASHES`; `host_engine.sh compare` reads a
capture. `scripts/profile.sh` from Quest; `scripts/timedemo.sh` reads a
run's result and heartbeats. CI builds the levers too. No change to the
patch.

### Sizes

| | M4, development | M5, development | M5, release |
|---|---:|---:|---:|
| `.text` | 318,616 | 318,832 | 292,648 |
| `.rodata` | 142,628 | 142,732 | 133,412 |
| Image (`.bin`) | 528,552 | 528,872 | 493,344 |
| `.data` | 66,228 | 66,228 | 66,204 |
| `.bss` | 311,636 | 397,140 | 392,996 |
| SRAM left for the heap | 146,148 | **60,644** | 64,812 |

`.bss` grew by the 76,800-byte frame and the 8,704-byte COLORMAP.

### Found

- **Drawing into SRAM is 1.58× drawing into PSRAM** (hardware-notes.md
  §9.3), and the copy into the pool is still a quarter of core 0 after it.
- **A device's frame differing from the host's by one pixel, every run**,
  was a read past a lump into the next zone block, which a host's fills
  cannot vary; padding the host's blocks can (emulator-lessons.md §11.2).
- **Upstream's `s_sound.c` prints a dozen lines for every sound and
  song** (`S_StartSound: ...`) in the development build: 2,033 in demo 3,
  none dropped under `-timedemo`, but **38,414 dropped in the owner's
  play**, more than UART0 at 115,200 baud carries. That file is now
  compiled quiet in the development build too (`-UFRANK_DOOM_QUIET
  -DFRANK_DOOM_QUIET=1` on it alone, no patch), as the release build
  always was; M9's soak needs zero dropped. The second session then
  dropped 12,750 and 6,016 lines, each in one second, at the two
  intermissions: `wi_stuff.c` traces every intermission frame. It is
  quiet the same way now.
- **A capture left running from an earlier session holds the probe's
  UART** and appends to its old file; `capture.sh` refuses the port. The
  runs above were read from that file.

### Not checked

- The log in play with `wi_stuff.c` quiet too: built, not flashed (M9's
  soak counts dropped lines).
- The release build on a device, and the Plus 2.
- Demos 2 and 3 with lever 3; lever 4.
- Doom II, Plutonia and TNT under `-timedemo`.
- The card's reads in play (the heartbeat has no card counter yet).

## M4: keyboard and the WAD list

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a PicoCalc,
flashed over SWD with `scripts/flash.sh`; development build, Arm GCC
15.2.Rel1, Pico SDK 2.3.1; UART0 captured with `scripts/capture.sh`. The
card held the original 1.9 IWADs (design.md §10.4). The owner accepted
D12 (the key map, for M4; M7 settles it), D13 (the WAD list) and the WAD
list's Esc of D14 before the work began. Driven over the UART
(`scripts/keys.sh`) by Claude, then played and checked on the keyboard
by the owner; the log read by Claude.

| Check | Result |
|---|---|
| The owner moves through the WAD list and Doom's menus, starts a new game and walks E1M1 | **passed** (the owner): `doom.wad` chosen from a list of four; over the UART, `doom2.wad` the same way and a new game started from Doom's menu |
| Every key, with each modifier, logged with its code; none lost | **passed**: 2,326 FIFO events in the owner's session, every one logged; **0 unknown, 0 stray, ring full 0, I²C errors 0, log dropped 0**; every code of the MCU's keymap seen but the power button's, which was not pressed |
| The Shift trap cannot strand an arrow | **passed**: six times on the keyboard Shift went down with Left or Right held, and the key map let go of it at once; the player did not keep turning (the owner). Over the UART, Left pressed, Shift pressed and released with no release of Left: Left let go with Shift's press |
| Alt with an arrow (§9.5) | **delivered** as the arrow with Alt held, all four; strafes (the owner) |
| Three keys at once (§9.5) | **delivered**: Up, Ctrl and Space held together, moving forward and firing (the owner); five keys at once at most (`` ` `` `/` Up Space Ctrl) |
| Alt with Space (§9.5) | the MCU's, as expected: no press of Space, `,` or `.` arrived under Alt; they change the backlights (the owner) |
| Space, `,` and `.` in a save's name | **typed** (the owner), the save cancelled |
| Esc in the WAD list | **powered off** (installed over SWD, so no loader): asked of the southbridge 5 ms after the key, the page shown; the PicoCalc on again normally |
| `I_Error()`'s page | **passed**: a `-timedemo demo1` build ended on it with "timed 1710 gametics in 2148 realtics"; a key restarted through the parked restart, and the second run timed the same |
| Key to event: FIFO read to `DG_GetKey()` | 2,326 events: median **17 ms**, p90 28 ms, p99 44 ms; up to 5.8 s while a level loads or wipes, when Doom does not poll |
| Key to frame: FIFO read to the first frame published after Doom took the press | 390 presses: median **39 ms**, p90 52 ms, longest 109 ms; the panel shows it one present (19 ms) later |
| Releases held over by `I_GetEvent()`'s one a tic | **54** in about eleven minutes of play |
| Polls | 50 a second at 4.82–4.92 ms each with no frames; **27–36, mostly 32–33, in play**: see Found |
| Core 1's stack, painted | **900 of 2,048 B** (M3: 788) |
| Core 0's stack, painted | 2,004 of 3,072 B, unchanged |
| Host tests | 7 of 7, with `test_keymap`, which fails when the Shift trap's release is taken out |
| Host engine: demo 1 against the frame-hash list | passed |
| `check_sources.sh` | passed |

**The code**: `kbd.c` from Quest (the ring, the 20 ms poll, UART key
injection), `keymap.c` (the MCU's pairs to Doom's codes, §9.2 and §9.3),
`input_shim.c` (`DG_GetKey()`, the key log, the counters), `exit.c` from
Quest, the WAD list and the exit pages in `page.c`, `core1.c`'s poll and
power-off request, and a second heartbeat line for the keyboard.
`scripts/keys.sh` from Quest. No change to the patch.

Decided in the code, within §9.2: Shift's alternates other than F6–F10
and Break are their base keys (PgUp is Up, Home is Tab, Insert is Enter,
or I under Alt), since Shift is nothing in play; and while Doom takes a
save's name (`saveStringEnter`), Space, `,` and `.` type themselves
instead of using and strafing.

### Sizes

| | M3, development | M4, development | M4, release |
|---|---:|---:|---:|
| `.text` | 313,776 | 318,616 | 292,456 |
| `.rodata` | 141,004 | 142,628 | 133,412 |
| Image (`.bin`) | 522,088 | 528,552 | 493,152 |
| `.data` | 66,228 | 66,228 | 66,204 |
| `.bss` | 310,176 | 311,636 | 307,492 |
| SRAM left for the heap | 147,608 | **146,148** | 150,316 |

### Found

- **The poll runs at the frame rate in play, not 50 Hz.** Core 1 presents
  a frame (19 ms) when one is ready and polls when 20 ms have passed, so
  at 32–34 frames a second each present is followed by one poll. The key
  map and the latencies above are with that rate. The poll rate is M7's to
  settle by play-testing (§16); polling between the present's rows is the
  way to 50 Hz if it is wanted (emulator-lessons.md §7.1).
- **Doom takes no keys during a level load or a wipe**: the Enter that
  started a game waited 0.9 s in the ring, and one in Doom's start-up
  5.8 s. Upstream's; nothing is lost.
- **`grep` takes the capture for binary** after a reset's noise bytes, and
  finds nothing in it; `grep -a` reads it all.
- The MCU findings (Alt with the arrows, five keys at once, Shift with
  Left and Right, held Insert) are in hardware-notes.md §6.3.

### Not checked

- **Alt with B** not logged (the owner reports the MCU shows the battery's
  state for it, and changes the backlights for Alt with Space, `,` and
  `.`); the power button's `0x91` not pressed.
- **Esc in the WAD list to the shell**: only the power-off path, since an
  SWD flash has no loader (M8 checks the shell's path with Doom's Quit).
- The pages for a card that does not mount, a card without `/doom`, a
  card without an IWAD, and the PSRAM's (from M3).
- The release build on a device; the Plus 2.
- `doom1.wad` and Freedoom.
- Run always on: Doom's run key is still upstream's Shift, which is never
  sent, so the player walks; `run = always` is M7's (§9.4).
- `picocalc.ini`'s `iwad` (M7).
- The device's frame-hash list and card reads in play (M5).

## M3: display and card

**2026-11-07**, on a Pimoroni Pico Plus 2 W (RP2350B rev 2, id
`7458DC82A89AAC12`) in a PicoCalc, flashed over SWD with
`scripts/flash.sh`; development build, Arm GCC 15.2.Rel1, Pico SDK 2.3.1;
UART0 captured with `scripts/capture.sh`. The card held the 2024
re-release's `doom.wad`, `doom2.wad`, `plutonia.wad` and `tnt.wad` on
FAT32, then the owner replaced them with the original 1.9 IWADs, the
reference data (design.md §10.4), for the last run. Run by Claude; the panel checked by the owner.

| Check | Result |
|---|---|
| Doom's title, attract demos and credit screens on the panel, centred, in the right colours, no stale rows in the margins after a demo's level ends (the owner's look) | **passed**, after the sky fix below; Doom II's loop |
| Card mount | 31–33 ms after an SWD reset (seven boots); **235 ms** after the card went in |
| `W_AddFile` per IWAD: open and directory read (directory in PSRAM) | `doom.wad` 2,308 lumps 38 ms; `doom2.wad` 2,928 lumps 27–40 ms; `plutonia.wad` 2,991 lumps 42 ms; `tnt.wad` 3,109 lumps 43 ms |
| Frames published, presented, dropped | 25–29 a second published, every one presented; **13,095 published, 0 dropped** over five boots |
| Present, a whole 320×240 frame by DMA from the uncached alias | **18.9–19.1 ms** mean, longest 21.1 ms |
| Core 0's copy (strategy A: `I_FinishUpdate()`'s PSRAM-to-PSRAM `memcpy`) | **9.5–9.7 ms** a frame, longest 11.3 ms |
| Core 0's publish (palette, margins, cache clean, pool) | 0.44–0.47 ms mean, longest 1.55 ms |
| Margins rule | buffers blacked 2–4 times a demo cycle, on leaving a level, as the rule says |
| Core 1's stack, painted | **788 of 2,048 B** (M1: 620; the presenter's DMA and the pages) |
| Core 0's stack, painted | 2,004 of 3,072 B, the same as M1, on all four IWADs' attract loops |
| No-card page, then a card pushed in | the page (the owner's look), then a restart through `picocalc_restart()` 0.5 s after insertion; Doom started; southbridge answering, 0 I²C errors |
| The four IWADs each started with `PICOCALC_DOOM_ARGS="-iwad X"` | passed, a minute of attract loop each |
| The original 1.9 IWADs, the card's four re-release ones replaced, each started the same way | passed, a minute each: `doom.wad` 2,306 lumps 38 ms, `doom2.wad` 2,919 lumps 40 ms, `plutonia.wad` 2,984 lumps 42 ms, `tnt.wad` 3,101 lumps 43 ms (sizes and lumps those of 1.9, design.md §10.4); present 18.9–19.1 ms, copy 9.5–9.7 ms, none dropped; right on the panel, all four, by the owner's look |
| Host tests | 6 of 6, with `test_framepool` (the pool, 200,000 random steps; the margins rule against a model of upstream's copy, which a broken rule fails) |
| Host engine: all 19 demos with the two new hunks | every one ends as in M2: the same tic, map, kills and items |
| `check_sources.sh` | passed |

Upstream's logging is on in the development build, so the frame rate is
not a measure (M5's gate is, with `-timedemo`).

**The code**: `framepool.c` (Quest's pool less its row stamps, with a
margins flag per buffer), `video_shim.c` (`DG_DrawFrame()` publishes,
`I_FinishUpdate()` link-wrapped only to time its copy), `core1.c`'s
presenter (Quest's DMA row read) and the one restart that parks core 1,
`page.c` and `font8x10.c` from Quest with Doom's pages, and
`dg_picocalc.c`'s card checks, IWAD choice and `I_Error()` page, with
`W_AddFile()` link-wrapped to time it. The frames live at the base of the
PSRAM, in upstream's unused 512 KB scratch region. `PICOCALC_DOOM_ARGS`
builds arguments in (§14.1). The test pattern is now off by default.

**The patch** gains two hunks: `w_wad.c`'s `ExtendLumpInfo()` takes the
WAD directory from the PSRAM (§12.1), and `r_plane.c` keeps the sky from
wrapping in the taller view (§7.7). The host engine gives `w_wad.c` a
`psram_malloc` on the C heap. Demo1's frame-hash list was remade: 281 of
1,709 tics differ (those with sky in view), tic 206 still the only
unstable one.

### Sizes

| | M1, development | M3, development | M3, release |
|---|---:|---:|---:|
| `.text` | 309,696 | 313,776 | 288,520 |
| `.rodata` | 138,092 | 141,004 | 132,260 |
| Image (`.bin`) | 515,008 | 522,088 | 488,064 |
| `.data` | 66,140 | 66,228 | 66,204 |
| `.bss` | 304,072 | 310,176 | 306,040 |
| SRAM left for the heap | 153,800 | **147,608** | 151,768 |

`.bss` grew by 6,104 bytes: the pool's palettes (1,536), two line
buffers and two index rows (1,920), the page's message and rows (1,600),
and the pool's and counters' state. The heap no
longer holds the WAD directory (64–89 KB), which is now in the PSRAM.

### Found

- **The sky drew a strip of the wrong rows along its top.** The owner saw
  it in the first demo: upstream's 208-row view is centred on row 104,
  the sky texture on row 100, so the top 4 rows (20 at the largest screen
  size) read above the 128-row texture and wrapped to its bottom. The host
  engine draws the same; it is upstream's, from its 240-row frame. Fixed
  in the patch (design.md §7.7). The frame hashes could not catch it: a
  list made from the code agrees with the code (emulator-lessons.md §5.9).
- **The copy costs what §7.3 estimated**: 9.6 ms a frame, a third of core
  0 at 35 frames a second. Strategy B is M5's first lever, as planned.
- **The present is the presenter's alone**: 19 ms, the same as with
  nothing on core 0 (hardware-notes.md §4.7), so core 1 presents up to
  about 50 frames a second, and Doom's cap is 35.
- **A CMake option's new default does not reach an existing build
  directory**: `build/pico` kept the test pattern on from its cache until
  configured with `-DPICOCALC_DOOM_TEST_PATTERN=OFF`.

### Not checked

- **Doom's menu over the title** (part of M3's done-when): no keyboard
  until M4, which checks it.
- The pages for a card that does not mount, a card without `/doom`, and a
  card without an IWAD; and the PSRAM's page. Only the no-card page was
  shown.
- `doom1.wad` and Freedoom.
- The release build on a device; the Plus 2.
- `I_Error()`'s page (shown by M4's error and `-timedemo` runs, M5).
- `picocalc.ini`'s `iwad` (M7, with the settings parser): M3 starts the
  build's `-iwad`, else the first of Doom's order, which is the only one
  on a card with one.
- The device's frame-hash list (M5).
- Card reads in play (M5's heartbeat).

## M2: host engine

**2026-11-06**, on the host only (macOS, Apple silicon, Apple clang 21),
CMake 4.4.5, upstream `153a950` with the patch; the owner's WADs in
`reference/` (design.md §10.4). Run by Claude with
`scripts/host_engine.sh`; not on a device. First run on the 2024
re-release IWADs; on **2026-11-07** every check below was run again on
the original 1.9 IWADs from the owner's purchase (The Ultimate Doom,
Doom II, Final Doom), which are now the reference, with the re-release's
`nerve.wad` and `masterlevels.wad` over Doom II 1.9. The figures are the
1.9 run's.

| Check | Result |
|---|---|
| `doom_host` builds from the patched upstream with a headless platform, upstream's `w_file_stdc.c` and `m_misc.c`, a null sound module, the firmware's engine definitions (shared, `cmake/engine-definitions.cmake`) and a 3 MB zone | passed |
| Every demo under `-timedemo` to its end: Doom 4, Doom II 3, Plutonia 3, TNT 3, No Rest for the Living 3, Master Levels 3 | **19 of 19** played to their ends (table below) |
| `nerve.wad` and `masterlevels.wad` with `-file` over Doom II: every map starts and runs a minute | passed, 9 and 21 maps |
| Frame-hash list of The Ultimate Doom 1.9's demo1 | 1,709 tics; **1,708 the same in four runs** with the zone filled 0x00, 0xFF, 0x55, 0xAA and moved 0, 16, 4,096, 48 bytes; tic 206 varies and is marked |
| The list reproduces against itself (`host_engine.sh compare`) | same 1,708, different 0, skipped 1 |
| Zone and directory, every map of every WAD to hand, 2,100 tics at skill 4 | 162 maps, no failure; design.md §12.1, §12.2 |
| Host tests | 5 of 5 passed, with `test_frame_hash` and `test_engine_demo1` |
| `test_engine_demo1` with another `doom.wad` (the re-release's) | skipped, naming both MD5s, not failed |
| The re-release IWADs, the same demos and maps (2026-11-06) | 19 of 19 demos to their ends, 162 maps; about 10 KB more of the zone per map |
| Firmware builds with the new patch hunk | passed; `.text` 518,208 (`arm-none-eabi-size`), `.bss` 306,392 |
| `check_sources.sh` | passed |

### Demos

All end with the player dead, as id's demos usually do; the kills and items
are those of real play, not of a player stuck against a wall. The fps is
the unoptimised host build's, at Doom's 35 Hz clock's resolution: scale
only. Peak and least free are of the 3 MB zone; reads in play are reads of
the WAD after the level's first frame (Doom skips its precache in a demo).

| WAD | Demo | Tics | Map | Kills | Items | fps | Zone peak not purgeable | Reads in play |
|---|---|---:|---|---:|---:|---:|---:|---:|
| Doom | 1 | 1,710 | E1M5 | 18 | 5 | 1,032 | 880,952 | 124 |
| Doom | 2 | 2,347 | E2M2 | 18 | 2 | 1,347 | 1,121,032 | 162 |
| Doom | 3 | 3,863 | E3M5 | 7 | 3 | 1,932 | 1,059,496 | 228 |
| Doom | 4 | 818 | E4M2 | 10 | 0 | 530 | 845,864 | 93 |
| Doom II | 1 | 1,205 | MAP11 | 8 | 0 | 767 | 993,344 | 187 |
| Doom II | 2 | 2,001 | MAP05 | 19 | 1 | 1,167 | 1,001,648 | 149 |
| Doom II | 3 | 4,471 | MAP26 | 35 | 24 | 1,932 | 1,017,672 | 258 |
| Plutonia | 1 | 7,403 | MAP17 | 63 | 24 | 2,334 | 1,189,864 | 531 |
| Plutonia | 2 | 3,483 | MAP10 | 15 | 1 | 1,625 | 1,089,968 | 283 |
| Plutonia | 3 | 5,662 | MAP12 | 49 | 12 | 2,154 | 1,201,632 | 548 |
| TNT | 1 | 4,531 | MAP01 | 34 | 3 | 2,087 | 1,005,808 | 257 |
| TNT | 2 | 3,653 | MAP12 | 21 | 0 | 1,705 | 1,434,320 | 275 |
| TNT | 3 | 3,433 | MAP13 | 33 | 0 | 1,646 | 1,350,784 | 218 |
| No Rest for the Living | 1 | 2,085 | MAP06 | 7 | 0 | 1,042 | 1,670,912 | 213 |
| No Rest for the Living | 2 | 2,001 | MAP01 | 14 | 10 | 1,094 | 1,975,824 | 246 |
| No Rest for the Living | 3 | 2,363 | MAP03 | 20 | 0 | 1,199 | 2,035,176 | 285 |
| Master Levels | 1 | 1,472 | MAP07 | 12 | 0 | 873 | 1,346,832 | 275 |
| Master Levels | 2 | 1,215 | MAP16 | 3 | 0 | 759 | 1,226,184 | 305 |
| Master Levels | 3 | 1,748 | MAP02 | 18 | 4 | 1,020 | 1,196,232 | 228 |

**The code**: `test/host/engine/dg_host.c` (the platform, the null sound
module, Doom's system calls, the zone sampled each frame, WAD reads
counted through upstream's `w_file_stdc.c` compiled with `fread` renamed),
`picocalc/frame_hash.c` (CRC-32 by nibble table, 64 bytes, and the
one-hash-a-tic rule; the device uses it from M3),
`scripts/host_engine.sh` (`demos`, `maps`, `hashes`, `compare`), and the
committed list `test/host/engine/doom-demo1.crc`.

**The patch** gains a hunk in `v_video.c`: `V_DrawPatch` centres and crops
a patch wider than the screen (design.md §7.6).

### Found

- **The re-release's full-screen pictures are 560 pixels wide and its
  `STBAR` 576**, and upstream skipped them all: no title, help,
  intermission or end pictures, and the status bar's background was never
  drawn, so it showed the zone's old contents (a floor flat, here), which
  differed from run to run and made every frame's hash differ. M1 had seen
  the title picture skipped. Fixed in the patch (design.md §7.6).
- **A sprite's top row can read past its patch.** With the status bar
  fixed, one tic of demo1 still varied: two pixels at the top of a
  medikit, read by the column drawer's `& 127` wrap from beyond the
  patch, out of the next zone block's header, whose pointers move from
  run to run. Vanilla Doom does the same. Marked unstable rather than
  patched.
- **No Rest for the Living's maps are far larger than Doom II's**: MAP04
  takes 2.33 MB of the zone's 3 MB, the most of any map; the IWADs' worst is
  TNT MAP20 at 1.74 MB.
- **The owner's purchase carries the original 1.9 data** beside the
  re-release's, in its DOS folder; its `DOOM.WAD` is The Ultimate Doom 1.9,
  identical to the owner's separate copy. Every picture in it is 320 wide.
  The hash list was made from the re-release first, and every hash
  differed once `reference/doom.wad` became the 1.9 WAD; the test now
  checks the WAD's MD5 against the list's header and skips on another.
- **Every demo ends the same on both releases**: the same tic, map,
  kills and items on the 1.9 IWADs as on the re-release's, whose levels
  and things are the same and whose pictures are not. Two data sets
  playing out alike is more evidence of sync, short of the exact
  comparison below.
- **Reads in play are mostly first sightings, not purges**: a larger zone
  barely cuts them in Plutonia's demos (531 to 477 at 4 MB). Sampling the
  lump cache each frame gave counts that rose with the zone's size; the
  read counter replaced it.
- **The shared engine definitions** moved to `cmake/engine-definitions.cmake`
  so the host engine cannot drift from the firmware's.

### Not checked

- **The demos compared exactly with stock doomgeneric.** Built (ozkl's,
  commit `e257a09`) with the same headless platform, but running a binary
  built from downloaded code was not allowed in this session; the owner
  decides. Until then sync rests on every demo playing to its end with the
  kills and items of real play.
- The device's frame-hash list (M3, with `frame_hash.c` on the device).
- Frames whose out-of-bounds reads land in another lump's data, the same
  on every host run but laid out differently on the device (24-byte zone
  headers, not 40): any would show as single tics differing in M3.
- The re-release's bunny scroll (`PFUB2`, 560 wide, drawn by
  `f_finale.c`), and the intermission and end pictures by eye.
- No PicoCalc hardware findings: M2 ran on the host only.

## M1: board bring-up

**2026-11-06**, on two boards in a PicoCalc: a Pimoroni Pico Plus 2 W
(RP2350B rev 2, id `7458DC82A89AAC12`), flashed over SWD with
`scripts/flash.sh`, and a Pimoroni Pico Plus 2 (RP2350B rev 2, id
`D4EE5DF59B9740A2`), started from the shell by UF2 Loader. Development
build, Arm GCC 15.2.Rel1, Pico SDK 2.3.1. UART0 captured with
`scripts/capture.sh`; the test pattern checked by the owner.

| Check | Result |
|---|---|
| The app partition under UF2 Loader (Plus 2) | 16,372 KB, 15,869 KB spare for a 502 KB image |
| Banner and heartbeats over UART0 | passed: 545 s (Plus 2 W, no IWAD), 426 s (Plus 2 W, Doom II demos), 65 s and more (Plus 2) |
| Clocks as configured | `clk_sys` and `clk_peri` 300 MHz, rail ~1,200 mV, both boards |
| Flash | QMI divider 4, RX delay 4, 75 MHz, both boards |
| PSRAM at 100 MHz (divider 3, RX delay 3) | round trip passed: 32,790 words in 17,140–17,205 µs (Plus 2 W), 18,286 µs (Plus 2) |
| LCD | SPI 75,000,000 Hz returned; init 424 ms; test pattern 19.4 ms |
| Test pattern: four corners, border, bars, ramp | right (the owner's look, Plus 2 W) |
| Southbridge | answers, version 0, a register read 4.84–4.89 ms, can power off; 0 I²C errors in every run |
| The shell's time, started from the shell (Plus 2) | 2026-11-06 21:56:03 UTC-5:00 both sides of the clock change; moved 2 ms in 1,970 µs of uptime; the shell's offset |
| Doom II starts, plays its title and demos unseen | passed, 25–30 frames a second with upstream's logging on (not a measure) |
| Core 0's stack, painted | **2,004 of 3,072 B** under Doom II's demos |
| Core 1's stack, painted | 620 of 2,048 B |
| Die temperature after a minute | 19 °C (Plus 2 W), 25 °C (Plus 2), uncalibrated |
| Log ring | 0 lines dropped in every run |
| Host tests | 3 of 3 passed, with `test_clock` |
| `check_sources.sh` | passed |

**The code**, from Quest by copy, rename, re-verify (D8): `board.c`
(less Quest's flash sweep), `log.c`, `lcd.c`, `southbridge.c`,
`psram_check.c`, `clock.c` (`get_fattime()` with it, ahead of M8, since
nothing writes yet), the shell's header; `core1.c` cut to bring-up, the
pattern, and the heartbeat's skeleton; `main.c` in §13.1's order. A
failure before Doom starts logs and stops core 0, not `panic()`, so core 1's
heartbeat carries on (M3 adds the pages).

### Sizes

| | M0, development | M1, development | M1, release |
|---|---:|---:|---:|
| `.text` | 298,356 | 309,696 | 284,328 |
| `.rodata` | 136,068 | 138,092 | 129,732 |
| Image (`.bin`) | 500,988 | 515,008 | 481,256 |
| `.data` | 65,468 | 66,140 | 66,116 |
| `.bss` | 299,660 | 304,072 | 299,932 |
| SRAM left for the heap | 158,884 | **153,800** | 157,964 |

The development build's 4 KB of extra `.bss` is the log ring.

### Found

- **Core 0 has 3,072 B of stack, not 4 KB**: EMU8950's `logsin_table`
  (1 KB) is in `.scratch_y`, at the bottom of core 0's SCRATCH_Y, and core
  1's stack is directly below it in SCRATCH_X. Doom II's demos reached
  2,004 B, so about 1 KB is left before core 0 writes over core 1's stack.
  §12.1's rule stands (move only if it comes near), with M3–M5 watching the
  heartbeat's figure on the commercial IWADs' big maps (HW §2.3).
- **The shell's offset is the one it starts a program with.** Flashed over
  SWD, the Plus 2 W found the record whole and set, with offset 0 and the
  right UTC; started from the shell, the Plus 2 had the shell's UTC-5:00.
- **The partition read asked for two words; it returns three.**
  `rom_get_partition_table_info` with one partition fills the flags word,
  then location and flags, and with a two-word buffer returns -13
  (`BOOTROM_ERROR_BUFFER_TOO_SMALL`): the banner said "no partition table"
  under UF2 Loader. Quest's copy has the same bug (its `partition_kb` read 0,
  Quest M7). Fixed to three words: under UF2 Loader on the Plus 2, twice,
  "app partition 16372 KB, 15869 KB spare", the shell's 16 MB loader
  build (HW §2.7).
- **`capture.sh` held the port after it was killed**: bash runs a trap
  only once the foreground `sleep` ends, so a timed capture kept reading,
  into its old file, for the rest of its time, and the next capture
  refused the port. The sleep now runs in the background without the
  port's descriptor, and the trap stops it; killed, the port is free at
  once.
- **The re-release `doom2.wad`'s title picture is 560 pixels wide**, and
  upstream's `V_DrawPatch` skips it as off-screen, every frame. For M3.
- **Upstream's development logging is heavy**: several lines per sound
  started. The log ring dropped none at 115200, but the frame rate above
  is not a measure of anything.

### Not checked

- Doom on the Plus 2 (its card has no IWAD); the test pattern on the Plus 2.
- The release build on a device.
- Core 0's stack on the other IWADs and their big maps (M3–M5).
- Frame rate, as a measure: M5's gate, with `-timedemo`, quiet logging.

## M0: skeleton, patch pipeline, CI

**2026-11-06**, on the host only (macOS, Apple silicon), Arm GCC 15.2.Rel1,
Pico SDK 2.3.1, CMake 4.4.5, upstream pinned at `153a950` (version 1.11).
Not on a device.

| Check | Result |
|---|---|
| Submodule `modules/frank-doom` at `153a950ba53f2dc7f1101702b57c1078d99575aa` | done |
| Configure copies and patches upstream into `frank-doom/`, with a stamp | passed |
| `scripts/check_sources.sh`: 95 compiled, 9 excluded, matching upstream | passed |
| `check_sources.sh` fails on an added engine file and on a changed filter line (a scratch copy of upstream) | passed |
| Host tests: the patch round trip, synthetic and on the real patch | 2 of 2 passed |
| Development build (`scripts/build.sh`) links | passed |
| Release build (`-DPICOCALC_DOOM_UART=OFF`) links | passed |
| A clean clone (no `--recurse-submodules`) builds a UF2 with `scripts/build.sh` alone | passed |
| CI on GitHub (pull request #1): host tests, source check, both firmware builds | passed |

**The patch** edits one file, `drivers/sdcard/sdcard.c`: `CLK_FAST`
overridable, built at 25 MHz, and `get_fattime()` compiled out for the
port's (§10.1, §13.2). The same hunks as Quest's; upstream's `sdcard.c` is
byte for byte the same as FRANK Quest's.

**The shims** (§5.3) are in `picocalc/`: `main.c` (no clock change yet),
`dg_picocalc.c` (upstream's PSRAM start-up, screen buffer, mount and
`I_*` calls, less HDMI, PS/2 and USB; `get_fattime()` returning FatFs's
fallback, 1 January 2022), and empty `video_shim.c`, `input_shim.c` and
`audio_shim.c`.

### Sizes, against design.md §3.4

Upstream's own build (§3.4) was for the same board, at its 252 MHz, with
HDMI, I²S, PS/2 and USB CDC; ours drops those and the platform file.

| | Upstream (§3.4) | M0, development | M0, release |
|---|---:|---:|---:|
| `.text` | 351,084 | 298,356 | 273,884 |
| `.rodata` | 136,452 | 136,068 | 127,820 |
| Image (`.bin`) | 563,140 | 500,988 | 468,268 |
| `.data` | 73,544 | 65,468 | 65,468 |
| `.bss` | 320,400 | 299,660 | 299,640 |
| SRAM left for the heap, `end` to `__HeapLimit` | 124,016 | **158,884** | 158,904 |

`.bss` fell by 20,740 bytes: the I²S `dma_buffers` (12,288) and HDMI, PS/2
and USB state, inside §12.1's estimate of 17–22 KB removed. The port's own
state (§12.1's +2 to +7 KB) arrives with M1–M6, so the heap figure will
fall. The largest SRAM users are as §3.4 says: `visplanes` 84,992,
`openings` 40,960, `mix_ring` 22,736, `ticdata` 20,480, `viewangletox`
16,384, `states` 27,076 in `.data`.

### Found

- **Doom's headers must come before `<stdbool.h>`** in the port's files:
  `doomtype.h` declares `false` and `true` as `boolean` enumerators, which
  the macros would turn into `0` and `1`. Upstream's platform file orders
  its includes that way.
- **Upstream's quiet build defines `printf(...)` as `(0)`**
  (`frank_doom_quiet_stdio.h`), which fails `-Wunused-value` in the port's
  files under `-Werror`; the port's code does not call `printf` where Doom's
  headers are included, and marks arguments used only by a compiled-out log.
- **Upstream's engine filters are applied to absolute paths**
  (`.*i_.*\.c$`), so building upstream from a directory whose path contains
  `i_` drops every engine file. Ours lists the files explicitly;
  `check_sources.sh` applies the filters to relative paths and checks that
  upstream's filter lines are unchanged.

### Not checked

- Anything on a device: M0 is not run.
- The SDK's default linker script and core 0's stack against Doom's
  recursion (M1, painted).
- CI's toolchain is 13.2.Rel1, as Quest's; the sizes above are 15.2.Rel1's.
  CI's `arm-none-eabi-size` (text + data, bss): development 496,720 and
  301,980; release 465,936 and 301,960, so 13.2's code is about 8 KB
  smaller.
