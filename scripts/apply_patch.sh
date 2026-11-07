#!/usr/bin/env bash
# apply_patch.sh — upstream with patches/frank-doom.patch applied
# (design.md §5.2).
#
#   scripts/apply_patch.sh             # frank-doom/: all of upstream, patched
#   scripts/apply_patch.sh --working   # working/: only the files the patch
#                                      # edits, patched, ready to edit
#
# The first is the build's copy; CMake runs it at configure time when the
# patch or the submodule's commit has changed. The second is where upstream
# files are edited: after a submodule bump, run it, fix any rejects (left
# beside their file as .rej), then run create_patch.sh.
#
# --working refuses a working/ that already has files in it, since those
# are edits not yet in the patch; --force discards them. --upstream DIR,
# --out DIR and --patch FILE override the defaults. Exits 1 if a hunk is
# rejected.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
upstream="$root/modules/frank-doom"
patch_file="$root/patches/frank-doom.patch"
out=
working=0
force=0
while [ $# -gt 0 ]; do
    case "$1" in
        --working) working=1; shift ;;
        --force) force=1; shift ;;
        --upstream) upstream="$2"; shift 2 ;;
        --out) out="$2"; shift 2 ;;
        --patch) patch_file="$2"; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "apply_patch.sh: unknown argument $1" >&2; exit 2 ;;
    esac
done
if [ -z "$out" ]; then
    if [ "$working" = 1 ]; then out="$root/working"; else out="$root/frank-doom"; fi
fi

if [ -z "$(ls -A "$upstream" 2>/dev/null)" ]; then
    echo "apply_patch.sh: no upstream at $upstream — run git submodule update --init" >&2
    exit 1
fi
if [ ! -f "$patch_file" ]; then
    echo "apply_patch.sh: no patch at $patch_file" >&2
    exit 1
fi
# Absolute, since patch -d changes directory before it opens -i's file.
patch_file="$(cd "$(dirname "$patch_file")" && pwd)/$(basename "$patch_file")"

# The files the patch edits, from its +++ lines (create_patch.sh writes
# them without timestamps).
files=()
while IFS= read -r f; do
    files+=("$f")
done < <(sed -n 's|^+++ b/||p' "$patch_file")

if [ "$working" = 1 ]; then
    if [ -n "$(find "$out" -type f ! -name .DS_Store 2>/dev/null | head -n1)" ] && [ "$force" = 0 ]; then
        echo "apply_patch.sh: $out has files in it; run create_patch.sh first, or pass --force to discard them" >&2
        exit 1
    fi
    rm -rf "$out"
    mkdir -p "$out"
    for f in ${files[@]+"${files[@]}"}; do
        mkdir -p "$out/$(dirname "$f")"
        cp -p "$upstream/$f" "$out/$f"
    done
else
    rm -rf "$out"
    mkdir -p "$out"
    cp -R "$upstream/." "$out/"
    rm -rf "$out/.git"
fi

if [ ${#files[@]} -eq 0 ]; then
    echo "apply_patch.sh: the patch edits no files"
    exit 0
fi

status=0
patch -p1 -s -d "$out" -i "$patch_file" || status=$?
# A patch applied with an offset may leave a backup; nothing here wants one.
for f in "${files[@]}"; do
    rm -f "$out/$f.orig"
done
if [ "$status" != 0 ]; then
    echo "apply_patch.sh: hunks rejected; see the .rej files under $out" >&2
    exit 1
fi
echo "apply_patch.sh: ${#files[@]} file(s) patched into $out"
