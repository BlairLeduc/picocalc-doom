#!/usr/bin/env bash
# The host engine still draws doom.wad's demo1 as the committed frame-hash
# list says (design.md §15): a change to the patch that changes the
# picture shows here, and the list is remade on purpose with
# scripts/host_engine.sh hashes test/host/engine/doom-demo1.crc.
# Skips (77) without the WAD, which is never in the repository, or when
# reference/doom.wad is not the WAD the list was made from (its MD5 is in
# the list's header): another doom.wad draws other pictures, and every
# hash would differ for no fault of the engine.
#
#   test_engine_demo1.sh DOOM_HOST ROOT
set -uo pipefail
doom="$1"
root="$2"
wad="${WADS:-$root/reference}/doom.wad"
if [ ! -f "$wad" ]; then
    echo "test_engine_demo1: no $wad; skipped"
    exit 77
fi
list="$root/test/host/engine/doom-demo1.crc"
want="$(sed -n 's/^# doom.wad \([0-9a-f]\{32\}\).*/\1/p' "$list")"
have="$( (md5 -q "$wad" 2>/dev/null || md5sum "$wad") | cut -c1-32)"
if [ "$have" != "$want" ]; then
    echo "test_engine_demo1: $wad is $have; the list was made from $want; skipped"
    exit 77
fi
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"$doom" -iwad "$wad" -timedemo demo1 -hosthashes "$tmp/demo1.crc" > "$tmp/log" 2>&1 || {
    tail -n 5 "$tmp/log"
    exit 1
}
"$root/scripts/host_engine.sh" compare "$tmp/demo1.crc" "$list"
