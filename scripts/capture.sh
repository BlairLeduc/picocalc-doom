#!/usr/bin/env bash
# capture.sh — capture UART0 (GP2 TX, GP3 RX) through the Debug Probe to
# a timestamped log (design.md §14.1, hardware-notes.md §2.7).
#
#   scripts/capture.sh                # until Ctrl-C, to out/uart-<time>.log
#   scripts/capture.sh 70             # 70 s, then stop
#   scripts/capture.sh 70 out/m1.log  # to a named file
#   UART_DEV=/dev/cu.usbmodem1234 scripts/capture.sh
#
# 115200 8-N-1. Start it before scripts/flash.sh so the banner is in the
# log. The port is discovered, not hard-coded: the single
# /dev/cu.usbmodem* (macOS) or /dev/ttyACM* (Linux) present, or UART_DEV.
# The descriptor is held open while the baud rate is set and while
# reading, so the adapter cannot revert to its defaults in between. Keep
# the Mac's display awake while the probe is in use (caffeinate -d): a
# sleeping display wedges it.
set -euo pipefail

case "${1:-}" in
    -h|--help) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

root="$(cd "$(dirname "$0")/.." && pwd)"
secs="${1:-0}"
out="${2:-$root/out/uart-$(date +%Y%m%d-%H%M%S).log}"

dev="${UART_DEV:-}"
if [ -z "$dev" ]; then
    shopt -s nullglob
    cands=(/dev/cu.usbmodem* /dev/ttyACM*)
    if [ ${#cands[@]} -ne 1 ]; then
        echo "capture.sh: found ${#cands[@]} candidate ports (${cands[*]:-none}); set UART_DEV" >&2
        exit 1
    fi
    dev="${cands[0]}"
fi

# Two readers on one port split the byte stream between them and both
# logs come out scrambled, so refuse rather than race a leftover capture.
if command -v lsof >/dev/null 2>&1 && lsof "$dev" >/dev/null 2>&1; then
    echo "capture.sh: $dev is already open:" >&2
    lsof "$dev" >&2
    exit 1
fi

mkdir -p "$(dirname "$out")"
exec 3<"$dev"
if [ "$(uname)" = Darwin ]; then
    stty -f "$dev" 115200 raw -echo cs8 -cstopb -parenb clocal
else
    stty -F "$dev" 115200 raw -echo cs8 -cstopb -parenb clocal
fi

cat <&3 >"$out" &
reader=$!
sleeper=
trap 'kill "$reader" $sleeper 2>/dev/null || true; wait "$reader" 2>/dev/null || true; exec 3<&-' EXIT INT TERM

echo "capture.sh: $dev -> $out (${secs}s; 0 = until interrupted)" >&2
if [ "$secs" -gt 0 ]; then
    # In the background, so a signal runs the trap at once: bash holds a
    # trap until a foreground command ends, and the reader would keep the
    # port for the rest of the time (found in M1).
    # Without the port's descriptor, which it would otherwise inherit.
    sleep "$secs" 3<&- &
    sleeper=$!
    wait "$sleeper"
else
    wait "$reader"
fi
