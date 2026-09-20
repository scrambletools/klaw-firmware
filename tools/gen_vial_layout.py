#!/usr/bin/env python3
"""Generate the Vial keymap layout and the RGB LED positions from the KLAW PCB.

    tools/gen_vial_layout.py [path/to/klaw_2.kicad_pcb] [preview.svg]

Switch centres and footprint rotations are read from the PCB, the matrix
position of each switch from its pad nets (column net directly, row net via
the diode). The PCB is the right half; the left half is its mirror image.
The encoders are placed as two columns between the halves, each the same
distance from its half, with a wider gap between the two columns.
"""
import json, re, sys, pathlib

UNIT = 19.05          # key pitch in mm
HALF_GAP = 0.5        # units between a half's inner column and its encoder column
ENC_GAP = 1.0         # units between the two encoder columns
ENC_TOP = 1.0         # y of the encoder push switches
ENC = "\n\n\n\n\n\n\n\n\ne"  # Vial marker for an encoder entry

root = pathlib.Path(__file__).resolve().parent.parent
pcb = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else root.parent / "klaw/pcb/klaw_2/klaw_2.kicad_pcb")
kb = json.load(open(root / "keyboards/klaw/keyboard.json"))
vial_path = root / "keyboards/klaw/keymaps/vial/vial.json"

# column index by controller pin, row index by net name
col_pins = kb["matrix_pins"]["cols"]
pin_of_net = {}
footprints = {}
text = pcb.read_text()
starts = [m.start() for m in re.finditer(r"\n\t\(footprint ", text)] + [len(text)]
for a, b in zip(starts, starts[1:]):
    body = text[a:b]
    ref = re.search(r'\(property "Reference" "([^"]*)"', body).group(1)
    at = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", body)
    pads = []
    for pad in re.finditer(r'\(pad "([^"]*)"(.*?)\n\t\t\)', body, re.S):
        net = re.search(r'\(net "([^"]*)"\)', pad.group(2))
        func = re.search(r'\(pinfunction "([^"]*)"\)', pad.group(2))
        pads.append((func.group(1) if func else pad.group(1), net.group(1) if net else None))
    footprints[ref] = (float(at.group(1)), float(at.group(2)), float(at.group(3) or 0), pads)
    if ref == "U1":
        # pin functions look like "F7/A0_17"; keep the AVR port pin name
        for func, net in pads:
            avr = [tok for tok in re.split(r"[/_]", func) if re.fullmatch(r"[A-F][0-7]", tok)]
            if avr:
                pin_of_net[net] = avr[0]

def col_index(net):
    pin = pin_of_net[net]
    for i, p in enumerate(col_pins):
        if pin == p:
            return i
    raise SystemExit(f"column net {net} on pin {pin} is not in keyboard.json")

def row_index(nets):
    for net in nets:
        if net and net.startswith("row"):
            return int(net[3:])
        if net and net.startswith("Net-(D"):
            diode = re.match(r"Net-\(D(\d+)-", net).group(1)
            for _, dnet in footprints["D" + diode][3]:
                if dnet and dnet.startswith("row"):
                    return int(dnet[3:])
    raise SystemExit(f"no row net among {nets}")

keys = []  # (x, y, kicad rotation, row, col)
for ref, (x, y, rot, pads) in footprints.items():
    if not re.match(r"SW\d+$", ref) or "ENC" in " ".join(n or "" for _, n in pads):
        continue
    nets = [n for _, n in pads]
    colnet = next(n for n in nets if n in pin_of_net and pin_of_net[n] in col_pins)
    keys.append((x, y, rot, row_index(nets), col_index(colnet)))

xmin = min(k[0] for k in keys); xmax = max(k[0] for k in keys); ymin = min(k[1] for k in keys)
right = []   # right half in board units, x from the inner thumb key
left = []    # mirror image, x from the pinky key
for x, y, rot, row, col in keys:
    r = (180 - rot + 180) % 360 - 180          # KiCad is counter-clockwise, KLE clockwise
    uy = (y - ymin) / UNIT + 0.5
    right.append(((x - xmin) / UNIT, uy, r, f"{row + 4},{col}"))
    left.append(((xmax - x) / UNIT + 0.5, uy, -r, f"{row},{col}"))
# The unrotated keys nearest the middle are the inner column. Its edge sets
# the encoder column positions, so both halves get the same gap.
left_edge = max(cx + 0.5 for cx, _, r, _ in left if r == 0)
mute_cx = left_edge + HALF_GAP + 0.5
play_cx = mute_cx + 1 + ENC_GAP
right_edge = min(cx - 0.5 for cx, _, r, _ in right if r == 0)
shift = play_cx + 0.5 + HALF_GAP - right_edge
placed = left + [(cx + shift, cy, r, label) for cx, cy, r, label in right]
for cx, (push, cw, ccw) in ((mute_cx, ("3,4", "0,1" + ENC, "0,0" + ENC)), (play_cx, ("7,4", "1,1" + ENC, "1,0" + ENC))):
    for j, label in enumerate((push, cw, ccw)):
        placed.append((cx, ENC_TOP + j, 0, label))

def num(v):
    v = round(v, 3) + 0.0
    return int(v) if v == int(v) else v

rows = [[{"r": num(r), "rx": num(cx), "ry": num(cy), "x": -0.5, "y": -0.5}, label]
        for cx, cy, r, label in sorted(placed, key=lambda k: (round(k[1], 1), k[0]))]
vial = json.load(open(vial_path))
vial["layouts"]["keymap"] = rows
with open(vial_path, "w") as f:
    json.dump(vial, f, indent=4)
    f.write("\n")
print(f"{len(rows)} keys written to {vial_path.relative_to(root)}")

# LED positions for the RGB matrix effects, QMK's 224 x 64 grid over both
# halves. The chain order and flags in keyboard.json stay as they are.
centre = {label: (cx, cy) for cx, cy, _, label in placed if not label.endswith(ENC)}
xs = [c[0] for c in centre.values()]; ys = [c[1] for c in centre.values()]
def grid(cx, cy):
    return (round((cx - min(xs)) / (max(xs) - min(xs)) * 224), round((cy - min(ys)) / (max(ys) - min(ys)) * 64))
for led in kb["rgb_matrix"]["layout"]:
    led["x"], led["y"] = grid(*centre["{},{}".format(*led["matrix"])])
kb_path = root / "keyboards/klaw/keyboard.json"
with open(kb_path, "w") as f:
    json.dump(kb, f, indent=4)
    f.write("\n")
print(f"{len(kb['rgb_matrix']['layout'])} LED positions written to {kb_path.relative_to(root)}")

if len(sys.argv) > 2:
    scale = 60
    width = int((max(k[0] for k in placed) + 1.5) * scale); height = int((max(k[1] for k in placed) + 1.5) * scale)
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}"><rect width="100%" height="100%" fill="#eee"/>']
    for cx, cy, r, label in placed:
        name = label.split("\n")[0] + ("e" if label.endswith("e") else "")
        svg.append(f'<g transform="translate({cx*scale},{cy*scale}) rotate({r})"><rect x="{-0.45*scale}" y="{-0.45*scale}" '
                   f'width="{0.9*scale}" height="{0.9*scale}" rx="6" fill="#fff" stroke="#333"/>'
                   f'<text y="5" font-size="13" text-anchor="middle" font-family="sans-serif">{name}</text></g>')
    svg.append("</svg>")
    pathlib.Path(sys.argv[2]).write_text("\n".join(svg))
