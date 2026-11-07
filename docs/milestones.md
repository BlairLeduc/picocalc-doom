# Milestone log

What each milestone verified, on which board, on what date, and what was
not checked, newest first. `design.md` §16 holds each milestone's scope
and done-when criteria; this file keeps the full record.

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
