#!/usr/bin/env bash
# profile.sh — sample a running core's program counter over SWD and say
# where its time goes, by function (design.md §16 M5, M9; hardware-notes.md
# §9.2). The core is not halted: each sample is a read of the DWT's PC
# sample register (DWT_PCSR) through the Debug Probe.
#
#   scripts/profile.sh                    # core 0, 10 s, build/pico's image
#   scripts/profile.sh 30                 # 30 s
#   scripts/profile.sh 30 path/to/image.elf
#   CORE=1 TOP=40 scripts/profile.sh 10   # core 1, the 40 busiest functions
#
# Attach while the scene to measure is on the panel; the program carries
# on. The image must be the one running, or the names are wrong. Samples
# land in out/profile-<time>.txt (raw PCs) and the report beside it
# (.report). A sample that reads 0xffffffff (core halted or sleeping) is
# counted apart.
set -euo pipefail

case "${1:-}" in
    -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

root="$(cd "$(dirname "$0")/.." && pwd)"
secs="${1:-10}"
image="${2:-$root/build/pico/picocalc-doom.elf}"
core="${CORE:-0}"
top="${TOP:-30}"
if [ ! -f "$image" ]; then
    echo "profile.sh: no image at $image" >&2
    exit 1
fi

# OpenOCD and the toolchain's nm, found as flash.sh and build.sh find them.
openocd="${OPENOCD:-}"
if [ -z "$openocd" ]; then
    if command -v openocd >/dev/null 2>&1; then
        openocd="$(command -v openocd)"
    else
        openocd="$(ls -d "$HOME"/.pico-sdk/openocd/*/openocd 2>/dev/null | sort -V | tail -n1 || true)"
    fi
fi
if [ -z "$openocd" ] || [ ! -x "$openocd" ]; then
    echo "profile.sh: OpenOCD not found; install the Raspberry Pi fork or set OPENOCD" >&2
    exit 1
fi
scripts=()
if [ -d "$(dirname "$openocd")/scripts" ]; then
    scripts=(-s "$(dirname "$openocd")/scripts")
fi
nm="$(command -v arm-none-eabi-nm || ls -d "$HOME"/.pico-sdk/toolchain/*/bin/arm-none-eabi-nm 2>/dev/null | sort -V | tail -n1 || true)"
if [ -z "$nm" ]; then
    echo "profile.sh: arm-none-eabi-nm not found" >&2
    exit 1
fi

mkdir -p "$root/out"
raw="$root/out/profile-$(date +%Y%m%d-%H%M%S).txt"
report="${raw%.txt}.report"

# DEMCR.TRCENA turns the DWT on; it is left on. No reset, no halt: init
# only examines the cores.
echo "profile.sh: core $core for ${secs}s -> $raw" >&2
"$openocd" ${scripts[@]+"${scripts[@]}"} \
    -f interface/cmsis-dap.cfg \
    -f target/rp2350.cfg \
    -c "adapter speed ${SWD_KHZ:-5000}" \
    -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
    -c "init" \
    -c "targets rp2350.cm$core" \
    -c "set demcr [read_memory 0xE000EDFC 32 1]; write_memory 0xE000EDFC 32 [expr {\$demcr | 0x01000000}]" \
    -c "set s {}; set end [expr {[clock milliseconds] + $secs * 1000}]
        while {[clock milliseconds] < \$end} { lappend s [read_memory 0xE000101C 32 1] }
        set f [open {$raw} w]; puts \$f [join \$s \"\\n\"]; close \$f" \
    -c "shutdown" 2>"$raw.openocd" || {
    echo "profile.sh: OpenOCD failed; its output is in $raw.openocd" >&2
    tail -n 5 "$raw.openocd" >&2
    exit 1
}

"$nm" -n -C --defined-only "$image" | python3 -I -c '
import bisect, collections, sys

raw, secs, top = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
addrs, names = [], []
for line in sys.stdin:
    parts = line.rstrip("\n").split(" ", 2)
    if len(parts) < 3 or parts[1] not in "TtWw":
        continue
    addrs.append(int(parts[0], 16) & ~1)
    names.append(parts[2])

def region(pc):
    if pc < 0x10000000: return "bootrom"
    if pc < 0x20000000: return "flash"
    if pc < 0x20082000: return "SRAM"
    return "other"

by_fn, by_region = collections.Counter(), collections.Counter()
n = idle = 0
for line in open(raw):
    line = line.strip()
    if not line:
        continue
    pc = int(line, 16) if line.lower().startswith("0x") else int(line)
    if pc == 0xFFFFFFFF:
        idle += 1
        continue
    pc &= ~1
    n += 1
    r = region(pc)
    by_region[r] += 1
    i = bisect.bisect_right(addrs, pc) - 1
    name = names[i] if i >= 0 and r != "bootrom" else "(%s %08x)" % (r, pc & ~0xFF)
    by_fn[(name, r)] += 1

total = n + idle
print("%d samples in %.0f s (%.0f a second); %d read 0xffffffff" % (total, secs, total / secs, idle))
if n == 0:
    sys.exit(1)
print("by region: " + ", ".join("%s %.1f%%" % (r, 100.0 * c / n) for r, c in by_region.most_common()))
print()
cum = 0
for (name, r), c in by_fn.most_common(top):
    cum += c
    print("%6.2f%% %6.2f%%  %-7s %s" % (100.0 * c / n, 100.0 * cum / n, r, name))
' "$raw" "$secs" "$top" | tee "$report"
