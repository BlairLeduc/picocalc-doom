#!/usr/bin/env bash
# timedemo.sh — read -timedemo runs from UART captures (design.md §14.3,
# §16 M5): for each boot, the arguments it was built with, Doom's result
# and its frames a second, and from the heartbeats while the demo ran,
# core 0's busy share, frames published a second, and core 0's copy and
# publish per frame. The gate's pass is 35 frames a second (§16 M5).
#
#   scripts/timedemo.sh out/m5/td1-A.log
#   scripts/timedemo.sh out/m5/td*.log
#
# Heartbeats before the first published frame (start-up, the card, the
# WAD directory) and after Doom's result are left out.
set -euo pipefail

case "${1:-}" in
    -h|--help|"") sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

for f in "$@"; do
    LC_ALL=C tr -d '\r' < "$f" | LC_ALL=C awk -v file="$f" '
    function finish() {
        if (args == "" && result == "") return
        printf "%s: %s\n", file, args
        printf "  %s\n", (result == "" ? "no result" : result)
        if (n > 0) {
            printf "  heartbeats %d: core 0 busy %.1f%% mean; published %.1f a second; copy %.2f ms, publish %.2f ms a frame\n",
                n, busy / n, pub / n, fin ? copy / fin / 1000 : 0, fin ? publ / fin / 1000 : 0
        }
        args = result = ""; n = busy = pub = copy = publ = fin = 0; running = 0
    }
    /^picocalc-doom [0-9a-f]/ { finish() }
    /^  arguments *:/ { sub(/^  arguments *: */, ""); args = $0 }
    /^I_Error: timed/ { sub(/^I_Error: /, ""); result = $0; running = 0 }
    /^hb [0-9]+:/ && result == "" {
        # hb N: core 0 busy B%, frames F, published P, ...; copy C us (max M), publish U us (max V), ...
        b = $0; sub(/.*busy /, "", b); sub(/%.*/, "", b)
        p = $0; sub(/.*published /, "", p); sub(/,.*/, "", p)
        fr = $0; sub(/.*frames /, "", fr); sub(/,.*/, "", fr)
        c = $0; sub(/.*copy /, "", c); sub(/ us.*/, "", c)
        u = $0; sub(/.*publish /, "", u); sub(/ us.*/, "", u)
        if (p + 0 > 0) running = 1
        if (!running) next
        n++; busy += b; pub += p; copy += c * fr; publ += u * fr; fin += fr
    }
    END { finish() }'
done
