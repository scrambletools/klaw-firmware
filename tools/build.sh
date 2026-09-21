#!/usr/bin/env bash
# Build the left and right KB2040 Vial images.
#
#   tools/build.sh
#
# If a KLAW is connected, its Vial configuration is dumped first so the
# compiled defaults match what is on the keyboard. A firmware built on a
# later day than the one flashed wipes the Vial storage on first boot, and
# the firmware then seeds it from these defaults; dumping first is what
# keeps edits made in Vial across that.
set -euo pipefail
cd "$(dirname "$0")/.."
VIAL_QMK=${VIAL_QMK:-$HOME/Development/vial-qmk}
TOOLCHAIN=${TOOLCHAIN:-$HOME/.config/data/qmk/bin}
CONVERTER=${CONVERTER:-kb2040}

if python3 tools/vial_dump.py KLAW reference/klaw_vial.json 2>/dev/null; then
    echo "dumped the connected KLAW into reference/klaw_vial.json"
else
    echo "no KLAW connected, building from the existing reference/klaw_vial.json"
fi
python3 tools/gen_keymap.py

for side in left right; do
    log=$(mktemp)
    if ! ( cd "$VIAL_QMK" && PATH=$PATH:$TOOLCHAIN QMK_HOME=$PWD QMK_USERSPACE=$OLDPWD \
        CONVERT_TO=$CONVERTER EXTRAFLAGS="-DINIT_EE_HANDS_${side^^}" make klaw:vial -j"$(nproc)" >"$log" 2>&1 ); then
        cat "$log"; rm -f "$log"; exit 1
    fi
    rm -f "$log"
    cp "$VIAL_QMK/.build/klaw_vial_${CONVERTER}.uf2" "klaw_vial_${CONVERTER}_${side}.uf2"
    echo "built klaw_vial_${CONVERTER}_${side}.uf2"
done
rm -f "klaw_vial_${CONVERTER}.uf2" "$VIAL_QMK/klaw_vial_${CONVERTER}.uf2"
