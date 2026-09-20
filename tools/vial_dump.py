#!/usr/bin/env python3
"""Dump a connected Vial keyboard's configuration to JSON.

    tools/vial_dump.py <name substring> <out.json>

Reads the layout definition, every layer, encoders, tap dances, combos, key
overrides, macros and QMK settings over raw HID. The JSON is what
gen_keymap.py consumes.
"""
import json, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from vial_hid import Vial

kb = Vial(sys.argv[1])
d = kb.dump()
json.dump(d, open(sys.argv[2], "w"), indent=1)
print(f"{kb.dev}: {d['definition']['name']}, {d['layers']} layers, {sum(1 for t in d['tap_dance'] if any(t))} tap dances, "
      f"{sum(1 for c in d['combo'] if any(c))} combos, {d['macro_count']} macros, {len(d['qmk_settings'])} settings -> {sys.argv[2]}")
