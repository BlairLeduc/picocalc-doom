#!/usr/bin/env bash
# keys.sh — type at the PicoCalc over UART0, development build
# (design.md §14.1; picocalc/kbd.h has the bytes).
#
#   scripts/keys.sh down down enter          # named keys, in order
#   scripts/keys.sh text:idkfa              # characters as typed
#   scripts/keys.sh alt+left                 # a chord: modifiers held
#   scripts/keys.sh hold:up 2 wait:1 up      # Up held for 2 s; a pause
#   KEYS_GAP=0.2 scripts/keys.sh down down   # seconds between keys
#
# Names: up down left right enter esc tab bs del space home end pgup pgdn
# ins break caps f1..f10, and a single printable character. A chord joins
# ctrl, alt, shift and one key with +. hold:NAME then SECONDS sends the
# press, waits, and sends the release; the MCU's repeats are not sent.
# wait:SECONDS pauses. restart restarts the program, as a card change
# does.
#
# The port is found as capture.sh finds it, or UART_DEV. Run capture.sh
# alongside to see what the keys did: one reader and one writer share
# the port without trouble.
set -euo pipefail

case "${1:-}" in
    -h|--help|"") sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
esac

dev="${UART_DEV:-}"
if [ -z "$dev" ]; then
    shopt -s nullglob
    cands=(/dev/cu.usbmodem* /dev/ttyACM*)
    if [ ${#cands[@]} -ne 1 ]; then
        echo "keys.sh: found ${#cands[@]} candidate ports (${cands[*]:-none}); set UART_DEV" >&2
        exit 1
    fi
    dev="${cands[0]}"
fi
gap="${KEYS_GAP:-0.1}"

# Set the port up only when nothing holds it: capture.sh has set it and
# holds it open, and setting it again makes the Debug Probe set its UART
# up again, which loses bytes in flight (found in picocalc-quest: 4 heartbeats in a
# 30-minute soak).
# Without lsof there is no telling, and setting it up under a capture
# loses its bytes, so refuse.
if ! command -v lsof >/dev/null 2>&1; then
    echo "keys.sh: lsof is needed to tell whether capture.sh holds $dev" >&2
    exit 1
fi
if ! lsof "$dev" >/dev/null 2>&1; then
    if [ "$(uname)" = Darwin ]; then
        stty -f "$dev" 115200 raw -echo cs8 -cstopb -parenb clocal
    else
        stty -F "$dev" 115200 raw -echo cs8 -cstopb -parenb clocal
    fi
fi
exec 4>"$dev"

# The MCU's code for a key name (picocalc/keymap.h).
code() {
    case "$1" in
        up) echo b5 ;; down) echo b6 ;; left) echo b4 ;; right) echo b7 ;;
        enter) echo 0a ;; esc) echo b1 ;; tab) echo 09 ;; bs) echo 08 ;;
        del) echo d4 ;; space) echo 20 ;; home) echo d2 ;; end) echo d5 ;;
        pgup) echo d6 ;; pgdn) echo d7 ;; ins) echo d1 ;; break) echo d0 ;;
        ctrl) echo a5 ;; alt) echo a1 ;; shift) echo a2 ;; caps) echo c1 ;;
        f[1-9]) printf '%02x\n' $((0x80 + ${1#f})) ;;
        f10) echo 90 ;;
        ?) printf '%02x\n' "'$1" ;;
        *) echo "keys.sh: no key named '$1'" >&2; exit 2 ;;
    esac
}

# One FIFO event: 0x01, state, code.
event() { printf "\\x01\\x$1\\x$2" >&4; }

while [ $# -gt 0 ]; do
    arg="$1"; shift
    case "$arg" in
        text:*)
            s="${arg#text:}"
            for ((i = 0; i < ${#s}; i++)); do
                printf '%s' "${s:i:1}" >&4
                sleep "$gap"
            done
            continue ;;
        hold:*)
            c="$(code "${arg#hold:}")"
            event 01 "$c"; sleep "${1:?keys.sh: hold: needs seconds}"; shift
            event 03 "$c" ;;
        ?)
            c="$(code "$arg")"
            event 01 "$c"; sleep 0.03; event 03 "$c" ;;
        *+*)
            IFS=+ read -r -a parts <<<"$arg"
            codes=()
            for p in "${parts[@]}"; do codes+=("$(code "$p")"); done
            for c in "${codes[@]}"; do event 01 "$c"; sleep 0.03; done
            for ((i = ${#codes[@]} - 1; i >= 0; i--)); do event 03 "${codes[i]}"; sleep 0.03; done ;;
        restart)
            printf '\x02' >&4; continue ;;
        wait:*)
            sleep "${arg#wait:}"; continue ;;
        *)
            c="$(code "$arg")"
            event 01 "$c"; sleep 0.03; event 03 "$c" ;;
    esac
    sleep "$gap"
done
