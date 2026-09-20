"""Minimal Vial / VIA raw HID client used by the other tools."""
import glob, lzma, json, os, select, struct

class Vial:
    def __init__(self, name):
        for h in glob.glob("/sys/class/hidraw/hidraw*"):
            if name in open(h + "/device/uevent").read() and b"\x06\x60\xff" in open(h + "/device/report_descriptor", "rb").read():
                self.dev = "/dev/" + os.path.basename(h)
                self.fd = os.open(self.dev, os.O_RDWR)
                return
        raise SystemExit(f"no Vial keyboard named {name!r} connected")

    def cmd(self, *b):
        os.write(self.fd, b"\x00" + bytes(b) + bytes(32 - len(b)))
        ready, _, _ = select.select([self.fd], [], [], 1.0)
        if not ready:
            raise SystemExit(f"no reply to {b}")
        return os.read(self.fd, 32)

    def vial(self, *b):
        return self.cmd(0xFE, *b)

    def definition(self):
        size = struct.unpack("<I", self.vial(0x01)[:4])[0]
        blob = b"".join(self.vial(0x02, p & 0xFF, p >> 8)[:32] for p in range((size + 31) // 32))
        return json.loads(lzma.decompress(blob[:size]))

    def unlocked(self):
        return bool(self.vial(0x05)[0])

    def dump(self):
        out = {"definition": self.definition()}
        r = self.vial(0x00); out["uid"] = r[4:12].hex()
        rows, cols = out["definition"]["matrix"]["rows"], out["definition"]["matrix"]["cols"]
        layers = out["layers"] = self.cmd(0x11)[1]
        out["keymap"] = [[[struct.unpack(">H", self.cmd(0x04, l, r, c)[4:6])[0] for c in range(cols)] for r in range(rows)] for l in range(layers)]
        out["encoders"] = [[list(struct.unpack(">HH", self.vial(0x03, l, i)[:4])) for i in range(2)] for l in range(layers)]
        n = self.vial(0x0D, 0x00)
        out["tap_dance"] = [list(struct.unpack("<5H", self.vial(0x0D, 0x01, i)[1:11])) for i in range(n[0])]
        out["combo"] = [list(struct.unpack("<5H", self.vial(0x0D, 0x03, i)[1:11])) for i in range(n[1])]
        out["key_override"] = [list(struct.unpack("<3H4B", self.vial(0x0D, 0x05, i)[1:11])) for i in range(n[2])]
        size = struct.unpack(">H", self.cmd(0x0D)[1:3])[0]
        out["macro_count"] = self.cmd(0x0C)[1]
        out["macro_buffer"] = b"".join(self.cmd(0x0E, o >> 8, o & 0xFF, min(28, size - o))[4:4 + min(28, size - o)] for o in range(0, size, 28)).hex()
        settings, last = {}, 0
        while True:
            ids = [q for q in struct.unpack("<16H", self.vial(0x09, last & 0xFF, last >> 8)[:32]) if q != 0xFFFF]
            if not ids:
                break
            for q in ids:
                settings[str(q)] = self.vial(0x0A, q & 0xFF, q >> 8)[1:29].hex()   # raw value, size is per setting
            last = ids[-1]
        out["qmk_settings"] = settings
        return out

    def restore(self, dump):
        """Write a dump of this same keyboard back. Macros need the unlock."""
        rows, cols = dump["definition"]["matrix"]["rows"], dump["definition"]["matrix"]["cols"]
        for l, layer in enumerate(dump["keymap"]):
            for r in range(rows):
                for c in range(cols):
                    self.set_keycode(l, r, c, layer[r][c])
            for idx, (ccw, cw) in enumerate(dump["encoders"][l]):
                self.set_encoder(l, idx, 0, ccw)
                self.set_encoder(l, idx, 1, cw)
        for op, key, fmt in ((0x02, "tap_dance", "<5H"), (0x04, "combo", "<5H"), (0x06, "key_override", "<3H4B")):
            for i, entry in enumerate(dump[key]):
                self.set_entry(op, i, struct.pack(fmt, *entry))
        skipped = [q for q, v in dump["qmk_settings"].items() if self.set_setting(int(q), bytes.fromhex(v)) != 0]
        if self.unlocked():
            self.set_macros(bytes.fromhex(dump["macro_buffer"]))
        return skipped

    # writers
    def set_keycode(self, layer, row, col, kc):
        self.cmd(0x05, layer, row, col, kc >> 8, kc & 0xFF)

    def set_encoder(self, layer, idx, direction, kc):
        self.vial(0x04, layer, idx, direction, kc >> 8, kc & 0xFF)

    def set_entry(self, op, idx, packed):
        return self.vial(0x0D, op, idx, *packed)[0]

    def set_setting(self, qsid, data):
        return self.vial(0x0B, qsid & 0xFF, qsid >> 8, *data)[0]

    def set_macros(self, buf):
        for o in range(0, len(buf), 28):
            chunk = buf[o:o + 28]
            self.cmd(0x0F, o >> 8, o & 0xFF, len(chunk), *chunk)
