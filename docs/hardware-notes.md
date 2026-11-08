# PicoCalc hardware notes

A standalone hardware reference for developers and agents writing software for
the **ClockworkPi PicoCalc**, primarily in C with the Raspberry Pi Pico SDK.
It covers wiring, peripheral protocols, timing, memory tradeoffs and observed
quirks. It can be copied into a new project without companion files.

**Scope, September–October 2026.** This is our current understanding of the platform.
Hardware observations cover Pico 2, Pico 2 W and Pico Plus 2 W configurations;
RP2040 guidance is based on documentation and SDK behavior, not equivalent
on-device validation. Measurements describe the stated board and workload,
not guaranteed limits for every unit. Distinguish requested/configured bus rates
from physical signal measurements, and successful listening/visual checks from
instrumented electrical validation. Official hardware and firmware references
are linked where relevant and collected in §11.

**Contents**

1. [The machine](#1-the-machine) · 2. [The processor board](#2-the-processor-board) ·
3. [Clocks and overclocking](#3-clocks-voltage-and-overclocking) ·
4. [Display](#4-the-display) · 5. [Audio](#5-audio) ·
6. [Keyboard and southbridge](#6-keyboard-battery-backlight-power) ·
7. [Storage](#7-storage) · 8. [On-chip odds and ends](#8-on-chip-odds-and-ends) ·
9. [Making it fast](#9-making-it-fast) · 10. [Bring-up checklist](#10-bring-up-order-and-trap-checklist) ·
11. [Official references](#11-official-references)

---

## 1. The machine

The PicoCalc is a **carrier board**, not a computer. It supplies a 320×320 SPI
LCD, a 4×N keyboard behind its own microcontroller, stereo PWM audio, an SD
slot, a battery and charger, and two backlights. The computer is a
Raspberry Pi Pico–form-factor module you plug into it, and everything on the
carrier reaches that module over **three buses and a pair of PWM pins**.

| Peripheral | Bus | Starting configuration |
|---|---|---|
| LCD (ST7789P / ST7365P class) | `spi1`, 4-wire | 25 MHz; 75 MHz has also been exercised (§4) |
| Southbridge — keyboard, battery, backlights, power | `i2c1`, addr `0x1F` | 10 kHz |
| SD card | `spi0` | 400 kHz init → 25 MHz requested |
| Audio | PWM, both channels of one slice | choose carrier and sample rate together (§5) |

### 1.1 GPIO map

Everything the carrier drives, plus the pins the processor board itself claims.
GP numbers are the Pico header numbering and are **the same on all five boards**
— the Pimoroni Plus 2 W maps its RP2350B pins so that GP0–GP29 land where a Pico
puts them. Use the module's board definition for its on-board peripherals.

| GP | Used by | Function |
|---:|---|---|
| 0–5 | — | Free on the carrier. Debug UART on GP2/GP3 (UART0) or GP4/GP5 (UART1); an add-on I²C device such as a DS3231 on GP4/GP5 (`i2c0`); see below |
| 6 | carrier | `i2c1` SDA → southbridge |
| 7 | carrier | `i2c1` SCL → southbridge |
| 8–9 | — | free |
| 10 | carrier | `spi1` SCK → LCD SCL |
| 11 | carrier | `spi1` TX → LCD SDI |
| 12 | carrier | `spi1` RX ← LCD SDO |
| 13 | carrier | LCD CS — **plain GPIO, driven by hand** |
| 14 | carrier | LCD D/CX — plain GPIO |
| 15 | carrier | LCD RST — plain GPIO |
| 16 | carrier | `spi0` RX ← SD MISO |
| 17 | carrier | SD CS — plain GPIO |
| 18 | carrier | `spi0` SCK |
| 19 | carrier | `spi0` TX → SD MOSI |
| 20–21 | — | free |
| 22 | carrier | SD card detect, **active low**, needs an internal pull-up |
| 23 | *board* | SMPS mode (non-W) / `WL_REG_ON` (W) |
| 24 | *board* | VBUS sense (non-W) / CYW43 data (W) |
| 25 | *board* | status LED (non-W) / CYW43 CS (W) |
| 26 | carrier | PWM5 A → audio **left**, via an RC low-pass into the amplifier |
| 27 | carrier | PWM5 B → audio **right** |
| 28 | — | free (ADC2) |
| 29 | *board* | VSYS/3 on ADC3 (non-W) / CYW43 clock (W) |

So a PicoCalc application has roughly **eleven free GPIOs** (0–5, 8–9, 20–21,
28) plus, on a Plus 2 W, the RP2350B's extra pins that are not on the header.
"Free" here means *not assigned to the peripherals listed above*; confirm against the
PicoCalc schematic before you route a signal to one, because the carrier may
still connect it.

GP0–GP5 offer two UARTs and one I²C bus between them, and two uses
compete for them:

| GP | Functions | Typical use |
|---:|---|---|
| 0 / 1 | UART0 TX/RX, `i2c0` SDA/SCL | The PicoCalc's USB-C serial is the Pico's default serial port (ClockworkPi wiki), so leave these to it |
| 2 / 3 | UART0 CTS/RTS; on RP2350, UART0 TX/RX through the `UART_AUX` function | Debug UART when GP4/GP5 are taken |
| 4 / 5 | UART1 TX/RX, `i2c0` SDA/SCL | Debug UART, **or** an add-on I²C device |

UART0 cannot be routed to GP4/GP5; those pins carry only UART1 or `i2c0`.
A DS3231 real-time clock (which the carrier lacks, §1.2) fitted on `i2c0`
at GP4 (SDA) and GP5 (SCL), 100 kHz, alone on the bus, takes those pins.
The debug UART then moves to UART0 on GP2 (TX) and GP3 (RX): set
`PICO_DEFAULT_UART=0`, `PICO_DEFAULT_UART_TX_PIN=2` and
`PICO_DEFAULT_UART_RX_PIN=3`, and the SDK selects the pins' `UART_AUX`
function itself. Used this way on a Plus 2 W since October 2026, in a
development build whose log and typed keys went over GP2/GP3 through the
Debug Probe. A firmware that runs alongside such a clock should leave
GP4/GP5 unconfigured even if it never uses the clock.

Whichever pins the UART uses are only free in a build that leaves the UART
alone, and the UART is how the Debug Probe gets your log (§2.7). Either
keep a development build with the UART and a release build without it, or
hand the pins back and forth at run time and say which is lost. The pads are 3.3 V at default drive
strength; a circuit built for a 5 V port may need a level shifter or a
buffer. For inputs, use the internal pull-up and active-low wiring (§2.2's
E9 erratum). Verified on a Plus 2 W on 2026-09-28: an LED driven push-pull
from a free pin, and GP2 grounded reading low against its pull-up.

### 1.2 Four things the hardware does not give you

Design around these from the start; each one has cost a project time.

- **No tearing-effect (TE) signal.** Six wires reach the panel and none of them
  is TE, so you cannot sync a present to the panel's own refresh. Tearing is
  managed by *when and in what order* you send, not by waiting for a strobe.
- **No keyboard interrupt.** The southbridge has no interrupt line to the
  processor (the pin exists in its firmware and is commented out). Input is
  polled, always.
- **No audio DAC.** PWM is the output hardware. Whatever you want to hear, you
  synthesize or decode in software.
- **No battery-backed real-time clock.** The RP2350's AON timer (RP2040's RTC)
  loses time at power-off. Wall-clock time comes from the network on a W board,
  or from the user. Once set, the RP2350's timer keeps running through a
  system reset (a watchdog reboot, an SWD reset) and through a move of
  `clk_sys`, since it counts its own 1 kHz tick: on a Pimoroni Pico Plus 2 W
  on 2026-10-29, read on each side of the 150 → 300 MHz switch (PLL, rail and
  flash divider), it advanced 1–2 ms across 1.3–1.6 ms of uptime, as its
  millisecond resolution allows, and agreed with the workstation's UTC.
  The same on 2026-11-06, with the flash at divider 4 and RX delay 4 at
  300 MHz: 2 ms across 1.50 ms of uptime on that Plus 2 W, twice, and 2 ms
  across 1.97 ms on a Pimoroni Pico Plus 2 (no W) started by UF2 Loader.

---

## 2. The processor board

### 2.1 The five boards

| Board | Part | Cores | SRAM | Flash | PSRAM | Radio |
|---|---|---|---:|---:|---:|---|
| Pico | RP2040 | 2 × M0+, 200 MHz max | 264 KB | 2 MB | — | — |
| Pico W | RP2040 | 2 × M0+, 200 MHz max | 264 KB | 2 MB | — | CYW43439 |
| Pico 2 | RP2350A | 2 × M33, 150 MHz | 520 KB | 4 MB | — | — |
| Pico 2 W | RP2350A | 2 × M33, 150 MHz | 520 KB | 4 MB | — | CYW43439 |
| Pimoroni Pico Plus 2 W | RP2350**B** | 2 × M33, 150 MHz | 520 KB | 16 MB | 8 MB | CYW43439 |

Device evidence covers the three RP2350 boards. The RP2040 rows describe
compatible module specifications, not an equivalent application test history.

**Record physical board identity separately from the SDK build target.** A
Pico Plus 2 W (RP2350 revision 2, 16 MiB Winbond flash) and a Pico 2 W
(revision 2, 4 MiB Winbond flash) both ran a `pico2` application build during
September 2026 tests. The Pico 2 W also ran a `pico2_w` build. A banner saying
`board=pico2` identifies compilation settings, not the installed module.
Successful common-peripheral operation does not qualify the wrong board
configuration for PSRAM, Wi-Fi, ADC channel selection or full flash capacity.

### 2.2 RP2040 versus RP2350, in the terms that change your code

| | RP2040 | RP2350 |
|---|---|---|
| Core | Cortex-M0+ | Cortex-M33 (or Hazard3 RISC-V, switchable at boot) |
| Float | **no FPU** — single precision is a bootrom software library | **single-precision FPU in hardware**; double is still software |
| SRAM | 264 KB | 520 KB |
| DMA channels | 12 | 16 |
| PWM slices | 8 | 12 |
| PIO blocks | 2 × 4 SMs | 3 × 4 SMs |
| ADC channels | 4 + temperature | 4 + temp (RP2350A) / 8 + temp (RP2350B) |
| Bank 0 GPIOs | 30 | 30 (A) / 48 (B) |
| External QSPI | flash only | flash **and** a second chip select for PSRAM |
| XIP cache | 16 KB | 16 KB |
| Also | — | HSTX, TrustZone, signed boot |

Two consequences worth stating plainly:

- **Prefer `float` when its precision is sufficient.** On RP2350 that is a hardware
  instruction and on RP2040 it is the cheaper of two software paths. A stray
  `double` literal (`2.0` instead of `2.0f`) silently promotes an entire
  expression.
- **An RP2040 build is a different memory problem, not a slower one.** A 320×320
  8-bit framebuffer is 100 KB: 19 % of an RP2350's SRAM and **39 % of an
  RP2040's**. If you intend to support a Pico, size the framebuffer question
  first and everything else after.

**Erratum RP2350-E9** affects GPIOs used as inputs with the internal *pull-down*
enabled: the pad can latch at an intermediate level instead of reading low.
Prefer active-low inputs with pull-ups (which is what the carrier does for card
detect), or fit an external pull-down, and read the current errata list in the
RP2350 datasheet before wiring a button to a spare pin.

### 2.3 SRAM is the scarce resource

On RP2350, 520 KiB total. Budget code placed in SRAM, static buffers, both
cores' stacks, DMA storage and the peak heap together. Static regions can fit
at link time while leaving too little space for initialization allocations.
Check the linker map and measure heap and stack high-water use under load;
allocator arena size and linker minimum-heap reservations may overlap.

Framebuffer arithmetic, since it dominates everything else:

| Buffer | Bytes | Verdict |
|---|---:|---|
| 320×320 @ 8 bpp indexed | 102,400 | fits, with room to work |
| 320×320 @ 16 bpp RGB565 | 204,800 | 39 % of SRAM; possible but crowds everything |
| Double-buffered 8 bpp | 204,800 | same cost, and see §4.10 |
| Double-buffered 16 bpp | 409,600 | fits within RP2350 total SRAM, but leaves only 122,880 bytes for everything else |
| Two DMA line buffers (320 px × 2 B × 2) | 1,280 | what a streaming blit needs |
| One 16 bpp tile row band (320 × 16 px) | 10,240 | a banded renderer's whole working set |

An 8-bit indexed canvas with a 256-entry RGB565 palette expanded at blit time
is a useful compromise. A band renderer or compact monochrome snapshot is
cheaper still (§4.10). Double buffering is an application memory-budget choice;
it is not categorically unavailable on RP2350, and it does not by itself
synchronize writes to the panel's scan.

Keep fixed capacities in one header rather than scattered `#define`s; you will
be trading them against each other repeatedly.

**A large alignment in `.bss` leaves a hole before it.** An audio ring
that the DMA wraps with its ring-size field must be aligned to its own
size, and an 8 KB ring raises the whole `.bss` output section's
alignment to 8 KB, so `.bss` starts at the next 8 KB boundary after
`.data` and the bytes between belong to nothing. Read from the ELF
(`arm-none-eabi-nm`, `objdump -h`) of a Pico SDK 2.3.1 RP2350 build,
2026-11-07: `.data` ended at `0x20016ef4` and `.bss` began at
`0x20018000`, 4,364 bytes unused. Code or initialised data moved into
SRAM (`.time_critical.*`, `.data.*` sections) fills the hole without
taking anything from the heap, until `.data` crosses the boundary and
`.bss`, with the heap's start, jumps 8 KB at once. Check where `.data`
ends whenever SRAM code is added.

**Core 0's default stack is small, and core 1's is right under it.** Read
from the Pico SDK 2.3.1 RP2350 default linker script, 2026-11-06: core 0's
stack starts at the top of SCRATCH_Y (4 KB at `0x20081000`) and grows down;
core 1's stack (`multicore_launch_core1`, 2 KB by default) is the top of
SCRATCH_X, at `0x20080800`–`0x20081000`, directly below. Anything placed in
`.scratch_y` (`__scratch_y("…")` data, `.section .scratch_y.*`) sits at the
bottom of SCRATCH_Y and comes out of core 0's room. `PICO_STACK_SIZE` only
sizes a dummy section; nothing stops core 0 at it. A core 0 that goes
deeper than its room writes over core 1's stack, silently, while both run.
**Measured** on a Pimoroni Pico Plus 2 W, 2026-11-06, Pico SDK 2.3.1: a
program whose OPL2 emulator keeps a 1 KB table in `.scratch_y`
(`__scratch_y_start__` `0x20081000`, `__scratch_y_end__` `0x20081400`) left
core 0 3,072 B. Painted from `__scratch_y_end__` to the stack pointer at the
top of `main()` and scanned for the first word changed, a Chocolate
Doom-derived engine reached **2,004 B** of it running its title and demos
of Doom II unseen at 300 MHz, about 1 KB short of core 1's stack. Paint
both stacks from bring-up and report both high-water marks; a core 0 at its
whole room has already run into core 1's.

### 2.4 PSRAM — Plus 2 W only

8 MB, on QMI chip select 1 (GPIO 47). With a matching board definition and
PSRAM-enabled runtime, it can be initialized before `main()` and exposed as a
memory-mapped window. Do not assume a generic `pico2` build enables it. Three things to know:

- **Verify it before trusting it.** The SDK's init checks the chip ID but not
  that the window works at speed. Write distinct patterns at spread offsets,
  `xip_cache_clean_all()` + `xip_cache_invalidate_all()` to force a real round
  trip, then read back. A marginal QMI configuration then degrades to SRAM-only
  instead of handing your allocator a region that hardfaults on use.
  It is cheap: on a Pimoroni Pico Plus 2 W at 300 MHz with the PSRAM at
  100 MHz (divider 3, RX delay 3), 2026-10-29, a word at every power-of-two
  offset plus 64 KB of pseudo-random words spread over the 8 MB, written,
  read back and written inverted, 32,790 words with three clean-and-invalidate
  round trips, took 16.3–16.5 ms and passed. The same check on the same
  board, 2026-11-06, another program's development build, with core 1
  running a polling loop from flash meanwhile, took 17.1–17.2 ms in two
  boots and passed.
- **PSRAM reads go through the same 16 KB XIP cache as instruction fetch from
  flash.** Cached reads are SRAM-speed; a cache-hostile access pattern is not,
  and it contends with your code. §9.3 has the measurement.
- **Its QMI timing is computed from `clk_sys` once, before `main`.** Change the
  system clock and it is wrong — see §3.
- **A copy from PSRAM to PSRAM is slow.** On a Pimoroni Pico Plus 2 W at
  300 MHz with the PSRAM at 100 MHz (divider 3), 2026-10-29, development
  build: copying a 320×200 8-bit frame (64,000 bytes) from one PSRAM buffer
  into another with `memcpy` through the cached window, clearing the 12,800
  bytes above and below it, and cleaning the destination's cache lines took
  **9.6 ms**, timed around the call: about 8 MB/s. A program that made two
  such copies on every screen update, at 36–43 updates a second, spent
  **64–83 % of core 0 in `memcpy` and `memset`** (PC samples, §2.7), and the
  programs themselves under 5 %. Copying only when the picture had changed,
  and once instead of twice, brought `memcpy` to 4 %. Count updates against
  changes before optimising anything else.

  The same copy at 320×240 (76,800 bytes), measured on a Pimoroni Pico
  Plus 2 W at 300 MHz with the PSRAM at 100 MHz, 2026-11-07, development
  build, a game copying its finished frame from one PSRAM buffer into
  another with `memcpy` through the cached window once a frame, 25–29
  frames a second, for minutes at a time: **9.5–9.7 ms** a frame on
  average, the longest 11.3 ms. Cleaning the destination's 76,800 bytes
  out of the XIP cache afterwards with `xip_cache_clean_range`, with a
  512-byte copy beside it, took **0.44–0.47 ms** on average, the longest
  1.55 ms.

**A DMA copy from SRAM into PSRAM's uncached alias** takes the copy off
the CPU. Measured on a Pimoroni Pico Plus 2 W at 300 MHz with the PSRAM at
100 MHz, 2026-11-07, development build, Arm GCC 15.2.Rel1: a game's
finished 320×240 8-bit frame (76,800 bytes) copied from SRAM to a buffer
at `0x15xxxxxx` by one DMA channel, 32-bit transfers, both addresses
incrementing, default priority, while core 0 ran the game's next tic from
flash and core 1 read finished frames out of the PSRAM by DMA from the
same alias: **4.4–4.7 ms** a copy on average, the longest 6.4–8.0 ms
(about 17 MB/s), against 6.1–6.2 ms of CPU time for the same copy through
the cached window followed by a clean. The DMA writes past the XIP cache,
so no clean is needed, but the writer must never have the buffer's lines
in the cache (clean and invalidate them once, then touch the buffer only
through the uncached alias). The other core's DMA reads of the PSRAM
slowed: a 320×240 frame read row by row and sent to the LCD took 22.8 ms
on average against 21.3 ms with the copy on the CPU, with frames
finished at 57 and 45 a second. The game, `-timedemo` with its copy
overlapped with the next tic: 44.5 to 57.2 frames a second.

Use PSRAM for **capacity** (level data, sprite banks, audio samples, a text
buffer) and SRAM for the **per-frame working set**.

### 2.5 The status LED

Not on the PicoCalc's front panel — it is the small LED on the processor board,
visible through the case. It is a different piece of hardware on each board:

- **Pico / Pico 2**: `PICO_DEFAULT_LED_PIN`, GPIO 25, a plain `gpio_put`.
- **Any W board**: there is no such pin. The LED is `CYW43_WL_GPIO_LED_PIN`,
  WL_GPIO 0 **on the wireless module**, reached by `cyw43_gpio_set`.

SDK 2.1's `pico_status_led` hides that difference and is the reason no board
conditional is needed. But the consequence does not go away: **lighting the LED
on a W board powers the radio**, so the first call costs the CYW43 firmware
upload (about a second) and leaves the module up.

If you use both the LED and Wi-Fi, bring the driver up **once**:
`status_led_init()` with no argument builds its own `async_context` and calls
`cyw43_driver_init` itself, which a later `cyw43_arch_init` then trips over.
Pass `status_led_init_with_context(cyw43_arch_async_context())` after your own
Wi-Fi init instead — one context, one driver, one initialised flag. This costs
72 bytes of otherwise-dead SRAM on a W board and is still the right trade.

### 2.6 Wi-Fi

Present on Pico W, Pico 2 W and Plus 2 W (all CYW43439). Two hardware facts
matter more than the driver API:

- **The radio needs SRAM.** The driver plus lwIP is a large fixed cost on a board
  that has no PSRAM to move it to. On a Pico 2 W this is the difference between
  budgets, not a rounding error.
- **TLS needs a measured peak-memory budget.** Its handshake heap competes
  with framebuffers and application state. PSRAM can make larger configurations
  practical, but HTTPS is not inherently limited to boards with PSRAM.

The driver reaches the chip over a **PIO SPI whose divider is fixed against
`clk_sys` when the bus comes up** (§3).

### 2.7 Getting code onto the board, and getting output back

- **SWD is a practical development path.** A CMSIS-DAP Debug Probe and OpenOCD
  can flash, verify and reset the RP2350 using `target/rp2350.cfg`; 5 MHz SWD
  has worked on tested units. The target must be powered for the debug port
  to respond. Connect SWDIO, SWCLK and ground correctly.
- **UF2 over USB works** when BOOTSEL is accessible. Identify the exact image
  and its intended startup behavior before treating a blank screen as a boot
  failure: diagnostic firmware may deliberately start blank and silent.
- **UART is a separate connection from SWD.** UART1 at **115200, 8-N-1**, TX
  GP4 and RX GP5, was verified in both directions through the Debug Probe.
  So was UART0 on GP2 (TX) / GP3 (RX) through `UART_AUX`, the arrangement
  to use when GP4/GP5 carry `i2c0` (§1.1).
  Cross TX/RX and share ground. Discover the serial endpoint rather than
  hardcoding a workstation path.
- **Keep a Mac's display awake while the probe is in use.** On a Mac, when
  the display sleeps the Debug Probe wedges: its serial port goes away
  ("Device not configured") or the UART goes silent, and it does not
  enumerate again until it is unplugged and plugged back in. `caffeinate -s`
  is not enough, since it prevents only system sleep and lets the display
  sleep; `caffeinate -d` (or `-dims`) keeps the display up. Found by the
  owner of the PicoCalc emulators, 2026-10-04, after two unexplained
  drop-outs: both cores were still running their normal loops each time.
- **UF2 Loader's app partition is the flash less 8 KB, as the loader was
  built.** UF2 Loader (pelrun, 2.5) puts one partition table in the first
  8 KB of an RP2350's flash, with one partition, "App", from 8 KB to the
  end of the flash size the loader was compiled for
  (`PICO_FLASH_SIZE_BYTES / 1024 - 8` KB, `stage3/partitions.json.in`).
  The released `bootloader_pico2.uf2` is built for the Pico 2's 4 MB, so
  on a Plus 2 or Plus 2 W, with 16 MB fitted, an application has
  **4,088 KB**, not 16 MB, unless the loader is rebuilt for 16 MB. The
  loader remaps XIP so the partition appears at `0x10000000`; an image
  built for that address runs unchanged. The bootrom's
  `get_partition_table_info` reads the partition from the table it keeps
  in RAM, without touching flash, but that table is only there once something
  has loaded it: a program started by the loader finds none, and reports
  no partition (read on a board, 2026-11-01). Read from source, 2026-10-31.
  **Read on a
  board**, 2026-11-01, with `picotool info -a` in BOOTSEL (no probe needed):
  the shell's 16 MB build, `2.5-shell`, has App at `0x2000` to `0xfff000`,
  16,372 KB. That build starts the shell at power-on; Up, F1 or F5 start the
  program.
- **A BOOTSEL copy under UF2 Loader goes into the App partition**, and may
  not finish. One such copy from a Mac (2026-11-01) left 3,840 bytes erased,
  from `0x100d2100` in the image, in the Gob engine's code: the sector was
  erased, its first block written, and nothing after. The rest matched only
  because the same image was already there. The drive going away does not
  show a copy finished; compare with `picotool save` and the UF2's blocks.
  Why it stopped is not known. After `flash_nuke.uf2`, the same image copied
  and ran on a Plus 2 W.
- **Configure stdio explicitly.** Select UART, USB CDC or a custom LCD
  `stdio_driver_t` in the application. Keep CMake's UART definitions consistent
  with the pin header. Multiple enabled stdio drivers can receive output;
  synchronous logging can also consume the time needed by audio or simulation.

- **Reset both cores together from OpenOCD, or core 1 is lost.** The
  `rp2350.cfg` shipped with the Pico SDK's OpenOCD (0.12.0+dev) gives each core
  `reset_config sysresetreq` and asserts it per core, and on RP2350 that resets
  only the core that asks. `reset run` — including the reset in
  `program … reset` — restarts core 0 first; the firmware boots and launches
  core 1 within milliseconds, and then OpenOCD resets core 1 back into the
  bootrom, where it waits for a launch that has already happened. It looks
  like core 1 printing one line and hanging, intermittently, depending on how
  far it got. Found 2026-09-22 on a Plus 2 W; SWD showed core 1 at PC `0xda`
  with the bootrom's `0xf0000000` stack. Use `reset halt` then `resume`, so
  both cores are held until the per-core resets are done. A power-on reset is
  unaffected.

On macOS, keeping the serial descriptor open while setting baud and reading
avoids an adapter reverting to its defaults between a separate `stty` command
and the reader. A startup banner plus consecutive heartbeat messages is more
useful boot evidence than a single line.

Three more things about the UART, each found the hard way:

- **A blocking `printf` costs real time.** At 115200 baud a few-hundred-byte
  status line takes ~30 ms, longer than an audio queue's slack (§5.8). On a
  core with a deadline, format into a ring and let the other core move bytes
  into the UART FIFO as it has room; count dropped lines.
- **Let one process read the port.** Two readers split the byte stream and
  both logs come out scrambled. Make a capture script refuse a port that
  something else already has open, and start the capture before flashing so
  the banner is in it.
- **The logs may have CR line endings** and UTF-8 in them. Some `grep`
  replacements (ugrep) print nothing on such a file; read it with Python if a
  search comes back empty.

The UART also works the other way: firmware that turns received bytes into
key events can be driven with no one at the keyboard, which is how scripted
performance runs and soaks are done. Pace the characters to what the
application can absorb.

**A build with no UART can still be read over SWD**, with both cores
running. OpenOCD started with `init` and no `reset` or `halt` reads memory
through the debug port's memory access (`read_memory addr 32 n`), and the
target never stops. Keep the counters in one block at a fixed symbol, with a
magic word and a layout number, and find its address with `nm`; a debugger
that attaches as gdb does halts the cores, and a halted audio producer
underruns. **Measured** on a Plus 2 W, 2026-10-05: 30 such reads over 44 s,
each a fresh OpenOCD, left the audio's underrun and late-refill counts at 0
and dropped no video frames. Each read takes about a second, most of it
OpenOCD starting.

**A profile over SWD, without stopping the core.** The Cortex-M33's DWT
has a PC sample register, `DWT_PCSR` at `0xE000101C`: each read returns
the address the core is executing. Set `DEMCR.TRCENA` (bit 24 of
`0xE000EDFC`) first. OpenOCD started with `init` and no `reset` or `halt`,
with the core's target selected (`targets rp2350.cm0`, or `cm1`; each core
has its own register), can read it in a Tcl loop and write the samples to a
file; `nm -n` turns them into functions. **Measured** on a Plus 2 W at
300 MHz through a CMSIS-DAP Debug Probe at 5 MHz SWD, 2026-10-29: **about
4,000 samples a second**, with the program running on and its frame and
key counters unaffected. A sample of `0xFFFFFFFF` means the core is halted.
It names only the function the PC is in, not who called it.

**With a debugger attached, a breakpoint halts the core, and the timer
stops with it.** The SDK's `_exit()` is a `bkpt` loop, so a program that
calls `exit()` halts its core when the probe has enabled debug (`DHCSR`
`S_HALT`, bit 17); without the probe, it faults. The RP2350's system
timer is paused by default while either core is halted in debug, so the
other core's `busy_wait_us` never returns and it stops too: no log, no
display, though it is not halted. Seen on a Plus 2 W, 2026-10-29. Read the
core's `pc`, and the stack for return addresses, to find the caller; a log
held in an SRAM ring can be dumped over SWD (`dump_image`) to recover the
last lines the stopped core never sent.

**A memory read over SWD is not atomic against the running core.** A block
of a few dozen words read while the firmware rewrites it can come back
half old and half new. In a 30-minute soak sampled every 10 s on a Plus 2 W
(2026-10-05), one such torn read made two windows of a field counter
read a tenth fast and a tenth slow. Read the block twice in one
OpenOCD session and keep it only when both reads agree, retrying a few
times; or have the writer bump a sequence word before and after the update
and discard a read whose two sequence words differ.

**Flashing over SWD does not stop the audio's DMA, and OpenOCD's buffers
can end up on the speaker.** OpenOCD's reset on the RP2350 resets only
the cores (`SYSRESETREQ` per core), so the DMA and PWM blocks run on
through `program`. A PWM audio output fed by DMA from a ring in SRAM keeps
playing that ring while the cores are halted. The Raspberry Pi fork's
`target/rp2350.cfg` puts the flash work area at `0x20010000`, 64 KB, and
`program` writes its algorithm and data buffers there. **Seen** on a
Plus 2 W in a PicoCalc, 2026-10-30, OpenOCD 0.12.0+dev: an image whose
8 KB audio ring sat at `0x20014000` played loud, garbled sound through the
speaker for the length of the next flashing. Images with the ring at
`0x20008000` and `0x20024000` were flashed silently the same day. A
linker change that moves `.bss` can move the ring into the work area
without warning. Before `program`, halt and hold DMA and PWM in reset
through RESETS' atomic set alias: `mww 0x40022000 0x00010004` (DMA bit 2,
PWM bit 16, RP2350). The SDK's start-up takes every block out of reset
again. **Checked by ear** the same day, the same image flashed over
itself from the same idle screen: the old sequence (`program`, then
`reset halt`, `resume`) was loud again; with `init`, `halt` and the
`mww` first, it was silent.

**Reset the cores before that, do not just halt.** OpenOCD's `halt`
stops core 0 alone, and the RP2350's timer pauses while either core is
halted in debug, so core 1 stops where it is: inside a southbridge I²C
transaction, it holds the bus for the whole flashing. **Seen** on the
same board, 2026-10-30: with `init`, `halt`, `mww`, `program`, the
southbridge's MCU did not answer after three flashings of four, until a
power cycle (the PicoCalc trap of §6.1). With `init`, `reset halt`, `mww`,
`program`, four flashings in a row, three of them with a game running,
left it answering every time.

**Setting the Debug Probe's UART again loses bytes in flight.** A
second program that opens the port and sets it up (`stty`, the same
115200 8-N-1 it already has) while a capture reads it makes the probe
set its UART up again, and bytes on the way are lost. **Seen** on the
same board, 2026-10-31: `keys.sh` set the port up on every key, about
300 times in a 30-minute soak, and 4 whole heartbeat lines were missing
from the capture while the firmware's log ring dropped none. Setting it
up only when no other program holds it, the next two 30-minute runs lost
none.

Opening the port again, without setting it up, can lose bytes too.
**Seen** on a Pimoroni Pico Plus 2 W on macOS, 2026-11-07, a capture
holding the port: a key-typing script that opened the port afresh for
each 12-second round of keys, 15 openings in 3 minutes, left one heartbeat
line missing from the capture and another missing 70 characters in its
middle, while the firmware's log ring dropped none. The same keys typed
by one process that opened the port once, for 3 minutes and then for 30,
lost nothing. Open the writer once for a whole run.

**The chip's reset record outlives a debugger reset.** POWMAN's
`CHIP_RESET` names the last reset of the whole chip, and OpenOCD's
resets only the cores, so after a flash it still names the last
power-on; the watchdog's `REASON` names a forced or timed watchdog
reset. The banner prints both (found 2026-10-31).

**PSRAM reads over SWD with the program running, no halt.** OpenOCD's
`init`, then `dump_image FILE ADDRESS LENGTH` at the cached XIP address
the program uses (`0x11xxxxxx`), then `shutdown`, reads PSRAM while both
cores run. **Measured** on a Pimoroni Pico Plus 2 W at 5 MHz SWD,
2026-11-01: 67,200 B in 0.34 s, about 194 KiB/s, repeatedly, with a game
playing and audio running; the game carried on and a UART capture on the
same probe kept logging. A byte-difference count the firmware computed
between the same two buffers matched the dump's exactly, so the read sees the data the CPU
sees. It lets you inspect a picture fault while it is still on the panel.

---

## 3. Clocks, voltage and overclocking

The SDK's default `clk_sys` is **150 MHz** on RP2350 and **125 MHz** on RP2040,
but neither default is the part's limit, and the two parts differ in whether
going faster is *supported* or merely *possible*.

**RP2040 is rated to 200 MHz**, not the 133 MHz its original datasheet
specified — Raspberry Pi re-rated the part, and the SDK carries the higher
figure as the fastest *supported* clock. It is not the default, for backward
compatibility; you opt in with `PICO_USE_FASTEST_SUPPORTED_CLOCK=1` (or by
setting `SYS_CLK_KHZ` yourself). Two things come with it, both visible in
`hardware/clocks.h`: the SDK switches to a 1200 MHz VCO ÷6 ÷1 for the system
PLL, and — because 200 MHz needs more than the stock rail —
`SYS_CLK_VREG_VOLTAGE_AUTO_ADJUST` turns itself on and raises the core to
**1.15 V** during clock init, with a settle delay. That is a supported
configuration, not an overclock.

**RP2350 is rated to 150 MHz and the SDK offers nothing above it.** It will run
past that — applications have run at 300 MHz on tested Pico 2, Pico 2 W
and Plus 2 W units — but **everything above 150 MHz is outside the datasheet**, and you are
raising the rail yourself (the rail note at the end of this section).

Either way, the interesting part is not whether the core survives. It is that
five other things are derived from `clk_sys`, and each of them breaks
differently.

**Measured, Plus 2 W, 2026-08-23** (interpreter throughput, so pure CPU work):

| `clk_sys` | CPU work | speedup | clock ratio |
|---:|---:|---:|---:|
| 150 MHz | 52.5 µs/statement | 1.000 | 1.000 |
| 200 | 39.5 | 1.329 | 1.333 |
| 250 | 31.5 | 1.667 | 1.667 |
| 300 | 25.5 | 2.059 | 2.000 |

Inside 3 % at every point: **CPU-bound code is purely clock-bound here.** Die
temperature went 24.3 → 26.9 °C over a 200-frame run at 300 MHz. That short run does not establish a thermal bound for other workloads,
ambient temperatures or units.

What you must fix up when you move the clock:

1. **`clk_peri` does not follow `clk_sys` by default.** `set_sys_clock_pll`
   parks it on the USB PLL at 48 MHz, so an `spi_set_baudrate(75 MHz)` quietly
   delivers 24 MHz and your display gets *slower* the faster you clock the CPU —
   we measured a present going 19.6 → 58 ms, flat across every overclock,
   because 48 MHz does not care what `clk_sys` is doing. Build with
   `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1`, then **re-apply your SPI
   baud rate after every clock change**.
2. **The SPI dividers are coarse, and it decides which overclocks are worth
   taking.** The SPI divides `clk_peri` by an even prescale times a post-divider,
   so only clocks that reach the panel's rate *exactly* keep the display at full
   speed:

   | `clk_sys` | LCD SPI actually gets |
   |---:|---:|
   | 150 MHz | **75 MHz** (÷2) |
   | 200 | 50 MHz |
   | 250 | 62.5 MHz |
   | 300 | **75 MHz** (÷4) |

   An overclock to 200 MHz makes your code 1.33× faster and your display
   **1.5× slower**. 300 MHz is the only overclock that is free on both.

   This table is about `clk_peri`, not about which part you are on, so it
   applies just as much to an RP2040 taking its supported 200 MHz: a Pico at
   200 MHz drives this panel at **50 MHz**, so every figure in §4.7 stretches by
   half again. Nothing here has measured that — but the wire term is arithmetic,
   and on an RP2040 the display, not the core, is what you would be trading
   away.
3. **PSRAM QMI timing is computed once, before `main`.** Divider, RX delay, max
   select and min deselect all come from `clk_sys` at runtime init. At 300 MHz
   three of them are wrong in the unsafe direction simultaneously, and the part
   does not fault — it returns occasional bad data. This was found as scattered
   corruption in a text buffer. Recompute and write `qmi_hw->m[1].timing`
   immediately after the PLL moves, using the PSRAM part's timing requirements
   and the SDK/QMI register definitions. Avoid tearing down XIP while code or
   data still depends on it.
4. **The CYW43 bus divider is fixed at bus bring-up.** It divides `clk_sys` by 2,
   so at 250 MHz the chip is clocked at 62 MHz instead of 37 and answers with
   garbage headers. Scale the divider with the clock
   (`cyw43_set_pio_clkdiv_int_frac8`). A *running* radio can be retuned by
   bringing only the bus down and up — `cyw43_spi_deinit` / `cyw43_spi_init` —
   which preserves the association, lwIP state and open sockets. Tearing down
   `cyw43_arch` is the wrong seam.
5. **A PWM audio carrier depends on `clk_sys`, divider and TOP + 1**, so the sample rate
   and every rate derived from it move with the clock. Recompute, do not
   hard-code (§5.2).

And the voltage: the core rail is 1.10 V, which is what 150 MHz is rated
against. Raise to `VREG_VOLTAGE_1_20` **before** the PLL goes up and lower it
**after** it comes back down, with a millisecond to settle, so the rail is never
the lower of the two while the clock is the higher.

Flash XIP also runs at the new clock, and there is little you can do about it —
but the bootrom's divider is conservative, 300 MHz has run test sessions
on the RP2350 boards above. Do not assume an excessive flash clock always
fails immediately or visibly; qualify data integrity as well as startup.

What the bootrom leaves, read off `qmi_hw->m[0].timing` on a Plus 2 W on
2026-09-27 in a build without an embedded boot stage 2 (the SDK's default on
RP2350): **clkdiv 3, RX delay 2**, so 50 MHz at 150 MHz and 100 MHz at 300 if
left alone. The divider is a register you can move. Double both fields
before raising the PLL and the flash sees at 300 MHz exactly what it saw at
150. The RX delay counts half `clk_sys` cycles, so it must scale too. Do it
from SRAM with interrupts off, since nothing may fetch from flash while the
timing changes. Do it once, at power-on, before stdio or any peripheral is
up, so each driver derives its rate from the clock it finds. To change the
clock later, save the choice and restart with the watchdog rather than retune
running peripherals.

**Every flash command puts the bootrom's flash timing back.** The SDK's
`flash_do_cmd`, `flash_range_erase` and `flash_range_program` take the
flash out of XIP and return it through the boot stage's XIP set-up, which
writes `qmi_hw->m[0].timing` afresh. The SDK saves and restores the
PSRAM's chip select (`m[1]`) around these calls, but not the flash's own
(read from SDK 2.3.1's `hardware_flash/flash.c`). So a divider doubled for
300 MHz is undone by the next flash command, and the flash then runs at
100 MHz without any visible fault. **Found** on a Pimoroni Pico Plus 2 W
(RP2350B, 16 MB) on 2026-10-29: a JEDEC id read (`0x9F`) made through
`flash_do_cmd` after the clock went up left the timing at clkdiv 3,
RX delay 2. Read the flash's ids before raising the clock, or write the
timing again after every flash command.

**The flash's read window at 300 MHz, measured.** On a Pimoroni Pico
Plus 2 W (RP2350B rev 2, 16 MB flash) in a PicoCalc, 2026-10-30, core
rail 1.20 V, room temperature: a 3.76 MB image read twice per setting
through the uncached, unallocating XIP window (`0x14000000`), from SRAM
with interrupts off, each read's checksum compared against one taken at
the timing in use, divider 4 and RX delay 4. That reference could pass a
timing that corrupts the same way every time, so a second run on
2026-10-31 took it at 50 MHz (divider 6, RX delay 2) instead. The RX
delays (in half `clk_sys` cycles) that read the image intact, first run
and second:

| Divider | Flash clock | RX delays intact, first run | Second run, 50 MHz reference | One read of the image |
|---:|---:|---|---|---:|
| 6 | 50 MHz | 0–7 | 0–7 | 170–180 ms |
| 5 | 60 MHz | 0–7 | 0–7 | 145–157 ms |
| 4 | 75 MHz | 1–7 (0 failed on one boot of two) | 0–7 | 123–132 ms |
| 3 | 100 MHz | 2–6 | 1–6 | 97–110 ms |
| 2 | 150 MHz | 3–5 | 2–5 | 75–85 ms |

The window narrows and moves later as the clock rises, and a setting at
its edge passes on one boot and fails on the next, so take the middle of
the window, not an edge that passed once. One board, one temperature:
qualify again on another board, or hot.

**The core rail survives a reset.** A 150 MHz build flashed over SWD after a
300 MHz run, or restarted by the watchdog, comes back with the rail still at
1.20 V: the regulator is not in the domain a system reset clears. The QMI
timing is set again by the bootrom, but the voltage is not. So a firmware
that ever raises the rail must set it at 150 MHz too, after the clock.
Found on a Plus 2 W on 2026-09-27 by reading `vreg_get_voltage()` in the
banner.

**300 MHz at 1.20 V held for 30 minutes of a game with audio, on
battery.** Measured on a Pimoroni Pico Plus 2 W (RP2350B rev 2) in a
PicoCalc, 2026-11-08, development build, Arm GCC 15.2.Rel1, PSRAM at
100 MHz, flash at 75 MHz, LCD SPI at 75 MHz: a Chocolate Doom engine
played one level with keys typed over the UART for 1,905 s, core 0 79 %
busy drawing 64 frames a second, core 1 sending the changed rows of each
(99 of 240 on average) and rendering OPL music, PWM audio at 49.7 kHz,
DMA copying each frame into PSRAM. One boot, no fault, no late audio
refill, no I²C error; the die read 21–24 °C throughout. Power: the
batteries throughout (the southbridge's charging bit clear in every
reading), 100 % to 98 % as the southbridge reports it. A run the day
before with a slightly smaller picture (core 0 75 % busy) gave the same,
23–25 °C, on battery for 1,180 s and USB after.

---

## 4. The display

### 4.1 The panel

A **320×320 IPS panel** on an ST7789P/ST7365P-class controller with **480 rows
of frame memory**. RGB565, 16 bits per pixel on the wire. The extra 160 rows of
memory are not wasted — they back the controller's hardware vertical scroll
(§4.8).

### 4.2 Wiring and SPI setup

Four-wire SPI on `spi1`: SCK on GP10, MOSI on GP11, MISO on GP12, with **CS
(GP13), D/CX (GP14) and RST (GP15) driven as ordinary GPIOs**. The SDK's SPI
peripheral does not manage chip select here; you do, and that turns out to
matter (§4.3).

Start at **25 MHz**. A **75 MHz** configuration has also been exercised on
hardware and supplies the faster measurements in §4.7; it is above the cited
62.5 MHz ST7789P limit and is not a guarantee for every controller or unit.
Identify the panel and check its specification. Report the rate returned by
`spi_init`/`spi_set_baudrate`, and label it as configured rather than physically
measured unless SCK was checked with an instrument.

### 4.3 The 40 ns chip-select rule — the one that costs a week

The controller requires a minimum **40 ns CS-high pulse** before a RAM write.
At 75 MHz you cannot insert that with a delay loop without paying for it on
every transaction. The trick is to let the instructions you already need supply
the gap:

```c
// DO NOT MOVE. These two lines must precede the CS low, not follow it:
// they are what creates the required CS-high interval.
spi_set_format(LCD_SPI, 16, 0, 0, SPI_MSB_FIRST);
gpio_put(LCD_DCX, 1);          // data
gpio_put(LCD_CSX, 0);          // ...now assert CS
```

Keep that ordering in every 16-bit write path and recheck the gap when
changing CPU speed or driver implementation. Get it wrong and you see intermittent corrupt pixels that look like a
wiring or clock problem.

### 4.4 Initialisation

Use the panel-specific gamma and power settings from the
[ClockworkPi firmware examples](https://github.com/clockworkpi/PicoCalc/tree/master/Code)
and the matching controller specification. A known-working RGB565 setup uses
`MADCTL=0x48`, `COLMOD=0x55` and entry mode `0x06`. Confirmed again 2026-09-22 on a Plus 2 W:
with ClockworkPi's gamma/power block and those three values, a corner-coded
test pattern showed correct orientation and R/B order at 75 MHz. Some examples use 18-bit
pixels; their pixel format and transfer width must be adapted together.
The initialization includes gamma, power/VCOM, interface and frame-rate
controls, inversion, display-function controls and manufacturer commands.
Generic controller defaults may not suit this glass.

A conservative tested reset sequence holds hardware reset low for at least
10 µs, waits 120 ms after release, sends software reset and waits at least
5 ms. After configuration, send `SLPOUT` and wait 120 ms before normal display
use. Clear visible frame memory before `DISPON` so startup does not show
uninitialized pixels. Initialize scroll registers if using hardware scroll.
Check the controller's requirements before shortening any of these waits.

### 4.5 Addressing and colour

`CASET` (columns) and `RASET` (rows) set an inclusive window; `RAMWR` then
streams pixels into it, left to right, top to bottom, wrapping at the window
edge. Window setup is three commands and eight bytes of parameter — at 75 MHz,
under a microsecond of wire, which is why **splitting a present into many
windows is nearly free** (§4.7).

RGB565 on the wire, big-endian: high byte first. The controller expands to
18 bits internally; entry mode `0x06` sets the conversion so that r(0) = b(0) = 0.

### 4.6 Getting pixels out

Three approaches, in ascending order of effort and descending order of CPU cost:

**(a) Blocking writes.** `spi_write16_blocking` per row or per pixel. Simple,
correct, and the CPU is stalled on the wire the whole time: a full 320-px row is
68 µs of doing nothing, ~22 ms for a screen. Fine for setup and text, wrong for
a frame loop.

**(b) One DMA channel, two line buffers, ping-ponged.** The pattern that fits
this panel:

```
lcd_blit_begin(x, y, w, h)     set CASET/RASET/RAMWR, 16-bit format, CS low, hold it
  lcd_blit_row(row) × h        expand row N into line buffer A
                               while DMA streams line buffer B to the SPI TX FIFO
lcd_blit_end()                 wait out the last DMA, drain the wire, release CS
```

The channel is configured **once** — DREQ-paced by `spi_get_dreq(spi1, true)`,
`DMA_SIZE_16`, read increment on, write increment off. Per row you set only the
read address and the transfer count. The CPU's visible cost per row drops from
~68 µs to ~5–8 µs (whatever your row transform costs), and it is spent doing
useful work rather than waiting.

Four details that are not optional:

- **Poll for completion; do not take an interrupt.** You have useful work — the
  next row — while you wait, and leaving the LCD DMA interrupt-free means your
  audio engine owns its DMA IRQ uncontended (§5.4).
- **Drain properly at the end.** The DMA only fills the TX FIFO. Wait for
  `SSPSR_BSY` to clear, discard everything the full-duplex SPI clocked into the
  RX FIFO, and clear the overrun flag (`SSPICR_RORIC`), or the next transaction
  inherits garbage.
- **Restore 8-bit format** when the window closes; commands are 8-bit.
- **Leave interrupts enabled for the whole blit.** The controller tolerates SPI
  clock pauses mid-window, so an audio refill IRQ may preempt a blit freely.
  This is load-bearing — see §5.7.

**(c) PIO.** Usually unnecessary for this interface: the SPI peripheral plus one DMA
channel already saturates the wire at 75 MHz, and PIO would buy you a
different pixel format or a wider bus that the panel does not have. Spend the
state machines elsewhere.

### 4.7 What the wire costs

Theoretical ceiling at 75 MHz, 16 bpp: **4.69 Mpx/s**, i.e. 21.8 ms for
320×320.

Measured on a **Pico 2, 2026-08-01**, timing forced full-region presents twenty
times through an 8-bpp-indexed → RGB565 expanding pipeline:

| Region | Pixels | Wall time |
|---|---:|---:|
| Full screen 320×320 | 102,400 | **25.6 ms** |
| 256×320 (a 16-column viewport) | 81,920 | 21.1 ms |
| 64×320 strip | 20,480 | 5.45 ms |
| One 16-px column, full height (16×320) | 5,120 | 1.259 ms |
| Fixed cost per present | — | 0.41 ms |

**The effective rate is 4.0 Mpx/s — 17–21 % below the wire math.** The figures
are internally consistent (slope 1.259 ms per column, intercept 0.41 ms), so the
gap is real work, not noise: the source copy, the palette expansion, and the
fact that a full-height present with one dirty span per tile row is *twenty*
blit windows rather than one.

Two conclusions to design against:

- **Cost is per pixel, not per present.** The intercept is 0.41 ms for the whole
  screen. Narrowing a viewport buys back exactly its area share; splitting a
  present into several windows is nearly free. A HUD column that never changes
  is worth its area in milliseconds every frame.
- **A native RGB565 source streams closer to the 21.8 ms wire figure**, because
  it deletes the expansion. That is the trade against §2.3's memory arithmetic:
  8 bpp costs ~17 % of present time and saves 100 KB.

**With the source cost removed, the wire and DMA alone run ~9.5 % over the
wire math.** Measured on a **Plus 2 W (RP2350B), 2026-10-03**, at 75 MHz:
two RGB565 line buffers filled once, then sent row after row through the
polled-DMA ping-pong of §4.6 in one window, eight runs each:

| Region | Pixels | Wire math | Wall time |
|---|---:|---:|---:|
| Full screen 320×320 | 102,400 | 21.8 ms | **23.90–23.93 ms** |
| 256×192 | 49,152 | 10.5 ms | **11.475–11.476 ms** |

That is the floor any renderer adds its own cost to.

**A 320×240 indexed frame read from PSRAM presents at the wire's pace.**
Measured on a **Pimoroni Pico Plus 2 W (RP2350B), 2026-10-29**, clk_sys
300 MHz, SPI 75 MHz configured, PSRAM at 100 MHz: core 1 presenting
320×240 8-bit frames out of PSRAM in one window, each row expanded
through a 256-entry RGB565 palette in SRAM (code in SRAM, the source read
a 32-bit word per four pixels) into one of two line buffers while the
other is on the wire, at 26–30 frames a second for 60–88 s per run:

| How the row is read from PSRAM | Present, wall time |
|---|---:|
| Through the cached XIP window | **18.12–18.16 ms** |
| Through the uncached alias (`0x15000000`) | 18.12–18.20 ms |
| DMA'd from the uncached alias into SRAM, a row ahead | 18.6–19.1 ms, longest 21.9 ms |

Wire math is 16.38 ms, so 10.7 % over, the same margin as the SRAM-only
figures above: the PSRAM read and the expansion hide behind the wire.

The DMA'd figure holds with a game engine running flat out on the other
core. Measured on a **Pimoroni Pico Plus 2 W, 2026-11-07**, the same
clocks, core 1 presenting each 320×240 8-bit frame whole, DMA'd a row
ahead from the uncached alias, while core 0 rendered and copied frames in
the PSRAM at 100 % busy: **18.9–19.1 ms** a present on average, the
longest 21.1 ms, at 23–29 frames a second over about eight minutes in
five boots; 13,095 frames published and every one presented, none
superseded.

The same holds for a 240×224 rectangle (Plus 2 W, 2026-10-08, 75 MHz, one
run each, at boot): **12.58 ms** row by row from two line buffers and
12.55 ms as one colour with the DMA's read address held, against 11.47 ms
of wire math, 9.7 % over. One 240-pixel row in a window of its own took
**64 µs** against 51 µs on the wire: the window's three commands cost
about 13 µs.

Frame ceilings, if you present serially on one core:

| Strategy | Present cost | Ceiling |
|---|---:|---:|
| Full screen every frame | 25.6 ms | ~39 fps with nothing left over |
| 256×320 viewport every frame | 21.1 ms | ~47 fps |
| Dirty tiles only, a few sprites moving | 1.6–2.7 ms | not the constraint |

**Measured, Pico Plus 2 W, September 2026, 25 MHz configured SPI1:** full
320×320 monochrome-to-RGB565 diagnostic transfers took **about 71.7 ms**,
including row generation and DMA waits. With LCD and I2C on core 1 and a
CPU/audio workload on core 0, transfers took **about 90–100 ms**. These are
software transfer durations, not panel refresh rates or wire-only timing.
Physical SCK and CS/D-C waveforms were not measured in that test series.

Static borders, diagonals, checkerboards and black clears passed visual checks
through all four corners. Alternating full-screen patterns showed tearing.
A battery-powered 30-minute combined display/audio/input run on the Plus 2 W
recorded 1,800 heartbeats, all eight scripted physical key events, zero I2C
errors and zero late audio refills; the operator reported no display corruption
or audio interruptions. Panel revision identifiers were not recorded.

**Rows that did not change, found for nothing by the DMA sniffer.** A
presenter that reads each row of an indexed frame from PSRAM into SRAM by
DMA can have the DMA's sniffer take the row's CRC-32 on the way, at no
CPU cost: seed `sniff_data` before each row's transfer, read it when the
transfer is done, and skip the row on the panel when its CRC and the
palette match the frame last sent, ending the RAM write before a run of
skipped rows and opening a new window (CASET, RASET, RAMWR) at the next
changed one. Both switches are needed: the channel's `SNIFF_EN` in its
control word **and** the sniffer's own enable with the channel's number
(`dma_sniffer_enable`); with only the first, `sniff_data` keeps its seed,
every row matches, and the panel silently freezes on its first frame
(found 2026-11-07, the first build). Measured on a Pimoroni Pico Plus 2 W
at 300 MHz, the LCD at 75 MHz, 2026-11-07, development build: a game
turning on the spot, frames finished about 70 a second at 35 game tics a
second, sent **98 rows a frame of 240** on average (a repeated frame
sends none; a new one most of its 3D view), and a present took **10.0
ms** on average against 20.4 ms sending every row.

### 4.8 Hardware vertical scroll

`VSCRDEF` defines top-fixed / scrolling / bottom-fixed bands within the 480 rows
of frame memory; `VSCSAD` sets which memory line appears at the top of the
scrolling band. Bump the offset and the whole band moves with **no pixels sent
at all** — you then only draw the newly exposed line or band.

The price is that **every blit must remap its y through the scroll offset**,
including the wrap around 480 rows. Get it wrong and drawing lands
somewhere plausible-but-wrong, which is a miserable class of bug. If you use
hardware scroll, put the remap inside your single `blit_begin` and give no other
path direct access to `RASET`.

For a game this is the cheapest lever on the machine: **a vertical scroller gets
its background for the cost of one exposed band instead of 25 ms**. It buys
nothing for horizontal motion, and it fights any other use of the scroll offset
(text scrolling, split screens) — you get one scroll register, so pick one
customer for it.

### 4.9 Readback and tearing

The MISO line is wired and `RAMRD` exists, so the frame memory *can* be read
back — but the controller's read timing is far slower than its write timing, so
this is a diagnostic, not a data path. **Keep your own copy of anything you need
to read.**

Treat readback as a **development-only test facility**, for example to compare a
known test pattern or checksum against the controller's frame memory during LCD
bring-up. Do not use it in the production rendering path or as a substitute for
a software framebuffer.

There is no TE line (§1.2). If you present a moving object faster than the panel
scans, you will see tearing. The mitigations available are ordering ones: keep
presents small and localised so a tear affects one tile rather than a whole
sprite; present the moving band before the static one; or accept it — at 320×320
and typical panel refresh, most games do.

### 4.10 Choosing a framebuffer model

The hardware constrains this more than taste does. Four models that work, with
what each costs:

| Model | SRAM | Present cost | Notes |
|---|---:|---|---|
| 8 bpp indexed full frame + palette | 100 KB | 25.6 ms full, linear in area | software reads are cheap; see §2.3 |
| 16 bpp full frame | 200 KB | ~21.8 ms full | fastest wire, crowds everything else |
| Band renderer (N-row strip, regenerated) | 5–20 KB | wire-rate, but you redraw *everything* every frame | no canvas to read back; scrolling is free |
| Dirty-region 8 bpp | 100 KB + tracker | 1–3 ms typical | best for a static background with moving objects |

Double buffering protects the source image from changes during DMA, subject
to the SRAM budget in §2.3. It does not make the LCD update atomic: without a
TE connection, even an immutable full-frame source can tear during scanout.
Band buffering and hardware scrolling reduce transfer work but do not provide
a general scan-synchronization signal.

If you do go dirty-region: a **20×20 grid of 16×16 tiles, one inclusive
`[min..max]` column span per tile row**, is 40 bytes of state for the whole
screen, O(1) to mark and ≤20 windows to flush. Over-mark rather than under-mark
— a few extra tiles cost microseconds and a missed tile leaves a stale sprite on
screen forever. Snapshot the dirty state and clear it *before* sending, so
writes that happen during the blit are tracked for the next present. Marking
overhead is negligible and we measured it (~20 µs over 288 rows against a 7.6 ms
draw). Keep geometry and dirty tracking independent of SDK APIs so they can
be tested on a host before hardware bring-up.

A **256×192 1-bit snapshot needs only 6,144 bytes**. It can be expanded into a
single **640-byte RGB565 row**, reused after DMA completion, or two rows to
overlap generation and transmission. Keep the snapshot immutable until the
whole presentation finishes. DMA completion releases its source buffer, but
CS/window changes must also wait for the SPI shifter to become idle.

Tested layouts on the 320×320 panel are native 256×192 at **(32,64)** and
nearest-neighbor 320×240 at **(0,40)**. Both were readable and visually centered;
the expected uneven pixel doubling at 1.25× was visible. Exact black margins
were verified by geometry tests, not visual inspection. If a driver still
transmits all 320×320 pixels, a smaller source image saves memory but does not
save wire time. Restrict the transmitted window to gain that benefit.

### 4.11 Backlight

The LCD backlight is **not** a PWM pin on the processor — it is register `0x05`
on the southbridge (§6). Levels are stepped to multiples of 16 and clamped to
16–240 by the keyboard MCU, so a smooth fade is 15 steps, not 256. Dimming on
pause is a cheap piece of polish and costs one I²C transaction. Read register
`0x05` before dimming and write that level back on resume: the level may be
one the user set with the MCU's own Alt chords (§6.3), not one your software
chose. Use the value only from a read that succeeded.

---

## 5. Audio

### 5.1 The output stage

There is no DAC. The PicoCalc routes **GP26 (left) and GP27 (right) through an
RC low-pass filter into an amplifier**. Two facts make the output stage almost
free:

- GP26 and GP27 are **channels A and B of the same PWM slice** (slice 5 on every
  board — `(gpio >> 1) & 7` — but ask `pwm_gpio_to_slice_num` and never hard-code
  it). One slice clocks both ears sample-locked, and the two compare registers
  share **one 32-bit word**: `left | (right << 16)`. A stereo frame is a single
  32-bit write, so you need one DMA stream, not two.
- The slice's wrap event is a **DMA pacing signal** (`pwm_get_dreq(slice)`), so
  a DMA channel can stream compare values from a buffer at exactly the sample
  rate with zero CPU per sample.

### 5.2 Choosing a carrier and a sample rate

In free-running, edge-aligned PWM, TOP and the divider set the carrier;
repeating each sample across wraps sets the logical sample rate:

```
period clocks = TOP + 1
carrier Hz    = clk_sys / (divider * (TOP + 1))
resolution    = log2(TOP + 1) bits
sample rate   = carrier / oversample
```

At 150 MHz and divider 1, **TOP=2047 gives a 73.2 kHz carrier at 11 bits**. That is the
sweet spot on this hardware: ultrasonic, so the RC filter and the speaker remove
it without any filtering of your own, and 11 bits is more dynamic range than the
speaker resolves. Writing each mixed frame twice (oversample 2) gives a
**36.6 kHz sample rate** — an 18.3 kHz Nyquist limit, comfortably above anything
the output stage passes.

Push the wrap up for resolution and the carrier drops into the audible band;
push it down for carrier headroom and you lose bits. If you want 12 bits, you
need the clock: at 300 MHz, TOP=4095 still leaves a 73 kHz carrier.

**Recompute these from `clock_get_hz(clk_sys)` at init and again on any clock
change** (§3), rather than baking 73,242 into a constant.

For accurate resampling, preserve the rational cadence: at 150 MHz, divider
1, TOP=2047 and two wraps per sample, it is **150,000,000 / 4096 samples/s**.
Integer reports of 73,242 Hz carrier and 36,621 Hz samples are truncated.
Do not feed those rounded reports back into long-running timing calculations.
Change clocks only through a coordinated playback stop/retune/restart path.

### 5.3 Streaming without gaps

Two DMA channels, each **chained to the other**, ping-ponging through a two-half
ring; each is paced by the slice's wrap DREQ and writes the 32-bit compare word.
When a half drains, that channel's IRQ refills it. Chaining is what makes
playback gapless — there is never a moment when no channel is armed.

Two hardware details that are not obvious and cost real debugging time:

- **Size and align the whole ring to a power of two, and use
  `channel_config_set_ring()` on the read address.** This is your safety net for
  IRQ starvation. If a refill is delayed past a half draining, a non-wrapping
  DMA marches into adjacent RAM and plays it as an audible burst of static; with
  the hardware wrap it cleanly replays the ring instead — silence at idle, a
  brief stutter under a note. You need this because **flash program/erase masks
  interrupts with XIP offline for tens of milliseconds** (§7.2), which no amount
  of careful IRQ design avoids.
- **When you re-arm a chained channel from its IRQ, reset both the read address
  and the transfer count.** A trigger, a chain's included, does not reload the
  read address: it carries on from where the last run ended. It does reload the
  count: writing `TRANS_COUNT` sets a RELOAD value, and every trigger copies it
  into the live counter, so a channel performs the same number of transfers each
  time it is started (the RP2350 datasheet's register description, as the
  `rp235x` PAC transcribes it; the RP2040's says the same). So a channel
  chained again before its IRQ has re-armed it plays a full half from where it
  stopped, which in a two-half ring is the *other* half, again; with the ring
  wrap that is a replay, not a march through SRAM. A refill handler that finds
  its channel already running has missed its deadline, and must leave it alone
  and count a late refill (§5.8). Earlier editions of this note said the count
  was not reloaded and that resetting only the address stormed the IRQ; that
  was never reproduced, and it does not fit the datasheet or the late-refill
  path forced on a board (EL §6.4: each 9 ms stall cost 2–3 late refills and
  about three halves of samples, with no storm, and playback carried on). A
  host simulation of the DMA with the count reloaded reproduces that figure, 3
  late refills a stall; without the reload it cannot recover. Set both anyway:
  it costs nothing, and the address must be set.

Ring size sets your latency and your deadline. 256 slots per half at 2×
oversample is 128 frames, i.e. **one refill every ~3.5 ms** — for 2,048 bytes of
SRAM. Halve it for latency, double it for slack.

### 5.4 The deadline, and IRQ priority

**Everything about audio on this machine reduces to one number: the refill
deadline.** At 3.5 ms, any handler that can run longer than that at the same
NVIC priority will produce audible clicks, because same-priority interrupts
cannot preempt each other.

A common source is a **southbridge transaction that blocks 4–5 ms on the
10 kHz I²C bus** (§6). Put that in a timer
IRQ at default priority and a sustained note clicks about ten times a second.

So: `irq_set_priority(DMA_IRQ_0, 0x40)` — above the default `0x80` — so the
audio refill preempts everything else. The I²C peripheral runs autonomously and
is unharmed by being preempted. And put **every function on the refill path**
behind `__not_in_flash_func`, so it survives flash writes and adds no XIP misses
to a latency-critical path.

Flash program/erase is the one accepted violator of the deadline. Everything
else is a bug.

### 5.5 What a mixer costs

A software PSG — 8 voices (three tone plus one noise per ear), phase-accumulator
oscillators, 16-bit LFSR noise, per-voice ADSR, integer throughout — costs about
**35 cycles per voice per frame**: 8 voices × 36.6 kHz ≈ **7 % of one core at
150 MHz**, in interrupt context, regardless of what the foreground is doing.
That is the budget for a synth. Sample playback is cheaper; anything with a
filter per voice is not.

Techniques that made that number achievable, all of them portable to whatever
you build:

- **Phase accumulators**: `phase += (freq << 32) / sample_rate`, take the
  waveform from the top bits. No trig, no tables, exact frequency.
- **Advance envelopes once per refill block, not per sample.** At 3.5 ms
  granularity this is inaudible for millisecond-scale envelopes and it keeps the
  inner loop to an add and a lookup.
- **A logarithmic volume ladder** (16 steps of 2 dB, the TI/Atari curve) scaled
  to 0..256. Linear volume sounds wrong and costs the same.
- **Fix your headroom by construction.** Four voices summed per ear, `>> 2`,
  saturate, bias to mid: four simultaneous full-scale voices cannot wrap. Wrap
  is not a soft failure — it is a full-scale discontinuity.
- **Crest factor is not loudness.** A square wave has a crest factor of 1 and
  speech has 4–8, so matching *peaks* leaves speech ~20 dB below a tone in the
  RMS the ear reads as loudness. If you mix synthesis and samples, tune by ear
  and expect the sample to need several times the "correct" gain.

An **OPL2 emulator** costs far more. MAME's YM3812 core, generating at
44,100 Hz for a game's intro music, took **17 % of core 0** at 300 MHz in
PC samples on a Plus 2 W (2026-10-29, development build, the output thrown
away). The cost is per output sample, so it falls with the rate.

Measured as the whole refill interrupt (the mixer, its rate converters and
the OPL emulator, all running from flash) at 36,621 Hz, on a Plus 2 W at
300 MHz, 2026-10-29, development build, playing through the PWM output of
§5.9, the same game intro: MAME's YM3812 core took **45–55 %** of core 0,
with a longest 512-frame refill of 10.8 ms; ScummVM's DOSBox OPL core took
**13–18 %**, longest refill 4.4 ms. A game asking for two OPL2 chips in
stereo (DOSBox only) took **36–44 %**. By ear MAME's sounded a little
softer, DOSBox's not bad.

For the foreground-to-IRQ handoff, a **lock-free SPSC queue per voice** (one
reserved slot so full and empty are distinguishable) removes almost all critical
sections; the few instructions that touch shared voice state go inside
`save_and_disable_interrupts()`.

### 5.6 Other things you could do with this output stage

- **Sample playback** — the DMA path is format-agnostic; anything you can turn
  into 11-bit stereo frames at your chosen rate plays.
- **Mono at double resolution** — not available: the two compare registers are
  physically separate outputs. You would sum in software and write both.
- **I²S to an external DAC over PIO** — possible in principle, but there is no
  DAC on the carrier and no free pins near the audio jack, so this means
  hardware work.
- **1-bit / PDM tricks** — the RC filter is sized for a ~73 kHz carrier; anything
  that puts energy lower will be heard.

### 5.7 Three bring-up findings

All three presented as "audible clicking" and all three had different causes.
The common thread is §5.4's deadline.

1. **The DMA ring wrap** (§5.3). Clicks at idle, static on a mode switch,
   because the IRQ reset the read address and a masked IRQ let the DMA run off
   the end of the buffer.
2. **The LCD driver was masking interrupts.** Stutter under any large screen
   update, because the driver wrapped every screen operation in
   `save_and_disable_interrupts()` — purely because a cursor-blink *timer* drew
   to the LCD from IRQ context. The fix was to make the timer set a flag and do
   the drawing in the foreground, after which all interrupt masking was deleted
   from the LCD driver. **The invariant that came out of it: the LCD is only ever
   touched from thread context, never from an interrupt handler.** Write that at
   the top of your display driver.
3. **IRQ priority** (§5.4), the keyboard poll case.

### 5.8 Count producer starvation separately

A software PCM queue can empty even while every DMA refill meets its deadline.
A stock-clock CPU workload initially missed approximately **14,000–18,000
samples/s with zero late DMA refills**, producing audible buzzing. Count both
PCM underrun samples and late DMA refills; one counter cannot replace the other.
Bound queues, define the silence value, and keep consuming samples when muted
so muting does not change application timing.

**The late path, forced.** A late refill rarely happens in normal running, so
its recovery goes unexercised. A scratch build that masked the producing
core's interrupts for 9 ms every 250 video fields (on a Plus 2 W,
2026-10-04, with §5.3's ring of two 128-frame halves at oversample 2)
counted 3 late refills per stall and lost three halves' worth of samples,
with no IRQ storm, no PCM underrun, and playback carrying on afterwards.
That is what the ring wrap and the both-registers re-arm of §5.3 buy.

A tested arrangement uses a 1,024-sample software queue (about 28 ms), starts
streaming after 768 samples (about 21 ms), and has two 128-sample DMA halves
adding up to about 7 ms. These are buffering calculations, not measured
end-to-end acoustic latency. Choose capacity and startup fill for the workload.

Left/right/stereo/mute operation passed physical listening checks on a Plus 2 W.
A phone app reported approximately **439 Hz** for a 440 Hz test tone; calibration
and resolution were not recorded. This does not measure the PWM carrier,
analog frequency response or waveform fidelity.

### 5.9 12 bits at 300 MHz, with the mixer in the refill interrupt

Measured on a Plus 2 W at 300 MHz and 1.20 V, 2026-10-29, development build,
playing ScummVM-derived game audio (OPL music, digitised speech and effects).

- **TOP 4095, divider 1, oversample 2** gives a 73.2 kHz carrier, 12 bits,
  and 300,000,000 / 8,192 = **36,621.09 samples a second**. Music, speech
  and effects sounded right by ear, at the right pitch and speed, through
  the PicoCalc's speakers. Gain: a signed 16-bit sample shifted right by 4
  about the mid-point (2,048) fills the 12 bits exactly.
- **Ring:** two halves of 512 frames (1,024 compare words each, 8 KB in
  all, aligned to 8 KB, with the read wrap of §5.3), so a refill every
  **14.0 ms**: 71.5 interrupts a second, as measured.
- **The producer can be the refill interrupt itself**: the handler calls
  the mixer for 512 frames, converts, and re-arms. Over some 90 minutes
  of play across six game engines, with the DOSBox OPL core, the longest
  refill was 7.8 ms; with nothing playing a refill takes 0.1–0.4 ms.
- **The late path, forced**, with these 14 ms halves: core 0's interrupts
  masked for 30 ms every 5 s through continuous music cost **2.8 late
  refills a stall** (26 over about 9 stalls), no interrupt storm, and
  70–73 refills a second around each stall; playback carried on each time.
  The same recovery as §5.8's 3.5 ms halves.
- **Masking a single interrupt in the NVIC is not immediate without a
  barrier** (Cortex-M33; inferred, not measured directly). Code that
  cleared the refill interrupt's enable bit and then, in the next few
  instructions, recorded that it had done so, sometimes ran the interrupt
  in between: the handler, taking the same lock, saw the bit already clear
  and recorded "was disabled", and the interrupt was never enabled again.
  Two runs of three hit it within three minutes of one game's music. A
  `DSB` and `ISB` after the write to `NVIC_ICER`, and handler code that
  never touches the lock's saved state, ended it: nine runs of two minutes,
  none. The symptom on this hardware is distinctive: with the interrupt
  dead and the DMA running, the ring's last 28 ms repeat for ever (§5.3),
  until something enables the interrupt or the board is power cycled.
- **A long critical section is a long stall.** If thread code masks the
  refill interrupt to protect shared mixer state, every lock held across
  card I/O stops the audio: one game engine read a sound from the SD card
  under its mutex and held the interrupt off for **163 ms**, three late
  refills, at a scene change.

### 5.10 A 99 kHz carrier at 49,718 Hz, and how loud the output is

Measured on a Plus 2 W at 300 MHz and 1.20 V, 2026-11-07, development
build, playing a game's OPL2 music (an emulator at 49,716 Hz) and 8-bit
sampled effects through the PWM output of §5.1–5.4: two halves of 512
frames, oversample 2, the refill interrupt on `DMA_IRQ_1` at priority
`0x40`, the mixer outside the interrupt (the interrupt only converts and
re-arms).

- **Divider 1, TOP 3016, oversample 2** gives a 99.4 kHz carrier,
  log2(3,017) = 11.6 bits, and 300,000,000 / 6,034 = **49,718.26 samples
  a second**. Music and effects sounded clean to the owner, by ear,
  through the speakers: no whine, the right pitch. A carrier above §5.2's
  73 kHz costs nothing audible here.
- **The refill interrupt that only converts** (a 16-bit sample times a
  gain, scaled to the period about its mid-point, saturated, each frame
  written twice) took **0.5–0.9 % of core 0**, its longest run 68–108 µs,
  at 97 refills a second, with every function on its path in SRAM. No
  late refills in any run.
- **The output is quiet at nominal level.** A mix whose loudest sample
  each second peaked at 900–1,600 of 32,768 (about −27 to −30 dBFS) was
  "very quiet" through the speakers. Eight times the gain, so the same
  peaks reach about 12,000, was "right", in the owner's judgement. Plan for
  a gain stage after the mixer, and saturate it.

### 5.11 What EMU8950 costs, and where a second core can hide it

Measured on a Plus 2 W at 300 MHz and 1.20 V, 2026-11-07, development
build, a game's recorded demo with OPL2 music and effects, PC samples over
SWD and the program's own timers.

- **EMU8950** (Graham Sanderson's OPL2 emulator, its slot renderer, linear
  mode, no rate converter) at **49,716 Hz** took about **18 % of a core**:
  some 1,100 cycles a sample for 18 operators. From flash beside a game's
  renderer, the whole mix (music and 8 effect channels) took 25–28 %;
  with the emulator's code in SRAM, 22–25 %. Its own Thumb assembly
  renderer (written for the RP2040) was no faster than its C on the M33.
  So the cost is compute, not instruction fetch.
- **A core that presents to the LCD by DMA is mostly waiting.** Sending
  a 320-pixel RGB565 row at 75 MHz SPI takes about 68 µs, and a core that
  converts the next row and then waits for the DMA spends most of each
  row idle. Rendering the OPL in 8-sample steps for up to 50 µs after
  each row is handed to the DMA put the music on that core at a cost of
  0.7 ms on a 19.7 ms frame (20.4 ms), while it still presented 36 frames
  a second. The same rendering done between frames instead left it
  presenting 30: a frame, the music and the keyboard poll did not fit in
  a second.
- **A southbridge keyboard poll costs about 4.9 ms** of the polling core
  each time, on the 10 kHz I²C bus (§6), blocking: 31–38 polls a second
  is 150–190 ms of that core's second.



---

## 6. Keyboard, battery, backlight, power

None of this is wired to the processor. An **STM32F103R8T6 on the PicoCalc
mainboard** owns the key matrix, both backlights, the battery gauge and the
power rail, and exposes all of it as an **I²C slave at address `0x1F`, 10 kHz**.

Protocol: write the register number, then read two bytes. **OR `0x80` into the
register number to write it.** Always use the `*_timeout_us` variants — a wedged
bus must not hang the machine.

| Reg | Name | Read gives | Notes |
|---|---|---|---|
| `0x01` | `VER` | firmware version | |
| `0x04` | `KEY` | FIFO depth (low 5 bits), caps bit 5, num bit 6 | |
| `0x05` | `BKL` | LCD backlight | writable; stepped to multiples of 16, clamped 16–240 |
| `0x08` | `RST` | — | **a read resets the MCU** after 1 s; a write resets after *value* seconds |
| `0x09` | `FIF` | `[state, key]`, one event per read | `[0,0]` when empty |
| `0x0A` | `BK2` | keyboard backlight | writable, steps of 32 |
| `0x0B` | `BAT` | percent in bits 0–6, **bit 7 = charging** | refreshed by the MCU every 20 s, not on demand. Bit 7 is the charger, not USB power: on a Plus 2 W on 2026-09-27 it read `100%` with bit 7 for over half an hour on USB, then `100%` without it, still on USB, once the charge finished. So bit 7 set proves USB power, and clear proves nothing |
| `0x0C` | `C64_MTX` | 10 bytes of raw matrix bitmap | reply is longer than two bytes |
| `0x0D` | `C64_JS` | arrows + Enter as a C64-style joystick | |
| `0x0E` | `OFF` | not 0 if the firmware can power off | write to power off; value is clamped to ≥6 and used as a **delay in seconds**. Seen working on a Plus 2 W on 2026-10-30, firmware reporting version `0x00`: a read gave a value not 0, and a write of 6 switched the PicoCalc off a few seconds later (not timed). The processor cannot tell whether it is on battery or USB, and it made no difference |

Registers `0x02` (`CFG`), `0x03` (`INT`), `0x06` (`DEB`) and `0x07` (`FRQ`) are
**not in the firmware's dispatch** and reply `[0,0]`. That matters: `CFG` is
where you would ask for raw keys instead of MCU-resolved modifiers, and you
cannot.

The two `C64_` registers are the only raw view, and neither covers the whole
keyboard. This is read from `keyboard.ino` and `picocalc_keyboard.ino`. It has
not been checked against the installed firmware, which reports version `0x00`.

- **`C64_MTX` (`0x0C`)** replies with ten bytes: the register number, eight
  column bytes and a ninth that is always `0xFF`. Bits 0–6 of column byte *c*
  are the 7×8 main matrix, active low. Bit 7 of bytes 0–7 carries eight of the
  side buttons: Alt, Ctrl, left Shift, right Shift, `0`, `9`, `]` and `[`.
  **The arrow keys are not in it.** The state persists between scans, so the
  register reads what the last 16 ms scan saw. At 10 kHz a ten-byte reply is
  roughly three times the bus time of an ordinary read.
- **`C64_JS` (`0x0D`)** is meant to hold the four arrows (bits 0–3: right, up,
  down, left) and Enter (bit 4), active low. `keyboard_process()` sets it to
  `0xFF` on **every** call, before the check that returns early until 16 ms
  have passed. The main loop calls it far more often than it scans, so a read
  almost always finds `0xFF`, nothing pressed. As published, the register
  looks broken.

So the arrows are only visible through the event FIFO, with Shift resolved
(§6.3). A raw poll could replace the FIFO for the other keys, at extra bus cost,
but it cannot get around the swallowed Shift+arrow chords.

### 6.1 What it costs, and how to poll it

- **Each transaction costs 4–5 ms of wall time** at 10 kHz. That is the single
  most expensive routine operation on this machine, and it is why §5.4 exists.
  A FIFO read of an empty FIFO (a one-byte register write, then a two-byte
  read) measured **4,825–4,845 µs** over a 655 s run of 19,651 polls at 30 Hz,
  with **zero** failed transactions (Plus 2 W, 2026-10-03).
- **Poll from your frame loop, in thread context** — at 25–60 Hz — not from a
  timer IRQ. Same cost, but it lands somewhere you can account for it and it
  cannot preempt anything.
- **Guard the bus with a flag.** An atomic bool checked before starting a
  transfer keeps a foreground battery read and a background key poll from
  interleaving.
- **Drain more than one FIFO entry per poll.** The FIFO holds 31 entries and a
  held key produces an event every 100 ms on top of press/release pairs; a
  one-event-per-tick consumer falls behind and the machine feels laggy.
- **The MCU resets its own I²C bus** if it sees no transaction for 2.5 s (after
  the first 10 s of uptime). A game that stops polling for seconds will come
  back to a slave that has just re-initialised.
- **Never reset the processor in the middle of a transaction.** It can
  leave the MCU answering nothing until the PicoCalc is switched off and
  on. Seen on a Plus 2 W, 2026-10-29: a program polling the FIFO every
  25 ms (in a transaction about a fifth of the time) rebooted itself
  with `watchdog_reboot`, and from the next boot every transaction failed
  with the address not acknowledged (1.1 ms each), 1,933 failures in 67 s,
  for as long as it ran. SDA and SCL both read high over SWD, so nothing
  was holding the bus; nine clocks on SCL and a STOP from the host did not
  bring the MCU back, and neither did its own 2.5 s bus reset above. A
  power cycle did; the same had been seen before by the board's owner.
  Twenty reboots made after stopping the polling core between
  transactions all came back answering. So stop I²C traffic before any
  reboot the program makes, including a return to a loader. An SWD reset
  is the same hazard; earlier programs that polled once a second never
  met it there. Check the version register at boot and say so on a
  failure: the screen and the rest of the program look normal with a dead
  keyboard.

### 6.2 The event model, and what to build from it

Every FIFO entry is a `[state, keycode]` pair: pressed=1, held=2, released=3.
Modifier codes are defined in the `0xA1`–`0xA5` range; not every defined code
is necessarily emitted by the installed firmware. The MCU scans the
matrix every ~26 ms and a held key produces **nothing for 300 ms, then one event
per 100 ms**.

That cadence is a *typing* cadence, and it is wrong for a game. Build a
**held-key bitmap from press/release events** — 256 bits is 32 bytes — and ask
"is this key down now" instead of consuming a character stream. A frame loop
reading one buffered character per frame consumes slower than the repeat
produces, so the backlog grows and the ship keeps turning after the player lets
go; and one character per frame means two keys can never be held at once. Keep a
separate "pressed since last frame" latch for edge-triggered actions.

**Observed firmware quirks, September 2026:** the version register returned
`0x00` (nibble-decoded as `0.0`), which did not identify a specific release.
A 762-event physical capture showed printable auto-repeat as additional
**pressed** events, while modifiers produced **held** events. Holding `j`
produced an initial press plus 16 repeated presses before release. A pressed
event is therefore not necessarily a new physical down edge.

The MCU retranslates keys at each transition. Releasing Shift first can give
press `A` / release `a`, or press `!` / release `1`. Canonicalize letters and
shifted punctuation for held-state identity, while retaining the translated
press for text input. The captured stream left no keys stuck after this
normalization. For actions requiring a fresh key press, consult prior down
state and require release before rearming; do not rely on state 1 alone.

Retranslation goes further when Alt is the modifier let go. Alt+I sends Insert
(`0xd1`, §6.3). Let go of Alt while I is still down, and the MCU's
auto-repeat arrives as more **presses of `i`**, then the release as `i`; no
release of Insert is ever sent (captured on a Plus 2 W, 2026-10-03). So a
held-key set must decide which physical key an Insert is when it arrives:
under Alt it is I, otherwise Enter.

Backspace is `0x08` and Enter is `0x0a`. Alt `0xa1`, both Shift keys
`0xa2`/`0xa3`, and Ctrl `0xa5` were observed; Symbol `0xa4` was not.
`F1`–`F5` arrive as `0x81`–`0x85`, and `F10` as `0x90`, the MCU's Shift+`F5`
(verified on a Plus 2 W, and `F10` also on a Pico 2 W, September 2026).
`F6` arrives as `0x86`, the MCU's Shift+`F1` (checked on a Pico 2 W and
a Plus 2 W, October 2026). A function key
pressed with Alt still held arrives as itself, so match it whatever the Alt
state. Esc is `0xb1`, the arrows `0xb4`–`0xb7` (left, up, down, right), Del
`0xd4`, and **Caps Lock is an event too**, `0xc1` pressed and released,
while the MCU also applies it to the letters it sends (a Plus 2 W,
2026-10-08: after it, letters arrived as capitals with no Shift). A host
that does not bind it must still accept it.

**A release can arrive with no press before it.** In the same capture,
Alt pressed and released, then a release of Space with no press of Space
before it. The operator did not mean to press Space with Alt held. One
reading is a rolled press: Space went down a moment before Alt came up, the
MCU took it as Alt+Space, which it consumes (§6.3), and the release came
after Alt's. A press lost some other way would look the same. It was seen
once and is not explained. Ignore a release for a key that is not down.

**Observed on a Plus 2 W, 2026-10-29**, every key pressed alone and with
Shift, Ctrl and Alt (808 events): every code in the MCU's published
keymap arrived except Caps Lock, which was not pressed, and no code
outside it. **F6–F10 arrive as `0x86`, `0x87`, `0x88`, `0x89`, `0x90`**
(Shift+F1…F5), so F7–F9 match the source. **Ctrl+Alt+Del is delivered**:
Ctrl `0xa5`, Alt `0xa1`, then Del `0xd4`, Del unchanged by either.
**Letters under Alt arrive as capitals** (Alt+F as `0x46`): the source
skips its lower-casing when Alt is held, so with Alt only Shift says
whether a capital was meant. Ctrl leaves letters lower case. The Space
release with no press was seen again, after Alt. Events in the FIFO
survive a reset of the processor: keys pressed while a program was being
flashed arrived at the next program's first poll. The power button sends
`0x91` pressed, with no release, from the MCU's PMU interrupt (source;
not observed).

### 6.3 The keymap decides which chords exist

**Shift is resolved in the MCU, not host-side, and it can swallow a key
entirely.** Each key has a character and an *alternate*; a shifted non-letter is
replaced by its alternate, and if the alternate is zero **nothing is sent at
all**. Shift + Left, Shift + Right, Shift + Space and Shift + Backspace are
therefore unreachable by any host code. The swallowing covers **release too**:
hold Left, press Shift, let go of Left, and no release event ever arrives, so
a held-key set built from events (§6.2) keeps Left down. Never design a
binding that needs Shift held together with an arrow, such as fire on Shift
while moving. (This follows from the keymap source; a design was changed for
it in September 2026 rather than tested to failure.) Ctrl and Alt are different — they pass
the key through unchanged alongside a modifier event, so Ctrl chords are yours
to define.

Several keys exist *only* as shifted alternates: Home is Shift+Tab, End is
Shift+Del, PgUp/PgDn are Shift+Up/Down, Break is Shift+Esc, Insert is
Shift+Enter or Alt+I. And Alt+`,`/`.`/Space/`B` are consumed by the MCU itself
for the backlights and battery display.

**Read the keymap before designing a binding.** The full register map, event
cadence, matrix tables and the list of swallowed chords are in
[official keyboard MCU source](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/keyboard.ino).
Match it to the installed firmware; a header constant does not prove that the
firmware dispatches that register or emits that event.

**Observed on a Plus 2 W, 2026-11-07**, a first-person game played from the
keyboard for about eleven minutes, and the keys pressed alone and with
Shift, Ctrl and Alt (B was missed with Alt): 2,326 FIFO events, the FIFO
drained every 20–33 ms. Measured from the log of every event; read
alongside the MCU's source above.

- **Every code in the MCU's published keymap arrived**, apart from the
  power button's `0x91`, which was not pressed, and **no code outside
  it**: `0x20`–`0x7e` (letters as capitals with Shift and
  with Alt; none was typed with Caps Lock on), F1–F10 as `0x81`–`0x89` and `0x90`, the four modifiers, Esc,
  the arrows, Caps Lock, and Break, Insert, Home, Del, End, PgUp and PgDn.
- **Alt with an arrow arrives as the arrow, with Alt held**: Alt `0xa1`
  pressed, then `0xb4`–`0xb7` unchanged, all four. **Alt with F1** arrives
  as `0x81` with Alt held.
- **Five keys held at once were all delivered**: `` ` ``, `/`, Up, Space and
  Ctrl, each with its own press and release, and Up, Ctrl and Space
  held together twice. Nothing was lost to ghosting in this session.
- **Alt with Space, `,` or `.` sends nothing** when the key goes down
  after Alt: in the whole session no press of any of them arrived under
  Alt. The MCU acts on them itself, as the board's owner reports: Alt+`,`
  and Alt+`.` change the screen's brightness, Alt+Space the keyboard's,
  and Alt+`B` shows the battery's state (Alt+`B` was not logged in this
  session). A Space already held when Alt goes down keeps sending its repeats
  (as presses) with Alt held.
- **Shift with Left or Right sends nothing**, release included: six times
  Shift went down with Left or Right held, and no event for that arrow
  came until it was pressed again. A program that lets go of a held Left
  or Right when Shift goes down keeps no arrow stuck.
- **Insert held (Shift+Enter) repeats as held events**, state 2, like the
  arrows and modifiers, not as presses like the printable keys.
- **Each FIFO read took 4.82–4.92 ms** (an empty read 4,823 µs, 50 a second
  with nothing else on the core), with no failed transaction in 693 s.

**2026-11-07, Pimoroni Pico Plus 2 W in a PicoCalc, 300 MHz, a game
playing with full 320×240 frames sent to the LCD at 75 MHz SPI (about 20 ms
each) on the same core as the keyboard poll; measured from UART logs over
1,009 s of play by hand, two boots.**

- **Polling between whole-frame sends, the FIFO was read 32–50 times a
  second, median 36**, against a 20 ms (50 Hz) schedule: each send is
  followed by one poll. Each read took **4.88 ms**. No failed I²C
  transaction in the session.
- **Read to use**: an event waited a median of 15 ms (p90 21 ms) from its
  FIFO read to being taken by the game, and a press reached the next frame
  sent a median of 30 ms after its read (p90 46 ms, longest 93 ms); the
  MCU's own scan comes before all of it. The player, using arrows, Ctrl,
  Space, Alt and `,` `.`, found the keys responsive at that rate.
- **Caps Lock works as a toggle key**: each tap is one press (state 1) and
  one release (state 3), with no repeats; four taps in play gave four
  presses.

---

## 7. Storage

### 7.1 SD card

Use `spi0` at **400 kHz for initialization**, then request **25 MHz** as a
conservative operating point. Card detect is **GP22, active low**, with an
internal pull-up. Transfers use 512-byte blocks. Distinguish SDSC byte
addressing from SDHC block addressing, bound command/token/busy waits and
retries, and report the configured rate.

The card uses a separate SPI instance from the LCD, so there is no shared
chip-select bus arbitration between them. They still compete for CPU time,
SRAM bandwidth and, if used, DMA channels. Card write latency can exceed a
frame or audio-buffer deadline. Pause at a defined application boundary or
provide an independently serviced audio path during storage operations.

Filesystem support is software policy: FAT32 support does not imply exFAT
support. Check missing/full media, removal, corrupt files and interrupted writes
on physical cards. Host fault tests and successful firmware builds do not
qualify card compatibility or power-loss recovery. The 400 kHz/25 MHz settings here are driver configuration guidance, not a
measured physical-card reliability result.
Use a temporary file and an explicit publish/recovery policy to preserve prior
valid data where possible; filesystem rename alone is not proof of power-loss
atomicity. A pattern that works: write `name.new` and close it, unlink the
old file, rename the new one into place, and on load, fall back to a whole
`name.new` when `name` is missing or fails its check, which is what an
interrupted publish leaves. FAT's read-only attribute is a convenient
user-visible write-protect flag.

**Measured** on a Plus 2 W, 2026-09-23, FatFs over polled SPI at 25 MHz
requested: reading a 2.5 KiB disc-image track (ten 256-byte sectors) took
17–19 ms and writing one 25–27 ms. Both are longer
than a 16.7 ms video frame, which is why card work belongs at a boundary
where the application can stop and feed audio silence.

**Measured** on a Plus 2 W, 2026-10-04, the same driver: the first
initialisation and FAT mount after a card is powered or inserted took
192–211 ms, most of it the card answering `ACMD41` busy; a mount of a card
already initialised, after a reset that left it powered, took 15 ms.
Opening and reading a 231-byte file took 9.7 ms, and finding that a file
is not there 7.4–7.6 ms. **Card detect is not a clean edge.** The slot is
push-to-eject, and pulling and reinserting a card read as out, in, out, in:
the contact closed for longer than a 250 ms debounce during the card's
travel, and the mount attempted then failed with FatFs's `FR_NOT_READY`
before the card was really in. Debounce card detect, and treat a failed
mount after an insertion as a state to retry on the next change rather than
an error, since nothing about it hangs.

**Measured** on a Plus 2 W, 2026-10-08, the same driver: opening a 16 KiB
file, reading it in 512-byte pieces and hashing it with SHA-1 took
20.2–20.6 ms (two files), and an 8 KiB file 11.7 ms. A cold mount took
231–238 ms and a warm one 14.6 ms, as before.

**Measured** on a Plus 2 W, 2026-10-08 to 2026-10-10, the same driver,
larger transfers, each with nothing else using the card:

| Transfer | Time |
|---|---|
| a 59–99-byte text file opened, read and parsed | 7.1–8.1 ms |
| a 6,400-byte track read from a 538 KB image | 22.9–23.4 ms |
| the same track written back into the image | 39–45 ms |
| a 65,876-byte file written to `name.new`, the old file unlinked, the new renamed | 281 ms |
| the same file read twice, start to end | 142 ms |
| the same file read once, its CRC checked | 78 ms |
| a 307,254-byte file written in 7,680-byte pieces, through `.new` and a rename | 938 ms |

The last includes drawing each piece's rows and a keyboard poll every
33 ms between pieces, about 0.13 s of it at §6.1's cost. So a large file
reads at about 0.85–0.93 MB/s and writes at about 0.23–0.38 MB/s, on top
of the ~10 ms an open costs (above): a quarter-second write is ordinary,
and holds up everything that waits for the card.

Three more things from the same driver on the same board:

- **Bound every wait below the southbridge's 2.5 s watchdog** (§6.1) when one
  core does both card work and keyboard polling. A card that stays
  `ACMD41` busy is the longest case; a 1 s limit kept every job inside it.
  Jobs measured 7–221 ms.
- **A transfer that fails marks the card uninitialised**, so the next mount
  runs the full SPI initialisation again rather than talking to a card in
  an unknown state.
- **macOS writes a `._name` file beside every file it copies** to a FAT card
  (AppleDouble metadata). A file list on the device should skip names
  starting `._`; they are not the user's files.

**Measured** on a Plus 2 W, 2026-10-29, FatFs with exFAT enabled over
`spi0` at 25 MHz requested (Elehobica's driver, hardware SPI), a 32 GB
card with 30.7 GB free: initialising the card and mounting it, including
FatFs's free-space count, took **246 ms** for a card just put in and
**63 ms** for one that stayed powered through a reset of the processor.
Scanning a directory of three folders, with up to one file check in each,
took 6 ms. Card detect, debounced for 500 ms, was a reliable trigger for
"a card went in" with the slot empty at power-on.

**Measured** on a Plus 2 W, 2026-11-07, the same driver and settings, a
FAT32 card: a mount after the processor was reset over SWD, the card
staying powered, took **31–33 ms** in seven boots; a mount after the card
was pushed in, the slot having been empty at boot, **235 ms**. Opening a
13.0–18.5 MB file and reading its 37–50 KB directory (2,308–3,109
sixteen-byte entries) in one piece took **27–43 ms**. Again, card detect
debounced for 500 ms caught the insertion cleanly, and a watchdog reboot
made with the core that polls the southbridge parked between transactions
left the southbridge answering, with no I²C errors after.

**Measured** on a Plus 2 W, 2026-11-07, the same driver and settings, the
same FAT32 card, at 300 MHz: a game's save file written a few seconds into
a level, the whole sequence (a temporary file created, written in many
small writes, closed, the old file removed and the temporary one renamed
over it), took **85 ms** while the game ran. Two small text files of
settings written one after the other at exit, then the volume unmounted,
took **35 ms** together with the program's other exit work. Making a new
directory works only when FatFs can have a buffer for clearing it, or is
refused one (see the next paragraph).

**`f_mkdir` allocates up to 32 KB** with FatFs's `FF_USE_LFN 3` (working
buffers on the heap): to clear the new directory's cluster it asks
`ff_memalloc()` for the cluster size, capped at 32 KB, halving the request
on each NULL and clearing sector by sector if every request fails. The Pico
SDK's `malloc()` panics on failure instead of returning NULL
(`PICO_MALLOC_PANIC`, on by default), so with less heap than a cluster the
first new directory stops the program in `panic("Out of memory")`; with a
debugger attached the core halts at the panic's breakpoint, with no fault
recorded. Seen on a Plus 2 W, 2026-11-07, with about 4 KB of heap free and
32 KB clusters. Wrapping `ff_memalloc()` so it returns NULL for a request
that does not fit lets FatFs's own fallback run.

**Prefer the card to internal flash for anything the application writes**,
such as settings. A text file on the card is one source of truth the user can
read and edit on any computer, it survives reflashing, and writing it opens no
interrupt hole (§7.2).

### 7.2 Internal flash, XIP, and the interrupt hole

Code executes from flash through the QMI in execute-in-place mode, cached by a
**16 KB XIP cache** shared by both cores (and, on a Plus 2 W, by PSRAM reads).
Reserve a non-overlapping region explicitly in the flash layout before using
spare flash for a filesystem such as LittleFS. Size it against the physical
flash and the firmware/update scheme. Typical SDK operation geometry is
**4096-byte erase sectors and 256-byte program pages**.

While flash is outside XIP mode, neither core, an ISR nor DMA may read code
or data from it. Use the SDK's
[`flash_safe_execute`](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_flash/include/pico/flash.h)
with the appropriate safety setup for the runtime. With `pico_multicore`,
initialize participation using `flash_safe_execute_core_init()` on the core
that must be locked out. Check return values; a coordination timeout is not
proof that the callback never ran. Masking only the calling core's interrupts
is insufficient when the other core can access flash.

Additional constraints:

- **Program from SRAM.** The source buffer must remain accessible while XIP is
  unavailable. Quiesce DMA that reads flash or a QMI-backed memory window.
- **Account for PSRAM cache state.** On RP2350, preserve dirty PSRAM data and
  follow the SDK's cache-maintenance requirements around QMI/flash operations.
- **Bound each operation.** One erase sector or program page per critical
  interval limits the stall, but an erase can still exceed the ~3.5 ms audio
  refill deadline. Ring wrapping prevents out-of-bounds reads; it does not
  guarantee uninterrupted new audio while software refills are stopped.
- **Choose the visible behavior.** Pause/mute explicitly or provision a path
  that can continue safely without flash. Do storage at defined application
  boundaries rather than inside a latency-sensitive frame loop.

---

## 8. On-chip odds and ends

### 8.1 Die temperature

The RP2350's own sensor, so it costs no bus time and cannot be blocked by a
wedged southbridge. **The ADC channel differs between parts and the SDK already
hides it**: input 4 on RP2350A (Pico 2, Pico 2 W, QFN-60) and input 8 on
RP2350B (Plus 2 W, QFN-80), because the bigger package adds four user inputs
below it. `ADC_TEMPERATURE_CHANNEL_NUM` is `NUM_ADC_CHANNELS - 1` and
`NUM_ADC_CHANNELS` comes from `PICO_RP2350A` in the board header, so **no board
conditional belongs in your code**.

Two things the sensor's accuracy decides:

- **Average a burst.** A conversion is 12 bits over 3.3 V — about 0.24 °C per
  LSB, and noisy at that. Sixteen conversions cost ~32 µs total, which is
  nothing beside a 4.6 ms southbridge read.
- **Round the answer.** The datasheet formula (`27 - (V - 0.706) / 0.001721`) is
  *uncalibrated*, with several degrees of part-to-part offset, and it reads the
  die, which sits above the room by however hard the chip is working. A tenth of
  a degree is already generous.

Test it by *loading* the processor and requiring the reading to rise: a wrong
channel returns a floating GPIO, which reads as a plausible number.

The ADC block can be initialised lazily and the bias left on; `adc_init()`
touches no GPIO function, so the audio PWM on GP26/27 is unaffected.

**Unless the build's board is not the board.** One `pico2` image (RP2350A)
is a convenient way to run on all three RP2350 boards, but on a Plus 2 W, an
RP2350B, `ADC_TEMPERATURE_CHANNEL_NUM` is then 4, which on the QFN-80 part is
a GPIO. So read the package off `SYSINFO_PACKAGE_SEL` (1 = QFN-60), choose
input 4 or 8 yourself, and write `AINSEL` directly, since
`adc_select_input()`'s check uses the header's channel count. On a Plus 2 W
on 2026-09-27 the banner said QFN-80, input 8, and the reading passed the
load test: 23 °C idle at 150 MHz, 25 °C at 300 MHz and a 4 MHz guest on
boot, 27 °C after two and a half minutes of a compute loop.

On battery, at 150 MHz with one core about 21 % busy and the
other presenting and polling, a Plus 2 W read **20–21 °C throughout two
30-minute runs** (2026-10-05, uncalibrated, room temperature not recorded).
At this clock the die is not a constraint.

### 8.2 The remaining ADC inputs

GP26 and GP27 are ADC0/ADC1 on the silicon but are the audio outputs here, so
they are gone. **GP28 (ADC2) is the one free analogue input** on a 30-pin board.
GP29 is VSYS/3 on non-W boards and the radio clock on W boards — on a W board,
reading VSYS means wrapping the access in `cyw43_thread_enter`/`exit`, and the
battery gauge on the southbridge (§6) is a better source anyway.

### 8.3 USB

The module's USB port is reachable through the case. It is available for CDC
stdio, for mass storage, or for a USB host stack — but see §2.7: nothing routes
`printf` there unless you ask.

---

## 9. Making it fast

This section is mostly about **how to measure on this machine**, which turned out
to matter more than any individual optimisation.

### 9.1 Measure on hardware, in the mode you ship

Three separate times a number that looked authoritative turned out to be an
artefact of the mode the harness ran in:

- **Presents can be free by accident.** A blit that returns early in text mode
  reported a present of 0.25 ms. Every "frame time" in a months-long series was
  a *body* time with no present in it.
- **The same bug hid a 25 ms cost.** Clearing the screen in text mode is a bare
  `memset`; in graphics mode it is a full-panel fill. A setup routine
  "regressed" from 20 ms to 66 ms purely because it was finally being measured in
  the mode it runs in.
- **Boundary and clipping modes interact with dirty tracking.** In one wrapping
  mode, a stroke's round cap spilled off the left edge onto the right and
  dirtied the entire tile row, making every measurement read as a full screen.

Corollaries we now follow:

- **Always carry a control** — a quantity in each run that should *not* change.
  When it finally moved 2 %, that told us we had reached the noise floor.
- **Expect ~2 % run-to-run spread** on this machine. Anything smaller is not a
  result. (A 150 MHz baseline read 65.5, 68.8 and 68.1 ms across three sittings
  on the same board.)
- **One board per comparison.** Different flash parts mean different
  instruction-fetch behaviour; we burned real time drawing a conclusion from the
  difference between a Pico 2 measurement and a Plus 2 W one.
- **Write measurements to a file.** A number on a 320×320 screen cannot be
  copied off it.

### 9.2 Instruction fetch through the XIP cache is a first-order cost

For large, branchy workloads, instruction fetch can dominate. Flash code uses a
16 KB cache shared by both cores and by PSRAM data. Moving hot functions into
SRAM with `__not_in_flash_func` produced, on a Plus 2 W:

| Tier | What moved | Frame | Speedup | SRAM cost |
|---|---|---:|---:|---:|
| — | baseline | 81.0 ms | | |
| 1 | 4 hot functions (3.1 KB of expression evaluator) | 65.5 | **1.24×** | 6.0 KB |
| 2 | 8 more (call path, variable lookup) | 53.2 | **1.23×** | 3.6 KB |
| 3 | loop and run-list path | 48.1 | 1.105× | 1.7 KB (fit in existing padding) |
| 4 | frames and bindings | 47.0 | 1.024× | 2.0 KB |

**1.72× cumulative for 13.6 KB of SRAM**, verified each time by checking that
the symbols moved from `0x1…` to `0x2…` in the image.

Three lessons that generalise well beyond an interpreter:

1. **Returns halve every tier.** Stop when the curve says so, not when the
   budget runs out.
2. **Every move relays the flash residue against the cache, and whatever is left
   adjacent to the boundary gets worse.** Tier 1 improved the evaluator and
   regressed procedure calls 29 %; tier 2 fixed calls and regressed the loop
   67 %; tier 3 fixed the loop and regressed calls again. It is self-limiting —
   the regressing line is always smaller than the set that improved — but **you
   must re-measure the whole profile after every move**, not just the thing you
   moved.
3. **It is not universal.** The same technique applied to a tight tile-copy loop
   produced *nothing* (§9.3). Instruction fetch dominates code that is large and
   branchy; a copy loop is data-bound and does not care. Measure before spending
   SRAM.

The functions worth moving were the big branchy ones **and** the tiny
constantly-called accessors (120–164 bytes, the best value per byte). Whole
modules are the wrong unit; individual hot functions are the right one.

### 9.3 Where the data lives matters as much as where the code lives

A bulk copy from PSRAM into an SRAM canvas, in 8-byte runs, measured **7.45–7.6
ms for 64,512 pixels** on a Plus 2 W — about 118 ns per pixel, only ~2× faster
than the SPI wire, on a 150 MHz core. We chased it:

| Suspect | Verdict |
|---|---|
| Per-row bookkeeping | **Red herring** — ~20 µs total. |
| Code not RAM-resident | **Disproved by experiment.** Moving the loop and its helpers to SRAM (+2 KB) gave 7.45 → 7.6 ms: no change, or slightly worse. Reverted. |
| **The data path** | **The leading explanation.** Source in PSRAM, destination in SRAM, small runs. That fits 118 ns/px far better than instruction fetch does. |

So: **keep per-frame working sets in SRAM.** Use PSRAM for capacity, not for the
hot path — or at least measure before assuming the XIP cache hides it.

**The cost lands on the other core.** Measured on a **Pimoroni Pico Plus 2 W
(RP2350B), 2026-10-29**, clk_sys 300 MHz, PSRAM at 100 MHz, flash at 50 MHz:
core 0 ran a C++ animation from flash (about 3.6 MB image) drawing 320×200
frames into PSRAM and copying each to a 320×240 buffer in PSRAM; core 1
read each finished buffer out of PSRAM to the LCD (§4.7). Core 0's frame
rate, over the same 15 seconds of the animation, four builds in one
sitting:

| Core 1 reads the frame | Core 0 frames/s | Core 0's loss |
|---|---:|---:|
| Not at all (takes it and gives it back) | 29.9 | — |
| By DMA from the uncached alias into SRAM | 28.0 | 6 % |
| Through the uncached alias, by the CPU | 27.6 | 8 % |
| Through the cached XIP window | 26.5 | **11 %** |

Reading through the cache evicts the other core's code and data from the
16 KB cache they share; reading past it still contends for the QMI but
leaves the cache alone. Reading past the cache needs the writer's lines
cleaned out first: `xip_cache_clean_range()` over the 76,800-byte buffer
took **0.45–0.69 ms** of core 0's time per frame (the longest seen), part
of the 6–8 %.

**A renderer's target buffer in SRAM instead of PSRAM: 1.58× the frame
rate.** Measured on a **Pimoroni Pico Plus 2 W (RP2350B), 2026-11-07**,
clk_sys 300 MHz at 1.20 V, PSRAM at 100 MHz, flash at 75 MHz, Arm GCC
15.2.Rel1: a Chocolate Doom engine (doomgeneric) run from flash, with its
3 MB zone heap (textures, sprites, flats) in PSRAM, replaying a recorded
demo as fast as it can (`-timedemo demo1`, The Ultimate Doom 1.9, 1,710
tics), core 1 sending finished frames to the LCD. The engine draws each
320×240 8-bit frame into one buffer, then copies it to a second buffer in
PSRAM that core 1 reads past the cache. Builds in one sitting, each run
once:

| Where the engine draws | Frames/s | The 76,800-byte copy to PSRAM |
|---|---:|---:|
| In PSRAM, through the cached window | 28.0 | 9.7 ms (PSRAM to PSRAM) |
| In SRAM | **44.2** | 6.1 ms (SRAM to PSRAM) |
| In SRAM, and the 8,704-byte light table read for every pixel copied from PSRAM to SRAM | 46.4 | 6.1 ms |

The renderer writes each pixel once, in columns, so a frame in PSRAM goes
through the 16 KB XIP cache one byte per line touched, and the cache lines
it evicts belong to the code and textures too. Even in the second build the copy
into PSRAM is a quarter of core 0 at 44 frames a second (the PC samples
put 28 % of core 0 in `memcpy`), at about 12.6 MB/s for a write through
the cached window followed by a clean. An 8.5 KB table read for every
pixel costs about 5 % left in PSRAM.

### 9.4 Small `memcpy` calls are call overhead

A sampler that copies one 8-pixel tile run at a time makes 28 calls per row and
8,000 for a screen. At that size the call *is* the cost. Hand-unroll, or
specialise for the two or three run lengths you actually support.

### 9.5 Core 1 can own slow peripherals

A working split at **150 MHz** gives core 0 CPU-intensive simulation and audio,
and core 1 the LCD and southbridge. Use an immutable frame handoff with explicit
ownership and a bounded input-event queue. Polled LCD DMA needs no interrupt;
the audio DMA IRQ can remain on core 0. Local interrupt masking is not a
multicore lock.

In a measured emulator workload on a Plus 2 W, initial flash execution managed
**0.55–0.60× real time**, with about 84% of wall time in guest execution and
12% in 10 kHz I2C polling. Moving peripheral work to core 1, executing code from
SRAM, scoped LTO and CPU/audio optimizations together restored short-run
real-time operation. This does not isolate a speedup for any single change.
A **20 µs hardware-timer wait** between peripheral service iterations avoided
continuous shared-state polling while DMA was active.

Short captures had zero remaining lag, PCM underruns, late DMA refills, input
loss and device errors. More than an hour of clean visual/audio operation was
subsequently reported on each of a Pico 2 W and Plus 2 W; those long observations
had no contemporaneous UART capture. They demonstrate a viable ownership split,
not a universal compute-headroom guarantee.

**Core 1's stack is 2 KB by default, whatever the linker script reserves.**
`multicore_launch_core1()` uses `PICO_CORE1_STACK_SIZE` (0x800 in SDK 2.3.1)
below `__StackOneTop`, so a linker script with a 4 KB SCRATCH_X still gives
core 1 only its top 2 KB (`0x20080800`–`0x20081000`), read from the ELF's
symbols, 2026-10-29. Measured with the stack painted before launch: an LCD
presenter, an I²C poll, a UART log pump and a `printf` heartbeat on core 1
reached **660 bytes** (Plus 2 W, Arm GCC 15.2, `-O3`).

Use compact source snapshots or generated bands when full-frame double
buffering would consume too much SRAM. Keep producer writes away from any
buffer still owned by DMA. Both cores share the XIP cache, so measure the main
workload with and without peripheral activity before assuming ideal overlap.
Coordinate flash writes with both cores (§7.2).

### 9.6 Levers, in order

If you need to reduce frame time:

1. **Transmit less area.** A smaller source only helps wire time if the driver
   also narrows the window; unchanged HUD regions need not be resent.
2. **Present less often than you simulate.** Retain simulation timing while
   dropping superseded presentation snapshots.
3. **Use hardware vertical scroll where appropriate** (§4.8).
4. **Overlap peripheral work on core 1** with explicit ownership (§9.5).
5. **Measure SRAM placement of hot code** before spending more memory (§9.2).
6. **Evaluate clock changes only with all peripheral clocks accounted for**
   (§3). At a requested 75 MHz SPI rate, 300 MHz has favorable divider arithmetic
   compared with 200/250 MHz, but it remains an RP2350 overclock.

### 9.7 `sleep_us` on core 1 interrupts core 0

`sleep_us`, `sleep_until` and `best_effort_wfe_or_timeout` set an alarm in
the SDK's **default alarm pool**, and that pool's IRQ belongs to the core
that initialised it — core 0, in `runtime_init`. A core 1 loop that sleeps
for 20 µs between iterations (§9.5's pattern) therefore takes an interrupt
**on core 0** every iteration, tens of thousands a second, in the middle of
whatever core 0 is doing.

In a 6502 emulator, on a Plus 2 W, switching core 1's wait to
`busy_wait_us_32(20)`, which polls the timer and sets no alarm, cut core 0's
cost per emulated instruction by 1.11×. Run-to-run spread fell from ±4 to
±0.1 cycles per instruction. It is still a hardware-timer wait rather than
spinning on shared state, which is what §9.5 asked for.

So measure core 0 with core 1 idle as well as busy. If core 0's timing
wanders while core 1 has nothing to do, look for sleeps on core 1. Use
`busy_wait_*` there, or give core 1 its own alarm pool.

**Check drivers when they change cores.** An SD driver brought over from
another project had one `sleep_us` in its initialisation; on core 1, it
would have interrupted core 0. Search any code bound for core 1 for `sleep_` and
`best_effort_wfe_or_timeout`.

### 9.8 SRAM placement, measured on an interpreter

§9.2's tiers, applied to a 6502 interpreter, gave
**1.12–1.19× for 25 KB**, not 1.7×. The same method, on a different kind
of code, gave a third of the gain:

- `-O3` inlined the bus into every opcode and made the interpreter 20 KB.
  But a real program uses a few dozen opcodes, and that part fits the
  16 KB cache.
- Moving the small callees first (4.4 KB) was worth 1.04–1.11×, and it
  regressed one workload 4 % (§9.2's relayout effect). The interpreter on
  top was worth another 1.06–1.08× on every workload, and it recovered
  the regression.
- A 256-byte const table moved to SRAM measured nothing.

Two build traps cost a pass each. **Build each tier in its own build
directory.** Make judges freshness by mtime, and a reconfigure that lands in
the same second as the last compile leaves stale objects: an image with half
its tier. **Check every symbol's address in each image**, not just the
first build's. Whatever an SRAM function calls in flash goes through a
long-branch veneer (`__name_veneer` in `nm`), so the veneer list is the
quick check that a tier is whole.

A third trap: **`static inline` is not a promise.** At `-O3`, GCC kept
out-of-line copies of a Z80 interpreter's small memory helpers (read,
16-bit immediate, push, pop) in ordinary `.text`, and the SRAM tier called
them in flash through veneers. Give such helpers the same section
attribute as their callers. It costs nothing where they are inlined.

The converse trap: **a hot function inlined into a cold caller runs where
the caller is.** A 6502 emulator (2026-10-08, gcc 15.2 `-O3`) marked its
run loop for SRAM, and its benchmark, which called the loop directly,
found it there. The emulator called it only from a once-a-field wrapper
in ordinary `.text`; GCC inlined the loop into the wrapper and dropped the
SRAM copy, so `nm` showed no symbol for it, and the loop ran from flash
and called the interpreter in SRAM through a veneer every instruction.
Mark the caller too, and look for every hot name in `nm`, not only the
ones that are there.

The same method on a Z80 interpreter (2026-10-03, Plus 2 W at 150 MHz, 30 KB
in SRAM) gave the same shape: **1.20×** on ZEXDOC's wide instruction mix,
and nothing (−1.2 %) on a small Forth inner-interpreter loop whose opcodes
already fit the cache.

The whole emulator around that Z80 interpreter (2026-10-04, same board and
clock, its real ROM, core 1 presenting) showed what the benchmark could
not, because the benchmark had the XIP cache to itself:

- **With the interpreter in flash, layout outside it decided the result.**
  Four builds whose differences were all outside the interpreter read
  37.9, 41.1, 34.2 and 31.8 % of core 0 on one compute loop, each steady
  within its run. With the interpreter in SRAM (33 KB), the same loop read
  23.3 % in three sittings, to 0.1 point. So measure placement before any
  other change, and compare other changes only at the placement you ship.
- **Once the interpreter left the cache, 8 KB of guest ROM data fitted it.**
  Copying the ROM into SRAM at boot changed nothing at that tier (to 0.1
  point), against 0.1–1.3 points with the interpreter still in flash.
- **Audio's cost fell with it.** The audio code cost the foreground 0.4–2.0 points with the interpreter in flash, mostly as extra
  cycles per guest instruction rather than time in audio code, and 0.4–0.8
  with it in SRAM.

A 6502 interpreter's benchmark agreed (2026-10-07, Plus 2 W at 150 MHz,
gcc 15.2 `-O3`, one core, the cache to itself). From flash it took 142.3
host cycles per instruction on a BASIC-shaped loop and 140.9 on Dormann's
functional test. **Moving only the small callees** (bus slow path, device
tick, ADC/SBC, 1.8 KB) **made it slower**, 154.7 and 147.6: the interpreter
stayed in flash and now called those functions through veneers. **Moving the
interpreter as well** (24 KB) gave 144.7 and 138.8, within 2 % of flash
either way. Each figure held to 0.02 % over seven to nine runs. So a tier of
callees alone is not a step on the way. Measure it, or skip it.

A fourth build trap: **CMake caches options in the build directory.** When a
project changes an option's default (here the SRAM tier), a directory
configured earlier keeps its cached value until the option is passed once.
A fresh directory per tier (above) avoids it.

---

## 10. Bring-up order and trap checklist

A practical bring-up sequence for `main()`; reserve DMA channels and IRQ
ownership explicitly rather than relying on initialization order:

1. **Southbridge I²C** — nothing else needs it, but the backlight does, and a
   dead bus is the first thing you want to know about.
2. **LCD** — reset, init sequence, clear frame memory, *then* `DISPON`.
3. **Keyboard** — on top of the southbridge.
4. **Audio** — the chained-buffer recipe needs two DMA channels and an IRQ;
   confirm that they do not conflict with display or storage allocations.
5. **PSRAM verification** (Plus 2 W), *before* handing the region to an
   allocator and before any flash mount, since flash writes drive the same QMI.
6. **Filesystems** — internal flash, then SD.
7. **Wi-Fi** — lazily, on first use; it costs a second and a lot of SRAM.

Checks to retain in each new driver/application:

- [ ] `spi_set_format()` and the `D/CX` write go **before** CS low — the 40 ns
      CS-high rule (§4.3).
- [ ] Drain the SPI RX FIFO and clear the overrun flag at the end of every DMA
      blit; restore 8-bit format.
- [ ] Never touch the LCD from an interrupt handler. Timers set flags.
- [ ] Never mask interrupts around a blit — audio has a 3.5 ms deadline.
- [ ] Audio DMA ring: power-of-two **and aligned**, with the hardware read wrap.
- [ ] Re-arm both read address **and** transfer count when re-arming a chained
      DMA channel from its IRQ.
- [ ] Audio IRQ priority above default (`0x40`); nothing else may run long at
      default priority.
- [ ] Every function on the audio IRQ path `__not_in_flash_func`.
- [ ] Verify PSRAM with a real round trip before handing it to an allocator.
- [ ] Re-apply the SPI baud rate, the PSRAM QMI timing, the CYW43 divider and
      the audio carrier after **any** change to `clk_sys` (§3).
- [ ] A firmware that ever raises the core rail sets it explicitly at 150 MHz
      too; the regulator survives a reset (§3).
- [ ] No `sleep_us`/`sleep_ms` in core 1's loop; `busy_wait_us_32` (§9.7).
- [ ] No blocking `printf` on a core with a deadline (§2.7).
- [ ] SD waits bounded below the southbridge's 2.5 s watchdog when one core
      does card work and keyboard polling (§7.1).
- [ ] Counters read over SWD read twice and kept only when they agree (§2.7).
- [ ] A Mac's display kept awake (`caffeinate -d`) while the Debug Probe is
      in use (§2.7).
- [ ] Each SRAM-placement tier built in its own build directory, and its
      symbols checked with `nm` (§9.8).
- [ ] Remap y through the vertical-scroll offset in *every* blit path, or do not
      use hardware scroll at all.
- [ ] Over-mark dirty regions; snapshot and clear before sending.
- [ ] No flash writes inside a frame loop; coordinate both cores and DMA
      before taking flash out of XIP mode (§7.2).
- [ ] Poll the keyboard from the frame loop, not a timer IRQ; guard the I²C bus
      with a busy flag; build held-key state from press/release events.
- [ ] Budget SRAM for frame storage, code, queues, heap and both-core stacks.
- [ ] Count PCM underruns separately from late DMA refills.
- [ ] Normalize shifted key releases and distinguish auto-repeat from fresh presses.
- [ ] Log physical board identity separately from the SDK build target.
- [ ] Profile in the mode you ship, with a control quantity, expecting 2 %
      spread, on one board.

---

## 11. Official references

- [ClockworkPi PicoCalc repository](https://github.com/clockworkpi/PicoCalc) —
  hardware, firmware, assembly instructions and controller documentation.
- [PicoCalc mainboard V2.0 schematic](https://github.com/clockworkpi/PicoCalc/blob/master/clockwork_Mainboard_V2.0_Schematic.pdf) —
  check carrier wiring and revision-specific details before using spare pins.
- [ST7365P controller specification](https://github.com/clockworkpi/PicoCalc/blob/master/ST7365P_SPEC_V1.0.pdf) —
  command, memory and electrical timing reference for that controller.
- [ClockworkPi firmware examples](https://github.com/clockworkpi/PicoCalc/tree/master/Code) —
  panel initialization and platform integration examples.
- [Keyboard MCU implementation](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/keyboard.ino),
  [register definitions](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/reg.h),
  and [key definitions](https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/keyboard.h) —
  protocol dispatch, translated events and reserved chords.
- [Raspberry Pi board documentation](https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html)
  and [Pico SDK documentation](https://www.raspberrypi.com/documentation/pico-sdk/) —
  board selection, clocks, peripherals and SDK APIs.
- [RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
  and [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf) —
  silicon limits, peripheral semantics and errata.

Match the schematic, controller and keyboard firmware to the actual device.
Upstream source can change; record the revision used when qualifying a driver.
