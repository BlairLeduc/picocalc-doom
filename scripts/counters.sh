#!/usr/bin/env bash
# counters.sh — read the heartbeat's counter block over SWD, with both
# cores running (design.md §14.2, §16 M10; picocalc/counters.h;
# hardware-notes.md §2.7). It is how the build that ships, which has no
# UART, is watched. From picocalc-quest's (D8).
#
#   scripts/counters.sh                          # one read, build/pico-release's image
#   scripts/counters.sh path/to/image.elf        # the image that is running
#   scripts/counters.sh --watch 30               # M10's soak: a read every 10 s for 30 min, then check
#   WATCH_EVERY=60 scripts/counters.sh --watch 30
#   scripts/counters.sh --check out/counters-....log   # check a watch's log only
#   scripts/counters.sh --hashes                 # the frame hash list, against the reference
#
# The block is found by its symbol in the image, so the image must be the
# one running. OpenOCD starts with `init` alone: no reset, no halt, so
# the audio plays on. A watch keeps one OpenOCD for the whole run and
# writes a line a read to out/counters-<time>.log. A read is taken twice,
# up to six times, until two agree, since a read can tear against the
# heartbeat writing the block (hardware-notes.md §2.7).
#
# --hashes reads the frame hash list a run with `-framehashes` in Doom's
# arguments kept (design.md §15; `args = -timedemo demo1 -framehashes` in
# picocalc.ini), once Doom has stopped, into out/counters-hashes.txt, and
# compares it with test/host/engine/doom-demo1.crc (scripts/host_engine.sh
# compare).
#
# The check, as soak_check.sh's for the development build (EL §11.5),
# from the first read in a level to the last, over at least the minutes
# asked for: every read finds the block; its seconds never go back (one
# boot) and keep pace with the wall clock; frames published and the game
# tic rising between every two reads, and the level tic too while in a
# level; keys taken in every minute; late refills, I2C errors, keys lost
# and dropped OPL writes not rising (any from before play is shown);
# underruns and OPL late halves only between two
# reads with a level load in them (the level tic gone back, or out of a
# level). Says whether the battery ran it throughout. Exits 1 if it fails.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
mode=read
minutes=0
image=
log=
while [ $# -gt 0 ]; do
    case "$1" in
        --watch) mode=watch; minutes="$2"; shift 2 ;;
        --check) mode=check; log="$2"; shift 2 ;;
        --hashes) mode=hashes; shift ;;
        -h|--help) sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) echo "counters.sh: unknown option $1" >&2; exit 2 ;;
        *) image="$1"; shift ;;
    esac
done
image="${image:-$root/build/pico-release/picocalc-doom.elf}"
every="${WATCH_EVERY:-10}"

