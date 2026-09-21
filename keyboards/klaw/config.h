// Copyright 2026 Scramble Tools
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Newer avr-libc no longer includes avr/io.h from util/delay.h, which
// quantum/send_string relies on for the timer registers. Upstream QMK has
// the fix in core (its send_string.c includes the header itself); vial-qmk
// does not yet, so the AVR Vial build still needs this
#ifdef __AVR__
#    include <avr/io.h>
#endif

// Piezo buzzer, only used when AUDIO_ENABLE is set in a keymap
#ifdef AUDIO_ENABLE
#    define AUDIO_PIN B5
#    ifdef MCU_RP
// GP9 is PWM slice 4, channel B on the RP2040
#        define AUDIO_PWM_DRIVER PWMD4
#        define AUDIO_PWM_CHANNEL RP2040_PWM_CHANNEL_B
#        define AUDIO_INIT_DELAY
#    endif
#endif

// 0.96" SSD1306 module on I2C (D1/D0), one per half
#ifdef OLED_ENABLE
#    define OLED_DISPLAY_128X64
#endif

// The primary sends the secondary the states shown on the OLED (audio, click,
// caps word). Not on the ATmega32U4, whose flash is full
#ifndef __AVR__
#    define SPLIT_TRANSACTION_IDS_KB KLAW_SYNC_STATE
#endif

// Give the split link a moment to settle before the primary decides it is alone
#define SPLIT_USB_TIMEOUT 2000

// One PCB serves both halves, so nothing electrical tells them apart. Each
// half keeps its side in EEPROM, written by a left or right firmware image
#define EE_HANDS
