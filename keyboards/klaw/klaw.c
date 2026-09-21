// Copyright 2026 Scramble Tools
// SPDX-License-Identifier: GPL-3.0-or-later
#include "quantum.h"
#include "transactions.h"
#include "oled_icons.h"

#if defined(AUDIO_ENABLE) && defined(SPLIT_KEYBOARD)
// Audio only runs on the primary, the secondary learns its state over the link
static uint8_t synced_audio_on;

static void sync_state_handler(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    synced_audio_on = *(const uint8_t *)in;
}

void keyboard_post_init_kb(void) {
    transaction_register_rpc(KLAW_SYNC_STATE, sync_state_handler);
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    static uint8_t  last_sent = 0xFF;
    static uint32_t last_time;
    if (!is_keyboard_master()) {
        return;
    }
    uint8_t on = audio_is_on();
    if (on != last_sent || timer_elapsed32(last_time) > 500) {
        if (transaction_rpc_send(KLAW_SYNC_STATE, sizeof(on), &on)) {
            last_sent = on;
            last_time = timer_read32();
        }
    }
}

static bool audio_indicator(void) {
    return is_keyboard_master() ? audio_is_on() : synced_audio_on;
}
#endif

#ifdef OLED_ENABLE
oled_rotation_t oled_init_kb(oled_rotation_t rotation) {
    return OLED_ROTATION_180;
}

// Keymaps name their layers per side by defining this; unnamed layers show a
// number. Each half shows the name for its own side, the layer state is
// synced to the secondary.
__attribute__((weak)) const char *layer_name_klaw(uint8_t layer, bool left) {
    return NULL;
}

#    ifndef __AVR__
static const char *current_name(char *fallback, size_t size, uint8_t *layer) {
    *layer           = get_highest_layer(layer_state | default_layer_state);
    const char *name = layer_name_klaw(*layer, is_keyboard_left());
    if (name == NULL) {
        snprintf(fallback, size, "Layer %u", *layer);
        name = fallback;
    }
    return name;
}

// Text at twice the size of the built-in font: the driver renders the line
// at normal size on the bottom line, that line is read back and every pixel
// expanded into a 2x2 block on the two target lines. Ten characters fit.
#        define BIG_CHARS (OLED_DISPLAY_WIDTH / (2 * OLED_FONT_WIDTH))
#        define LINES (OLED_DISPLAY_HEIGHT / 8)
#        define SCRATCH_LINE (LINES - 1)

static uint8_t stretch(uint8_t nibble) {
    uint8_t out = 0;
    for (uint8_t bit = 0; bit < 4; bit++) {
        if (nibble & (1 << bit)) {
            out |= 3 << (2 * bit);
        }
    }
    return out;
}

static void blank_line(uint8_t line) {
    for (uint8_t x = 0; x < OLED_DISPLAY_WIDTH; x++) {
        oled_write_raw_byte(0, line * OLED_DISPLAY_WIDTH + x);
    }
}

// line is the upper of the two 8 pixel lines, the text is centred on it
static void oled_write_big(uint8_t line, const char *text) {
    char clipped[BIG_CHARS + 1];
    strlcpy(clipped, text, sizeof(clipped));
    oled_set_cursor(0, SCRATCH_LINE);
    oled_write(clipped, false);

    uint8_t              small[OLED_DISPLAY_WIDTH];
    oled_buffer_reader_t reader = oled_read_raw(SCRATCH_LINE * OLED_DISPLAY_WIDTH);
    memcpy(small, reader.current_element, sizeof(small));
    blank_line(SCRATCH_LINE);

    uint8_t  used = strlen(clipped) * OLED_FONT_WIDTH;
    uint8_t  x0   = (OLED_DISPLAY_WIDTH - 2 * used) / 2;
    uint16_t top = line * OLED_DISPLAY_WIDTH, bottom = top + OLED_DISPLAY_WIDTH;
    blank_line(line);
    blank_line(line + 1);
    for (uint8_t x = 0; x < used; x++) {
        uint8_t hi = stretch(small[x] & 0x0F), lo = stretch(small[x] >> 4);
        oled_write_raw_byte(hi, top + x0 + 2 * x);
        oled_write_raw_byte(hi, top + x0 + 2 * x + 1);
        oled_write_raw_byte(lo, bottom + x0 + 2 * x);
        oled_write_raw_byte(lo, bottom + x0 + 2 * x + 1);
    }
}

static void draw_icon(uint8_t x0, uint8_t line, const uint8_t *icon) {
    for (uint8_t x = 0; x < ICON_WIDTH; x++) {
        oled_write_raw_byte(pgm_read_byte(icon + x), line * OLED_DISPLAY_WIDTH + x0 + x);
        oled_write_raw_byte(pgm_read_byte(icon + ICON_WIDTH + x), (line + 1) * OLED_DISPLAY_WIDTH + x0 + x);
    }
}

// Layer name on top, state icons along the bottom, redrawn on change
static void render(void) {
    static uint32_t shown = 0xFFFFFFFF;
    char            fallback[BIG_CHARS + 1];
    uint8_t         layer;
    const char     *name = current_name(fallback, sizeof(fallback), &layer);
    bool            caps = host_keyboard_led_state().caps_lock;
    bool            rgb = false, audio = false;
#        ifdef RGB_MATRIX_ENABLE
    rgb = rgb_matrix_is_enabled();
#        endif
#        if defined(AUDIO_ENABLE) && defined(SPLIT_KEYBOARD)
    audio = audio_indicator();
#        endif
    uint32_t state = layer | (caps << 8) | (rgb << 9) | (audio << 10);
    if (state == shown) {
        return;
    }
    shown = state;
    for (uint8_t line = 0; line < LINES; line++) {
        blank_line(line);
    }
    oled_write_big(0, name);
    if (caps) {
        draw_icon(24, LINES - 2, icon_caps);
    }
    if (audio) {
        draw_icon(56, LINES - 2, icon_audio);
    }
    if (rgb) {
        draw_icon(88, LINES - 2, icon_rgb);
    }
}
#    else
static void render(void) {
    uint8_t     layer = get_highest_layer(layer_state | default_layer_state);
    const char *name  = layer_name_klaw(layer, is_keyboard_left());
    oled_set_cursor(0, 0);
    if (name == NULL) {
        oled_write_P(PSTR("Layer "), false);
        oled_write_ln(get_u8_str(layer, ' '), false);
    } else {
        oled_write_ln(name, false);
    }
    oled_set_cursor(0, 7);
    oled_write_ln_P(host_keyboard_led_state().caps_lock ? PSTR("CAPS") : PSTR("    "), false);
}
#    endif

bool oled_task_kb(void) {
    if (!oled_task_user()) {
        return false;
    }
    render();
    return false;
}
#endif
