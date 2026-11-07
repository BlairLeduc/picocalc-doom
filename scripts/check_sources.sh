#!/usr/bin/env bash
# check_sources.sh — does cmake/upstream-sources.cmake still match
# upstream's build? (design.md §5.4)
#
#   scripts/check_sources.sh
#
# Upstream's sources are the files its CMake files name (its CMakeLists.txt
# and the driver files it adds) plus its engine glob,
# src/doomgeneric/doomgeneric/*.c, less the files its filters drop. The
# filters are reproduced here, and the script first checks that upstream's
# CMakeLists.txt still has exactly those filter lines, so a change to them
# is reported rather than missed. Every upstream source must be either
# compiled here or in FD_EXCLUDED_SOURCES, nothing both, nothing here
# unknown to upstream, and every file must exist. Run it after a submodule
# bump; CI runs it on every push.
#
# Upstream applies its filters to absolute paths, so a checkout whose path
# contains "i_" would drop every engine file there; here they are applied to
# paths relative to upstream's root, which is what upstream means.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
upstream="${UPSTREAM:-$root/modules/frank-doom}"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if [ ! -f "$upstream/CMakeLists.txt" ]; then
    echo "check_sources.sh: no $upstream/CMakeLists.txt — run git submodule update --init" >&2
    exit 1
fi

status=0

# The glob and filters this script reproduces, as upstream writes them.
expected_filters='file(GLOB DOOMGENERIC_SOURCES src/doomgeneric/doomgeneric/*.c)
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*doomgeneric_.*\\.c$")
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*i_.*\\.c$")
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*w_file_stdc\\.c$")
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*m_misc\\.c$")'
sed -e 's/[[:space:]]*#.*//' "$upstream/CMakeLists.txt" |
    grep -E '^(file\(GLOB|list\(FILTER) DOOMGENERIC_SOURCES' >"$tmp/filters" || true
if [ "$(cat "$tmp/filters")" != "$expected_filters" ]; then
    echo "upstream's engine glob or filters changed; update this script and the lists:"
    diff <(printf '%s\n' "$expected_filters") "$tmp/filters" | sed 's/^/    /' || true
    status=1
fi

# The glob, filtered.
(cd "$upstream" && for f in src/doomgeneric/doomgeneric/*.c; do
    case "$f" in
        *doomgeneric_*.c|*i_*.c|*w_file_stdc.c|*m_misc.c) ;;
        *) echo "$f" ;;
    esac
done) >"$tmp/upstream.raw"

# Files named in upstream's CMake files. ${CMAKE_CURRENT_LIST_DIR} is the
# naming file's own directory; drivers/usbhid is left out because upstream
# builds it only with USB_HID_ENABLED, which is off by default.
for f in CMakeLists.txt src/fatfs/CMakeLists.txt drivers/sdcard/sdcard.cmake \
         drivers/ps2kbd/CMakeLists.txt drivers/ps2mouse/CMakeLists.txt; do
    dir="$(dirname "$f")"
    sed -e 's/#.*//' -e "s|\${CMAKE_CURRENT_LIST_DIR}/|$dir/|g" "$upstream/$f" |
        grep -oE '[A-Za-z0-9_./*-]+\.(c|cpp|S)\b' |
        grep -v '\*' |
        sed 's|^\./||'
done >>"$tmp/upstream.raw"
LC_ALL=C sort -u "$tmp/upstream.raw" >"$tmp/upstream"

# Ours, read by CMake itself so that comments and quoting are CMake's.
cat >"$tmp/print.cmake" <<'EOF'
include(${LISTS})
set(all)
foreach(list ${FD_ALL_SOURCE_LISTS})
    list(APPEND all ${${list}})
endforeach()
string(REPLACE ";" "\n" all "${all}")
string(REPLACE ";" "\n" excluded "${FD_EXCLUDED_SOURCES}")
string(REPLACE ";" "\n" added "${FD_ADDED_SOURCES}")
file(WRITE ${OUT}/compiled "${all}\n")
file(WRITE ${OUT}/excluded "${excluded}\n")
file(WRITE ${OUT}/added "${added}\n")
EOF
cmake -DLISTS="$root/cmake/upstream-sources.cmake" -DOUT="$tmp" -P "$tmp/print.cmake"
for f in compiled excluded added; do
    grep -v '^$' "$tmp/$f" | LC_ALL=C sort -u >"$tmp/$f.s" || true
    mv "$tmp/$f.s" "$tmp/$f"
done
LC_ALL=C sort -u "$tmp/compiled" "$tmp/excluded" >"$tmp/ours"

report() {
    if [ -s "$2" ]; then
        echo "$1:"
        sed 's/^/    /' "$2"
        status=1
    fi
}
comm -23 "$tmp/upstream" "$tmp/ours" >"$tmp/missing"
comm -13 "$tmp/upstream" "$tmp/ours" | comm -23 - "$tmp/added" >"$tmp/unknown"
comm -12 "$tmp/compiled" "$tmp/excluded" >"$tmp/both"
while IFS= read -r f; do
    [ -f "$upstream/$f" ] || echo "$f"
done <"$tmp/ours" >"$tmp/absent"

report "upstream builds these, and they are neither compiled nor excluded" "$tmp/missing"
report "listed here, but upstream does not build them" "$tmp/unknown"
report "both compiled and excluded" "$tmp/both"
report "listed here, but not in the submodule" "$tmp/absent"

if [ "$status" = 0 ]; then
    echo "check_sources.sh: $(wc -l <"$tmp/compiled" | tr -d ' ') compiled, $(wc -l <"$tmp/excluded" | tr -d ' ') excluded, matching upstream $(git -C "$upstream" rev-parse --short HEAD 2>/dev/null || echo '(unknown commit)')"
fi
exit "$status"
