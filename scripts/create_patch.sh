#!/usr/bin/env bash
# create_patch.sh — patches/frank-doom.patch from working/ (design.md §5.2).
#
#   scripts/create_patch.sh
#
# Every file under working/ is diffed against the same path under
# modules/frank-doom/, in sorted order, labelled a/<path> and b/<path>
# with no timestamps, so the same working/ always gives the same patch.
# Adding a file to the patch is copying it into working/. A file with no
# upstream counterpart is refused: new code goes in picocalc/ (§5.2,
# rule 3). So is a working/ with .rej files left in it.
#
# --upstream DIR, --working DIR and --patch FILE override the defaults.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
upstream="$root/modules/frank-doom"
working="$root/working"
patch_file="$root/patches/frank-doom.patch"
while [ $# -gt 0 ]; do
    case "$1" in
        --upstream) upstream="$2"; shift 2 ;;
        --working) working="$2"; shift 2 ;;
        --patch) patch_file="$2"; shift 2 ;;
        -h|--help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "create_patch.sh: unknown argument $1" >&2; exit 2 ;;
    esac
done

if [ ! -d "$working" ]; then
    echo "create_patch.sh: no $working; run apply_patch.sh --working first" >&2
    exit 1
fi
rejects="$(find "$working" -name '*.rej')"
if [ -n "$rejects" ]; then
    echo "create_patch.sh: unresolved rejects:" >&2
    echo "$rejects" >&2
    exit 1
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

count=0
while IFS= read -r f; do
    if [ ! -f "$upstream/$f" ]; then
        echo "create_patch.sh: working/$f is not an upstream file; new code goes in picocalc/" >&2
        exit 1
    fi
    rc=0
    diff -u --label "a/$f" --label "b/$f" "$upstream/$f" "$working/$f" >>"$tmp" || rc=$?
    case "$rc" in
        0) echo "create_patch.sh: $f is unchanged from upstream" >&2 ;;
        1) count=$((count + 1)) ;;
        *) echo "create_patch.sh: diff failed on $f" >&2; exit 1 ;;
    esac
done < <(cd "$working" && find . -type f ! -name .DS_Store ! -name '*.orig' | sed 's|^\./||' | LC_ALL=C sort)

mkdir -p "$(dirname "$patch_file")"
cat "$tmp" >"$patch_file"
echo "create_patch.sh: $count file(s) in $patch_file"
