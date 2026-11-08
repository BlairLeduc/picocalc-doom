#!/usr/bin/env bash
# Every screen size Doom's menu offers plays doom.wad's demo1 to its end
# with the zone intact (design.md §7.1): with a border (sizes 3-9) the
# view's surround is drawn into a 208-row buffer in the zone, and
# upstream's V_DrawPatch() moved any patch at y >= 168 down 40 rows for
# the 240-row status bar, so at sizes 8 and 9 the border's bottom edge was
# drawn past that buffer's end, over the next block (M9; the patch's
# v_video.c hunk). -hostcheckzone checks every block after each frame.
# Skips (77) without the WAD, which is never in the repository.
#
#   test_engine_screens.sh DOOM_HOST ROOT
set -uo pipefail
doom="$1"
root="$2"
wad="${WADS:-$root/reference}/doom.wad"
if [ ! -f "$wad" ]; then
    echo "test_engine_screens: no $wad; skipped"
    exit 77
fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
failed=0
for size in 3 4 5 6 7 8 9 10 11; do
    "$doom" -iwad "$wad" -timedemo demo1 -hostscreenblocks "$size" -hostcheckzone \
        > "$tmp/log" 2>&1
    status=$?
    if grep -aq '^timed [0-9]* gametics' "$tmp/log"; then
        echo "screen size $size: $(grep -a '^timed' "$tmp/log" | head -n 1)"
    else
        echo "screen size $size: FAILED (exit $status): $(tail -n 1 "$tmp/log")"
        failed=1
    fi
done
exit "$failed"
