#!/usr/bin/env bash
# The host engine still draws doom.wad's demo1 as the committed frame-hash
# list says (design.md §15): a change to the patch that changes the
# picture shows here, and the list is remade on purpose with
# scripts/host_engine.sh hashes test/host/engine/doom-demo1.crc.
# Skips (77) without the WAD, which is never in the repository.
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
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"$doom" -iwad "$wad" -timedemo demo1 -hosthashes "$tmp/demo1.crc" > "$tmp/log" 2>&1 || {
    tail -n 5 "$tmp/log"
    exit 1
}
"$root/scripts/host_engine.sh" compare "$tmp/demo1.crc" "$root/test/host/engine/doom-demo1.crc"
