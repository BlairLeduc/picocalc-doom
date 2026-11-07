# PicoCalc Doom: design

A port of [FRANK DOOM](https://github.com/rh1tech/frank-doom), Mikhail
Matveev's Chocolate Doom/doomgeneric port for the RP2350, to the ClockworkPi
PicoCalc fitted with a Pimoroni Pico Plus 2 or Plus 2 W.

This document covers the shape of the port: what is taken from upstream and
how, what replaces FRANK's hardware, how the two cores share the work, the
memory budget, the milestones, and the constants and decisions still open.
It is written against two portable companions in this directory:

- `hardware-notes.md`: the authority on the PicoCalc. Cited as **HW §N**.
- `emulator-lessons.md`: what earlier PicoCalc ports taught. Cited as
  **EL §N**. Doom is not an emulator, but the port-side lessons (core
  ownership, frame handoff, audio, keyboard, measuring, process) apply
  unchanged.

It follows the method of `picocalc-quest` (the FRANK Quest port by the same
owner), whose design and code this port starts from (D8). That project is
cited as **Quest**, with its own section numbers where they help.

Plain **§N** points within this document. Dates are written in full.
Estimates are labelled as estimates; everything else is read from source,
from the companions' measurements, or from the upstream build measured on
2026-11-06 (§3.4).

**Status, 2026-11-06.** M0 done: the skeleton, the patch pipeline and CI;
the engine links, not yet run on a board (`milestones.md`). Upstream is
pinned at commit `153a950ba53f2dc7f1101702b57c1078d99575aa` (2026-05-12,
version 1.11).

**Contents**

1. [Goals and non-goals](#1-goals-and-non-goals) ·
2. [Decisions](#2-decisions) ·
3. [What upstream is](#3-what-upstream-is) ·
4. [Hardware mapping](#4-hardware-mapping) ·
5. [Source strategy: submodule, patch, shims](#5-source-strategy-submodule-patch-shims) ·
6. [Architecture](#6-architecture) ·
7. [Display](#7-display) ·
8. [Audio](#8-audio) ·
9. [Input](#9-input) ·
10. [Storage](#10-storage) ·
11. [Clocks, voltage, PSRAM and flash](#11-clocks-voltage-psram-and-flash) ·
12. [Memory budget](#12-memory-budget) ·
13. [Boot, time and exit](#13-boot-time-and-exit) ·
14. [Development workflow](#14-development-workflow) ·
15. [Testing](#15-testing) ·
16. [Milestones](#16-milestones) ·
17. [Unverified constants](#17-unverified-constants) ·
18. [Risks](#18-risks) ·
19. [Dropped and deferred](#19-dropped-and-deferred)

---

## 1. Goals and non-goals

**Goals**

- Run Doom, Doom II, Final Doom and compatible IWADs (FreeDoom, the
  shareware `doom1.wad`) on the PicoCalc, with the WADs on the SD card in
  upstream's `/doom` folder, so a card prepared for FRANK DOOM works here.
- Picture on the PicoCalc's LCD, centred vertically (D3); sound effects and
  OPL music from the PicoCalc's PWM audio; play from the PicoCalc's own
  keyboard.
- Carry upstream as an unmodified submodule plus one patch, as Quest does,
  so following upstream is a patch refresh rather than a merge (D1).
- Work with the PicoCalc shell: leave to it, and take the date and time
  from the clock it keeps (D4).
- One release image, for the Pimoroni Pico Plus 2, which also runs on a
  Plus 2 W (D2).

**Non-goals**

- Any board without PSRAM. Upstream keeps the zone, the screen and the
  music in the 8 MB PSRAM and will not run without it (§3). Pico, Pico 2
  and Pico 2 W are out.
- Heretic, Hexen and Strife. Upstream names their IWADs in a list, but its
  engine is doomgeneric's, which is Doom only.
- Multiplayer. Nothing uses the Plus 2 W's radio (D2).
- A mouse, a joystick, USB host or PS/2. The PicoCalc has none wired.
- Engine changes beyond what the PicoCalc needs. Gameplay is upstream's,
  and so Chocolate Doom's.

---

## 2. Decisions

Numbered and dated (EL §14.7). **Settled** decisions are the owner's and
are not reopened without the owner. **Proposed** ones are this document's
recommendation and need the owner's word before the milestone that depends
on them.

| # | Date | Decision | State |
|---|---|---|---|
| D1 | 2026-11-06 | Port by patching upstream source, kept as a submodule, as `picocalc-quest` does. | Settled |
| D2 | 2026-11-06 | Target the Pimoroni Pico Plus 2 and Plus 2 W (RP2350B, 8 MB PSRAM). Ship one image, built for the Plus 2; no Wi-Fi. | Settled |
| D3 | 2026-11-06 | Centre the picture vertically on the 320×320 panel: upstream's 320×240 frame at y = 40, with its 320×200 screens centred inside it (§7.1). | Settled |
| D4 | 2026-11-06 | Return to the shell through `picocalc_shell.h`, and take the date and time from the clock the shell keeps (§13). | Settled |
| D5 | 2026-11-06 | Develop with a Raspberry Pi Debug Probe on SWD and UART0, GP2 (TX) and GP3 (RX), 115200 8-N-1, as the shell and Quest do. GP4/GP5 are left to `i2c0` for the shell's optional DS3231 (§4, §14.1). | Settled |
| D6 | 2026-11-06 | Whatever is learned about the PicoCalc goes into `hardware-notes.md` as it is learned, written to stand alone (§16). | Settled |
| D7 | 2026-11-06 | Ship at **300 MHz**, core rail 1.20 V, not upstream's 252 MHz at 1.50 V (§11.1). | Settled 2026-11-06 for bring-up; confirmed or revised at the M5 gate |
| D8 | 2026-11-06 | Start from `picocalc-quest`'s PicoCalc layer and tools, by copy, rename, re-verify (EL §14.6); it ports the same upstream author's sibling project, with the same HDMI, I²S, PS/2 and SD code. | Settled 2026-11-06 |
| D9 | 2026-11-06 | Licence the port's own code **GPL-2.0-or-later**, and relicense the owner's copied Quest code to match (§5.5). Upstream ships GPL-2.0's text, and some of its own files carry no notice, so GPL-3.0-only code could not be combined with them. | Settled 2026-11-06 |
| D10 | 2026-11-06 | No internal flash writes. Settings, Doom's configuration and saves live on the card (HW §7.1, EL §8.7). | Proposed |
| D11 | 2026-11-06 | Keep upstream's audio shape (mixing in the game loop, the IRQ only copying), and run the PWM at **49,718 Hz**, the OPL emulator's own rate, so no rate converter is needed (§8). | Proposed |
| D12 | 2026-11-06 | The key map of §9.2: arrows move and turn, Ctrl fires, Space uses, Alt and `,` `.` strafe, running is always on, **Shift is bound to nothing in play**. | Proposed; settled by the owner's play-testing in M7 |
| D13 | 2026-11-06 | The port draws its own WAD list, and boots straight into the game when the card has one IWAD or `picocalc.ini` names one (§13.1). | Proposed |
| D14 | 2026-11-06 | Doom's own Quit (F10, or the menu) leaves to the shell, or powers off without the loader, after Doom saves its configuration; it no longer reboots (§13.3). | Proposed |
| D15 | 2026-11-06 | Take Doom command-line arguments from the shell (`picocalc-doom -file sigil.wad -warp 1 1`) and from `picocalc.ini`'s `args` (§13.4). | Proposed |
| D16 | 2026-11-06 | The project is a git repository on GitHub, `BlairLeduc/picocalc-doom`, with upstream as a submodule; each milestone lands as a pull request with CI (EL §14.7). | Settled |

---

## 3. What upstream is

FRANK DOOM (Mikhail Matveev) is doomgeneric (ozkl's portable cut of
Chocolate Doom) with Graham Sanderson's OPL and sound code from
`rp2040-doom`, EMU8950 for the OPL2, and an RP2350 platform layer for the
FRANK and Murmulator boards. Everything is vendored; there are no
submodules. The tree is 6.5 MB and 250 C, C++ and header files; the engine
is 95 C files, about 62,800 lines.

### 3.1 FRANK-specific code

| Path | Lines | What it does |
|---|---:|---|
| `src/main.c` | 75 | Voltage, flash QMI timing, clock (252/378/504 MHz), USB-CDC stdio, then `doomgeneric_Create()` and the tick loop |
| `src/doomgeneric_rp2350.c` | 1,003 | **The platform**: every `DG_*` function, the start screen (an IWAD menu in a 5×7 font), PSRAM init, HDMI init, SD mount, PS/2 and USB start-up, and `I_Error`, `I_Quit`, `I_AtExit`, `I_ZoneBase` and the other `I_*` system calls |
| `src/board_config.h` | — | M1/M2 pin layouts, clock and voltage defaults, PSRAM pin by package |
| `drivers/HDMI.c` | 700 | PIO HDMI scan-out of an 8-bit 320×240 buffer, palette by `graphics_set_palette()` |
| `src/pico/audio.c` | 423 | I²S over PIO, two chained DMA channels (10, 11) on `DMA_IRQ_1` at priority `0x80`, a fill callback from the IRQ |
| `src/pico/i_picosound.c` | 672 | Doom's sound module: 16 channels, ADPCM or PCM effects resampled with a low-pass, the music generator, a 4-slot mix ring |
| `src/pico/i_oplmusic.c`, `src/midifile.c`, `src/opl/` | ~5,300 | MUS/MIDI to OPL; EMU8950 with `rp2040-doom`'s slot renderer (C++ and Arm assembly) |
| `drivers/psram_init.c` | 78 | QMI CS1 set-up; timing derived from `clk_sys` when called |
| `drivers/psram_allocator.c` | 174 | A bump allocator over PSRAM, with no free: 512 KB scratch, 4 MB permanent, 4 MB temporary for music |
| `src/doomgeneric_fatfs/` | 595 | WAD reads through FatFs; `fopen` and friends link-wrapped to FatFs; `M_*` file helpers |
| `drivers/sdcard/` | — | Elehobica's FatFs disk layer; PIO SPI or hardware SPI. **The same `sdcard.c` as FRANK Quest's**, byte for byte |
| `drivers/ps2kbd`, `ps2mouse`, `usbhid` | — | PS/2 over PIO; TinyUSB host HID |

### 3.2 How it runs

- **One core.** Nothing launches core 1. The engine, the mixer, HDMI's DMA
  IRQ and input all run on core 0.
- **Video.** Doom draws into `I_VideoBuffer`, 320×240 bytes `Z_Malloc`'d
  from the zone, so in PSRAM. `I_FinishUpdate()` copies it into
  `DG_ScreenBuffer` (also PSRAM), which HDMI scans continuously, then
  `DG_DrawFrame()` writes all 256 palette entries if Doom changed the
  palette.
- **A 240-row frame in play.** Upstream sets `SCREENHEIGHT` to 240. At the
  default screen size (10) the 3D view is 208 rows, not vanilla's 168, and
  `V_CopyRect()` and `V_DrawPatch()` add 40 to any y at or below row 168,
  so the 32-row status bar lands at rows 208–239. Screen size 11 gives a
  240-row view. Outside a level (title, menus over the title, intermission,
  finale) Doom draws its usual 320×200 and `I_FinishUpdate()` copies those
  200 rows to rows 20–219, blacking rows 0–19 and 220–239 **once**, on the
  change from a level.
- **Audio.** The output rate is **49,716 Hz**, EMU8950's native rate
  (3.58 MHz / 72; built with `EMU8950_NO_RATECONV`). The game loop's
  `I_UpdateSound()` mixes music and 16 effect channels into a ring of four
  slots, each one tic of sound (1,421 frames), as far ahead as the ring
  allows; the DMA IRQ copies one slot into the I²S buffer, or silence when
  the ring is empty. Nothing in the IRQ mixes.
- **Input.** `DG_GetKey()` returns Doom key codes directly
  (doomgeneric's `TranslateKey()` is the identity). `I_GetEvent()` stops
  after the first key release in each call, so at most one release is
  posted a tic.
- **Storage.** WADs are read lump by lump from the card through FatFs, and
  cached in the zone. The working directory is `/doom`; Doom's
  configuration files are written there, and saves go to
  `/doom/.savegame/` for every IWAD alike.
- **Memory.** `I_ZoneBase()` takes 3 MB of PSRAM for the zone (2 MB if
  that fails). The WAD directory (`lumpinfo`) is `calloc`'d from the SDK's
  heap, **in SRAM** (§12.1). MIDI conversion uses the 4 MB temporary half.
- **Clock.** 252 MHz at 1.50 V by default (the HDMI pixel clock wants a
  multiple of 25.2 MHz), 378 MHz at 1.60 V or 504 MHz at 1.65 V by option,
  with `vreg_disable_voltage_limit()`. PSRAM capped at 133 MHz, flash at
  88 MHz.
- **No exit.** `I_Quit()` reboots through the watchdog. `I_AtExit()` does
  nothing, so Doom's `M_SaveDefaults()` never runs: a change made in Doom's
  menus is lost at the next power-on. `I_Error()` prints and spins.
- **No clock.** The SD driver's `get_fattime()` returns 0, so every file
  written is dated 1980. Doom itself never asks the time of day.

### 3.3 What the port keeps, replaces and drops

| Upstream | Port |
|---|---|
| The engine (`src/doomgeneric/doomgeneric/`), less the files upstream already excludes | Kept, compiled as upstream compiles it |
| Sound module, OPL, MIDI (`src/pico/i_*.c`, `src/opl/`, `src/midifile.c`) | Kept |
| FatFs, the SD driver, the FatFs glue (`src/doomgeneric_fatfs/`) | Kept; the SD driver patched as Quest's |
| PSRAM init and allocator | Kept, with a verify step added (§11.3) |
| `src/main.c`, `src/doomgeneric_rp2350.c` | **Replaced** by `picocalc/` (§5.3) |
| `src/pico/audio.c` (I²S) | **Replaced** by a PWM shim with the same API (§8) |
| `drivers/HDMI.c`, PS/2, USB HID | Not compiled |

### 3.4 Upstream, built (2026-11-06)

Built unchanged with its own CMake for `PICO_BOARD=pimoroni_pico_plus2_rp2350`
at its default 252 MHz, Pico SDK 2.3.1, Arm GCC 15.2.Rel1, M1 layout, PS/2
input, in the scratchpad; not run on a board.

| | Bytes |
|---|---:|
| `.text` | 351,084 |
| `.rodata` | 136,452 |
| Image (`.bin`) | 563,140 |
| `.data` | 73,544 |
| `.bss` | 320,400 |
| SRAM left for the heap, to the top of RAM | 124,016 |

The largest SRAM users are the renderer's working set (`visplanes` 84,992,
`openings` 40,960, `mix_ring` 22,736, `ticdata` 20,480, `opl_temp_buffer`
16,384, `viewangletox` 16,384, `drawsegs` 12,288, the I²S `dma_buffers`
12,288) and, in `.data`, Doom's mutable tables (`states` 27,076,
`mobjinfo` 12,604). About 75 KB of `.data` is not code; no renderer
function is placed in SRAM.

The image is small: 563 KB, against Quest's 3.8 MB. It fits even the stock
UF2 Loader's 4,088 KB partition (HW §2.7), and makes SRAM placement of hot
code cheap to try (§16, M9).

---

## 4. Hardware mapping

| Function | FRANK | PicoCalc + Plus 2 | Port work |
|---|---|---|---|
| Display | HDMI over PIO, 320×240 CLUT8 scanned continuously | ST7365P-class 320×320 LCD on `spi1`, GP10–15, RGB565, written on demand (HW §4) | Frame pool and presenter on core 1 (§7) |
| Audio | I²S DAC over PIO, 49,716 Hz | PWM slice 5, GP26/27, RC filter, no DAC (HW §5) | PWM shim behind upstream's I²S API (§8) |
| Keyboard | PS/2 over PIO, or USB HID host | Southbridge MCU at `i2c1` 0x1F, GP6/7, 10 kHz, translated key events (HW §6) | Input layer on core 1, key map on core 0 (§9) |
| Mouse | PS/2 or USB | None | None: Doom plays from the keyboard |
| SD card | `spi0`, M1 or M2 pins | `spi0`: MISO GP16, CS GP17, SCK GP18, MOSI GP19; detect GP22 (HW §7.1) | Pin defines and one clock constant (§10) |
| PSRAM | CS GP47 on RP2350B | CS GP47 on RP2350B (HW §2.4) | A verify step (§11.3) |
| Console | USB CDC, or UART | UART0 on GP2 (TX) / GP3 (RX) through the Debug Probe (D5) | stdio through a log ring (§14.2) |
| Real-time clock | — | The shell's clock in the POWMAN timer, kept from a DS3231 on `i2c0`, GP4/GP5 | Read the POWMAN timer only (§13.2); never touch GP4/GP5 |
| Battery, backlight, power off | — | Southbridge registers (HW §6) | Power off on exit without the loader (§13.3) |

GP numbers are the same on the Plus 2 and Plus 2 W (HW §1.1). The port must
not touch GP23–25 or GP29: they are the radio's on a Plus 2 W, and the
Plus 2's board header puts its status LED on GP25 (HW §2.5). The image
never lights the LED, and reads the battery from the southbridge, not VSYS.
GP0/GP1 carry the PicoCalc's USB-C serial by default and are left alone.

---

## 5. Source strategy: submodule, patch, shims

### 5.1 Layout

Quest's, with the names changed:

```
picocalc-doom/
  CMakeLists.txt                 the build root; ours
  cmake/upstream.cmake           configure-time copy and patch, with a stamp
  cmake/upstream-sources.cmake   upstream files we compile, as lists (§5.4)
  modules/frank-doom/            submodule, pinned; never edited
  patches/frank-doom.patch       every change to an upstream file, one patch
  frank-doom/                    gitignored: upstream copied and patched by the build
  working/                       gitignored: upstream files being edited, at their upstream paths
  reference/                     gitignored: the owner's WADs for the host engine (doom.wad); never committed
  picocalc/                      the port's own code (§5.3)
  test/host/                     host tests for picocalc/, and the host engine (§15)
  scripts/                       apply_patch.sh, create_patch.sh, check_sources.sh,
                                 build.sh, build_release.sh, flash.sh, capture.sh,
                                 keys.sh, counters.sh, profile.sh, gate.sh, soak.sh
  docs/                          hardware-notes.md, emulator-lessons.md, design.md,
                                 milestones.md (from M0)
  CLAUDE.md                      agent instructions (from M0, EL §14.1)
```

### 5.2 The patch pipeline

Quest's, unchanged (Quest §5.2):

1. **Patch at configure time.** `cmake/upstream.cmake` copies
   `modules/frank-doom/` to `frank-doom/` and applies the patch when the
   submodule's commit, the patch or `apply_patch.sh` has changed since the
   last copy (a stamp holds all three hashes), and lists them in
   `CMAKE_CONFIGURE_DEPENDS`.
2. **`create_patch.sh` walks `working/`.** Every file there is diffed
   against the same path in the submodule. Adding a file to the patch is
   copying it into `working/`.
3. **No new files in the patch.** New code goes in `picocalc/`. The patch
   holds edits to upstream files only.

Following upstream: bump the submodule, `apply_patch.sh`, fix rejects in
`working/`, `create_patch.sh`, `check_sources.sh` (§5.4), build.

### 5.3 Shims before patches

doomgeneric exists to be ported: a platform implements `DG_Init`,
`DG_DrawFrame`, `DG_SleepMs`, `DG_GetTicksMs`, `DG_GetKey` and
`DG_SetWindowTitle`, and the engine calls nothing else of it. Upstream's
whole platform is one file, `doomgeneric_rp2350.c`, so the port **does not
compile it** and provides those functions, and the `I_*` system calls the
same file defined, in `picocalc/`. Most of the PicoCalc work needs no patch
at all.

| Upstream calls | Upstream implementation (not compiled) | PicoCalc implementation |
|---|---|---|
| `main()` | `src/main.c` | `picocalc/main.c`: clocks, log, core 1, checks, then `doomgeneric_Create()` and the tick loop (§13.1) |
| `DG_Init`, `DG_StartScreen` | `doomgeneric_rp2350.c` | `picocalc/dg_picocalc.c`: PSRAM verify, mount, the WAD list (§13.1) |
| `DG_DrawFrame`, `DG_ScreenBuffer` | `doomgeneric_rp2350.c` | `picocalc/video_shim.c`: publish the frame and its palette to the pool, claim the next (§7.2) |
| `DG_GetKey` | `doomgeneric_rp2350.c` | `picocalc/input_shim.c`: pop the key map's queue (§9) |
| `DG_GetTicksMs`, `DG_SleepMs` | `doomgeneric_rp2350.c` | `picocalc/dg_picocalc.c`: the system timer; the sleep timed for the heartbeat's busy share (§14.3) |
| `I_Error`, `I_Quit`, `I_AtExit`, `I_ZoneBase`, `I_Realloc`, `I_Init`, `I_Print*`, `I_Tactile`, `I_GetMemoryValue`, `I_ConsoleStdout`, `I_InitJoystick`, `I_BindJoystickVariables` | `doomgeneric_rp2350.c` | `picocalc/dg_picocalc.c` and `picocalc/fatal.c`: an error page; exit through the at-exit list (§13) |
| `i2s_init`, `i2s_deinit`, `i2s_set_fill_callback`, `i2s_start`, … | `src/pico/audio.c` | `picocalc/audio_shim.c` over the PWM ring (§8) |

The patch, **expected**, to be confirmed as each milestone reaches it:

| File | Change | Milestone |
|---|---|---|
| `drivers/sdcard/sdcard.c` | `CLK_FAST` overridable, set to 25 MHz; its `get_fattime()` compiled out for the PicoCalc clock's (§10.1, §13.2). The same hunks as Quest's, on the same file | M0 |
| `src/doomgeneric/doomgeneric/i_video.c` | `I_VideoBuffer` from the port's SRAM buffer rather than the zone, behind a build switch (§7.3). Later, perhaps, `I_FinishUpdate()`'s copy handed to the port's DMA (§7.3) | M5, M9 |
| `src/doomgeneric/doomgeneric/w_wad.c` | `ExtendLumpInfo()` allocates the WAD directory from PSRAM, not the SRAM heap (§12.1) | M3 |
| `src/doomgeneric/doomgeneric/d_main.c` | After Doom loads its configuration, `picocalc.ini`'s run choice is applied: always on by default (§9.4) | M7 |
| `src/doomgeneric/doomgeneric/r_data.c` | Perhaps: the colormaps copied into SRAM (§16, M5's levers) | M5 or M9 |

Each patched hunk carries a comment citing the section of this document
that requires it (EL §14.1).

### 5.4 Which upstream files are built

Upstream's `CMakeLists.txt` is not used (Quest §5.4): it is a whole
project, with its own board, clock, pins and drivers. Upstream gathers the
engine with `file(GLOB src/doomgeneric/doomgeneric/*.c)`, drops every
`doomgeneric_*.c`, every `i_*.c`, `w_file_stdc.c` and `m_misc.c`, then adds
back `i_video.c`, `i_timer.c`, `i_input.c`, `i_sound.c`, `i_endoom.c`,
`doomgeneric.c` and `wi_stuff.c`, its sound, OPL and MIDI files, and its
FatFs glue. `cmake/upstream-sources.cmake` holds the **result** as explicit
lists, less `src/main.c`, `src/doomgeneric_rp2350.c`, `src/pico/audio.c`,
`drivers/HDMI.c` and the PS/2, USB HID and PIO-SPI drivers, and with
upstream's compile definitions (`CMAP256`, `DOOMGENERIC_RESX=320`,
`DOOMGENERIC_RESY=240`, `FEATURE_SOUND`, `USE_PICO_SOUND`,
`USE_OPL_MUSIC=1`, `USE_EMU8950_OPL=1`, `NUM_SOUND_CHANNELS=16`, the
`NO_USE_*` and `EMU8950_*` set, `PICO_ON_DEVICE=1`) and its `fopen` …
`rename` link wraps.

`scripts/check_sources.sh` reproduces upstream's glob and filters on the
submodule and diffs the result against our lists, so a bump that adds or
removes an engine file is reported. CI runs it.

Upstream uses the SDK's default linker script, and so does the port, until
a measurement says otherwise (core 0's stack, §12.1).

### 5.5 Borrowed code

| Source | What | How |
|---|---|---|
| `picocalc-quest/picocalc/` (same owner) | `board.c` (clocks, 300 MHz path, flash QMI, rail, identity), `clock.c` (the shell's time), `core1.c`, `framepool.c`, `lcd.c`, `southbridge.c`, `kbd.c`, `keymap.c`, `log.c`, `page.c`, `font8x10.c`, `psram_check.c`, `settings.c`, `status.c`, `exit.c`, `fatal.c`, `counters.h`, `syscalls.c` | Copy, rename prefixes (`picocalc_quest_` → `picocalc_doom_`), re-verify, bring the tests (EL §14.6) |
| `picocalc-quest/picocalc/` | `audio_shim.c` (PWM ring), `video_shim.c`, `input_shim.c` | **Adapt**: upstream's I²S, `DG_*` and Doom-key seams in place of Cabal's |
| `picocalc-quest/scripts/`, `cmake/`, CI | The patch pipeline, the probe tools, the gate and soak scripts | Copy and rename |
| `picocalc-shell/include/picocalc_shell.h` | Return to the shell, its clock, its arguments; MIT or GPL-3.0 | Copy the shell's current header (2026-11-05, with arguments), newer than Quest's |
| `picocalc-quest/picocalc/` | `pointer.c`; the Cabal-specific parts of `main.c` | **Not used** |

**Licence (D9).** Chocolate Doom and doomgeneric are GPL-2.0-or-later;
EMU8950 is MIT; the I²S driver the port does not compile says
GPL-3.0-or-later. Upstream's `LICENSE` is GPL-2.0's text, and its own
files, such as `psram_init.c` and `psram_allocator.c`, carry no notice, so
whether they allow a later version is not written down. Quest's code is
GPL-3.0, and GPL-3.0-only code cannot be combined with GPL-2.0-only code.
The owner wrote Quest's PicoCalc layer and can license the copies here
GPL-2.0-or-later; then the whole is distributable whichever way upstream's
files are read. A copied file keeps its attribution (the 8×10 font's, the
ClockworkPi panel initialisation's).

Anything moving onto core 1 is searched for `sleep_` (EL §2.3, HW §9.7).

---

## 6. Architecture

### 6.1 Which core owns what

Upstream runs on one core. The port splits it as Quest does (EL §2.3), and
for the same reason the **SD card stays on core 0**: the engine reads lumps
synchronously, from wherever it is.

| Core 0 | Core 1 |
|---|---|
| Doom: game logic, renderer, menus | LCD presenting (§7) |
| Mixing (music and effects) in the game loop (§8) | Southbridge I²C: keyboard FIFO, battery (§9) |
| The audio DMA IRQ at priority `0x40`: a copy and a conversion (§8) | Draining the log ring to UART0 (§14.2) |
| SD card and FatFs, as the engine asks | Pages: the WAD list, errors, exit (§13) |
| The key map: held set to Doom key events (§9.1) | |

```
            core 0                                          core 1
 ┌──────────────────────────────┐                 ┌────────────────────────────┐
 │ D_DoomLoop: tics, render     │                 │ southbridge poll (~50 Hz)  │
 │   into I_VideoBuffer (SRAM)  │   key events    │   [state, code] + time     │
 │ key map ◄────────────────────┼──── SPSC ───────┤                            │
 │ I_FinishUpdate:              │                 │                            │
 │   copy into a pool buffer    │  pool: ready    │ take ready buffer          │
 │   (PSRAM) + palette          ├───────────────► │   palette → RGB565 rows    │
 │ DG_DrawFrame = publish       │                 │   DMA to LCD, rows 40–279  │
 │                              │                 │ release buffer             │
 │ I_UpdateSound: OPL + SFX     │                 │                            │
 │   → mix ring (4 tics)        │                 │                            │
 │ DMA IRQ (0x40): ring → PWM   │   log ring      │ pump log → UART0           │
 │ SD/FatFs (spi0)              ├───────────────► │                            │
 └──────────────────────────────┘                 └────────────────────────────┘
```

### 6.2 Invariants

- **The LCD is touched only from core 1, in thread context** (HW §5.7). No
  interrupt masking around a blit.
- **Core 1 never reads engine memory.** It reads only a published pool
  buffer and the palette that came with it (§7.2).
- **No `sleep_*` on core 1**; it waits with `busy_wait_us_32` (HW §9.7).
- **Core 0 never blocks on the UART.** stdio goes into the log ring
  (§14.2).
- **Everything on the audio refill path is in SRAM** (HW §5.4).
- **No flash writes** (D10).
- **No reset with a southbridge transaction in flight** (HW §6.1). Every
  reboot or exit the port makes goes through one function that parks
  core 1 between transactions first, as Quest's `picocalc_restart()` does.
- **Shift is never part of a binding in play** (HW §6.3, §9.3).

---

## 7. Display

### 7.1 Geometry

Upstream's frame is 320×240. The port draws it **at y = 40**, centred on
the 320-row panel (D3). What fills it depends on Doom's state (§3.2):

| Panel rows | Frame rows | In a level | Title, menus, intermission, finale |
|---|---|---|---|
| 0–39 | — | Band, normally black (§7.5) | Band |
| 40–59 | 0–19 | 3D view | Black (upstream's top margin) |
| 60–247 | 20–207 | 3D view (panel rows 40–247, 208 rows at the default screen size) | **The 320×200 screen** (panel rows 60–259: 60 rows above, 60 below) |
| 248–259 | 208–219 | Status bar (panel rows 248–279) | The 320×200 screen |
| 260–279 | 220–239 | Status bar | Black (upstream's bottom margin) |
| 280–319 | — | Band | Band |

So both readings of "320×200 (240?)" hold: in play the 240-row frame is
centred, and outside a level the 200-row screen inside it is centred too.
Pixels are 1:1; nothing is scaled (EL §5.4).

### 7.2 The frame pool

HDMI scans one buffer for ever and never falls behind; a presenter on the
other core can. So `DG_ScreenBuffer` becomes the buffer core 0 is filling
in Quest's **three-buffer pool** (EL §2.4, Quest §7.2): `free`, `filling`,
`ready`, `rendering`, an older `ready` dropped at publish, each transition
under one SIO spinlock, and a dropped-frame counter.

- **`DG_DrawFrame()` publishes.** `I_FinishUpdate()` has just copied the
  frame into `DG_ScreenBuffer` and calls `DG_DrawFrame()`, which publishes
  that buffer and points `DG_ScreenBuffer` at a newly claimed one. Upstream's
  `I_FinishUpdate()` needs no change for this.
- **The palette travels with the frame.** When Doom has changed it
  (`palette_changed`, a pickup or damage flash, a gamma change), the shim
  converts upstream's `colors[256]` to RGB565 in a pending table; every
  publish copies the 512-byte table into the buffer's record, so core 1
  never sees a palette half-written.
- **Margins.** Outside a level upstream writes only rows 20–219 and blacks
  the margins once, on leaving a level, in whichever buffer was current. With
  three buffers the other two would keep a level's rows there. The shim
  keeps a "margins black" flag per buffer and blacks a claimed buffer's
  margins when Doom is not in a level and the flag is clear: 12,800 bytes,
  only on the first frames after leaving a level.
- **The buffers live in PSRAM**, in upstream's 512 KB scratch region below
  the bump allocator, which nothing in upstream's tree uses: no file calls
  its getters. Three of 76,800 bytes take 225 KB of it.

Doom renders at most once a tic, so at most 35 frames a second are
published; the pool's dropped count says when core 1 cannot keep up.

### 7.3 Where the frame is drawn, and the copy

The per-frame working set belongs in SRAM (HW §2.4, §9.3), and the copy
from Doom's buffer into a frame is the first cost to watch: Quest found
64–83 % of core 0 in PSRAM-to-PSRAM copies before it cut them (HW §2.4).
Upstream draws into the zone, so PSRAM, and copies PSRAM to PSRAM.

| Strategy | Core 0 per frame, **estimated** | SRAM | Notes |
|---|---:|---:|---|
| A. Upstream: `I_VideoBuffer` in the zone, CPU copy to the pool | ~9.6 ms copy (HW §2.4's 8 MB/s), and the renderer's column writes through the XIP cache | 0 | At 35 frames a second, a third of core 0 in the copy alone |
| B. `I_VideoBuffer` in SRAM, CPU copy to the pool through the cached window | a few ms (not measured on this board) | 76,800 | The renderer writes SRAM; one patched line (§5.3) |
| C. As B, the copy by DMA to the pool through the uncached alias | ~0, plus a wait at the start of the next render | 76,800 | Overlaps the copy with the next tic's game logic; patches `I_FinishUpdate()` |

**Start with A**, which needs no patch, to get pictures in M3; **measure B
in M5**, where it is the first lever of the gate; C is an M9 lever. The
choice is a build switch (`PICOCALC_DOOM_VIDEO_BUF`), so each is a control
for the others (EL §12). B and C fit only with the WAD directory moved to
PSRAM (§12.1).

Core 1 reads frames from PSRAM as Quest settled in its M2: by DMA from the
uncached alias into an SRAM row a row ahead, which cost core 0 least
(HW §4.7, §9.3). The uncached reads need the writer to clean the buffer's
lines out of the XIP cache at publish, 0.45–0.69 ms a frame (HW §9.3);
under C the DMA writes past the cache and needs no clean.

### 7.4 The presenter

Core 1's loop: take the `ready` buffer; for each of its 240 rows, expand
320 indexed pixels through its palette into one of two RGB565 line buffers
while DMA sends the other (HW §4.6); release the buffer; poll the keyboard
when due (§9.1); pump the log.

A full 320×240 present from PSRAM takes **18.1 ms** (HW §4.7, measured at
300 MHz, 75 MHz SPI). At 35 frames a second that is 63 % of core 1, and the
keyboard polls take about another 20 % (§9.1). **Core 1 has room for the
present and the keyboard, and little else.**

Doom redraws the whole 3D view each frame, so presenting only changed rows
saves little in play; the status bar and borders are what stay still.
Dirty rows are an M9 lever, measured, not a plan.

**Tearing.** There is no TE line (HW §1.2), and a fast turn will show a
tear. Accepted, as on most PicoCalc games (HW §4.9).

### 7.5 The bands

The 40-row bands above and below are outside anything Doom draws. The
bottom band shows the port's transient messages (a settings line refused,
the run mode changed, the card missing), drawn by core 1 in the 8×10 font
only when they change and cleared when their time is up, as Quest §7.4.
Doom's own messages stay in its frame. A perf line is a development-build
option, measured on and off (EL §12).

---

## 8. Audio

### 8.1 Output

PWM on GP26/27, both channels of one slice, one 32-bit compare word per
stereo frame, two chained DMA channels over a power-of-two aligned ring
with the hardware read wrap, both read address and count reset on re-arm
(HW §5.1–5.3), `DMA_IRQ_1` at priority `0x40` (HW §5.4).

**The rate (D11).** EMU8950 is built to run at its native rate, 49,716 Hz,
with its converter compiled out, and upstream's mixer runs at that rate. On
the PicoCalc at 300 MHz, divider 1, **TOP = 3016** and oversample 2 give:

```
carrier     300,000,000 / 3,017       = 99,436 Hz   (ultrasonic, well above HW §5.2's 73 kHz)
resolution  log2(3,017)               = 11.6 bits
sample rate 300,000,000 / (3,017 × 2) = 49,718.26 Hz
```

0.005 % fast against 49,716: music plays 0.005 % quick, inaudible, and
nothing else depends on it, since upstream's producer fills the ring as the
consumer drains it and the OPL's own timing counts samples. So no rate
converter, no change to upstream's sound code, and HW §5.2's ultrasonic
carrier. TOP is computed from `clock_get_hz(clk_sys)` at init
(`clk_sys / (2 × 49,716) − 1`, rounded), never written as a constant. The
alternative, 36,621 Hz as Quest, would need a 49,716 → 36,621 converter in
front of upstream's mixer, or EMU8950's own converter built back in, for
nothing gained.

A signed 16-bit sample becomes `((s × 3,017) >> 16) + 1,508` about the
mid-point; the conversion is pure code with a host test against an
independent model (§15).

### 8.2 The shim

Upstream's `i_picosound.c` is kept unchanged. It calls `i2s_init()`,
`i2s_set_fill_callback()` and `i2s_start()`, and its fill callback hands
over one slot of the mix ring, 1,421 stereo frames, or silence. The port's
`audio_shim.c` implements those names:

- The PWM ring has **two halves of 512 frames** (10.3 ms each at
  49,718 Hz; 1,024 compare words, 4 KB a half, 8 KB aligned to 8 KB).
- The refill IRQ converts 512 frames from a staging buffer of 1,421 frames
  in SRAM, calling upstream's fill callback for the next slot whenever the
  staging buffer runs out, and re-arms the channel.
- `i2s_dma_write()` and the rest of the push API, which upstream's sound
  module does not use with a fill callback set, are there for the link and
  count a call.
- The ring and the IRQ start at upstream's `I_InitSound()` and run on.

Nothing in the IRQ mixes. Upstream's channels are changed only by the game
loop, and the mix ring's indices are written one side each, so no lock
masks the IRQ, which removes the class of fault Quest found in Cabal's
`lockMutex()` (Quest §8.2, HW §5.9).

### 8.3 Costs and counters

**Producer starvation is the expected failure, not late refills.** The game
loop mixes up to four tics ahead (114 ms). A stall longer than that (a level
load, a run of card reads for lumps not yet cached, a frame far over budget)
empties the ring, and the IRQ plays silence: an underrun, counted in
samples (HW §5.8). Doom on DOS also went quiet during a level load.

Counters (HW §5.8, EL §6.2): underrun samples, late refills (channel
already running on entry), the IRQ's longest run, and mixing's share of
core 0, timed around `I_UpdateSound()`. The late path is forced once with a
scratch build (EL §6.4).

**Unknown, measured in M6:** EMU8950's cost on a Cortex-M33 at 300 MHz.
`rp2040-doom` ran it on a Cortex-M0+; Quest measured MAME's and DOSBox's
OPL at 13–55 % of core 0 at 36,621 Hz (HW §5.5), and EMU8950 with its
slot renderer is built to be lighter than either. If it costs too much,
the levers in order: the slot renderer and mixer in SRAM (HW §9.2); mixing
on core 1, if the gate leaves it room (§7.4 says it will not have much).

**Effect latency.** A sound started now is mixed into the next free slot,
behind up to four tics already mixed: up to ~114 ms late. If the owner
hears it, the ring can shrink to two or three slots against the underrun
count (M6).

---

## 9. Input

### 9.1 From the southbridge to Doom

Quest's path (Quest §9.1, EL §7.1). Core 1 drains the southbridge FIFO and
pushes the MCU's `[state, code]` pairs, each with the time it read them,
into a 64-entry SPSC ring. Core 0 owns the key map (`keymap.c`): the
held-key set built from press and release, canonical identities for keys
the MCU retranslates (HW §6.2), and the translation to Doom key codes,
which `DG_GetKey()` returns one at a time.

- **Auto-repeat is dropped.** The MCU's repeats arrive as extra presses
  (HW §6.2). Doom wants one key-down and one key-up; it makes no repeats of
  its own in play, and its menus step on each press.
- **Everything about a held key is fixed at its press**, and its release
  sends the matching key-up however the MCU translates it (EL §7.1).
- **Poll every 20 ms** (50 Hz), against Quest's 25 ms: Doom is an action
  game, and the MCU scans its matrix only every 16–26 ms (HW §6.2), so a
  faster poll buys little. Each poll costs 4.8 ms on the 10 kHz bus
  (HW §6.1), about 24 % of core 1 at 50 Hz, polled between presents.
- **Latency, estimated**: up to one MCU scan (≤ 26 ms), one poll (≤ 20 ms),
  one tic (≤ 28.6 ms) and one render and present (~30 ms), so 50–100 ms
  from finger to panel. M4 measures it.
- `I_GetEvent()` posts at most one key release a tic (§3.2). Two arrows
  let go together reach Doom a tic apart. Left as upstream's, and counted
  in M4 to see whether it matters.

### 9.2 Key map (D12, proposed)

| PicoCalc | Code | Doom |
|---|---|---|
| Up, Down | `0xb5`, `0xb6` | Forward, back |
| Left, Right | `0xb4`, `0xb7` | Turn; with Alt held, strafe (Doom's own `key_strafe`) |
| Ctrl | `0xa5` | Fire |
| Space | `0x20` | Use (open doors, switches) |
| `,` `.` | ASCII | Strafe left, right (vanilla Doom's keys) |
| `1`–`7` | ASCII | Weapons |
| Esc | `0xb1` | Menu |
| Enter, Backspace | `0x0a`, `0x08` | Menu select, back; text entry for save names |
| `y`, `n` | ASCII | Confirm, cancel |
| Tab | `0x09` | Automap |
| `-`, `=` | ASCII | Screen size, automap zoom |
| F1–F5 | `0x81`–`0x85` | Help, save, load, volume, detail |
| F6–F10 (Shift+F1…F5) | `0x86`–`0x90` | Quicksave, end game, messages, quickload, **quit** (§13.3) |
| Alt+F1 | F1 with Alt held | F11, gamma |
| Break (Shift+Esc) | shifted alternate | Pause |
| Letters | ASCII | Lower-cased, for cheats and save names |
| Shift | `0xa2`/`0xa3` | **Nothing in play**; still resolves typed characters in the MCU |
| Caps Lock | `0xc1` | Accepted, not passed on |
| Power button | `0x91` | Accepted, not passed on |

**Running is always on** (D12): Doom's run key is Shift by default, and
Shift cannot be used (§9.3). `picocalc.ini` can turn it off, or put running
on a held key the MCU delivers with the arrows (§10.3). The bindings are
set as Doom's own `key_*` variables (§9.4), so a player can still change
them in `default.cfg`.

F6–F10 and Break reach Doom without the Shift that made them (Quest found
SCI taking Shift+F7 for another key, Quest §9.2). Shifted punctuation is
sent as the character the MCU resolved. The table is data in `picocalc/`,
and a host test checks that every code the MCU can send maps to exactly
one entry and that no binding needs a chord the MCU swallows (EL §7.2).

### 9.3 The Shift trap

Shift with Left or Right sends nothing, **not even the release** (HW §6.3):
hold Left, press Shift, let go of Left, and the held set keeps Left down,
so the player turns for ever. Shift with Up or Down sends PgUp and PgDn.
Doom's default run key is Shift. So:

- **No binding in play uses Shift** (D12), and the key map never sends
  Doom `KEY_RSHIFT`, so Doom's own run-on-Shift cannot fire either.
- **When Shift goes down, the key map releases a held Left or Right**,
  since their releases may never come. If the player is still holding the
  arrow, the MCU's repeats resume as presses once Shift is up (HW §6.2),
  and the key is held again.
- **PgUp and PgDn are Up and Down's identities** in the held set, so a
  release that arrives retranslated still releases the arrow.

A host test drives each of these sequences.

### 9.4 Applying the bindings

Doom keeps its bindings in `key_*` variables that `default.cfg` overrides.
The port's map sends Doom's own key codes (`KEY_FIRE`, `KEY_USE`,
`KEY_RALT`, the arrows, ASCII), so the defaults in upstream's
`m_controls.c` already fit Ctrl, Space, Alt and the arrows. Doom's run test
(`G_BuildTiccmd()`) runs when `key_speed` is held, or always when
`key_speed` is out of range or `joyb_speed` is past the last joystick
button (the "`joyb_speed = 31`" hack Chocolate Doom keeps). So `run =
always` sets `joyb_speed` to 31; `run = hold` sets `key_speed` to the run
key; `run = off` leaves `key_speed` at upstream's `KEY_RSHIFT`, which the
key map never sends. One hunk in `d_main.c`, after Doom loads its
configuration, applies `picocalc.ini`'s choice (§5.3).

### 9.5 What M4 must find out

From the MCU's source, not yet seen on a board (§17):

- **Alt with an arrow** delivers the arrow with Alt held. HW §6.3 says Alt
  passes keys through and consumes only `,` `.` Space `B`; the arrows
  under Alt have not been logged.
- **Three keys at once**, such as Up, Ctrl and Space, all arrive. The
  arrows are outside the matrix (HW §6), which helps; the rest have not
  been tried for ghosting.
- **Alt with Space** is the MCU's (HW §6.3), so strafing on Alt and using
  at once is impossible; `,` `.` strafe without that limit.

---

## 10. Storage

### 10.1 Driver

The same `sdcard.c` as Quest's (§3.1), so the same settings: hardware SPI
on `spi0`, the PicoCalc's pins, `CLK_FAST` made overridable and set to
25 MHz (HW §7.1), `get_fattime()` compiled out for the PicoCalc clock's
(§13.2). `pio_spi.c` is not compiled. Upstream's FatFs configuration
(exFAT and long names on, `FF_FS_REENTRANT` 0) is kept.

The card stays on core 0 (§6.1). WAD lumps are read on demand: Doom
precaches a level's graphics when it loads (`precache`, on by default), so
reads in play are lumps the zone purged or sounds heard for the first
time. Each is 7–45 ms of the frame it lands in (HW §7.1). The heartbeat
counts card reads a second in play, which says whether the zone is big
enough (§12.2).

Card detect (GP22, active low, pull-up) is read at boot for a page naming
the problem. Removal in play is not handled; the next start finds it.

### 10.2 Card layout

Upstream's, so a card works with both:

```
/doom/*.wad            IWADs and PWADs
/doom/default.cfg      Doom's settings: volumes, bindings, screen size (Doom writes it)
/doom/*doom.cfg        Chocolate Doom's extra settings (Doom writes it)
/doom/.savegame/       saves, doomsav0.dsg … doomsav7.dsg
/doom/picocalc.ini     the port's settings (§10.3), optional
```

**Saves are shared between IWADs** (§3.2): Doom and Doom II both use
`/doom/.savegame/doomsav0.dsg` and the rest, so a slot saved in one shows
in the other's list, and loading it there fails or worse. Quest met the same
shape of fault in its config domains (Quest §10.2). The fix is upstream's
own: Chocolate Doom keeps a directory per IWAD, `.savegame/<iwad>/`, which
upstream's embedded branch of `M_GetSaveGameDir()` dropped. M8 puts it back
if the owner agrees, which moves existing saves out of sight.

The WAD list skips macOS's `._` files (HW §7.1).

### 10.4 The owner's WADs

The owner's copies (2026-11-06, in `reference/`, gitignored) are the 2024
re-release of Doom and Doom II, which ships the add-ons as PWADs. Read from
each file's header and directory; what each needs is read from upstream's
source, **not yet run**. The rows marked **No** for good (Legacy of Rust
and its resources, `iddm1.wad`, `extras.wad`) were removed from
`reference/` the same day at the owner's request; SIGIL and SIGIL II stay
there, since they are deferred, not dropped (§19).

| File | Kind | Lumps | Size | Maps | Demos | MD5 | Here |
|---|---|---:|---:|---|---|---|---|
| `doom.wad` | IWAD | 2,308 | 12,996,515 | E1M1–E4M9 (36) | 4 | `3b37188f6337f15718b617c16e6e7a9c` | **Runs**; the WAD list's Doom |
| `doom2.wad` | IWAD | 2,928 | 14,951,361 | MAP01–32 | 3 | `64a4c88a871da67492aaa2020a068cd8` | **Runs** |
| `plutonia.wad` | IWAD | 2,991 | 17,704,206 | MAP01–32 | 3 | `e47cf6d82a0ccedf8c1c16a284bb5937` | **Runs** |
| `tnt.wad` | IWAD | 3,109 | 18,507,237 | MAP01–32 | 3 | `ad7885c17a6b9b79b09d7a7634dd7e2c` | **Runs** |
| `nerve.wad` (No Rest for the Living) | PWAD | 117 | 4,039,441 | MAP01–09 | 3 | `23422eb42833ac7b0dd59c0c7ae18a6f` | **Partly**: with `-file` over Doom II its nine maps replace MAP01–09, but this engine has no mission pack for it, so the level names are Doom II's and the episode does not end after MAP08 |
| `masterlevels.wad` | PWAD | 277 | 3,920,169 | MAP01–21 | 3 | `ab3ce78e085e50a61f6dff46aabbfaeb` | **Partly**, the same way: Doom II's names, and skies by map number, where the re-release sets them in `UMAPINFO` |
| `sigil.wad` | PWAD | 149 | 4,660,156 | E5M1–E5M9 | 4 | `08ee05388c137db5f5d7996e89425b95` | **No**: a fifth episode, which a vanilla engine cannot reach (§19) |
| `sigil2.wad` | PWAD | 185 | 42,333,362 | E6M1–E6M9 | 4 | `953f65cf079d0ba9a25be2c407da7ec1` | **No**: a sixth episode; most of its size is digital music this engine cannot play |
| `id1.wad` (Legacy of Rust), `id1-res.wad`, `id1-tex.wad`, `id1-weap.wad`, `id1-mus.wad`, `id24res.wad` | PWADs | — | — | MAP01–… (17) | 4 | — | **No**: built for the re-release's ID24 features (§19) |
| `iddm1.wad` | PWAD | 2,030 | 24,769,152 | MAP01–26 | 0 | `cb92010b8ec05f8924ac966a8ed95b74` | **No** point: deathmatch maps, and there is no multiplayer |
| `extras.wad` | PWAD | 170 | 625,140,164 | 0 | 0 | `2e76d93d52ef64fb9db3cee2437c686b` | **No**: the re-release launcher's own content. Do not copy it to the card |

Doom engines identify an IWAD by its file name, not its contents, so the
re-release IWADs are picked up like any others. Their extra lumps
(`GAMECONF`, `SKYDEFS`, `UMAPINFO`, `DMXGUS`) are unknown to this engine and
ignored; M2 checks that they are and that their demos play in sync. PWADs
are not in the WAD list (D13); they are loaded with `-file`, from
`picocalc.ini`'s `args` or the shell (§13.4).

### 10.3 Settings

`/doom/picocalc.ini`, a `key = value` text file read at boot on core 0,
with every default in one function and Quest's parser (EL §8.7, Quest
§10.3). Only what Doom's own `default.cfg` cannot say goes here:

```
iwad      = doom2.wad      start this IWAD without the list (D13)
args      = -file sigil.wad -skill 3     more Doom arguments (D15)
run       = always         always, or hold (on the key below), or off
run_key   = z              the key that runs when run = hold; never Shift
```

Volumes, bindings, screen size, gamma and messages are Doom's, in its own
menus and `default.cfg`. A line that cannot be used is logged with its
number and why, and named in the bottom band when the game starts; the
rest applies. The file is written only by the user (EL §8.7's
"saving is a deliberate action": there is nothing to save).

---

## 11. Clocks, voltage, PSRAM and flash

### 11.1 The clock (D7)

Upstream runs at 252 MHz at 1.50 V, for HDMI's pixel clock, with the
voltage limit disabled. The PicoCalc evidence covers 150 and 300 MHz
(HW §3), and the LCD's SPI divider favours exactly those two: at 300 MHz
`clk_peri / 4` is the panel's 75 MHz, at 252 MHz it would be 63 MHz.

So the port ships at **300 MHz at 1.20 V**, with Quest's bring-up order
(EL §9.4): start at 150 MHz; read the flash's id; rail to 1.20 V; flash QMI
divider and RX delay set from SRAM; PLL to 300 MHz; `clk_peri` onto
`clk_sys` (`PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1`); only then
stdio, PSRAM, I²C, LCD, audio and SD. The rail is set explicitly at
power-on in case another firmware left it high (HW §3).

Whether 300 MHz is enough is the M5 gate (§16). If not, the levers come
first, and only then a higher clock, as an owner decision with its own
SPI, PWM, PSRAM and thermal checks.

### 11.2 Board target

`PICO_BOARD=pimoroni_pico_plus2_rp2350` (RP2350B, 16 MB flash, PSRAM CS on
GP47), as Quest. The banner logs the physical board separately from the
build target (HW §2.1): flash size from the flash id, package from
`SYSINFO`; whether a CYW43 answers is not probed.

### 11.3 PSRAM

Upstream's `psram_init()` derives the QMI timing from `clk_sys` when it
runs, which is right as long as it runs after the clock moves (HW §3,
item 3); `main()` guarantees that. Built with `PSRAM_MAX_FREQ_MHZ=100`, as
Quest, it gets divider 3 (100 MHz) at 300 MHz, which Quest's round trip
qualified on a Plus 2 W (HW §2.4). The port adds that round trip after init
and shows a page naming the failure rather than handing Doom a bad zone.

### 11.4 Flash

The image is about 0.6 MB of 16 MB. Code runs from flash through the 16 KB
XIP cache, which PSRAM reads share, and Doom's renderer reads textures,
flats and colormaps from the zone in PSRAM through that same cache: the
code and the renderer's data compete (HW §9.2, §9.3). The flash runs at
Quest's qualified 75 MHz (divider 4, RX delay 4 at 300 MHz, HW §3), and
the port makes no flash command after the clock moves (HW §3: any flash
command restores the bootrom's timing).

Because the image is small, the hot renderer code can move into SRAM for
little (§16, M5 and M9).

---

## 12. Memory budget

### 12.1 SRAM

Upstream, built (§3.4): `.data` 73,544 and `.bss` 320,400 bytes leave
**124,016** for the SDK heap. **Estimated** for the port:

| Item | Bytes |
|---|---:|
| Removed: I²S `dma_buffers` | −12,288 |
| Removed: HDMI, PS/2 and USB state | −5,000 to −10,000, to be measured |
| PWM ring, 2 × 512 frames × 2 slots × 4 B | 8,192 |
| Audio staging, 1,421 frames × 4 B | 5,684 |
| Two RGB565 line buffers, two palettes | 2,304 |
| Key ring, held set | ~2,000 |
| Log ring, page text rows, pool records, counters | ~6,500 |
| **Port, net, before the video buffer** | **about +2,000 to +7,000** |
| `I_VideoBuffer` in SRAM (§7.3 strategies B and C) | 76,800 |

**The heap's one big user is the WAD directory.** `ExtendLumpInfo()`
`calloc`s 28 bytes a lump. Read from the owner's WADs in `reference/`
(2026-11-06, §10.4), with the shareware IWAD estimated:

| IWAD | Lumps | Directory, bytes |
|---|---:|---:|
| `doom1.wad` (shareware, not to hand) | ~1,264 | ~35,400 |
| `doom.wad` | 2,308 | 64,624 |
| `doom2.wad` | 2,928 | 81,984 |
| `plutonia.wad` | 2,991 | 83,748 |
| `tnt.wad` | 3,109 | 87,052 |

Every IWAD is 13–19 MB, more than the PSRAM, which is why upstream reads
lumps from the card on demand rather than loading the WAD.

A PWAD extends the directory, and the extension holds old and new at once:
Doom II with `masterlevels.wad` (277 lumps) peaks at (2,928 + 3,205) × 28 =
171,724 bytes, past the whole heap. With `I_VideoBuffer` in SRAM, TNT alone
would not fit. So the patch moves
the directory to PSRAM (§5.3) in M3, before anything competes for the
heap. The directory is read on every lump lookup, in small pieces, which
the XIP cache serves well.

**Core 0's stack** is the SDK's default; Doom's BSP walk recurses. The
heartbeat reports the painted high-water of both stacks from M1, and the
port moves to a larger stack only if it comes near.

**Core 1's stack** is 2 KB (HW §9.5); Quest's presenter, poll, log pump
and pages reached 724 bytes.

CI prints `arm-none-eabi-size` on every build (EL §2.1); M0 records the
port's real figures against these.

### 12.2 PSRAM (8 MB)

| Region | Upstream | Port |
|---|---:|---:|
| Scratch, at the base | 512 KB, unused | 512 KB: the three frame buffers (225 KB) |
| Permanent: zone (3 MB), screen buffer, the WAD directory (port) | 4 MB | 4 MB, less the screen buffer, plus the directory |
| Temporary: MIDI conversion | 4 MB | 4 MB |

**Is a 3 MB zone enough?** Doom's zone holds the level, its precached
graphics and every cached lump; purgeable lumps are dropped when it fills
and read again when next needed. Too small shows as card reads in play
(§10.1) or as `Z_Malloc` failing on a big map. The host engine measures the
zone's low-water on every map of every IWAD to hand (M2), and the device's
heartbeat counts card reads in play. The temporary half is 4 MB for one
song's conversion, which is generous; the zone can grow into it if M2 says
it must.

### 12.3 Flash

About 0.6 MB of 16 MB; nothing else is stored in flash (D10). Both UF2
Loader builds hold it: the shell's 16 MB build gives 16,372 KB, the stock
4 MB one 4,088 KB (HW §2.7).

---

## 13. Boot, time and exit

The port works with the PicoCalc shell the way Quest does (D4), through the
one header the shell publishes, `picocalc_shell.h` (MIT or GPL-3.0), taken
from the shell's repository as of 2026-11-05.

### 13.1 Boot

Clocks (§11.1); log ring and stdio; banner (board, chip revision, clocks,
rail, PSRAM result, southbridge version, the shell's time if set, version
from `git describe`); southbridge; LCD cleared before `DISPON`; core 1;
PSRAM verify; card check; settings; the shell's arguments; then
`doomgeneric_Create()`, whose `DG_Init()` and `DG_StartScreen()` are the
port's.

**The WAD list (D13).** `DG_StartScreen()` finds the IWADs in `/doom` that
Doom's IWAD table knows, by name. With one, or with `iwad =` in
`picocalc.ini`, or an `-iwad` from the shell, it starts that one at once
(EL §10: boot straight into the game). Otherwise core 1 draws a page in the
8×10 font listing them by title ("DOOM II: Hell on Earth (doom2.wad)");
Up, Down and Enter choose, **Esc leaves** (§13.3). The page names where Esc
goes, decided before it is shown. Either way it then sets Doom's `myargv`:
`-iwad`, then `args`, then the shell's words.

A failure before Doom starts shows a page naming it, never a blank screen
(EL §8.1): no PSRAM, no card, a card that does not mount, no `/doom`, no
IWAD. For the card's pages, core 0 then watches card detect, debounced
500 ms (HW §7.1), and restarts the program when a card goes in, parking
core 1 first (§6.2), as Quest §13.1.

**`I_Error()`** keeps its message and core 1 draws "Doom stopped with an
error." with the message wrapped below; any key restarts, through the same
parked restart. Chocolate Doom ends a `-timedemo` run through `I_Error()`
with its result ("timed N gametics in M realtics (F fps)"), so that page,
and the log line, are also how a timing run reports (§14.3).

### 13.2 The date and time

The PicoCalc has no battery-backed clock (HW §1.2). The shell keeps UTC in
the RP2350's always-on (POWMAN) timer, which survives the reboot into a
program, and the local offset in a checked record in POWMAN scratch 0–2.
`picocalc_shell_time(&utc_ms, &offset_min)` returns both, or false when
the clock is not set (a power-on that started the program without the
shell).

Quest's `clock.c` gives the port `picocalc_clock_now()`: local broken-down
time read from the timer **on every call**, and whether it is real. Doom
never asks the time of day, so it has two callers:

- **`get_fattime()`**, replacing the SD driver's: every file the port or
  Doom writes (saves, `default.cfg`, Chocolate Doom's config) carries the
  shell's local date and time. With no clock set, FatFs's fixed default,
  1 January 2022, where upstream gave none.
- **The banner**, which logs it, read before and after the move to 300 MHz
  (the POWMAN timer counts its own tick, HW §1.2).

What the port must **not** do, by the header's contract: stop the timer,
change its tick source, set it other than through
`picocalc_shell_set_time()` (which the port has no reason to call), or
write POWMAN scratch 0–2. The build links `hardware_powman`, which defines
`PICOCALC_SHELL_HAS_TIME`. The conversion to calendar fields has Quest's
host test.

### 13.3 Exit (D14)

Doom has a way out already: Quit Game in the menu, or F10, then `y`, calls
`I_Quit()`. Upstream's reboots; the port's leaves:

1. **Runs the at-exit list.** The port's `I_AtExit()` keeps the functions
   Doom registers, and `I_Quit()` runs them: `M_SaveDefaults()` writes
   `default.cfg` and Chocolate Doom's config to the card, so a change made
   in Doom's menus survives (it never did upstream, §3.2). ENDOOM is not
   shown (§19).
2. **Unmounts FatFs**, which flushes it. The reboot does not wait.
3. **To the shell**, when UF2 Loader started the image
   (`picocalc_shell_loader_present()`) and a card is in
   (`picocalc_shell_card_present()`): the page "Back to the shell." for
   2 s, core 1 parked between transactions (§6.2), then
   `picocalc_shell_exit()`, after which UF2 Loader's stage 3 loads
   `/BOOT2350.UF2`.
4. **Otherwise, power off** through the southbridge (`OFF` with 6 s,
   HW §6), as Quest does when copied on in BOOTSEL. If the southbridge
   cannot, the page says so and any key restarts.

Esc in the WAD list (§13.1) takes the same path, from step 2. Holding Up,
F1 or F5 at power-on also returns to the shell, for any program; that is
UF2 Loader's and needs nothing from the port.

### 13.4 Arguments from the shell (D15)

Doom is a command-line program, and the shell now passes arguments to a
program that declares it takes them (`PICOCALC_SHELL_TAKES_ARGS`, in
`picocalc_shell.h`). The port declares `"picocalc-doom"`, reads
`/.shell/args` with its own FatFs at start, removes it, and parses it with
the header's functions. Every word goes to Doom's `myargv` after
`picocalc.ini`'s `args`, so the shell's win where Doom takes the last.
Words that follow `-iwad`, `-file`, `-merge`, `-deh` and `-playdemo` are
files, made full paths from the user's directory with
`picocalc_shell_args_path()`, since Doom's own working directory is
`/doom`. With no file (the loader's menu, a power-on), Doom starts as it
would with no arguments.

---

## 14. Development workflow

### 14.1 Probe and UART

As Quest §14.1 (D5; HW §2.7, EL §13.1):

- **Flash over SWD** with OpenOCD: `init`, `reset halt`, DMA and PWM held
  in reset (`mww 0x40022000 0x00010004`), `program`, `reset halt`,
  `resume`. Never `reset run` (core 1 is lost), never a bare `halt` (a
  core 1 caught in an I²C transaction wedges the southbridge), and never
  leave the PWM's DMA running into OpenOCD's work area (all HW §2.7).
  `scripts/flash.sh` does this.
- **Capture UART0** (TX GP2, RX GP3, 115200 8-N-1) before flashing, so the
  banner is in the log; `scripts/capture.sh` refuses a port another
  process holds. `PICO_DEFAULT_UART=0`, `PICO_DEFAULT_UART_TX_PIN=2`,
  `PICO_DEFAULT_UART_RX_PIN=3`; the SDK selects the pins' UART_AUX
  function.
- **Keep the Mac's display awake** (`caffeinate -d`) while the probe is in
  use (HW §2.7).
- **Type over the UART**: printable bytes are their keys, `0x01 state code`
  any event as the MCU would send it, `0x02` restarts. `scripts/keys.sh`
  names them (`up`, `hold:up 2`, `ctrl`, `text:iddqd`, `f10 y`). With it a
  run needs nobody at the keyboard.
- **Build-time arguments**: `PICOCALC_DOOM_ARGS` (for example
  `"-iwad doom1.wad -timedemo demo1"`) wins over the settings and the
  shell, so a scripted run knows what it booted with (EL §8.7, §13.1). Each
  in its own build directory.

### 14.2 Two builds

- **Development**: stdio on UART0 through a `stdio_driver_t` that writes
  into the log ring, which core 1 drains as the UART has room; dropped
  lines counted (EL §2.3). Upstream's logs on (`FRANK_DOOM_QUIET=0`); its
  `psram_malloc()` prints every allocation of 64 KB or more. UART keys on.
- **Release**: no UART, no stdio, GP2/GP3 free, upstream quiet. The counter
  block at a fixed symbol, with a magic word and layout number, read over
  SWD without halting (HW §2.7) by `scripts/counters.sh`, as Quest §14.2.

### 14.3 Heartbeat

Once a second on the development build: core 0 busy share (wall time not
in `DG_SleepMs`), frames rendered, published, presented and dropped, the
longest present, tics run, mixing's share and the IRQ's longest refill,
underrun samples, late refills, card reads, key events and their mean and
longest wait, keys unknown to the map, stray releases, I²C errors, log
lines dropped, both stacks' high-water, the zone's free bytes, die
temperature, battery and charging. Every counter must be able to fire
(EL §12).

**The workload is Doom's own.** `-timedemo demo1` (and `demo2`, `demo3`)
plays a recorded demo as fast as the machine can, one frame per tic,
deterministic, and reports frames per second through `I_Error()` (§13.1).
It is the gate's measure (M5), every lever's control comparison (EL §12),
and the release's regression check. Play-tests by hand judge what
`-timedemo` cannot: input feel, sound, tearing.

---

## 15. Testing

The engine is Chocolate Doom's, tested by Chocolate Doom and by decades of
demos; the port tests what it adds, and whether the engine on the board is
the engine on the host.

**Host tests** (CTest, no framework, exit 77 is a skip, EL §11.1), for the
portable parts of `picocalc/`, under `-Wall -Wextra -Werror`, as Quest:

- The frame pool, through a long random interleaving (EL §2.4), and the
  margins rule (§7.2).
- The key map: every MCU code to exactly one entry; no swallowed chord
  bound; canonical releases; the Shift trap's three rules (§9.3); Insert
  decided by Alt (HW §6.2).
- Palette to RGB565, and the 16-bit-to-compare-word conversion, against
  independent models (§8.1).
- The audio shim's staging: 1,421-frame slots in, 512-frame halves out,
  across the wrap, with silence on an empty ring.
- The shell's time to calendar fields (Quest's).
- The settings parser, and the argument builder (§13.4).
- `create_patch.sh` then `apply_patch.sh` reproduces `working/`.

**The host engine** (EL §11.2). The patched upstream sources built for the
workstation with a headless `DG_*` platform in `test/host/`, upstream's own
`w_file_stdc.c` and `m_misc.c` in place of the FatFs glue, a null sound
module, and the same `SCREENHEIGHT`, `CMAP256` and zone size. It is a tool,
not a CTest, since it needs WADs, which never go in the repository:

- **Demo sync.** `-timedemo` of each IWAD's demos plays to its end.
  Upstream's changes to the renderer should not reach game logic; a desync
  says one did.
- **Frame hashes.** A CRC of each frame (pixels and palette index) during
  `-timedemo demo1`. Under `-timedemo` Doom renders exactly one frame per
  tic, so the list is deterministic. **The same list from the device** shows
  the engine on the board is the host's (EL §13.3), and it is the check that
  catches a marginal PSRAM or flash timing, which corrupts quietly
  (HW §3).
- **Zone low-water**, warping to every map of every IWAD to hand and
  running a minute of tics in each (§12.2), and the WAD directory's size
  (§12.1).

**Device checks** at every milestone (§16), each dated, on a named board,
with a not-checked list (EL §14.3). The owner supplies the WADs; the
firmware names no PWAD (EL §10).

**CI** (GitHub Actions): host tests; `check_sources.sh`; the firmware build
for both build types; `arm-none-eabi-size` in the log.

---

## 16. Milestones

Each milestone ends with something that runs and something that is
measured (EL §14.3), and is small enough to finish in one or two sittings.
Results go in `docs/milestones.md`: date, board, compiler, what was checked
and by whom, and a **not checked** list carried forward. One branch and
pull request per milestone, CI green on both builds (EL §14.7).

**Anything learned about the PicoCalc goes into `hardware-notes.md` as it
is learned** (D6), not at the end: a measurement (present times, card
latency, mixing cost, a key chord the MCU does or does not deliver, a PWM
rate that sounds right or wrong), a quirk, a pin use, or a correction to
something the notes say. Each entry stands alone: board, date, build and
workload, measured or read from source, and no reference to this
project's documents or code (EL §14.1). What generalises beyond the
PicoCalc about porting (patching an upstream, the gate, the host engine)
goes into `emulator-lessons.md` the same way. **A milestone is not done
until its hardware findings are in the notes.**

```
 M0  skeleton, patch pipeline, CI                         (host)
 M1  board bring-up                                       (device)
 M2  host engine: demos in sync, frame hashes, zone       (host, alongside M1)
 M3  display and card: the title and demos on the panel
 M4  keyboard and the WAD list
 M5  first play ── GATE: is 300 MHz enough?
 M6  audio
 M7  controls play-tested, settings
 M8  saves, configuration, quit to the shell, the time, arguments
 M9  performance pass and soak
 M10 release
```

### M0: Skeleton, patch pipeline, CI

- Repository layout of §5.1; submodule pinned at `153a950`.
- Quest's patch pipeline, renamed; `cmake/upstream-sources.cmake` and
  `check_sources.sh` reproducing upstream's glob (§5.4).
- `picocalc/` with empty shims (§5.3) so the engine links for
  `pimoroni_pico_plus2_rp2350`.
- The first patch: `sdcard.c` only.
- CI: host test skeleton, source check, firmware build, size.
- `CLAUDE.md`: build and test commands, the invariants of §6.2, the settled
  decisions, and D6's rule for the hardware notes.

**Done when** a clean clone builds a UF2 with one command; the patch round
trip passes; the size report is recorded against §3.4. **Measured**:
`.text`, `.data`, `.bss`, heap left. **Not on the device.**

### M1: Board bring-up

- `main.c` and `board.c` from Quest: 150 → 300 MHz per §11.1, rail 1.20 V,
  flash at 75 MHz.
- Log ring, development stdio on UART0 (D5), banner, heartbeat skeleton,
  both stacks painted.
- `flash.sh` and `capture.sh` with the probe.
- PSRAM init (upstream's) and the round trip (§11.3).
- Southbridge, LCD and the corner-coded test pattern (HW §10, EL §13.3).
- `clock.c`: the banner prints the shell's time, before and after the move
  to 300 MHz.

**Done when** the banner and a minute of heartbeats arrive over UART;
PSRAM verifies at 300 MHz; the test pattern is right in all four corners
and colours (the owner's look); started from the shell, the banner's time
matches the shell's on both sides of the clock change. **Measured**: the
clocks as configured, the SPI rate returned, PSRAM verify time, die
temperature after a minute.

### M2: Host engine

Runs alongside M1 and M3; needs no board.

- The host build of §15 with a headless platform.
- The demos of the four IWADs in `reference/` (§10.4) under `-timedemo`
  to their ends. A demo that desyncs is checked under stock Chocolate Doom
  on the workstation before upstream is blamed: some releases ship a demo
  recorded on another version.
- `nerve.wad` and `masterlevels.wad` loaded with `-file` over Doom II:
  their maps start and play, as §10.4 expects.
- The frame-hash list for `-timedemo demo1`, kept as the device's reference.
- Zone low-water and directory size on every map of every IWAD to hand.

**Done when** every demo plays to its end in sync, the hash list is
reproducible run to run, and §12's tables have measured rows. **Measured**:
host fps (for nothing but scale), zone low-water per map, lumps per IWAD.

### M3: Display and card

- SD on `spi0` (§10.1); FatFs mounts; `/doom` checked; pages for the
  failures (§13.1).
- The WAD directory moved to PSRAM (§12.1).
- The frame pool, `DG_DrawFrame()` publishing, the palette with the frame,
  the margins rule (§7.2); core 1's presenter at y = 40.
- `DG_StartScreen()` starting `iwad =` or the only IWAD; no list yet.
- Strategy A of §7.3 (upstream's copy), no patch to `i_video.c`.

**Done when** Doom's title screen, its menu over it, and its attract-mode
demos play on the panel, centred, in the right colours, with no stale rows
in the margins after a demo's level ends (the owner's look). **Measured**:
card mount and IWAD open time, `W_Init` time per IWAD, present time, frames
published, presented and dropped per second, core 0's copy time, core 1
stack high-water.

### M4: Keyboard and the WAD list

- Southbridge polling at 50 Hz on core 1, the event ring, the key map of
  §9.2 with its host test, the Shift trap (§9.3), UART key injection.
- `DG_GetKey()`; the WAD list page; Esc there leaves (power off or shell;
  the full exit is M8).
- `I_Error()`'s page.
- The questions of §9.5 answered on the keyboard, and logged.

**Done when** the owner moves through the WAD list and Doom's menus, starts
a new game and walks E1M1; every key, with each modifier, is logged with its
code and none is lost; the Shift trap cannot strand an arrow.
**Measured**: key-to-event latency (FIFO to `DG_GetKey`), and from a key to
the frame that shows it, by UART timestamps; releases delayed by
`I_GetEvent()`'s one a tic (§9.1).

### M5: First play, and the gate

- E1M1–E1M3 of the IWAD to hand played by keyboard, no audio.
- `-timedemo` of `doom.wad`'s four demos on the device, through
  `PICOCALC_DOOM_ARGS`; the device's frame hashes for demo 1 against M2's.

**The gate.** `-timedemo` frames per second at 300 MHz, with core 1
presenting, audio not yet present, and core 0's busy share in the heaviest
of the levels played. **Pass, proposed**: every demo at **35 fps or more**,
so normal play holds Doom's 35-frame cap in typical scenes. **Fail**: apply
the levers in order, each against the last as a control, and measure again
before any other milestone:

1. `I_VideoBuffer` in SRAM (§7.3, strategy B).
2. The colormaps in SRAM (8,704 bytes, a patch to `R_InitColormaps()`):
   every pixel looks one up, from the zone in PSRAM through the cache the
   code needs.
3. `R_DrawColumn`, `R_DrawSpan`, their low-detail and fuzz variants, and
   the masked-column path in SRAM (HW §9.2, §9.8: check each symbol and its
   callers with `nm`).
4. The copy by DMA (§7.3, strategy C).

Only if those fall short does the owner decide on a higher clock (D7).
Record the result either way, with the PC-sample profile of HW §2.7 for the
heaviest demo.

**Done when** the levels are played for ten minutes; the device's hash
list equals the host's; the gate's result and the levers' numbers are
recorded. **Measured**: fps per demo, busy share per level, the profile.

### M6: Audio

- The PWM shim of §8.2 at 49,718 Hz, upstream's sound module unchanged.
- Counters: underruns, late refills, the IRQ's longest run, mixing's share.
- The late path forced once with a scratch build (EL §6.4).

**Done when** effects and OPL music play correctly by ear (pitch, speed,
no carrier whine) in E1M1 and a Doom II map if owned, through the speakers
and the headphone jack. **Measured**: mixing's share of core 0 per tic with
music; underruns over ten minutes of play and across level loads; the gate
again, `-timedemo` with audio on.

### M7: Controls and settings

- Play-testing with the owner settles D12: the bindings, always-run, the
  strafe keys, whether Alt+arrows feel right, the poll rate.
- The `d_main.c` hunk applying the port's defaults (§9.4).
- `picocalc.ini` (§10.3) with its host test; the bottom band's messages.

**Done when** the owner plays a full episode-1 level comfortably on the
PicoCalc's keyboard and the bindings are recorded as D12's settlement; a
settings file with a bad line boots with the line named and the rest
applied. **Measured**: key-to-motion latency again at the settled poll rate.

### M8: Saves, configuration, exit, time, arguments

- The at-exit list and `I_Quit()` of §13.3; exit to the shell or power off.
- `get_fattime()` from the shell's clock (§13.2).
- Saves per IWAD (§10.2), if the owner agrees.
- Arguments from the shell (§13.4), if D15 is accepted.

**Done when** a save made in Doom loads after a power cycle; a change made
in Doom's options menu survives a quit and a power cycle; F10 then `y`
returns to the shell when started by it, and powers off when installed from
BOOTSEL; a save made after starting from the shell carries the shell's local
date and time on the card, and one made after a cold power-on without the
shell carries the fallback; `picocalc-doom -file <pwad> -warp 1 1` from the
shell starts that map with that PWAD. **Measured**: save write time;
time from `y` to the shell's first screen.

### M9: Performance pass and soak

In measured steps, each against a control build in the same sitting
(EL §12), keeping only what pays: whichever of M5's levers were not needed
for the gate; dirty rows in the presenter (§7.4); the slot renderer and
mixer in SRAM (§8.3); a smaller mix ring for effect latency (§8.3). Each
SRAM tier in its own build directory, symbols checked (HW §9.8).

Then a **30-minute soak** on battery, a level playing with keys typed over
the UART throughout, a script checking the log: one boot, heartbeats
throughout, late refills, I²C errors and dropped log lines zero, underruns
only at level loads, and the game visibly advancing (EL §11.5): frames
published rising in every heartbeat, and the tic count with them.

**Done when** the soak passes and every lever is recorded as kept or
dropped with its number.

### M10: Release

- Release build (§14.2); its own 30-minute soak with counters read over
  SWD and keys pressed by hand (EL §11.5).
- Checked on a Plus 2 and a Plus 2 W if both are to hand; otherwise say
  which.
- The `-timedemo` frame hashes equal M2's on the release image.
- README (card layout, keys, the shell, what is supported), credits and
  licence texts (upstream's `LICENSE` and attributions kept),
  `scripts/build_release.sh` producing
  `dist/picocalc-doom-v<x.y>-pico-plus2.uf2`.

**Done when** the release UF2 boots from UF2 Loader and from BOOTSEL, an
episode's first level plays with sound, and the documents describe what
shipped.

---

## 17. Unverified constants

Every value the port depends on that has not been checked on this hardware
with this workload (EL §14.2). Each becomes confirmed, or changes, at the
milestone named.

| Constant | Value | Source | Confidence | Settled in |
|---|---|---|---|---|
| Stable at 300 MHz, 1.20 V, under Doom | yes | HW §3; Quest's soak (other workload) | medium | M5, M9 soak |
| PSRAM at 100 MHz (divider 3) at 300 MHz | works | Quest's M1 round trip on a Plus 2 W (HW §2.4) | high | M1 |
| LCD SPI at 75 MHz | works | HW §4.4, Quest M1 | high | M1 |
| Flash at 75 MHz (divider 4, RX delay 4) | works | HW §3's sweep, Quest M8 | high | M1 |
| SD at 25 MHz with upstream's driver | works | the same `sdcard.c` as Quest's, confirmed there | high | M3 |
| Full 320×240 present from PSRAM | ~18.1 ms | HW §4.7 | high | M3 |
| PWM at 49,718 Hz: TOP 3016, oversample 2, 99.4 kHz carrier | right pitch, no audible carrier | arithmetic (§8.1); HW §5.2 on carriers | medium | M6 |
| EMU8950 plus 16 effect channels per tic at 300 MHz | affordable | not measured; `rp2040-doom` ran it on an M0+ | low | M6 |
| Alt+arrow delivered as the arrow with Alt held | yes | HW §6.3, from the MCU's source | medium | M4 |
| Three keys at once (arrow, Ctrl, Space) delivered | yes | not tried | low | M4 |
| Shift+Left/Right swallowed, release included | yes | HW §6.3, from the MCU's source | high | M4, by the trap's test on the keyboard |
| Lumps per IWAD (§12.1) | 2,308 / 2,928 / 2,991 / 3,109 (Doom, Doom II, Plutonia, TNT); shareware ~1,264 | the owner's WADs, read 2026-11-06 (§10.4) | confirmed, but the shareware count | Confirmed 2026-11-06 |
| This engine runs the 2024 re-release IWADs (extra lumps such as `GAMECONF`, `SKYDEFS`, `UMAPINFO`, ignored) | yes | not run; vanilla engines are said to | medium | M2 |
| A 3 MB zone holds every map of the commercial IWADs | yes | upstream ships it; not measured | medium | M2, M5 |
| SRAM left after the port, with `I_VideoBuffer` in SRAM and the directory in PSRAM | ~40 KB | §12.1's estimate on §3.4's build | medium | M0, M5 |
| Core 0's default stack is enough for Doom | yes | upstream runs on it | medium | M1, painted |
| Upstream's renderer changes do not desync vanilla demos | yes | not checked | medium | M2 |
| POWMAN timer unaffected by the move to 300 MHz | yes | HW §1.2, Quest M1 | high | M1 |
| The image fits UF2 Loader's partition | yes; 563 KB upstream | §3.4; HW §2.7 | high | M10 |

---

## 18. Risks

| Risk | Effect | Mitigation |
|---|---|---|
| Rendering through PSRAM and a shared 16 KB XIP cache is too slow at 300 MHz | Below 35 fps in play | The gate in M5 with its levers in order (SRAM video buffer, colormaps, column and span drawers, DMA copy); the PC-sample profile says which; a higher clock only as the owner's decision |
| The frame copy costs a third of core 0 (strategy A) | Low frame rate before any engine cost | Measured in M3; strategy B is the first lever (§7.3) |
| SRAM runs out with the video buffer in SRAM | Link fails, or the heap fails at `W_Init` | The directory to PSRAM in M3 (§12.1); sizes printed on every build |
| Keyboard latency or rollover is poor for an action game | Doom feels sluggish, or keys drop | 50 Hz poll; latency measured in M4; bindings play-tested in M7; `,` `.` strafing that does not need Alt |
| A held arrow sticks after Shift (HW §6.3) | The player turns for ever | Shift bound to nothing; held Left/Right released when Shift goes down; host-tested (§9.3) |
| Sound starves during level loads and card reads | Gaps in music | Expected and counted as underruns; the ring wrap keeps it clean (HW §5.3); shown only at loads in the soak |
| Mixing (OPL) costs too much on core 0 | Frame rate falls with music | Measured in M6; SRAM placement; mixing on core 1 only if the gate leaves room |
| Upstream's GPL-2.0 text and Quest's GPL-3.0 code cannot be combined | Cannot distribute | D9: license the port GPL-2.0-or-later, before M0 copies Quest's files |
| Saves shared between IWADs (§10.2) | One game's save loads in another | Per-IWAD directories in M8, if the owner agrees |
| A reset during a southbridge transaction kills the MCU until a power cycle (HW §6.1) | Dead keyboard, screen looks normal | Every reboot and exit parks core 1 first (§6.2) |
| Upstream changes files we patch | Patch rejects on a bump | Small patch, shims first (doomgeneric's seam), `check_sources.sh`; bump deliberately |

---

## 19. Dropped and deferred

**Dropped**, with reasons, so they are not re-planned (EL §14.5):

- **Pico, Pico 2, Pico 2 W.** No PSRAM; upstream's zone and screen live
  there.
- **A Plus 2 W image.** Nothing uses the radio (D2).
- **Multiplayer.** No network without the radio, and no second PicoCalc
  link.
- **Heretic, Hexen, Strife.** Not in upstream's engine.
- **A mouse.** None wired; Doom plays from the keyboard.
- **Scaling to fill the panel.** 320×240 to 320×320 is uneven, and costs
  wire time for nothing (EL §5.4).
- **Uncapped frame rate.** Chocolate Doom renders once a tic, by design.
- **Legacy of Rust** (`id1.wad` and its resource WADs) and anything else
  built for the 2024 re-release's ID24 features: new monsters, weapons and
  map formats that a vanilla engine does not have.
- **`iddm1.wad`.** Deathmatch maps; no multiplayer.
- **`extras.wad`.** The re-release launcher's content, 625 MB.

**Deferred**, with what would bring each back:

- **ENDOOM** on quit. Doom's 80×25 text screen fits the panel in a 4×12
  font (320×300). If the owner wants it; it delays the exit by however long
  it is shown.
- **Aspect correction.** Doom's 320×200 was drawn for pixels 1.2 times
  taller than wide; on the PicoCalc's square pixels it looks squat, as on
  any 1:1 display. A 1.2× vertical stretch of the 200-row screens fits the
  240 rows, but upstream's 240-row play layout already uses them for a
  taller 3D view, and the stretch is uneven (six rows for five). If the
  owner prefers vanilla's layout and proportions.
- **A faster keyboard bus.** The southbridge runs `i2c1` at 10 kHz
  (HW §6); whether its MCU answers faster has not been tried. If M4's
  latency or core 1's time calls for it, a measured experiment, written up
  in the hardware notes either way.
- **A port menu** (run mode, poll rate, save `picocalc.ini`). Doom's own
  menus cover volumes, bindings and screen size. If the settings file
  proves awkward.
- **An overclock above 300 MHz.** If the gate fails after its levers.
- **Hardware vertical scroll.** Nothing in Doom scrolls in a way it would
  help.
- **Mixing on core 1.** If M6 finds mixing heavy and the gate leaves core 1
  room, which §7.4 says it will not have much of.
- **No Rest for the Living and the Master Levels as proper mission packs**
  (§10.4). Later Chocolate Doom has `pack_nerve` and `pack_master`: their
  level names, NRFTL ending after MAP08 with MAP09 as its secret, and
  launching by `-pack`. A backport into this engine is a patch of
  `d_main.c`, `g_game.c`, `hu_stuff.c` and the intermission. If the owner
  wants them to play as on the re-release.
- **SIGIL and SIGIL II** (§10.4). Episodes 5 and 6 need the episode menu,
  the map-name, sky and intermission tables, and the end-of-episode logic
  extended beyond E4, or `UMAPINFO` support; a vanilla build of SIGIL that
  replaced E3 was released, but the owner's copy is not it. If the owner
  wants them, as an engine change beyond D1's minimal patch.
