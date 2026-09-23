# klaw-firmware

QMK firmware for the [KLAW](https://github.com/scrambletools/klaw), a
reversible 34-key split keyboard derived from GEIGEIGEIST's KLOR. The main
target is a Vial build for the Adafruit KB2040; the ATmega32U4 Pro Micro is
still supported with a trimmed feature set.

The keyboard definition lives in `keyboards/klaw/` and is built against
separate QMK checkouts. QMK's external userspace only overlays keymaps, not
keyboard definitions, so the keyboard folder is symlinked into the checkouts
instead of copied.

## Contents

* `keyboards/klaw/` - keyboard definition: `keyboard.json`, pins, split and
  audio configuration, OLED code
* `keyboards/klaw/keymaps/default/` - plain QMK keymap: QWERTY with home row
  mods, Nav / Num / Sym layers on the thumbs, encoder map
* `keyboards/klaw/keymaps/vial/` - the Vial keymap, generated from the
  keyboard's own Vial configuration
* `reference/klaw_vial.json` - that configuration, dumped from the keyboard
* `tools/build.sh` - dumps the connected keyboard, regenerates the keymap and
  builds both KB2040 images
* `tools/vial_dump.py`, `tools/vial_restore.py` - save and restore the Vial
  configuration over USB
* `tools/gen_keymap.py` - turns the dump into `keymap.c` and the seeded
  defaults
* `tools/gen_vial_layout.py` - generates the Vial key drawing and the LED map
  from the KiCad PCB
* `tools/gen_icons.py` - turns ASCII art into the OLED status icons
* `tools/flash.sh` - hands-free flashing of an ATmega32U4 over Caterina

## What each build enables

The keyboard definition sets a baseline; the Vial keymap adds to it on the
RP2040 and trims it on the ATmega32U4 to fit the flash.

| Feature | QMK default keymap, either controller | Vial keymap on KB2040 | Vial keymap on ATmega32U4 |
|---|---|---|---|
| Split with handedness in EEPROM | yes | yes | yes |
| Both encoders with per-layer map | yes | yes | yes |
| OLEDs with per-side layer names and status icons | yes, small font on AVR, big on RP2040 | yes, big font and icons | yes, small font, caps text |
| Per-key RGB | yes, 8 effects | yes, all 49 effects | yes, 2 effects |
| Media and system keys | yes | yes | yes |
| Mouse keys | yes | yes | no |
| Bootmagic reset | yes | yes | yes |
| Buzzer | off | on, with tunes and key click | off |
| Vial: dynamic keymap, tap dance, combos, key overrides, settings, macros | no | yes, with seeded defaults | keymap and macros only |
| Chordal Hold home row | no | yes | no |
| N-key rollover | off | off | off |

The buzzer configuration and N-key rollover are candidates for the upstream
QMK definition later.

## Setup

Two checkouts: `qmk_firmware` for the default keymap and upstream work,
`vial-qmk` for the Vial builds. Known good commits are vial-qmk `dd43959a`
and qmk_firmware `08c662f2`.

    git clone --depth 1 https://github.com/qmk/qmk_firmware ~/Development/qmk_firmware
    cd ~/Development/qmk_firmware && git submodule update --init --depth 1 lib/lufa lib/printf
    qmk config user.qmk_home=$HOME/Development/qmk_firmware
    qmk config user.overlay_dir=$HOME/Development/klaw-firmware
    ln -s $HOME/Development/klaw-firmware/keyboards/klaw ~/Development/qmk_firmware/keyboards/klaw

    git clone --depth 1 https://github.com/vial-kb/vial-qmk ~/Development/vial-qmk
    ln -s $HOME/Development/klaw-firmware/keyboards/klaw ~/Development/vial-qmk/keyboards/klaw

Toolchains: the AVR tools (avr-gcc, avrdude) must be on `PATH`. RP2040 builds
need `arm-none-eabi-gcc`; the QMK CLI installs its own copy under
`~/.config/data/qmk/bin` (`qmk setup` fetches it), which `make` only sees
when it is on `PATH`. `tools/build.sh` adds it itself.

The plain `qmk_firmware` checkout above only fetches the AVR submodules. To
build the default keymap for RP2040 there as well, add the ChibiOS ones:

    cd ~/Development/qmk_firmware && git submodule update --init --depth 1 \
        lib/chibios lib/chibios-contrib lib/pico-sdk

## Building and flashing the KB2040

The everyday loop is: edit in Vial, then build with the keyboard plugged in:

    tools/build.sh

It dumps the connected half into `reference/klaw_vial.json`, regenerates the
keymap and builds `klaw_vial_kb2040_left.uf2` and
`klaw_vial_kb2040_right.uf2` (ignored by git). Always build with the
keyboard connected after editing in Vial: a firmware built on a later day
than the one on the keyboard wipes the Vial storage on first boot, and what
comes back is what was compiled in, so a build made from a stale dump
silently reverts the edits made since.

By hand, the equivalent is one build per side in the vial-qmk checkout:

    cd ~/Development/vial-qmk
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware \
        CONVERT_TO=kb2040 EXTRAFLAGS=-DINIT_EE_HANDS_LEFT make klaw:vial -j16
    cp .build/klaw_vial_kb2040.uf2 ~/Development/klaw-firmware/klaw_vial_kb2040_left.uf2
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware \
        CONVERT_TO=kb2040 EXTRAFLAGS=-DINIT_EE_HANDS_RIGHT make klaw:vial -j16
    cp .build/klaw_vial_kb2040.uf2 ~/Development/klaw-firmware/klaw_vial_kb2040_right.uf2

Two images are needed because one PCB serves both halves and nothing
electrical tells them apart. The firmware keeps each half's side in its
EEPROM (`EE_HANDS`), and the image written at boot decides it: the left
image goes on the left half, the right image on the right half. With the
side stored, either half can be the primary. Only the primary's keymap is
used, so keep both halves' Vial configuration the same if you swap sides.

Flash both halves after every firmware change before using the keyboard:
halves running different images can disagree about the split link, and the
secondary's keys then never arrive until both are updated.

To flash, get the half to show up as the `RPI-RP2` USB drive and copy the
`.uf2` onto it. The KLAW reset button pulls the footprint's RST pin, which is
the RP2040 RUN pin on the KB2040, so the controller's own buttons are not
needed once it is soldered down:

* Running QMK: double tap the KLAW reset button within half a second. A
  single tap only restarts the firmware. Bootmagic (hold the top pinky key
  while plugging in) works as well, as does a `QK_BOOT` key assigned in Vial.
* Factory CircuitPython, or any firmware with a serial port: open the port at
  1200 baud and close it: `stty -F /dev/ttyACM0 1200 hupcl`. The double tap
  only works once QMK runs, because QMK implements it.
* Nothing responding: hold the KB2040's BOOT button while plugging in.

The build's flash targets wait for the drive and copy the file, and their
split variants add the handedness define:

    CONVERT_TO=kb2040 make klaw:vial:uf2-split-left
    CONVERT_TO=kb2040 make klaw:vial:uf2-split-right

The converter name matters: the KB2040 routes three of the column pins (B1,
B2, B6) to different GPIOs than the Sparkfun Pro Micro RP2040 and its
clones, which need `CONVERT_TO=sparkfun_pm2040` or `CONVERT_TO=rp2040_ce`
instead. An image built for the wrong board scans the wrong columns.

## Keymap and Vial configuration

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
only in EEPROM). Restoring writes into the half on USB only, and macros only
while that half is unlocked in Vial (Security, Unlock, then hold Q and T).
Vial's own "Save current layout" and "Load saved layout" do the same job
with `.vil` files; `reference/klaw.vil` is one such export.

