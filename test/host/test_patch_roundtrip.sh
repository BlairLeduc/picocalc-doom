#!/usr/bin/env bash
# test_patch_roundtrip.sh — create_patch.sh then apply_patch.sh reproduces
# working/ exactly (design.md §15).
#
#   test_patch_roundtrip.sh synthetic   # a made-up upstream, every case
#   test_patch_roundtrip.sh repo        # the real patch against the
#                                       # submodule; 77 (skip) without it
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
apply="$root/scripts/apply_patch.sh"
create="$root/scripts/create_patch.sh"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

synthetic() {
    local up="$tmp/up" w="$tmp/working"
    mkdir -p "$up/src/engine" "$up/drivers" "$w/src/engine" "$w/drivers"
    printf 'int a;\nint b;\nint c;\n' >"$up/src/main.c"
    printf '#define X 1\n#define Y 2\n' >"$up/src/engine/config.h"
    printf 'void f(void) {}\n' >"$up/drivers/no-newline.c"
    printf 'untouched\n' >"$up/README.md"
    printf 'gitdir: ../.git/modules/x\n' >"$up/.git"

    # Edits at the start, the middle and the end, and one at a missing
    # final newline.
    printf '/* ours */\nint a;\nint b2;\nint c;\nint d;\n' >"$w/src/main.c"
    printf '#define X 1\n#define Y 3\n' >"$w/src/engine/config.h"
    printf 'void f(void) { }' >"$w/drivers/no-newline.c"

    "$create" --upstream "$up" --working "$w" --patch "$tmp/p1.patch" >/dev/null
    [ "$(grep -c '^+++ b/' "$tmp/p1.patch")" = 3 ] || fail "the patch should edit 3 files"
    grep -q '^--- a/drivers/no-newline.c$' "$tmp/p1.patch" || fail "labels are not a/<path> without timestamps"
    sed -n '1p' "$tmp/p1.patch" | grep -q '^--- a/drivers/' || fail "files are not in sorted order"

    # The round trip: apply into an empty working/, which must match.
    "$apply" --working --upstream "$up" --out "$tmp/w2" --patch "$tmp/p1.patch" >/dev/null
    diff -r "$w" "$tmp/w2" || fail "apply --working did not reproduce working/"

    # And the patch from that copy is the same patch, byte for byte.
    "$create" --upstream "$up" --working "$tmp/w2" --patch "$tmp/p2.patch" >/dev/null
    cmp -s "$tmp/p1.patch" "$tmp/p2.patch" || fail "the patch changed on a second round trip"

    # A relative --patch, as typed by hand: patch -d must not lose it.
    (cd "$tmp" && "$apply" --working --upstream up --out w-rel --patch p1.patch >/dev/null) ||
        fail "apply_patch.sh failed with a relative --patch"
    diff -r "$w" "$tmp/w-rel" || fail "a relative --patch did not reproduce working/"

    # The build's copy: all of upstream, the edits applied, no .git.
    "$apply" --upstream "$up" --out "$tmp/full" --patch "$tmp/p1.patch" >/dev/null
    [ ! -e "$tmp/full/.git" ] || fail "the build's copy kept upstream's .git"
    cmp -s "$tmp/full/README.md" "$up/README.md" || fail "an unpatched file changed"
    for f in src/main.c src/engine/config.h drivers/no-newline.c; do
        cmp -s "$tmp/full/$f" "$w/$f" || fail "$f in the build's copy differs from working/"
    done

    # apply --working keeps unsaved edits unless forced.
    if "$apply" --working --upstream "$up" --out "$tmp/w2" --patch "$tmp/p1.patch" 2>/dev/null; then
        fail "apply --working overwrote a working/ with files in it"
    fi
    "$apply" --working --force --upstream "$up" --out "$tmp/w2" --patch "$tmp/p1.patch" >/dev/null

    # No new files in the patch (§5.2, rule 3).
    printf 'new\n' >"$tmp/w2/src/new.c"
    if "$create" --upstream "$up" --working "$tmp/w2" --patch "$tmp/p3.patch" 2>/dev/null; then
        fail "create_patch.sh accepted a file upstream does not have"
    fi
    rm "$tmp/w2/src/new.c"

    # Unresolved rejects stop it.
    : >"$tmp/w2/src/main.c.rej"
    if "$create" --upstream "$up" --working "$tmp/w2" --patch "$tmp/p3.patch" 2>/dev/null; then
        fail "create_patch.sh ignored a .rej file"
    fi
    rm "$tmp/w2/src/main.c.rej"

    # A hunk that no longer applies is reported, and leaves a .rej.
    printf 'int a;\nint z;\nint c;\n' >"$up/src/main.c"
    if "$apply" --working --force --upstream "$up" --out "$tmp/w3" --patch "$tmp/p1.patch" 2>/dev/null; then
        fail "apply_patch.sh succeeded with a rejected hunk"
    fi
    [ -f "$tmp/w3/src/main.c.rej" ] || fail "no .rej beside the rejected file"
}

repo() {
    local up="$root/modules/frank-doom" patch="$root/patches/frank-doom.patch"
    if [ -z "$(ls -A "$up" 2>/dev/null)" ] || [ ! -f "$patch" ]; then
        echo "skip: no submodule or no patch"
        exit 77
    fi
    # The files, not the patch's text: GNU and BSD diff choose different
    # but equivalent hunks for the same edit.
    "$apply" --working --out "$tmp/w" --patch "$patch" >/dev/null
    "$create" --working "$tmp/w" --patch "$tmp/p.patch" >/dev/null
    "$apply" --working --out "$tmp/w2" --patch "$tmp/p.patch" >/dev/null
    diff -r "$tmp/w" "$tmp/w2" || fail "the regenerated patch does not reproduce working/"
}

case "${1:-}" in
    synthetic) synthetic ;;
    repo) repo ;;
    *) echo "usage: $0 synthetic|repo" >&2; exit 2 ;;
esac
echo ok