# The block's 32-bit words, in counters.h's order; then the version's 8.
check() {
    python3 -I - "$1" "$2" <<'EOF'
import sys

FIELDS = ["magic", "layout", "seconds", "busy", "busy_max", "rendered", "published",
          "presents", "dropped", "present_max_us", "late", "underruns", "music_late",
          "opl_dropped", "keys", "ring_full", "unknown", "stray", "i2c_errors",
          "log_dropped", "core0_stack", "core1_stack", "heap", "tic", "leveltime",
          "gamestate", "temp_c", "battery", "loader", "image_kb", "partition_kb",
          "hashes", "hash_list"]
MAGIC, LAYOUT = 0x4d4f4f44, 1
GS_LEVEL = 0

def parse(line):
    parts = line.split()
    ms, words = int(parts[0]), [int(w, 0) for w in parts[1:]]
    if len(words) != len(FIELDS) + 8:
        return ms, None
    c = dict(zip(FIELDS, words))
    for k in ("temp_c", "battery"):
        if c[k] >= 1 << 31: c[k] -= 1 << 32
    raw = b"".join(w.to_bytes(4, "little") for w in words[len(FIELDS):])
    c["version"] = raw.split(b"\0")[0].decode("ascii", "replace")
    return ms, c

def battery(c):
    b = c["battery"]
    return "unknown" if b < 0 else "%d%%%s" % (b & 0x7F, " charging" if b & 0x80 else "")

def show(c):
    pm = lambda v: "%d.%d%%" % (v // 10, v % 10)
    return ("%s, up %d s: core 0 busy %s (max %s); frames drawn %d, published %d, "
            "presented %d, dropped %d, present max %d us; audio late %d, underruns %d, "
            "OPL late %d, OPL dropped %d; keys %d, ring full %d, unknown %d, stray %d; "
            "i2c errors %d, log dropped %d; stacks core 0 %d B, core 1 %d B, heap %d B; "
            "tic %d, level tic %d, state %d; temp %d C, battery %s; %s, image %d KB, "
            "partition %s" % (
            c["version"], c["seconds"], pm(c["busy"]), pm(c["busy_max"]), c["rendered"],
            c["published"], c["presents"], c["dropped"], c["present_max_us"], c["late"],
            c["underruns"], c["music_late"], c["opl_dropped"], c["keys"], c["ring_full"],
            c["unknown"], c["stray"], c["i2c_errors"], c["log_dropped"], c["core0_stack"],
            c["core1_stack"], c["heap"], c["tic"], c["leveltime"], c["gamestate"],
            c["temp_c"], battery(c),
            "from UF2 Loader" if c["loader"] else "not from UF2 Loader", c["image_kb"],
            "%d KB" % c["partition_kb"] if c["partition_kb"] else "none")
            + ("; frame hashes %d at 0x%08x" % (c["hashes"], c["hash_list"])
               if c["hash_list"] else ""))

path, minutes = sys.argv[1], float(sys.argv[2])
reads, problems = [], []
for n, line in enumerate(open(path), 1):
    line = line.strip()
    if not line:
        continue
    ms, c = parse(line)
    if c is None or c["magic"] != MAGIC:
        if reads:                      # before core 1 ran, no block yet
            problems.append("read %d found no counter block" % n)
        continue
    if c["layout"] != LAYOUT:
        problems.append("read %d is layout %d, not %d" % (n, c["layout"], LAYOUT))
        continue
    reads.append((ms, c))

if not reads:
    print("counters.sh: no reads")
    sys.exit(1)
if minutes == 0:                       # one read: show it
    print(show(reads[-1][1]))
    sys.exit(1 if problems else 0)

# From the first read in a level: the WAD list and the title are not play.
start = next((i for i, (_, c) in enumerate(reads)
              if c["gamestate"] == GS_LEVEL and c["leveltime"] > 0), None)
if start is None:
    print("counters.sh: FAILED: the game never ran (no read in a level)")
    sys.exit(1)
play = reads[start:]

loads = under_at_load = late_at_load = 0
for (ms0, a), (ms1, b) in zip(play, play[1:]):
    wall = (ms1 - ms0) / 1000.0
    up = b["seconds"] - a["seconds"]
    at = "%d-%d s" % (a["seconds"], b["seconds"])
    if up < 0:
        problems.append("a reboot between %d s and %d s of uptime" % (a["seconds"], b["seconds"]))
        continue
    if abs(up - wall) > max(3.0, wall * 0.05):
        problems.append("%d heartbeats in %.0f s at %s" % (up, wall, at))
    if b["published"] == a["published"]:
        problems.append("no frames published at %s" % at)
    if b["tic"] <= a["tic"]:
        problems.append("the game tic did not rise at %s" % at)
    load = (b["leveltime"] < a["leveltime"] or a["gamestate"] != GS_LEVEL
            or b["gamestate"] != GS_LEVEL)
    loads += 1 if b["leveltime"] < a["leveltime"] else 0
    if not load and b["leveltime"] <= a["leveltime"]:
        problems.append("the level tic did not rise in a level at %s" % at)
    u = b["underruns"] - a["underruns"]
    if u and load:
        under_at_load += u
    elif u:
        problems.append("%d slots underrun away from a level load at %s" % (u, at))
    m = b["music_late"] - a["music_late"]
    if m and load:
        late_at_load += m
    elif m:
        problems.append("%d OPL late halves away from a level load at %s" % (m, at))

first, last = play[0][1], play[-1][1]
span = (play[-1][0] - play[0][0]) / 60000.0
if span < minutes - 0.5:
    problems.append("%.1f minutes of play read, not %g" % (span, minutes))
# Keys taken in every minute of play.
t0 = play[0][0]
for m in range(int(span)):
    lo, hi = t0 + m * 60000, t0 + (m + 1) * 60000
    inside = [c for ms, c in play if lo <= ms <= hi]
    if len(inside) >= 2 and inside[-1]["keys"] == inside[0]["keys"]:
        problems.append("no keys taken in minute %d" % (m + 1))
# The failure counters must not rise in play; any count from before it
# (the start of the title music) is shown, not failed.
FAILURES = ("late", "i2c_errors", "ring_full", "opl_dropped", "log_dropped")
for k in FAILURES:
    if last[k] != first[k]:
        problems.append("%s %d in play" % (k.replace("_", " "), last[k] - first[k]))
before = ["%s %d" % (k.replace("_", " "), first[k]) for k in FAILURES + ("music_late",) if first[k]]
charging = sum(1 for _, c in play if c["battery"] < 0 or c["battery"] & 0x80)

print("first: " + show(first))
print("last:  " + show(last))
s = max(1, last["seconds"] - first["seconds"])
print("%d reads over %.1f minutes of play; frames drawn %.1f and presented %.1f a second; "
      "keys %d; level loads %d, at loads underrun slots %d and OPL late halves %d; "
      "core 0 stack max %d B; temp %d C at the end" % (
      len(play), span, (last["rendered"] - first["rendered"]) / s,
      (last["presents"] - first["presents"]) / s, last["keys"] - first["keys"],
      loads, under_at_load, late_at_load, last["core0_stack"], last["temp_c"]))
print("battery %s to %s; %s" % (battery(first), battery(last),
      "on battery throughout" if charging == 0 else
      "%d of %d reads charging or unknown" % (charging, len(play))))
if before:
    print("before play: " + ", ".join(before))
for p in problems[:20]:
    print("  FAIL: " + p)
print("counters.sh: %s" % ("FAILED" if problems else "passed"))
sys.exit(1 if problems else 0)
EOF
}

if [ "$mode" = check ]; then
    check "$log" "${SOAK_MIN:-30}"
    exit
fi

if [ ! -f "$image" ]; then
    echo "counters.sh: no image at $image" >&2
    exit 1
fi

# OpenOCD and the toolchain's nm, found as profile.sh finds them.
openocd="${OPENOCD:-}"
if [ -z "$openocd" ]; then
    if command -v openocd >/dev/null 2>&1; then
        openocd="$(command -v openocd)"
    else
        openocd="$(ls -d "$HOME"/.pico-sdk/openocd/*/openocd 2>/dev/null | sort -V | tail -n1 || true)"
    fi
fi
if [ -z "$openocd" ] || [ ! -x "$openocd" ]; then
    echo "counters.sh: OpenOCD not found; install the Raspberry Pi fork or set OPENOCD" >&2
    exit 1
fi
scripts=()
if [ -d "$(dirname "$openocd")/scripts" ]; then
    scripts=(-s "$(dirname "$openocd")/scripts")
fi
nm="$(command -v arm-none-eabi-nm || ls -d "$HOME"/.pico-sdk/toolchain/*/bin/arm-none-eabi-nm 2>/dev/null | sort -V | tail -n1 || true)"
if [ -z "$nm" ]; then
    echo "counters.sh: arm-none-eabi-nm not found" >&2
    exit 1
fi

addr="$("$nm" "$image" | awk '$3 == "picocalc_doom_counters" { print "0x" $1 }')"
if [ -z "$addr" ]; then
    echo "counters.sh: $image has no picocalc_doom_counters" >&2
    exit 1
fi
words=41                                   # 33 fields and the version's 8

mkdir -p "$root/out"

# The frame hash list: the block for its address and length, then the list.
swd_read() {   # swd_read ADDR WORDS FILE
    "$openocd" ${scripts[@]+"${scripts[@]}"} \
        -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
        -c "adapter speed ${SWD_KHZ:-5000}" \
        -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
        -c "init" \
        -c "set f [open {$3} w]; puts \$f [read_memory $1 32 $2]; close \$f" \
        -c "shutdown" 2>"$3.openocd" || {
        echo "counters.sh: OpenOCD failed; its output is in $3.openocd" >&2
        tail -n 5 "$3.openocd" >&2
        exit 1
    }
}
if [ "$mode" = hashes ]; then
    out="$root/out/counters-hashes.txt"
    swd_read "$addr" "$words" "$out.block"
    set -- $(cat "$out.block")
    [ "$#" -eq "$words" ] && [ "$(( $1 ))" -eq $(( 0x4d4f4f44 )) ] ||
        { echo "counters.sh: no counter block at $addr" >&2; exit 1; }
    shift 31; n=$(( $1 )); list=$(( $2 ))
    if [ "$list" -eq 0 ] || [ "$n" -eq 0 ]; then
        echo "counters.sh: no frame hashes (is -framehashes in Doom's arguments?)" >&2
        exit 1
    fi
    echo "counters.sh: $n frame hashes at $(printf 0x%08x "$list")" >&2
    swd_read "$list" $(( n * 2 )) "$out.raw"
    tr ' ' '\n' < "$out.raw" | grep . | paste - - |
        while read -r tic crc; do printf '%d %08x\n' "$(( tic ))" "$(( crc ))"; done > "$out"
    exec "$root/scripts/host_engine.sh" compare "$out"
fi

if [ "$mode" = watch ]; then
    log="$root/out/counters-$(date +%Y%m%d-%H%M%S).log"
    reads=$(( minutes * 60 / every + 1 ))
    echo "counters.sh: $reads reads, every ${every}s, at $addr -> $log" >&2
else
    log="$root/out/counters-last.log"
    reads=1
fi
: > "$log"

# Each read is a line: the wall clock in ms, then the words.
"$openocd" ${scripts[@]+"${scripts[@]}"} \
    -f interface/cmsis-dap.cfg \
    -f target/rp2350.cfg \
    -c "adapter speed ${SWD_KHZ:-5000}" \
    -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
    -c "init" \
    -c "for {set i 0} {\$i < $reads} {incr i} {
            if {\$i > 0} { sleep [expr {$every * 1000}] }
            set t [clock milliseconds]
            set a [read_memory $addr 32 $words]
            for {set k 0} {\$k < 5} {incr k} {
                set b [read_memory $addr 32 $words]
                if {\$a eq \$b} break
                set a \$b
            }
            set f [open {$log} a]
            puts \$f \"\$t \$a\"
            close \$f
        }" \
    -c "shutdown" 2>"$log.openocd" || {
    echo "counters.sh: OpenOCD failed; its output is in $log.openocd" >&2
    tail -n 5 "$log.openocd" >&2
    exit 1
}

check "$log" "$minutes"