VIA validates its EEPROM against the firmware's build date. A firmware built
on a later day wipes that storage, which holds the Vial tap dances, macros
and QMK settings, and also clears the Vial unlock. To survive that, the
keymap seeds the entries from `vial_defaults.h` whenever it boots into a
freshly reset storage. The ATmega32U4 build has no room for the seeding.

The home row keys are mod-taps such as `LSFT_T(KC_A)`, with Chordal Hold
switched on in the QMK settings. Chordal Hold is QMK's equivalent of ZMK's
positional hold-tap: a home row mod settles as held only when the next key
is on the other hand, otherwise it is a tap. The handedness map lives at the
end of `keymap.c`; thumb keys and encoder pushes are exempt so layer taps
can be held with either hand. The tapping term is a Vial setting (QMK
Settings tab) rather than a compile-time value, since vial-qmk routes it
through its settings.

Layer names are compile-time only, neither Vial nor QMK stores them on the
keyboard. The `LAYER_NAMES` list in `gen_keymap.py`, one pair of names per
layer for the left and right half, ends up as `layer_name_klaw()` in
`keymap.c`.

The keymap has no caps lock key. Hyprland's default options turn the caps
lock key into Compose and toggle caps lock with both shifts, which on the
KLAW means holding the A and ; home row keys together. `CW_TOGG` (Caps
Word) assigned in Vial is the alternative that involves no host setting.

## OLED

