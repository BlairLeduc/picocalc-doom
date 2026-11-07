# picocalc-doom

A port of FRANK DOOM (Chocolate Doom/doomgeneric for the RP2350) to the
ClockworkPi PicoCalc with a Pimoroni Pico Plus 2 or Plus 2 W. Read
`docs/design.md` before changing anything; it is the plan, and its section
numbers (§N) and decisions (DN) are cited throughout the code.
`docs/hardware-notes.md` (HW §N) is the authority on the PicoCalc;
`docs/emulator-lessons.md` (EL §N) holds what earlier ports taught.
`docs/milestones.md` records what each milestone verified.

## Build and test

```sh
git submodule update --init                  # upstream, pinned, in modules/frank-doom
scripts/build.sh                             # development UF2: build/pico/picocalc-doom.uf2
scripts/build.sh -DPICOCALC_DOOM_UART=OFF build/pico-release   # the build that ships

cmake -S . -B build/host -DPICOCALC_DOOM_HOST=ON && cmake --build build/host
ctest --test-dir build/host --output-on-failure --no-tests=error   # host tests

scripts/check_sources.sh                     # our source lists still match upstream's build
```

`build.sh` finds the Pico SDK and Arm toolchain under `~/.pico-sdk/` unless
`PICO_SDK_PATH` names one that exists, and prints `arm-none-eabi-size`.
Build each variant in its own directory.

## Upstream, the patch, and our code (§5)

- `modules/frank-doom/` is the submodule. **Never edit it.**
- `frank-doom/` (gitignored) is upstream copied and patched by CMake at
  configure time. Never edit it either; it is overwritten.
- Every change to an upstream file is in `patches/frank-doom.patch`. To
  change one: `scripts/apply_patch.sh --working`, edit the file under
  `working/` at its upstream path, `scripts/create_patch.sh`. To add a file
  to the patch, copy it from the submodule into `working/` first.
- **No new files in the patch.** New code goes in `picocalc/`, as shims
  behind doomgeneric's `DG_*` seam and upstream's own APIs (§5.3). Prefer a
  shim to a patch.
- Each patched hunk carries a `picocalc-doom:` comment citing the design
  section that requires it.
- `cmake/upstream-sources.cmake` lists the upstream files we compile.
  After a submodule bump: `apply_patch.sh --working`, fix rejects,
  `create_patch.sh`, `check_sources.sh`, build.
- Doom's headers come before `<stdbool.h>` (and so before any Pico SDK
  header) in our files: `doomtype.h` declares `false` and `true` as enum
  values.

## Invariants (§6.2)

- The LCD is touched only from core 1, in thread context. No interrupt
  masking around a blit.
- Core 1 never reads engine memory; only a published pool buffer and the
  palette that came with it, and a published mix slot and EMU8950's state
  (§8.4).
- No `sleep_*` on core 1; it waits with `busy_wait_us_32`. Anything moved
  onto core 1 is searched for `sleep_`.
- Core 0 never blocks on the UART; stdio goes into the log ring.
- Everything on the audio refill path is in SRAM.
- No flash writes.
- No reset with a southbridge transaction in flight: every reboot or exit
  goes through one function that parks core 1 first.
- Shift is never part of a binding in play.
- Never touch GP23–25 or GP29 (the Plus 2 W's radio; the Plus 2's LED), or
  GP4/GP5 (the shell's DS3231 on `i2c0`). GP0/GP1 are left alone.

## Settled decisions (§2)

- **D1** Port by patching upstream, kept as a submodule, as `picocalc-quest`.
- **D2** Target Pico Plus 2 and Plus 2 W; one image, built for the Plus 2; no Wi-Fi.
- **D3** Upstream's 320×240 frame at y = 40 on the 320×320 panel.
- **D4** Exit to the shell through `picocalc_shell.h`; the date and time from the shell's clock.
- **D5** Debug Probe on SWD and UART0, GP2 (TX) / GP3 (RX), 115200 8-N-1.
- **D6** PicoCalc findings go into `docs/hardware-notes.md` as learned (below).
- **D7** 300 MHz at 1.20 V (revisited at the M5 gate).
- **D8** Start from `picocalc-quest`'s PicoCalc layer: copy, rename, re-verify.
- **D9** The port's code is GPL-2.0-or-later (`SPDX-License-Identifier` in every file).
- **D13** The port draws its own WAD list; straight into the game with one IWAD or `iwad =`.
- **D16** GitHub `BlairLeduc/picocalc-doom`; one branch and pull request per milestone, CI green.

Proposed decisions (D10, D11, D14, D15) need the owner's word before the
milestone that depends on them. D12 (the key map) is accepted for M4 and
settled by play-testing in M7; D14's Esc in the WAD list is settled, Doom's
Quit is not.

## Hardware notes (D6)

Whatever is learned about the PicoCalc goes into `docs/hardware-notes.md`
**as it is learned**, not at the end: a measurement, a quirk, a pin use, a
key chord the MCU does or does not deliver, or a correction to what the
notes say. Each entry stands alone: board, date, build and workload,
measured or read from source, and **no reference to this project's
documents or code**. What generalises about porting goes into
`docs/emulator-lessons.md` the same way. A milestone is not done until its
hardware findings are in the notes.

## Device work

Flash with `scripts/flash.sh` (from M1), never OpenOCD by hand: never
`reset run`, never a bare `halt` (HW §2.7). Capture UART0 before flashing.
Results of each milestone go in `docs/milestones.md`: date, board,
compiler, what was checked and by whom, and a not-checked list.
