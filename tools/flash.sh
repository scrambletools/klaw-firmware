#!/usr/bin/env bash
# Flash a KLAW half over its Caterina bootloader.
#
#   tools/flash.sh [hex] [timeout_seconds]
#
# If a Pro Micro serial port is present (Arduino sketch or a firmware with a
# CDC port), it is reset into the bootloader with the 1200 baud touch. Otherwise
# the script waits for you to press the reset button on the PCB (or to plug the
# half in while holding its top pinky key for Bootmagic).
set -euo pipefail

HEX=${1:-$(dirname "$0")/../klaw_vial.hex}
TIMEOUT=${2:-600}
AVRDUDE_CONF=${AVRDUDE_CONF:-$HOME/.local/opt/avrdude/avrdude_Linux_64bit/etc/avrdude.conf}

[ -f "$HEX" ] || { echo "hex not found: $HEX" >&2; exit 1; }

port_with_pid() {
    for port in /dev/ttyACM* /dev/ttyUSB*; do
        [ -e "$port" ] || continue
        if udevadm info -q property "$port" 2>/dev/null | grep -q "ID_MODEL_ID=$1"; then
            echo "$port"
            return 0
        fi
    done
    return 1
}

if sketch=$(port_with_pid 8036); then
    echo "touching $sketch at 1200 baud to enter the bootloader"
    stty -F "$sketch" 1200 hupcl
else
    echo "no serial port found, press the reset button on the KLAW now"
fi

echo "waiting up to ${TIMEOUT}s for the Caterina bootloader"
deadline=$(( $(date +%s) + TIMEOUT ))
until boot=$(port_with_pid 0036); do
    [ "$(date +%s)" -lt "$deadline" ] || { echo "timed out" >&2; exit 1; }
    sleep 0.2
done
sleep 0.3
echo "bootloader on $boot, flashing $(basename "$HEX")"
CONF_ARG=()
[ -f "$AVRDUDE_CONF" ] && CONF_ARG=(-C "$AVRDUDE_CONF")
avrdude "${CONF_ARG[@]}" -p m32u4 -c avr109 -P "$boot" -b 57600 -D -U flash:w:"$HEX":i
