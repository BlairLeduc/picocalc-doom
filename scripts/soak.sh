#!/usr/bin/env bash
# soak.sh — the soak of design.md §16 M9 (emulator-lessons.md §11.5):
# flash a development build, capture UART0 throughout, play a level with
# keys typed over the UART for the whole run, then check the log with
# scripts/soak_check.sh.
#
#   scripts/soak.sh                         # 30 minutes, build/pico's image
#   scripts/soak.sh 5                       # 5 minutes
#   scripts/soak.sh 30 path/to/image.elf out/m9/soak.log
#
# The card's picocalc.ini must go straight into a game (iwad = and an
# argument such as -skill that starts one; design.md §10.3). Once the
# first frame is published the script types iddqd and idkfa (so the
# player neither dies nor runs dry, and the level stays the one loaded),
# then walks, turns, strafes, fires and uses, round and round, until the
# time is up. Switch the PicoCalc on, on battery if that is what is to be
# recorded, and keep the Mac's display awake (caffeinate -d).
set -euo pipefail

case "${1:-}" in
    -h|--help) sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

root="$(cd "$(dirname "$0")/.." && pwd)"
minutes="${1:-30}"
image="${2:-$root/build/pico/picocalc-doom.elf}"
log="${3:-$root/out/soak-$(date +%Y%m%d-%H%M%S).log}"
keys="$root/scripts/keys.sh"
secs=$((minutes * 60))

[ -f "$image" ] || { echo "soak.sh: no image at $image" >&2; exit 1; }
mkdir -p "$(dirname "$log")"

# The capture runs past the end of the keys: the boot before them, and a
# few heartbeats after.
"$root/scripts/capture.sh" $((secs + 120)) "$log" > /dev/null 2>&1 &
capture=$!
trap 'kill $capture 2>/dev/null || true' INT TERM
sleep 1
# A capture that could not open the probe's port (another still holding
# it) ends at once and leaves no log.
if ! kill -0 $capture 2>/dev/null; then
    echo "soak.sh: the capture did not start; is the probe's port in use?" >&2
    exit 1
fi
"$root/scripts/flash.sh" "$image" > /dev/null 2>&1 ||
    { echo "soak.sh: flash.sh failed" >&2; kill $capture; exit 1; }

# The game is running once a heartbeat counts a frame published.
for _ in $(seq 1 90); do
    if LC_ALL=C grep -aq 'published [1-9]' "$log" 2>/dev/null; then break; fi
    sleep 1
done
if ! LC_ALL=C grep -aq 'published [1-9]' "$log"; then
    echo "soak.sh: no frame published in 90 s; see $log" >&2
    kill $capture 2>/dev/null || true
    exit 1
fi
sleep 3
KEYS_GAP=0.15 "$keys" text:iddqd wait:1 text:idkfa > /dev/null
echo "soak.sh: playing for $minutes minutes; the log is $log"

# One round is about 12 s of play: forward, turns both ways, strafes,
# firing with each of three weapons, a door or switch, and back. The
# rounds go to one keys.sh, which opens the port once: opening it again
# for each round lost bytes of the capture (M9's trial soak).
round=(hold:up 1.5 hold:left 0.8 hold:ctrl 1 2 hold:up 1
       hold:right 1.2 hold:ctrl 0.8 space hold:, 0.6 hold:. 0.6
       5 hold:ctrl 0.6 hold:down 1 hold:right 0.5 3 hold:ctrl 0.5)
plan=()
for _ in $(seq 1 $((secs / 12 + 1))); do plan+=("${round[@]}"); done
"$keys" "${plan[@]}" > /dev/null

wait $capture || true
"$root/scripts/soak_check.sh" "$log" "$minutes"
