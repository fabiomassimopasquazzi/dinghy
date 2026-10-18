/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.4 for M-VAVE FM-1.
 * All kick sound is synthesized in real-time: no samples.
 * The original Dinghy platform remains unchanged.
 */
#define APP_NAME "EXTRATONE"
#include "platform.c"
#include "extratone_kick.h"

#define KEY0 53u
#define C_ACC RGB(100, 245, 165)
#define C_MUT RGB(110, 120, 130)

static ext_kick_t kick;
static ext_settings_t p = {
    38u, 2300u, 48u, 35u, 175u, 7500u, 35u,
    0u, 0u, 0u, 0u, 0, 0u, 0u,
    4u, 90u, 45u, 70u, 0u,
    78u, 48u, 75u, 90u, 0u, 0u, 0u, 0u
};
static void app_audio(int32_t *out, uint32_t n) { ext_audio(&kick, &p, out, n); }

static uint32_t bounded_add(uint32_t v, int32_t delta, uint32_t min, uint32_t max)
{
    int32_t n = (int32_t)v + delta;
    if (n < (int32_t)min) n = (int32_t)min;
    if (n > (int32_t)max) n = (int32_t)max;
    return (uint32_t)n;
}

/* Seven fixed CCs: compatible with EC4, with CCs on MIDI channel 1. */
static int set_cc(uint32_t cc, uint32_t val)
{
    if (cc == 20u) p.rate_hz = val * 150u / 127u;
    else if (cc == 21u) p.start_hz = 80u + val * 5920u / 127u;
    else if (cc == 22u) p.end_hz = 20u + val * 240u / 127u;
    else if (cc == 23u) p.sweep_ms = 3u + val * 347u / 127u;
    else if (cc == 24u) p.decay_ms = 20u + val * 1180u / 127u;
    else if (cc == 25u) p.drive = val;
    else if (cc == 26u) p.fm_depth = val * 20000u / 127u;
    else if (cc == 27u) p.roll_on = val >= 64u;
    else if (cc == 28u) p.tune_semitones = (int32_t)(val * 48u / 127u) - 24;
    else if (cc == 29u) p.fm_ratio_x2 = 1u + val * 15u / 127u;
    else if (cc == 30u) p.fm_decay_ms = 5u + val * 495u / 127u;
    else if (cc == 31u) p.click_amount = val;
    else if (cc == 32u) p.sub_body = val;
    else if (cc == 33u) p.filter_cutoff = val;
    else if (cc == 34u) p.filter_res = val;
    else if (cc == 35u) p.filter_env_amt = val;
    else if (cc == 36u) p.filter_env_ms = 5u + val * 1495u / 127u;
    else if (cc == 37u) p.filter_bypass = val >= 64u;
    else return 0;
    ext_recalc(&p);
    return 1;
}

/* No printf / libc dependencies on target. */
static void show_value(uint32_t y, const char *label, uint32_t v, const char *unit)
{
    char buf[36], dig[12];
    uint32_t i = 0, k = 0;
    while (*label && i < 15u) buf[i++] = *label++;
    buf[i++] = ' ';
    do { dig[k++] = (char)('0' + v % 10u); v /= 10u; } while (v && k < 10u);
    while (k) buf[i++] = dig[--k];
    if (*unit) { buf[i++] = ' '; while (*unit && i < sizeof(buf) - 1u) buf[i++] = *unit++; }
    buf[i] = 0;
    draw_text_box(8, y, 224, &FONT_S, buf, C_WHITE, 0);
}
static void show_tune(void)
{
    char s[] = "TUNE +00 ST";
    int32_t t = p.tune_semitones;
    uint32_t abs_t = (uint32_t)(t < 0 ? -t : t);
    s[5] = t < 0 ? '-' : '+';
    s[6] = (char)('0' + abs_t / 10u);
    s[7] = (char)('0' + abs_t % 10u);
    draw_text_box(8, 118, 224, &FONT_S, s, C_WHITE, 0);
}
static void show_ratio(void)
{
    char s[] = "FM RATIO 0.0";
    s[9] = (char)('0' + p.fm_ratio_x2 / 2u);
    s[11] = p.fm_ratio_x2 & 1u ? '5' : '0';
    draw_text_box(8, 92, 224, &FONT_S, s, C_WHITE, 0);
}
static void show_panel(uint32_t page)
{
    const char *label = page == 0u ? "MAIN - PERFORMANCE"
                      : page == 1u ? "ENV - PITCH AND DECAY"
                      : page == 2u ? "TIMBRE - FM ENGINE"
                                   : "LP24 - RESONANT FILTER";
    draw_text_box(0, 63, 240, &FONT_S, label, C_ACC, 1);
    if (page == 0u) {
        show_value(92, "RATE", p.rate_hz, "HZ");
        show_tune();
        show_value(144, "FM", p.fm_depth, "");
        show_value(170, "DRIVE", p.drive, "");
    } else if (page == 1u) {
        show_value(92, "START", p.start_hz, "HZ");
        show_value(118, "END", p.end_hz, "HZ");
        show_value(144, "SWEEP", p.sweep_ms, "MS");
        show_value(170, "DECAY", p.decay_ms, "MS");
    } else if (page == 2u) {
        show_ratio();
        show_value(118, "FM DECAY", p.fm_decay_ms, "MS");
        show_value(144, "CLICK", p.click_amount, "");
        show_value(170, "SUB BODY", p.sub_body, "");
    } else {
        show_value(92, "CUTOFF", EXT_LP24_HZ[p.filter_cutoff], "HZ");
        show_value(118, "RESONANCE", p.filter_res, "");
        show_value(144, "ENV AMT", p.filter_env_amt, "");
        show_value(170, "ENV TIME", p.filter_env_ms, "MS");
    }
    draw_text_box(0, 198, 240, &FONT_S,
                  page == 3u ? (p.filter_bypass ? "LP24 BYPASS" : "LP24 ON  SUB SAFE")
                             : (p.roll_on ? "ROLL ON" : "ROLL OFF"),
                  (page == 3u && p.filter_bypass) ? C_MUT : C_ACC, 1);
}

