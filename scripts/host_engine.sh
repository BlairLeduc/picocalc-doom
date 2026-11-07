#!/usr/bin/env bash
# host_engine.sh — run the host engine over the owner's WADs (design.md §15,
# M2). The WADs are never in the repository; they are read from
# reference/, or the directory named by WADS.
#
#   scripts/host_engine.sh demos            every demo of every WAD to hand,
#                                           under -timedemo, to its end
#   scripts/host_engine.sh maps [TICS]      warp to every map, run TICS tics
#                                           (2100, a minute) at skill 4, and
#                                           report the zone and the cache
#   scripts/host_engine.sh hashes [OUT]     the frame-hash list of doom.wad's
#                                           demo1, the device's reference
#   scripts/host_engine.sh compare FILE [REF]  a device's list against the
#                                           reference; FILE is a list, or a
#                                           UART capture from a build with
#                                           PICOCALC_DOOM_FRAME_HASHES=ON,
#                                           whose "fh TIC CRC" lines are read
#
# Results go to out/host-engine/ (OUT_DIR overrides): a log per run and a
# table per command. Builds build/host first if doom_host is missing.
#
# The hash list is made four times, with the zone filled with a different
# byte and at a different base each time, and every zone block followed by
# 128 bytes of that byte. A tic whose hash changes reads memory Doom never
# wrote, a zone block's header, or past a block's end (a sprite column whose
# first row maps to texture row -1 reads row 127, up to 127 bytes past its
# lump, as in vanilla Doom); it cannot match the device's, whose blocks have
# other neighbours and smaller headers, and is written "-" for compare to
# skip.
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
wads="${WADS:-$root/reference}"
out="${OUT_DIR:-$root/out/host-engine}"
build="${BUILD_DIR:-$root/build/host}"
doom="$build/test/host/engine/doom_host"
ref_default="$root/test/host/engine/doom-demo1.crc"

die() { echo "host_engine.sh: $*" >&2; exit 1; }

need_doom() {
    if [ ! -x "$doom" ]; then
        cmake -S "$root" -B "$build" -DPICOCALC_DOOM_HOST=ON -DCMAKE_BUILD_TYPE=Release >/dev/null &&
            cmake --build "$build" -j --target doom_host >/dev/null ||
            die "could not build doom_host"
    fi
}

# The WAD sets: a name, then Doom's arguments. Only those to hand are run.
wad_sets() {
    local iwad
    for iwad in doom doom2 plutonia tnt; do
        [ -f "$wads/$iwad.wad" ] && echo "$iwad -iwad $wads/$iwad.wad"
    done
    if [ -f "$wads/doom2.wad" ]; then
        [ -f "$wads/nerve.wad" ] &&
            echo "nerve -iwad $wads/doom2.wad -file $wads/nerve.wad"
        [ -f "$wads/masterlevels.wad" ] &&
            echo "masterlevels -iwad $wads/doom2.wad -file $wads/masterlevels.wad"
    fi
}

# One value from a log's "host: LINE key value ..." line: field LOG LINE KEY
field() {
    awk -v line="$2" -v key="$3" '
        $1 == "host:" && $2 == line {
            for (i = 3; i < NF; i++) if ($i == key) { print $(i + 1); exit }
        }' "$1"
}

cmd_demos() {
    need_doom
    mkdir -p "$out/demos"
    local table="$out/demos.txt" name args n log rc
    printf '%-13s %-5s %7s %-6s %6s %5s %5s %5s %8s %8s %10s %10s\n' \
        wad demo tics map health kills items fps peak_used low_free play_reads play_bytes > "$table"
    while read -r name args; do
        for n in 1 2 3 4; do
            log="$out/demos/$name-demo$n.log"
            # shellcheck disable=SC2086
            "$doom" $args -timedemo "demo$n" > "$log" 2>&1
            rc=$?
            if grep -qi "^host: error W_GetNumForName: demo$n not found" "$log"; then
                continue
            fi
            if [ "$rc" -ne 0 ] || ! grep -q "^host: end timedemo" "$log"; then
                printf '%-13s demo%-1s did not play to its end; see %s\n' "$name" "$n" "$log" | tee -a "$table"
                continue
            fi
            printf '%-13s demo%-1s %7s %-6s %6s %5s %5s %5.0f %8s %8s %10s %10s\n' "$name" "$n" \
                "$(field "$log" state gametic)" "$(field "$log" state map)" \
                "$(field "$log" player health)" "$(field "$log" player kills)" \
                "$(field "$log" player items)" \
                "$(sed -n 's/^host: timed .*(\([0-9.]*\) fps)/\1/p' "$log")" \
                "$(field "$log" zone peak_used)" "$(field "$log" zone low_free)" \
                "$(field "$log" reads play)" "$(field "$log" reads play_bytes)" >> "$table"
        done
    done < <(wad_sets)
    cat "$table"
}

