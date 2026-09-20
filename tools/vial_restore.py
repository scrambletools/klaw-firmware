#!/usr/bin/env python3
"""Write a Vial dump back into the same keyboard.

    tools/vial_restore.py <name substring> <dump.json>

Restores layers, encoders, tap dances, combos, key overrides and QMK
settings over raw HID; macros too when the keyboard is unlocked. Use it after
a Vial storage reset, or to bring the second half in line with the first.
"""
import json, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from vial_hid import Vial

kb = Vial(sys.argv[1])
dump = json.load(open(sys.argv[2]))
if kb.definition()["matrix"] != dump["definition"]["matrix"]:
    raise SystemExit("dump is for a keyboard with a different matrix")
skipped = kb.restore(dump)
print(f"restored {dump['layers']} layers, encoders, entries and settings to {kb.dev}" + (f" (settings not in this firmware: {skipped})" if skipped else ""))
print("macros restored" if kb.unlocked() else "keyboard is locked: macros not written, unlock in Vial and run again")
