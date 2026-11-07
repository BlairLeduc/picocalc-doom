# Milestone log

What each milestone verified, on which board, on what date, and what was
not checked, newest first. `design.md` §16 holds each milestone's scope
and done-when criteria; this file keeps the full record.

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
