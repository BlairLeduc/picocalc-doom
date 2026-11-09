#!/usr/bin/env bash
# soak_check.sh — check a soak's UART capture (design.md §16 M9,
# emulator-lessons.md §11.5). Prints one line per check and PASS or FAIL;
# exits 1 if any check fails.
#
#   scripts/soak_check.sh out/m9/soak.log          # a 30-minute soak
#   scripts/soak_check.sh out/m9/soak.log 5        # a 5-minute one
#
# The checks, from the banner on (the capture begins before the flash),
# and from the first heartbeat in play (frames published and the
# level clock running) to the last:
#   - one boot: one banner in the capture;
#   - heartbeats throughout: every second from the first to the last, at
#     least MINUTES of them;
#   - the failure counters zero in every heartbeat: late refills, I2C
#     errors, log lines dropped, keys lost to a full ring, OPL writes
#     dropped;
#   - underruns and OPL late halves only at level loads: none but within
#     10 s after a "level:" line or a change of game state (into the
#     intermission, say), where core 0 reads the card (M10);
#   - the game visibly advancing: frames published in every heartbeat, the
#     game tic rising in every one, and the level tic in every one in a
#     level (state 0), so the game was running, not only drawing;
#   - keys read: Doom took typed keys in every minute.
# A summary follows: frames drawn and presented a second, core 0's busy
# share, the temperature and battery, the heap and stacks.
set -euo pipefail

case "${1:-}" in
    -h|--help|"") sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac
log="$1"
minutes="${2:-30}"
[ -f "$log" ] || { echo "soak_check.sh: no $log" >&2; exit 2; }

LC_ALL=C tr -d '\r' < "$log" | LC_ALL=C awk -v minutes="$minutes" '
function field(line, name,    v) {
    v = line
    if (!sub(".*" name " ", "", v)) return ""
    sub(/[^0-9].*/, "", v)
    return v
}
function fail(what) { failures[++nfail] = what }
# The capture starts before the flash: what the program before printed
# comes first, and belongs to no part of this run.
/^picocalc-doom [0-9a-f]+/ { boots++ }
!boots { next }
/^level: / { nlevel++; level_at[nlevel] = last_hb + 1 }
/^hb [0-9]+: / {
    s = $2; sub(/:/, "", s); s += 0
    last_hb = s
    pub = field($0, "published"); tic = field($0, "game tic")
    lt = field($0, "level tic"); st = field($0, "state")
    if (!started) {
        if (pub + 0 > 0 && lt + 0 > 0) { started = 1; first = s; prev_s = s - 1 }
        else next
    }
    if (s != prev_s + 1) fail("heartbeat " prev_s " followed by " s)
    prev_s = s; n++
    if (pub + 0 == 0) fail("hb " s ": no frame published")
    if (n > 1 && tic + 0 <= prev_tic + 0) fail("hb " s ": game tic did not rise (" tic ")")
    if (n > 1 && st + 0 == 0 && prev_st + 0 == 0 && lt + 0 <= prev_lt + 0)
        fail("hb " s ": level tic did not rise in a level (" lt ")")
    if (field($0, "i2c errors") + 0 != 0) fail("hb " s ": i2c errors " field($0, "i2c errors"))
    if (field($0, "log dropped") + 0 != 0) fail("hb " s ": log dropped " field($0, "log dropped"))
    if (n > 1 && st + 0 != prev_st + 0) { nlevel++; level_at[nlevel] = s; nstate++ }
    prev_tic = tic; prev_lt = lt; prev_st = st
    frames += field($0, "frames"); presented += field($0, "presented")
    b = $0; sub(/.*busy /, "", b); sub(/%.*/, "", b); busy += b
    t = field($0, "temp"); if (tmin == "" || t + 0 < tmin) tmin = t + 0; if (t + 0 > tmax) tmax = t + 0
    bat = $0; sub(/.*battery /, "", bat); sub(/[%,].*/, "", bat); if (n == 1) bat0 = bat; bat1 = bat
    h = $0; sub(/.*heap /, "", h); sub(/ of.*/, "", h); if (h + 0 > heap) heap = h + 0
    hs = $0; sub(/.*heap [0-9]+ of /, "", hs); sub(/ B.*/, "", hs); heapsize = hs
    c0 = $0; sub(/.*core 0 /, "", c0); sub(/ of.*/, "", c0); if (c0 + 0 > st0) st0 = c0 + 0
    c1 = $0; sub(/.* B, core 1 /, "", c1); sub(/ of.*/, "", c1); if (c1 + 0 > st1) st1 = c1 + 0
}
started && /^hb [0-9]+ keys: / {
    s = $2 + 0
    if (field($0, "ring full") + 0 != 0) fail("hb " s ": keys lost, ring full " field($0, "ring full"))
    tk = field($0, "taken"); keys += tk
    m = int((s - first) / 60); if (tk + 0 > 0) keymin[m] = 1
}
started && /^hb [0-9]+ audio: / {
    s = $2 + 0
    if (field($0, "late") + 0 != 0) fail("hb " s ": late refills " field($0, "late"))
    u = field($0, "underrun") + 0
    if (u > 0) {
        ok = 0
        for (i = 1; i <= nlevel; i++) if (s >= level_at[i] && s <= level_at[i] + 10) ok = 1
        if (ok) under_load += u; else fail("hb " s ": " u " slots underrun away from a level load")
    }
}
started && /^hb [0-9]+ opl: / {
    s = $2 + 0
    # Cumulative since boot: judged by its rise, as the underruns are.
    lh = field($0, "late halves") + 0
    if (!opl_seen) { opl_seen = 1; prev_lh = lh; if (lh) late_before = lh }
    if (lh > prev_lh) {
        ok = 0
        for (i = 1; i <= nlevel; i++) if (s >= level_at[i] && s <= level_at[i] + 10) ok = 1
        if (ok) late_load += lh - prev_lh; else fail("hb " s ": " lh - prev_lh " OPL late halves away from a level load")
    }
    prev_lh = lh
    if (field($0, "dropped") + 0 != 0) fail("hb " s ": OPL writes dropped " field($0, "dropped"))
}
END {
    printf "boots: %d\n", boots
    if (boots != 1) fail("boots: " boots ", not 1")
    if (!started) { print "FAIL: the game never ran (no heartbeat with frames and the level clock)"; exit 1 }
    printf "heartbeats in play: %d (%d to %d)\n", n, first, prev_s
    if (n < minutes * 60) fail("only " n " heartbeats in play, not " minutes * 60)
    full = int(n / 60)
    for (m = 0; m < full; m++) if (!(m in keymin)) fail("minute " m + 1 ": no key taken")
    printf "keys taken: %d; level loads and changes of state: %d; at them, underrun slots %d, OPL late halves %d\n", keys, nlevel, under_load, late_load
    if (late_before) printf "OPL late halves before play: %d\n", late_before
    printf "drawn %.1f a second, presented %.1f; core 0 busy %.1f%%; temperature %d-%d C; battery %s%% to %s%%\n",
        frames / n, presented / n, busy / n, tmin, tmax, bat0, bat1
    printf "heap %d of %d B; stacks: core 0 %d B, core 1 %d B\n", heap, heapsize, st0, st1
    if (nfail) {
        for (i = 1; i <= nfail && i <= 20; i++) print "  " failures[i]
        if (nfail > 20) printf "  ... and %d more\n", nfail - 20
        printf "FAIL: %d failures\n", nfail
        exit 1
    }
    print "PASS"
}'
