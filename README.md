# klaw-firmware

QMK firmware for the [KLAW](https://github.com/scrambletools/klaw) reversible 34-key split keyboard, a
derivative of GEIGEIGEIST's KLOR.

The keyboard definition lives in `keyboards/klaw/` and is built against a
separate `qmk_firmware` checkout. QMK's external userspace only overlays
keymaps, not keyboard definitions, so the keyboard folder is symlinked into
the checkout instead of copied.

## Layout

* `keyboards/klaw/` - keyboard definition (`keyboard.json`, OLED code)
* `keyboards/klaw/keymaps/default/` - default keymap: QWERTY with home row
  mods, Nav / Num / Sym layers on the thumbs, encoder map
* `keyboards/klaw/keymaps/vial/` - keymap built for Vial, with VialRGB so the
  per-key RGB is controlled from the Vial lighting tab. Generated from the
  keyboard's own Vial configuration (see below). Builds for the ATmega32U4
  Pro Micro and for the Adafruit KB2040
* `tools/flash.sh` - hands-free flashing over the Caterina bootloader
* `tools/gen_vial_layout.py` - regenerates the Vial layout drawing from the
  PCB, see below
* `tools/vial_dump.py`, `tools/vial_restore.py` - save and restore the
  KLAW's Vial configuration over USB, see below
* `tools/gen_keymap.py` - regenerates the compiled keymap from that
  configuration, see below
* `reference/klaw_vial.json` - the KLAW's current Vial configuration

## Setup

    # once: QMK checkout and CLI
    git clone --depth 1 https://github.com/qmk/qmk_firmware ~/Development/qmk_firmware
    cd ~/Development/qmk_firmware && git submodule update --init --depth 1 lib/lufa lib/printf
    qmk config user.qmk_home=$HOME/Development/qmk_firmware
    qmk config user.overlay_dir=$HOME/Development/klaw-firmware
    ln -s $HOME/Development/klaw-firmware/keyboards/klaw \
          $HOME/Development/qmk_firmware/keyboards/klaw

The AVR toolchain (avr-gcc, avrdude) must be on `PATH`. The build copies
the resulting `klaw_default.hex` into this folder (ignored by git).

RP2040 builds need `arm-none-eabi-gcc`. The QMK CLI installs its own copy
under `~/.config/data/qmk/bin` (`qmk setup` fetches it), which `make` does
not see unless it is on `PATH`:

    export PATH=$PATH:$HOME/.config/data/qmk/bin

The plain `qmk_firmware` checkout above only fetches the AVR submodules. To
build the default keymap for RP2040 there as well, add the ChibiOS ones:

    cd ~/Development/qmk_firmware && git submodule update --init --depth 1 \
        lib/chibios lib/chibios-contrib lib/pico-sdk

## Build and flash

    qmk compile -kb klaw -km default
    qmk flash -kb klaw -km default

Flash each half on its own. `qmk flash` waits for a new serial port, then
press the PCB reset button (double tap on some Pro Micro clones) to enter the
Caterina bootloader. The first half plugged into USB becomes the primary.

Hands-free alternative when the board already runs QMK or an Arduino
sketch: open its serial port at 1200 baud to drop into the bootloader, then
flash with avrdude within about eight seconds. The bootloader usually reuses
the same `/dev/ttyACM` name, so identify it by USB product id `0036`:

    stty -F /dev/ttyACM0 1200 hupcl
    avrdude -p m32u4 -c avr109 -P /dev/ttyACM0 -b 57600 -D \
            -U flash:w:klaw_default.hex:i

## Vial

Vial needs the [vial-qmk](https://github.com/vial-kb/vial-qmk) fork, kept as
a second checkout. Use `make` there, not `qmk compile`, and pass the paths
explicitly because the fork misreads `qmk config` output:

    git clone --depth 1 https://github.com/vial-kb/vial-qmk ~/Development/vial-qmk
    ln -s ~/Development/klaw-firmware/keyboards/klaw ~/Development/vial-qmk/keyboards/klaw
    cd ~/Development/vial-qmk
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware make klaw:vial -j16
    ~/Development/klaw-firmware/tools/flash.sh ~/Development/klaw-firmware/klaw_vial.hex

The ATmega32U4 has 28672 bytes for firmware and Vial plus VialRGB use about
5 KB of it, so the vial keymap turns off mouse keys, one-shot keys, tap
dance, combos, key overrides, QMK settings and the costlier RGB effects.
Breathing and the left-right gradient remain. The build lands within a few
hundred bytes of the limit, so adding anything means removing something.

Vial's unlock combo is the top pinky keys of the left half (Q and T).

The key drawing Vial shows comes from `vial.json`, which is generated from
the PCB so that column stagger, the splay of the ring and pinky columns and
the thumb key angles match the board. After moving switches in KiCad:

    tools/gen_vial_layout.py ../klaw/pcb/klaw_2/klaw_2.kicad_pcb preview.svg

The script reads each switch's position and rotation from the PCB and its
matrix position from the pad nets, mirrors the board for the left half and
places the encoder controls in two columns between the halves. It also
rewrites the LED coordinates in `keyboard.json` from the same positions, so
the RGB effects follow the real key placement. The LED chain order there was
checked against the DIN/DOUT nets of the PCB. The optional second argument
writes a quick preview. Rebuild and reflash afterwards, the drawing is
compiled into the firmware.

The RP2040 build enables every RGB effect vial-qmk ships, so the Vial
lighting tab lists them all. The ATmega32U4 build keeps its short list. Its
Vial image is now within about 150 bytes of the flash limit, mostly because
the rotated layout drawing compresses less well than the old grid.

## Keymap

The Vial keymap has four layers, named per half on the OLEDs:

| Layer | Left half | Right half |
|---|---|---|
| 0 | ALPHA | ALPHA |
| 1 | NUMBER | NAVIGATE |
| 2 | CUT/PASTE | CODE |
| 3 | FUNCT | SYMBOL |

The source of truth is the keyboard itself. `reference/klaw_vial.json` is a
dump of its Vial configuration: layers, encoders, tap dances, combos, key
overrides, macros and QMK settings. Two tools move it in and out over USB,
and a third turns it into the compiled defaults:

    tools/vial_dump.py KLAW reference/klaw_vial.json      # save
    tools/vial_restore.py KLAW reference/klaw_vial.json   # restore
    tools/gen_keymap.py                                   # regenerate keymap.c

`gen_keymap.py` writes `keymaps/vial/keymap.c` (layers, encoder map, layer
names, Chordal Hold handedness) and `vial_defaults.h` (everything Vial keeps
only in EEPROM). Edit in Vial, dump, regenerate, rebuild. Restoring writes
into the half on USB only, and macros only while that half is unlocked in
Vial (Security, Unlock, then hold Q and T).

Layer names are compile-time only, neither Vial nor QMK stores them on the
keyboard. The `LAYER_NAMES` list in `gen_keymap.py`, one pair of names per
layer for the left and right half, ends up as `layer_name_klaw()` in
`keymap.c`. Each half's OLED shows nothing but its own name for the active
layer, centred.

The home row keys are mod-taps such as `LSFT_T(KC_A)`, with Chordal Hold
switched on in the QMK settings. Chordal Hold is QMK's equivalent of ZMK's
positional hold-tap: a home row mod settles as held only when the next key
is on the other hand, otherwise it is a tap. The handedness map lives at the
end of `keymap.c`; thumb keys and encoder pushes are exempt so layer taps
can be held with either hand. The tapping term is a Vial setting (QMK
Settings tab) rather than a compile-time value, since vial-qmk routes it
through its settings.

VIA validates its EEPROM against the firmware's build date. A firmware built
on a later day wipes that storage, which holds the Vial tap dances, macros
and QMK settings. To survive that, the keymap seeds the entries from
`vial_defaults.h` whenever it boots into a freshly reset storage. The
restore tool is still the way to update a connected keyboard without
reflashing. The ATmega32U4 build has no room for the seeding or for Chordal
Hold and keeps the plain layers.

## Adafruit KB2040

The same keyboard definition builds for the KB2040, an RP2040 controller in
the Pro Micro footprint, through QMK's converter feature, which remaps every
AVR pin name to the matching GPIO. The vial-qmk checkout ships the ChibiOS
and pico-sdk submodules already, so only the ARM toolchain from Setup is
needed:

    cd ~/Development/vial-qmk
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware \
        CONVERT_TO=kb2040 EXTRAFLAGS=-DINIT_EE_HANDS_LEFT make klaw:vial -j16
    cp .build/klaw_vial_kb2040.uf2 ~/Development/klaw-firmware/klaw_vial_kb2040_left.uf2
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware \
        CONVERT_TO=kb2040 EXTRAFLAGS=-DINIT_EE_HANDS_RIGHT make klaw:vial -j16
    cp .build/klaw_vial_kb2040.uf2 ~/Development/klaw-firmware/klaw_vial_kb2040_right.uf2

Two images are needed because one PCB serves both halves and nothing
electrical tells them apart. The firmware keeps each half's side in its
EEPROM (`EE_HANDS`), and the image written at boot decides it: flash the left
image on the left half and the right image on the right half, once each.
Without this, QMK treats whichever half is on USB as the left one and the
keymap comes out mirrored when the right half is the primary. With the side
stored, either half can be the primary. Only the primary's keymap is used,
so keep both halves' Vial configuration the same if you swap sides.

The converter name matters: the
KB2040 routes three of the column pins (B1, B2, B6) to different GPIOs than
the Sparkfun Pro Micro RP2040 and its clones, which need
`CONVERT_TO=sparkfun_pm2040` or `CONVERT_TO=rp2040_ce` instead. An image
built for the wrong board scans the wrong columns.

With 2 MB of flash none of the AVR trimming applies: the RP2040 build keeps
mouse keys, one-shot keys, tap dance, combos, key overrides, QMK settings,
matrix mirroring and every RGB effect enabled in `keyboard.json`. The
keymap's `rules.mk` and `config.h` switch these on whenever a converter is
in use. Audio stays off on both controllers.

To flash, get the half to show up as the `RPI-RP2` USB drive, then copy the
`.uf2` onto it. The KLAW reset button pulls the footprint's RST pin, which is
the RP2040 RUN pin on the KB2040, so the controller's own buttons are not
needed once it is soldered down:

* Running QMK: double tap the KLAW reset button within half a second. A
  single tap only restarts the firmware. `QK_BOOT` (Z on the Sym layer) and
  Bootmagic (hold the top pinky key while plugging in) work as well.
* Factory CircuitPython, or any firmware with a serial port: open the port at
  1200 baud and close it, as `tools/flash.sh` does for the AVR build:
  `stty -F /dev/ttyACM0 1200 hupcl`. The double tap only works once QMK
  runs, because QMK implements it.
* Nothing responding: hold the KB2040's BOOT button while plugging in.

The build's flash target waits for the drive and copies the file, and its
split variants add the handedness define for you:

    CONVERT_TO=kb2040 make klaw:vial:uf2-split-left
    CONVERT_TO=kb2040 make klaw:vial:uf2-split-right

Flash each half on its own with its own image. The Vial unlock combo and
`vial.json` are shared between both controllers.

## Hardware summary

| Function | Pro Micro pin | KB2040 GPIO |
|---|---|---|
| Rows | C6 D7 E6 B4 | GP5 GP6 GP7 GP8 |
| Columns (pinky to inner) | F7 B1 B3 B2 B6 | GP26 GP18 GP20 GP19 GP10 |
| Encoder A/B | F5/F4 left, F4/F5 right | GP28/GP29 left, GP29/GP28 right |
| RGB data | D3 (17 SK6812MINI-E per half) | GP0 (PIO driver) |
| OLED | I2C on D1/D0 | I2C1 on GP2/GP3 |
| Buzzer | B5 | GP9 |
| Split serial | D2 | GP1 (PIO driver) |

Pin assignments were taken from the KiCad netlist of `klaw/pcb/klaw_1`.

## License

GPL-3.0-or-later, same as the KLAW hardware.
