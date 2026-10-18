/* SPDX-License-Identifier: GPL-3.0-only */
/* Dinghy: the smallest FM-1 firmware to build your own on. A sine wave on the keys, and that is all.
 * This is the file to change. Everything below it (platform.c) is done: updates, the USB rescue,
 * USB-MIDI, USB audio, keys, knobs, LEDs, the screen, a settings sector.
 *
 *   keys       play sines, 4 voices; the notes go out over USB-MIDI too
 *   OCT- OCT+  the octave (kept in flash)
 *   MASTER     the volume (platform.c)
 *   USB-MIDI   notes in play the sines, with their velocity
 *
 * Rules of the road (the chip has no FPU and no libgcc): integers only, no float, no 64-bit division.
 * tests/host.c runs this file on a computer: the screen as a PNG, the sound as a WAV.
 * What platform.c offers: docs/API.md. The chip's rules: docs/RULES.md. */
#include "platform.c"

/* ---- EXTRATONE CORE: sample-free digital FM kick */
#include "extratone_kick.h"
static ext_kick_t kick;
static ext_settings_t params = {0, 340, 55, 8, 10, 7600, 55};
static uint32_t last_note = 255;
static void app_audio(int32_t *out, uint32_t n) { ext_audio(&kick, &params, out, n); }
static void play(uint32_t note, uint32_t vel)
{
    if (note > 127u || !vel) return;
    last_note = note;
    ext_trigger(&kick, &params);
}
static void stop(uint32_t note) { (void)note; }
static int quiet(void) { return kick.amplitude_q15 == 0; }

/* ---- the screen: the name, the last note big, the keyboard with what sounds, the octave */
#define C_ACC RGB(180, 140, 255)                /* lilac, as the logo */
#define C_KEY RGB(210, 210, 210)
#define C_OFF RGB(90, 90, 90)
#define KEY0 53u                                /* key 0 = F3 */
static int black(uint32_t note) { return (0x54Au >> (note % 12u)) & 1u; }

static void draw_note(void)
{
    static const char *const NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    char s[8] = "-";
    if (last_note < 128u) {
        const char *p = NAME[last_note % 12u];
        uint32_t i = 0;
        while (*p) s[i++] = *p++;
        if (last_note >= 12u) s[i++] = (char)('0' + last_note / 12u - 1u);
        s[i] = 0;
    }
    draw_text_box(0, 82, 240, &FONT_L, s, C_WHITE, 1);
}

static void draw_keys(uint32_t lit)             /* bit k: key k sounds */
{
    uint32_t k, w = 0;
    cv_begin(240, 56, C_BLACK);
    for (k = 0; k < 27u; k++)                   /* 16 white keys of 14 px from x = 8 */
        if (!black(KEY0 + k)) {
            cv_rect(8 + (int32_t)w * 14, 0, 13, 56, lit >> k & 1u ? C_ACC : C_KEY);
            w++;
        }
    for (k = 0, w = 0; k < 27u; k++)            /* the black ones on the line between two white */
        if (black(KEY0 + k))
            cv_rect(8 + (int32_t)w * 14 - 5, 0, 9, 34, lit >> k & 1u ? C_ACC : C_BLACK);
        else
            w++;
    cv_blit(0, 140);
}

static void draw_status(int32_t oct)
{
    char s[12] = "OCTAVE +0";
    s[7] = oct < 0 ? '-' : '+';
    s[8] = (char)('0' + (oct < 0 ? -oct : oct));
    draw_text_box(0, 212, 240, &FONT_S, s, C_OFF, 1);
}