static void app_main(void)
{
    uint32_t prev_keys = 0, page = 0, last_screen = 0, dirty = 1, prev_leds = ~0u, k;
    uint8_t st, d1, d2;
    ext_recalc(&p);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 13, 240, &FONT_L, "EXTRATONE", C_ACC, 1);
    draw_text_box(0, 47, 240, &FONT_S, "FM KICK 0.4", C_MUT, 1);
    draw_text_box(0, 222, 240, &FONT_S, "FX FILTER  GLO BYPASS", C_MUT, 1);
    show_panel(page);
    for (;;) {
        uint32_t keys, down, up, b, released;
        int32_t d[4];
        platform_poll();
        keys = fm1_in.notes;
        down = keys & ~prev_keys;
        up = prev_keys & ~keys;
        prev_keys = keys;
        for (k = 0; k < 27u; ++k) {
            if ((down >> k) & 1u) {
                ext_trigger(&kick, &p);
                midi_send(0x90, KEY0 + k, 100);
            }
            if ((up >> k) & 1u) midi_send(0x80, KEY0 + k, 0);
        }
        if (keys != prev_leds) {
            for (k = 0; k < 27u; ++k)
                fm1_led_key(KEY_LED(k), (keys >> k) & 1u);
            prev_leds = keys;
        }

        b = fm1_input_edges(&released);
        if (b & (1u << BTN_ENV)) { page = page == 1u ? 0u : 1u; dirty = 1; }
        if (b & (1u << BTN_EDIT)) { page = page == 2u ? 0u : 2u; dirty = 1; }
        if (b & (1u << BTN_FX)) { page = page == 3u ? 0u : 3u; dirty = 1; }
        if (b & (1u << BTN_GLO)) { p.filter_bypass ^= 1u; dirty = 1; }
        if (b & (1u << BTN_PLAY)) {
            p.roll_on ^= 1u;
            if (p.roll_on && p.rate_hz) ext_trigger(&kick, &p);
            dirty = 1;
        }
        fm1_led_key(BTN_ENV, page == 1u);
        fm1_led_key(BTN_EDIT, page == 2u);
        fm1_led_key(BTN_FX, page == 3u);
        fm1_led_key(BTN_GLO, p.filter_bypass == 0u);
        fm1_led_key(BTN_PLAY, p.roll_on != 0u);
        for (k = 0; k < 4u; ++k) d[k] = fm1_enc_take(ENC_KNOB1 + k);
        if (d[0] || d[1] || d[2] || d[3]) {
            if (page == 0u) {
                p.rate_hz = bounded_add(p.rate_hz, d[0], 0u, 150u);
                p.tune_semitones = (int32_t)bounded_add((uint32_t)(p.tune_semitones + 24),
                                                        d[1], 0u, 48u) - 24;
                p.fm_depth = bounded_add(p.fm_depth, d[2] * 250, 0u, 20000u);
                p.drive = bounded_add(p.drive, d[3] * 2, 0u, 127u);
            } else if (page == 1u) {
                p.start_hz = bounded_add(p.start_hz, d[0] * 80, 80u, 6000u);
                p.end_hz = bounded_add(p.end_hz, d[1] * 2, 20u, 260u);
                p.sweep_ms = bounded_add(p.sweep_ms, d[2] * 4, 3u, 350u);
                p.decay_ms = bounded_add(p.decay_ms, d[3] * 10, 20u, 1200u);
            } else {
                p.fm_ratio_x2 = bounded_add(p.fm_ratio_x2, d[0], 1u, 16u);
                p.fm_decay_ms = bounded_add(p.fm_decay_ms, d[1] * 5, 5u, 500u);
                p.click_amount = bounded_add(p.click_amount, d[2] * 2, 0u, 127u);
                p.sub_body = bounded_add(p.sub_body, d[3] * 2, 0u, 127u);
            }
            if (page == 3u) {
                p.filter_cutoff = bounded_add(p.filter_cutoff, d[0], 0u, 127u);
                p.filter_res = bounded_add(p.filter_res, d[1], 0u, 127u);
                p.filter_env_amt = bounded_add(p.filter_env_amt, d[2], 0u, 127u);
                p.filter_env_ms = bounded_add(p.filter_env_ms, d[3] * 10, 5u, 1500u);
            }
            ext_recalc(&p);
            dirty = 1;
        }

        while (midi_get(&st, &d1, &d2)) {
            if ((st & 0xF0u) == 0x90u && d2) ext_trigger(&kick, &p);
            else if ((st & 0xF0u) == 0xB0u && (st & 15u) == 0u)
                if (set_cc(d1, d2)) dirty = 1;
        }
        if (dirty && fm1_ms - last_screen >= 55u) {
            show_panel(page);
            last_screen = fm1_ms;
            dirty = 0;
        }
    }
}