cmd_maps() {
    need_doom
    local tics="${1:-2100}"
    mkdir -p "$out/maps"
    local table="$out/maps.txt" name args maps m log
    printf '%-13s %-6s %6s %8s %8s %9s %10s %10s\n' \
        wad map lumps dir_bytes peak_used low_free play_reads play_bytes > "$table"
    while read -r name args; do
        case "$name" in
            doom) maps="$(for e in 1 2 3 4; do for m in 1 2 3 4 5 6 7 8 9; do echo "$e $m"; done; done)" ;;
            nerve) maps="$(seq 1 9)" ;;
            masterlevels) maps="$(seq 1 21)" ;;
            *) maps="$(seq 1 32)" ;;
        esac
        while read -r m; do
            log="$out/maps/$name-$(echo "$m" | tr ' ' '_').log"
            # shellcheck disable=SC2086
            "$doom" $args -warp $m -skill 4 -hosttics "$tics" -hostclock > "$log" 2>&1
            if ! grep -q "^host: end tics" "$log"; then
                printf '%-13s %-6s failed; see %s\n' "$name" "$m" "$log" | tee -a "$table"
                continue
            fi
            printf '%-13s %-6s %6s %8s %8s %9s %10s %10s\n' "$name" \
                "$(field "$log" state map)" "$(field "$log" wad lumps)" \
                "$(field "$log" wad directory)" "$(field "$log" zone peak_used)" \
                "$(field "$log" zone low_free)" "$(field "$log" reads play)" \
                "$(field "$log" reads play_bytes)" >> "$table"
        done <<< "$maps"
    done < <(wad_sets)
    cat "$table"
}

cmd_hashes() {
    need_doom
    local dest="${1:-$out/doom-demo1.crc}" i fill offset
    [ -f "$wads/doom.wad" ] || die "no $wads/doom.wad"
    mkdir -p "$out/hashes"
    local fills=(0x00 0xff 0x55 0xaa) offsets=(0 16 4096 48)
    for i in 0 1 2 3; do
        fill=${fills[$i]}
        offset=${offsets[$i]}
        "$doom" -iwad "$wads/doom.wad" -timedemo demo1 \
            -hostzonefill "$fill" -hostzoneoffset "$offset" -hostzonepad 128 \
            -hosthashes "$out/hashes/run$i.crc" > "$out/hashes/run$i.log" 2>&1 ||
            die "demo1 failed; see $out/hashes/run$i.log"
    done
    local md5
    md5="$( (md5 -q "$wads/doom.wad" 2>/dev/null || md5sum "$wads/doom.wad") | cut -c1-32)"
    {
        echo "# Frame hashes of doom.wad's demo1 under -timedemo (design.md §15):"
        echo "# game tic, then the CRC-32 of the last frame drawn in it,"
        echo "# pixels then palette (picocalc/frame_hash.h). \"-\": the frame read"
        echo "# memory Doom never wrote, or past a zone block's end, and differs"
        echo "# from run to run."
        echo "# doom.wad $md5, made by scripts/host_engine.sh hashes."
        paste "$out/hashes/run0.crc" "$out/hashes/run1.crc" \
              "$out/hashes/run2.crc" "$out/hashes/run3.crc" |
            awk '{ same = ($2 == $4 && $2 == $6 && $2 == $8) &&
                          ($1 == $3 && $1 == $5 && $1 == $7);
                   print $1, (same ? $2 : "-") }'
    } > "$dest"
    echo "$dest: $(grep -vc '^#' "$dest") tics, $(grep -c ' -$' "$dest") unstable"
}

cmd_compare() {
    local file="${1:-}" ref="${2:-$ref_default}"
    [ -n "$file" ] || die "compare needs the device's list"
    LC_ALL=C tr -d '\r' < "$file" | awk -v ref="$ref" '
        BEGIN {
            while ((getline line < ref) > 0) {
                if (line ~ /^#/) continue
                split(line, f, " "); want[f[1]] = f[2]; n++
            }
        }
        /^#/ { next }
        $1 == "fh" { $1 = $2; $2 = $3 }
        $1 !~ /^[0-9]+$/ || $2 !~ /^([0-9a-f]+|-)$/ { next }
        { seen[$1] = 1
          if (!($1 in want)) { extra++; next }
          if (want[$1] == "-") { skipped++; next }
          if (want[$1] == $2) same++
          else { if (diff < 10) print "tic " $1 ": device " $2 ", host " want[$1]; diff++ } }
        END {
            for (t in want) if (!(t in seen)) missing++
            printf "same %d, different %d, skipped %d, missing %d, extra %d (of %d)\n",
                same, diff, skipped, missing, extra, n
            exit (diff || missing) ? 1 : 0
        }'
}

case "${1:-}" in
    demos) shift; cmd_demos "$@" ;;
    maps) shift; cmd_maps "$@" ;;
    hashes) shift; cmd_hashes "$@" ;;
    compare) shift; cmd_compare "$@" ;;
    *) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