/* All CC on channel 1. No host computer needed for internal panel control. */
static void cc(uint32_t number, uint32_t value)
{
    if (number == 20u) params.rate_hz = value * 150u / 127u;
    else if (number == 21u) params.start_hz = 80u + value * 820u / 127u;
    else if (number == 22u) params.end_hz = 30u + value * 120u / 127u;
    else if (number == 23u) params.sweep_shift = 6u + value * 6u / 127u;
    else if (number == 24u) params.decay_shift = 8u + value * 4u / 127u;
    else if (number == 25u) params.drive = value;
    else if (number == 26u) params.fm_depth = value * 16000u / 127u;
}
static uint32_t adjust(uint32_t x, int32_t delta, uint32_t lo, uint32_t hi)
{
    int32_t y = (int32_t)x + delta;
    return (uint32_t)(y < (int32_t)lo ? (int32_t)lo : y > (int32_t)hi ? (int32_t)hi : y);
}
static void app_main(void)
{
    struct { int8_t oct; } set = {0};           /* what survives a power-off (add yours, up to 256 bytes) */
    uint32_t prev = 0, k, t_change = 0, dirty = 0, lit_drawn = ~0u, note_drawn = ~0u;
    int32_t oct_drawn = 99;
    uint8_t held[27] = {0};                     /* the note each key plays */
    settings_load(&set, sizeof set);
    if (set.oct < -2 || set.oct > 2) set.oct = 0;

    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_L, APP_NAME, C_ACC, 1);
    draw_text_box(0, 52, 240, &FONT_S, "FM KICK CORE 0.1", C_OFF, 1);

    for (;;) {
        uint32_t keys, down, up, rel, b, lit = 0, base = (uint32_t)(KEY0 + 12 * set.oct);
        uint8_t st, d1, d2;
        platform_poll();

        keys = fm1_in.notes;                    /* bit k: key k (0 = F3 .. 26 = G5) */
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        for (k = 0; k < 27u; k++) {                 /* (a key stops the note it started: the octave may change meanwhile) */
            if (down >> k & 1u) { held[k] = (uint8_t)(base + k); play(held[k], 100); midi_send(0x90, held[k], 100); }
            if (up >> k & 1u) { stop(held[k]); midi_send(0x80, held[k], 0); }
        }

        b = fm1_input_edges(&rel);              /* buttons pressed since the last call (bit = button id) */
        if (b & (1u << BTN_OCT_DOWN) && set.oct > -2) { set.oct--; dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && set.oct < 2) { set.oct++; dirty = 1; t_change = fm1_ms; }

        while (midi_get(&st, &d1, &d2)) {       /* USB-MIDI in, any channel */
            if ((st & 0xF0u) == 0x90u && d2) play(d1, d2);
            else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) stop(d1);
            else if ((st & 0xF0u) == 0xB0u && (st & 0x0Fu) == 0u) cc(d1, d2);
        }

        params.rate_hz = adjust(params.rate_hz, fm1_enc_take(ENC_KNOB1), 0u, 150u);
        params.start_hz = adjust(params.start_hz, 5 * fm1_enc_take(ENC_KNOB2), 80u, 900u);
        params.fm_depth = adjust(params.fm_depth, 125 * fm1_enc_take(ENC_KNOB3), 0u, 16000u);
        params.drive = adjust(params.drive, fm1_enc_take(ENC_KNOB4), 0u, 127u);
        base = (uint32_t)(KEY0 + 12 * set.oct);
        lit = keys;  /* LED reflects the physical keys currently pressed */
        for (k = 0; k < 27u; k++)
            fm1_led_key(KEY_LED(k), lit >> k & 1u);
        fm1_led_key(BTN_OCT_DOWN, set.oct < 0);
        fm1_led_key(BTN_OCT_UP, set.oct > 0);

        if (lit != lit_drawn) { lit_drawn = lit; draw_keys(lit); }   /* redraw only what changed */
        if (last_note != note_drawn) { note_drawn = last_note; draw_note(); }
        if (set.oct != oct_drawn) { oct_drawn = set.oct; draw_status(set.oct); }

        if (dirty && quiet() && fm1_ms - t_change > 2000u) {   /* 2 s after the change, when silent (an erase stalls the sound) */
            settings_save(&set, sizeof set);
            dirty = 0;
        }
    }
}