Each half shows its own name for the active layer at the top, centred, at
twice the size of the built-in font, and 16x16 icons along the bottom for
caps (caps lock or Caps Word), audio on, key click on and RGB on. The
secondary learns the host's lock state through the split link's indicator
sync and the other states through a small keyboard-level transaction, so
both screens agree. The icons are ASCII art in `tools/gen_icons.py`, which
writes `keyboards/klaw/oled_icons.h`. The screens redraw only when
something changes.

## Audio

The KB2040 build drives the piezo through the RP2040's hardware PWM. Audio
plays a startup and a goodbye tune; `AU_TOGG` turns it on and off and the
state is kept in EEPROM, `CK_TOGG` adds a click per keypress with `CK_UP`
and `CK_DOWN` for its pitch. Assign those keycodes in Vial (Quantum tab).
Each half clicks for its own keys through its own buzzer. QMK's own clicky
would sound on the primary for the keys of both halves, so it stays off;
instead each half watches its own matrix and plays the same click for every
press it sees. The primary handles the `CK_` keycodes, keeps the on/off
state in the keyboard's EEPROM word and sends it to the secondary together
with the pitch. Encoder turns get their own sound, a two-note slide rising
clockwise and falling the other way, from the half whose encoder moved, and
follow the same on/off state. Tunes only play on the primary. The tunes are compile-time
`SONG()` definitions in the keymap's `config.h`.

## Key drawing and LED map from the PCB

The key drawing Vial shows comes from `vial.json`, generated from the PCB so
that column stagger, the splay of the ring and pinky columns and the thumb
key angles match the board. After moving switches in KiCad:

    tools/gen_vial_layout.py ../klaw/pcb/klaw_2/klaw_2.kicad_pcb preview.svg

The script reads each switch's position and rotation from the PCB and its
matrix position from the pad nets, mirrors the board for the left half and
places the encoder controls in two columns between the halves. It also
rewrites the LED coordinates in `keyboard.json` from the same positions, so
the RGB effects follow the real key placement. The LED chain order there was
checked against the DIN/DOUT nets of the PCB. The optional second argument
writes a quick preview. Rebuild and reflash afterwards, the drawing is
compiled into the firmware.

## ATmega32U4 builds

Default keymap:

    qmk compile -kb klaw -km default
    qmk flash -kb klaw -km default

`qmk flash` waits for a new serial port, then press the PCB reset button
(double tap on some Pro Micro clones) to enter the Caterina bootloader.
Hands-free alternative when the board already runs QMK or an Arduino sketch:
open its serial port at 1200 baud to drop into the bootloader, then flash
with avrdude within about eight seconds; identify the bootloader port by USB
product id `0036`:

    stty -F /dev/ttyACM0 1200 hupcl
    avrdude -p m32u4 -c avr109 -P /dev/ttyACM0 -b 57600 -D \
            -U flash:w:klaw_default.hex:i

Vial keymap, built with `make` in the vial-qmk checkout because the fork
misreads `qmk config` output:

    cd ~/Development/vial-qmk
    QMK_HOME=$PWD QMK_USERSPACE=$HOME/Development/klaw-firmware make klaw:vial -j16
    ~/Development/klaw-firmware/tools/flash.sh ~/Development/klaw-firmware/klaw_vial.hex

The ATmega32U4 has 28672 bytes for firmware and Vial plus VialRGB use about
5 KB of it, so the vial keymap turns off mouse keys, one-shot keys, tap
dance, combos, key overrides, QMK settings, the seeding, Chordal Hold, the
lock-state sync and the costlier RGB effects. Breathing and the left-right
gradient remain. The build lands about 120 bytes under the limit, so adding
anything means removing something. Handedness still needs one build per
side with `-DINIT_EE_HANDS_LEFT` or `-DINIT_EE_HANDS_RIGHT`, or the
`avrdude-split-left` and `avrdude-split-right` flash targets.

## Hardware summary

| Function | Pro Micro pin | KB2040 GPIO |
|---|---|---|
| Rows | C6 D7 E6 B4 | GP5 GP6 GP7 GP8 |
| Columns (pinky to inner) | F7 B1 B3 B2 B6 | GP26 GP18 GP20 GP19 GP10 |
| Encoder A/B | F5/F4 left, F4/F5 right | GP28/GP29 left, GP29/GP28 right |
| RGB data | D3 (17 SK6812MINI-E per half) | GP0 (PIO driver) |
| OLED | I2C on D1/D0 | I2C1 on GP2/GP3 |
| Buzzer | B5 | GP9 (PWM slice 4 B) |
| Split serial | D2 | GP1 (PIO driver) |

Pin assignments were taken from the KiCad netlist of the hardware
repository's `pcb/klaw_2`. The first right half has its encoder B line
rerouted to A1, see the keyboard readme.

## License

GPL-3.0-or-later, same as the KLAW hardware.
