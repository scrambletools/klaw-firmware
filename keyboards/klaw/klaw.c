// Copyright 2026 Scramble Tools
// SPDX-License-Identifier: GPL-3.0-or-later
#include "quantum.h"
#include "transactions.h"
#include "oled_icons.h"

#if defined(SPLIT_KEYBOARD) && !defined(__AVR__)
// States that only the primary knows, sent to the secondary for its OLED and
// for the key click, which each half plays for its own keys
#    ifdef AUDIO_ENABLE
#        include "audio.h"
#        ifdef AUDIO_CLICKY
#            include "process_clicky.h"
extern float          clicky_freq;
extern float          clicky_rand;
extern audio_config_t audio_config;
#        endif
#    endif
#    ifdef CAPS_WORD_ENABLE
#        include "caps_word.h"
#    endif

enum { STATE_AUDIO = 1 << 0, STATE_CLICKY = 1 << 1, STATE_CAPS_WORD = 1 << 2 };

typedef struct __attribute__((packed)) {
    uint8_t  flags;
    uint16_t clicky_freq;
} sync_state_t;

static sync_state_t synced;

static uint8_t first_local_row(void) {
    return is_keyboard_left() ? 0 : MATRIX_ROWS / 2;
}

#    if defined(AUDIO_ENABLE) && defined(AUDIO_CLICKY)
// One click per physical key press, played by the half the key is on. QMK's
// own clicky runs on the primary for the keys of both halves, so it stays
// off and its keycodes are handled here. The on/off state lives in the
// keyboard's EEPROM word and reaches the secondary with the pitch.
static bool click_on;

static void click_load(void) {
    click_on = eeconfig_read_kb() & 1;
}

static void click_save(void) {
    eeconfig_update_kb(click_on ? 1 : 0);
}

bool pre_process_record_kb(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case QK_AUDIO_CLICKY_TOGGLE:
        case QK_AUDIO_CLICKY_ON:
        case QK_AUDIO_CLICKY_OFF:
            if (record->event.pressed) {
                click_on = keycode == QK_AUDIO_CLICKY_TOGGLE ? !click_on : keycode == QK_AUDIO_CLICKY_ON;
                click_save();
            }
            return false;
        case QK_AUDIO_CLICKY_UP:
            if (record->event.pressed) clicky_freq_up();
            return false;
        case QK_AUDIO_CLICKY_DOWN:
            if (record->event.pressed) clicky_freq_down();
            return false;
        case QK_AUDIO_CLICKY_RESET:
            if (record->event.pressed) clicky_freq_reset();
            return false;
    }
    return pre_process_record_user(keycode, record);
}

// QMK's click sound: a short rest, a high blip and a lower one, slightly random
static void play_click(float freq) {
    static float song[][2] = {{0.0f, 1}, {440.0f, 3}, {440.0f, 1}};
    song[1][0]             = 2.0f * freq * (1.0f + clicky_rand * ((float)rand() / (float)RAND_MAX));
    song[2][0]             = freq * (1.0f + clicky_rand * ((float)rand() / (float)RAND_MAX));
    PLAY_SONG(song);
}

static void click_task(void) {
    static matrix_row_t previous[MATRIX_ROWS / 2];
    bool                on;
    float               freq;
    if (is_keyboard_master()) {
        on   = audio_is_on() && click_on;
        freq = clicky_freq;
    } else {
        // follow the primary's audio switch, without touching this half's EEPROM
        audio_config.enable = (synced.flags & STATE_AUDIO) != 0;
        on                  = audio_config.enable && (synced.flags & STATE_CLICKY);
        freq                = synced.clicky_freq ? synced.clicky_freq : clicky_freq;
    }
    for (uint8_t row = 0; row < MATRIX_ROWS / 2; row++) {
        matrix_row_t now = matrix_get_row(first_local_row() + row);
        if (on && (now & ~previous[row])) {
            play_click(freq);
        }
        previous[row] = now;
    }
}
#    endif

static sync_state_t local_state(void) {
    sync_state_t state = {0};
#    ifdef AUDIO_ENABLE
    if (audio_is_on()) state.flags |= STATE_AUDIO;
#        ifdef AUDIO_CLICKY
    if (click_on) state.flags |= STATE_CLICKY;
    state.clicky_freq = (uint16_t)clicky_freq;
#        endif
#    endif
#    ifdef CAPS_WORD_ENABLE
    if (is_caps_word_on()) state.flags |= STATE_CAPS_WORD;
#    endif
    return state;
}

static void sync_state_handler(uint8_t in_len, const void *in, uint8_t out_len, void *out) {
    memcpy(&synced, in, sizeof(synced));
}

void keyboard_post_init_kb(void) {
    transaction_register_rpc(KLAW_SYNC_STATE, sync_state_handler);
#    if defined(AUDIO_ENABLE) && defined(AUDIO_CLICKY)
    audio_config.clicky_enable = false; // the click is played here, not by QMK
    click_load();
#    endif
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    static sync_state_t last_sent = {0xFF, 0};
    static uint32_t     last_time;
#    if defined(AUDIO_ENABLE) && defined(AUDIO_CLICKY)
    click_task();
#    endif
    if (!is_keyboard_master()) {
        return;
    }
    sync_state_t state = local_state();
    if (memcmp(&state, &last_sent, sizeof(state)) != 0 || timer_elapsed32(last_time) > 500) {
        if (transaction_rpc_send(KLAW_SYNC_STATE, sizeof(state), &state)) {
            last_sent = state;
            last_time = timer_read32();
        }
    }
}

static uint8_t shared_state(void) {
    return is_keyboard_master() ? local_state().flags : synced.flags;
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
    uint8_t         flags = 0;
#        ifdef SPLIT_KEYBOARD
    flags = shared_state();
#        endif
    bool caps   = host_keyboard_led_state().caps_lock || (flags & STATE_CAPS_WORD);
    bool audio  = flags & STATE_AUDIO;
    bool clicky = flags & STATE_CLICKY;
    bool rgb    = false;
#        ifdef RGB_MATRIX_ENABLE
    rgb = rgb_matrix_is_enabled();
#        endif
    uint32_t state = layer | (caps << 8) | (audio << 9) | (clicky << 10) | (rgb << 11);
    if (state == shown) {
        return;
    }
    shown = state;
    for (uint8_t line = 0; line < LINES; line++) {
        blank_line(line);
    }
    oled_write_big(0, name);
    // four icon slots along the bottom: caps, audio, click, RGB
    if (caps) {
        draw_icon(8, LINES - 2, icon_caps);
    }
    if (audio) {
        draw_icon(40, LINES - 2, icon_audio);
    }
    if (clicky) {
        draw_icon(72, LINES - 2, icon_clicky);
    }
    if (rgb) {
        draw_icon(104, LINES - 2, icon_rgb);
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
