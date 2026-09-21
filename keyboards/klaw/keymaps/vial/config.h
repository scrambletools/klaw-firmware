// Copyright 2026 Scramble Tools
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#define VIAL_KEYBOARD_UID {0xA4, 0xC2, 0x11, 0xF6, 0xEF, 0x65, 0xF4, 0xF4}

// Hold both top pinky keys of the left half (Q and T) to unlock Vial
#define VIAL_UNLOCK_COMBO_ROWS {0, 0}
#define VIAL_UNLOCK_COMBO_COLS {0, 4}

#define DYNAMIC_KEYMAP_LAYER_COUNT 4

// The ATmega32U4 build drops what does not fit next to Vial. The RP2040 build
// keeps one-shot keys, matrix mirroring and every effect from keyboard.json.
#ifdef __AVR__
#    define LAYER_STATE_8BIT
#    define NO_ACTION_ONESHOT

// Matrix mirroring only feeds reactive RGB effects, which are off here
#    undef SPLIT_TRANSPORT_MIRROR

// Only the cheap RGB effects fit next to Vial on the ATmega32U4
#    undef ENABLE_RGB_MATRIX_SPLASH
#    undef ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
#    undef ENABLE_RGB_MATRIX_BAND_SAT
#    undef ENABLE_RGB_MATRIX_BAND_VAL
#    undef ENABLE_RGB_MATRIX_ALPHAS_MODS
#    undef ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#else
// Every effect vial-qmk ships, so the Vial lighting tab offers them all
#    define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#    define RGB_MATRIX_KEYPRESSES
#    define ENABLE_RGB_MATRIX_ALPHAS_MODS
#    define ENABLE_RGB_MATRIX_BAND_PINWHEEL_SAT
#    define ENABLE_RGB_MATRIX_BAND_PINWHEEL_VAL
#    define ENABLE_RGB_MATRIX_BAND_SAT
#    define ENABLE_RGB_MATRIX_BAND_SPIRAL_SAT
#    define ENABLE_RGB_MATRIX_BAND_SPIRAL_VAL
#    define ENABLE_RGB_MATRIX_BAND_VAL
#    define ENABLE_RGB_MATRIX_BREATHING
#    define ENABLE_RGB_MATRIX_CYCLE_ALL
#    define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#    define ENABLE_RGB_MATRIX_CYCLE_OUT_IN
#    define ENABLE_RGB_MATRIX_CYCLE_OUT_IN_DUAL
#    define ENABLE_RGB_MATRIX_CYCLE_PINWHEEL
#    define ENABLE_RGB_MATRIX_CYCLE_SPIRAL
#    define ENABLE_RGB_MATRIX_CYCLE_UP_DOWN
#    define ENABLE_RGB_MATRIX_DIGITAL_RAIN
#    define ENABLE_RGB_MATRIX_DUAL_BEACON
#    define ENABLE_RGB_MATRIX_FLOWER_BLOOMING
#    define ENABLE_RGB_MATRIX_GRADIENT_LEFT_RIGHT
#    define ENABLE_RGB_MATRIX_GRADIENT_UP_DOWN
#    define ENABLE_RGB_MATRIX_HUE_BREATHING
#    define ENABLE_RGB_MATRIX_HUE_PENDULUM
#    define ENABLE_RGB_MATRIX_HUE_WAVE
#    define ENABLE_RGB_MATRIX_JELLYBEAN_RAINDROPS
#    define ENABLE_RGB_MATRIX_MULTISPLASH
#    define ENABLE_RGB_MATRIX_PIXEL_FLOW
#    define ENABLE_RGB_MATRIX_PIXEL_FRACTAL
#    define ENABLE_RGB_MATRIX_PIXEL_RAIN
#    define ENABLE_RGB_MATRIX_RAINBOW_BEACON
#    define ENABLE_RGB_MATRIX_RAINBOW_MOVING_CHEVRON
#    define ENABLE_RGB_MATRIX_RAINBOW_PINWHEELS
#    define ENABLE_RGB_MATRIX_RAINDROPS
#    define ENABLE_RGB_MATRIX_RIVERFLOW
#    define ENABLE_RGB_MATRIX_SOLID_MULTISPLASH
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_CROSS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTICROSS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_NEXUS
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_SIMPLE
#    define ENABLE_RGB_MATRIX_SOLID_REACTIVE_WIDE
#    define ENABLE_RGB_MATRIX_SOLID_SPLASH
#    define ENABLE_RGB_MATRIX_SPLASH
#    define ENABLE_RGB_MATRIX_STARLIGHT
#    define ENABLE_RGB_MATRIX_STARLIGHT_DUAL_HUE
#    define ENABLE_RGB_MATRIX_STARLIGHT_DUAL_SAT
#    define ENABLE_RGB_MATRIX_STARLIGHT_SMOOTH
#    define ENABLE_RGB_MATRIX_TYPING_HEATMAP
#endif

// Board-specific repair: on the right half the controller's A2 castellation
// (F5) never bonded to the board, so the encoder's B line is bodged to the
// spare A1 castellation (F6). The left half is untouched.
#undef ENCODER_B_PINS_RIGHT
#define ENCODER_B_PINS_RIGHT { F6 }

// Buzzer: startup and goodbye tunes, key click available through CK_TOGG.
// AU_TOGG turns the audio on and off, the state is kept in EEPROM
#ifdef AUDIO_ENABLE
#    define STARTUP_SONG SONG(STARTUP_SOUND)
#    define GOODBYE_SONG SONG(GOODBYE_SOUND)
#    define AUDIO_CLICKY
#endif

#define TAPPING_TERM 200
#define PERMISSIVE_HOLD
#define QUICK_TAP_TERM 0

// Home row mods settle as held only with a key of the other hand, like ZMK's
// positional hold-tap. Handedness map in keymap.c. vial-qmk already defines
// this when QMK settings are enabled and exposes it as a runtime toggle.
// Not on the ATmega32U4, whose flash is full.
#if !defined(__AVR__) && !defined(CHORDAL_HOLD)
#    define CHORDAL_HOLD
#endif
