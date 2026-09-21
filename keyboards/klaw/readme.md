# KLAW

34-key reversible split keyboard derived from GEIGEIGEIST's KLOR.
One PCB serves both halves: the left build populates the top face, the right
build the bottom face. Hardware repository: <https://github.com/scrambletools/klaw> (KiCad 10 project).

* Controller: Pro Micro compatible, either the ATmega32U4 (Caterina
  bootloader) or an Adafruit KB2040 through the `kb2040` converter
* Per-key SK6812MINI-E RGB (17 per half, data on D3)
* EC11 encoder per half (F4/F5) with push switch in the key matrix
* 0.96" SSD1306 OLED per half on I2C, 128x64. Each half shows the name of
  the active layer for its own side at the top, centred, at twice the
  built-in font size (12x16 pixel characters, up to 10) by scaling the
  driver's rendering in `klaw.c`, and 16x16 icons along the bottom for caps
  (caps lock or Caps Word), audio on, key click on and RGB on. The icons are ASCII art in
  `tools/gen_icons.py`, which writes `oled_icons.h`. Names come from
  `layer_name_klaw()` in the keymap. The AVR build shows the name and a
  caps lock text in the plain 6x8 font
* Piezo buzzer on B5. Driven by hardware PWM on the RP2040 build (GP9,
  PWM slice 4); off on the ATmega32U4, whose flash is full
* TRRS between halves, soft serial on D2

## Matrix

Rows `C6 D7 E6 B4`, columns `F7 B1 B3 B2 B6` (pinky to inner index on the left
half). Diodes are COL2ROW. The right half scans the same matrix and is exposed
as rows 4-7. Row 3 holds the two thumb keys (columns 1 and 2) and the encoder
push switch (column 4).

KLOR's outermost column (F6) is not routed on KLAW, so the matrix is 4x5
instead of 4x6.

## Board-specific repair

On the right half of the first build the controller's A2 castellation (F5,
the encoder's B line) never bonded to the board. A bodge wire runs from the
spare A1 castellation (F6, GP27 on the KB2040) to that encoder pin instead,
and the Vial keymap's `config.h` sets `ENCODER_B_PINS_RIGHT` to F6. Remove
that override for a board without the bodge.

## Layouts

* `LAYOUT`: 36 positions, the 34 keys plus both encoder push switches.
* `LAYOUT_split_3x5_2`: the 34 keys only, compatible with the
  `split_3x5_2` community layout.

## Build and flash

    qmk compile -kb klaw -km default
    qmk flash -kb klaw -km default

Vial: build `klaw:vial` from a vial-qmk checkout (see the repository README).

Adafruit KB2040: add `CONVERT_TO=kb2040` to either build. The result is a
`.uf2` to copy onto the `RPI-RP2` drive that appears after a double tap of
the KLAW reset button (wired to RST, the RP2040 RUN pin), or after a 1200
baud touch of the serial port while the board still runs CircuitPython.
The AVR pin names above stay valid, the converter maps
them to the KB2040's GPIOs, which differ from other RP2040 Pro Micro boards
on three column pins.

The halves store their side in EEPROM (`EE_HANDS`). Build the firmware once
with `-DINIT_EE_HANDS_LEFT` for the left half and once with
`-DINIT_EE_HANDS_RIGHT` for the right half, or use the `uf2-split-left` and
`uf2-split-right` flash targets.

Flash each half separately. When the flasher waits for a serial port, press
the reset button on the PCB (double tap on some Pro Micro clones). Hold the
top pinky key of a half while plugging it in to enter the bootloader with
Bootmagic.
