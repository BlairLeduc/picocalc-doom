# Milestone log

What each milestone verified, on which board, on what date, and what was
not checked, newest first. `design.md` §16 holds each milestone's scope
and done-when criteria; this file keeps the full record.

## M2: host engine

**2026-11-06**, on the host only (macOS, Apple silicon, Apple clang 21),
CMake 4.4.5, upstream `153a950` with the patch; the owner's WADs in
`reference/` (design.md §10.4). Run by Claude with
`scripts/host_engine.sh`; not on a device.

| Check | Result |
|---|---|
| `doom_host` builds from the patched upstream with a headless platform, upstream's `w_file_stdc.c` and `m_misc.c`, a null sound module, the firmware's engine definitions (shared, `cmake/engine-definitions.cmake`) and a 3 MB zone | passed |
| Every demo under `-timedemo` to its end: Doom 4, Doom II 3, Plutonia 3, TNT 3, No Rest for the Living 3, Master Levels 3 | **19 of 19** played to their ends (table below) |
| `nerve.wad` and `masterlevels.wad` with `-file` over Doom II: every map starts and runs a minute | passed, 9 and 21 maps |
| Frame-hash list of Doom's demo1 | 1,709 tics; **1,708 the same in four runs** with the zone filled 0x00, 0xFF, 0x55, 0xAA and moved 0, 16, 4,096, 48 bytes; tic 206 varies and is marked |
| The list reproduces against itself (`host_engine.sh compare`) | same 1,708, different 0, skipped 1 |
| Zone and directory, every map of every WAD to hand, 2,100 tics at skill 4 | 162 maps, no failure; design.md §12.1, §12.2 |
| Host tests | 5 of 5 passed, with `test_frame_hash` and `test_engine_demo1` |
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
| Doom | 1 | 1,710 | E1M5 | 18 | 5 | 1,050 | 891,472 | 124 |
| Doom | 2 | 2,347 | E2M2 | 18 | 2 | 1,369 | 1,131,544 | 162 |
| Doom | 3 | 3,863 | E3M5 | 7 | 3 | 1,932 | 1,070,080 | 228 |
| Doom | 4 | 818 | E4M2 | 10 | 0 | 200 | 856,408 | 93 |
| Doom II | 1 | 1,205 | MAP11 | 8 | 0 | 301 | 1,003,968 | 187 |
| Doom II | 2 | 2,001 | MAP05 | 19 | 1 | 1,187 | 1,012,240 | 149 |
| Doom II | 3 | 4,471 | MAP26 | 35 | 24 | 1,932 | 1,028,320 | 258 |
| Plutonia | 1 | 7,403 | MAP17 | 63 | 24 | 2,356 | 1,200,320 | 531 |
| Plutonia | 2 | 3,483 | MAP10 | 15 | 1 | 1,647 | 1,100,472 | 283 |
| Plutonia | 3 | 5,662 | MAP12 | 49 | 12 | 2,178 | 1,212,216 | 549 |
| TNT | 1 | 4,531 | MAP01 | 34 | 3 | 2,087 | 1,016,296 | 257 |
| TNT | 2 | 3,653 | MAP12 | 21 | 0 | 1,728 | 1,444,880 | 275 |
| TNT | 3 | 3,433 | MAP13 | 33 | 0 | 1,646 | 1,361,384 | 218 |
| No Rest for the Living | 1 | 2,085 | MAP06 | 7 | 0 | 1,058 | 1,681,504 | 213 |
| No Rest for the Living | 2 | 2,001 | MAP01 | 14 | 10 | 1,094 | 1,986,416 | 246 |
| No Rest for the Living | 3 | 2,363 | MAP03 | 20 | 0 | 1,199 | 2,045,768 | 285 |
| Master Levels | 1 | 1,472 | MAP07 | 12 | 0 | 873 | 1,357,448 | 275 |
| Master Levels | 2 | 1,215 | MAP16 | 3 | 0 | 759 | 1,237,000 | 305 |
| Master Levels | 3 | 1,748 | MAP02 | 18 | 4 | 1,037 | 1,206,864 | 228 |

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
  takes 2.34 MB of the zone's 3 MB, the most of any map; the IWADs' worst is
  TNT MAP31 at 1.77 MB.
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
- Doom's bunny scroll (`PFUB2`, 560 wide, drawn by `f_finale.c`), and the
  intermission and end pictures by eye.
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
