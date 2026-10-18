/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.7 for M-VAVE FM-1.
 * All kick sound is synthesized in real-time: no samples.
 * The original Dinghy platform remains unchanged.
 */
#define APP_NAME "EXTRATONE"
#include "platform.c"
#include "extratone_kick.h"
#include "extratone_presets.h"

#define KEY0 53u
#define C_ACC RGB(100, 245, 165)
#define C_MUT RGB(110, 120, 130)

static ext_kick_t kick;
static ext_settings_t p;
static uint32_t active_preset;
static uint32_t preset_edited;
static void app_audio(int32_t *out, uint32_t n) { ext_audio(&kick, &p, out, n); }

static uint32_t bounded_add(uint32_t v, int32_t delta, uint32_t min, uint32_t max)
{
    int32_t n = (int32_t)v + delta;
    if (n < (int32_t)min) n = (int32_t)min;
    if (n > (int32_t)max) n = (int32_t)max;
    return (uint32_t)n;
}

static void load_preset(uint32_t ix)
{
    if (ix >= EXT_PRESET_COUNT) ix = EXT_PRESET_COUNT - 1u;
    active_preset = ix;
    p = EXT_PRESETS[ix].settings;
    p.roll_on = 0u; /* preset switching stops automatic retrigger */
    preset_edited = 0u;
    ext_recalc(&p);
    kick = (ext_kick_t){0}; /* no old envelope/filter state leaks into new preset */
}

/* MIDI CCs, channel 1, for Faderfox EC4 and Ableton. */
static int set_cc(uint32_t cc, uint32_t val)
{
    if (cc == 51u) {
        load_preset(val * (EXT_PRESET_COUNT - 1u) / 127u);
        return 1;
    }
    if (cc == 20u) p.rate_hz = val * 150u / 127u;
    else if (cc == 21u) p.start_hz = 80u + val * 5920u / 127u;
    else if (cc == 22u) p.end_hz = 20u + val * 240u / 127u;
    else if (cc == 23u) p.sweep_ms = 3u + val * 347u / 127u;
    else if (cc == 24u) p.decay_ms = 20u + val * 1180u / 127u;
    else if (cc == 25u) p.drive = val;
    else if (cc == 26u) p.fm_depth = val;
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
    else if (cc == 38u) p.filter_route = val >= 64u;
    else if (cc == 39u) p.shape_a = val;
    else if (cc == 40u) p.shape_b = val;
    else if (cc == 41u) p.level_b = val;
    else if (cc == 42u) p.fm_feedback = val;
    else if (cc == 43u) p.level_a = val;
    else if (cc == 44u) p.output_trim = val;
    else if (cc == 45u) p.b_mode = val < 43u ? 0u : val < 86u ? 1u : 2u;
    else if (cc == 46u) p.b_detune_cents = (int32_t)(val * 200u / 127u) - 100;
    else if (cc == 47u) p.pitch_curve = val;
    else if (cc == 48u) p.amp_attack_ms = val * 100u / 127u;
    else if (cc == 49u) p.amp_sustain = val;
    else if (cc == 50u) p.amp_release_ms = 5u + val * 995u / 127u;
    else return 0;
    ext_recalc(&p);
    preset_edited = 1u;
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
static const char *shape_name(uint32_t sh)
{
    if (sh < 32u) return "SINE-TRI";
    if (sh < 64u) return "TRI-SAW";
    if (sh < 96u) return "SAW-SQR";
    return "SQR-PWM";
}
static const char *b_mode_name(uint32_t mode)
{
    return mode == 0u ? "LAYER: A+B"
         : mode == 1u ? "MOD: B TO A FM"
                      : "BOTH: FM + B";
}
static void show_detune(uint32_t y, int32_t cents)
{
    char buf[] = "B DETUNE +000 CT";
    uint32_t abs_cents = (uint32_t)(cents < 0 ? -cents : cents);
    buf[9] = cents < 0 ? '-' : '+';
    buf[10] = (char)('0' + (abs_cents / 100u));
    buf[11] = (char)('0' + ((abs_cents / 10u) % 10u));
    buf[12] = (char)('0' + (abs_cents % 10u));
    draw_text_box(8, y, 224, &FONT_S, buf, C_WHITE, 0);
}
static void show_preset(void)
{
    char buf[36];
    const char *name = EXT_PRESETS[active_preset].name;
    uint32_t i = 0u;
    buf[i++] = 'P';
    buf[i++] = (char)('1' + active_preset);
    buf[i++] = ' ';
    while (*name && i < sizeof(buf) - 3u) buf[i++] = *name++;
    if (preset_edited) { buf[i++] = ' '; buf[i++] = '*'; }
    buf[i] = 0;
    draw_text_box(0, 47, 240, &FONT_S, buf, C_MUT, 1);
}
static void show_panel(uint32_t page)
{
    const char *label = page == 0u ? "MAIN"
                      : page == 1u ? "PITCH ENVELOPE"
                      : page == 2u ? "FM ENGINE"
                      : page == 3u ? "LP24 FILTER"
                      : page == 4u ? "OSCILLATORS"
                      : page == 5u ? "A/B/SUB MIXER"
                                   : "AMP ADSR ENVELOPE";
    show_preset();
    draw_text_box(0, 63, 240, &FONT_S, label, C_ACC, 1);
    if (page == 0u) {
        show_value(92, "RATE", p.rate_hz, "HZ");
        show_tune();
        show_value(144, "CLICK", p.click_amount, "");
        show_value(170, "DRIVE", p.drive, "");
    } else if (page == 1u) {
        show_value(92, "P START", p.start_hz, "HZ");
        show_value(118, "P END", p.end_hz, "HZ");
        show_value(144, "P SWEEP", p.sweep_ms, "MS");
        show_value(170, "P CURVE", p.pitch_curve, "");
    } else if (page == 2u) {
        show_ratio();
        show_value(118, "INDEX", p.fm_depth, "");
        show_value(144, "FEEDBACK", p.fm_feedback, "");
        show_value(170, "FM DECAY", p.fm_decay_ms, "MS");
    } else if (page == 3u) {
        show_value(92, "CUTOFF", EXT_LP24_HZ[p.filter_cutoff], "HZ");
        show_value(118, "RESONANCE", p.filter_res, "");
        show_value(144, "ENV AMT", p.filter_env_amt, "");
        show_value(170, "ENV TIME", p.filter_env_ms, "MS");
    } else if (page == 4u) {
        show_value(92, "SHAPE A", p.shape_a, "");
        show_value(118, "SHAPE B", p.shape_b, "");
        draw_text_box(8, 144, 224, &FONT_S, b_mode_name(p.b_mode), C_WHITE, 0);
        show_detune(170, p.b_detune_cents);
    } else if (page == 5u) {
        show_value(92, "LEVEL A", p.level_a, "");
        show_value(118, "LEVEL B", p.level_b, "");
        show_value(144, "SUB LEVEL", p.sub_body, "");
        show_value(170, "OUT TRIM", p.output_trim, "");
    } else {
        show_value(92, "A ATTACK", p.amp_attack_ms, "MS");
        show_value(118, "D DECAY", p.decay_ms, "MS");
        show_value(144, "S SUSTAIN", p.amp_sustain, "");
        show_value(170, "R RELEASE", p.amp_release_ms, "MS");
    }
    draw_text_box(0, 198, 240, &FONT_S,
                  page == 3u ? (p.filter_bypass ? "LP24 BYPASS" : p.filter_route ? "LP24 SUB SAFE" : "LP24 FULL SERIAL")
                             : page == 1u ? "CURVE 0=EXP 127=LIN"
                             : page == 6u ? (p.amp_sustain ? "ADSR: HOLD NOTE" : "ADSR: ONE SHOT")
                             : page == 4u ? "SHAPE A / SHAPE B"
                             : b_mode_name(p.b_mode),
                  (page == 3u && p.filter_bypass) ? C_MUT : C_ACC, 1);
    if (page == 4u) {
        char line[36];
        const char *a = shape_name(p.shape_a);
        const char *b = shape_name(p.shape_b);
        uint32_t i = 0;
        line[i++] = 'A'; line[i++] = ':';
        while (*a && i < 15u) line[i++] = *a++;
        line[i++] = ' '; line[i++] = 'B'; line[i++] = ':';
        while (*b && i < sizeof(line) - 1u) line[i++] = *b++;
        line[i] = 0;
        draw_text_box(8, 198, 224, &FONT_S, line, C_ACC, 0);
    }
}

static void app_main(void)
{
    uint32_t prev_keys = 0, page = 0, last_screen = 0, dirty = 1, prev_leds = ~0u, k;
    uint8_t st, d1, d2;
    load_preset(0u);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 13, 240, &FONT_L, "EXTRATONE", C_ACC, 1);
    draw_text_box(0, 47, 240, &FONT_S, "EXTRATONE 0.7", C_MUT, 1);
    draw_text_box(0, 222, 240, &FONT_S, "PRESETS KNOB / ARP=ADSR", C_MUT, 1);
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
        if (up && !keys && !p.roll_on) ext_note_off(&kick, &p);
        if (keys != prev_leds) {
            for (k = 0; k < 27u; ++k)
                fm1_led_key(KEY_LED(k), (keys >> k) & 1u);
            prev_leds = keys;
        }

        b = fm1_input_edges(&released);
        if (b & (1u << BTN_ENV)) { page = page == 1u ? 0u : 1u; dirty = 1; }
        if (b & (1u << BTN_EDIT)) { page = page == 2u ? 0u : 2u; dirty = 1; }
        if (b & (1u << BTN_FX)) { page = page == 3u ? 0u : 3u; dirty = 1; }
        if (b & (1u << BTN_LFO)) { page = page == 4u ? 0u : 4u; dirty = 1; }
        if (b & (1u << BTN_SEQ)) { page = page == 5u ? 0u : 5u; dirty = 1; }
        if (b & (1u << BTN_ARP)) { page = page == 6u ? 0u : 6u; dirty = 1; }
        if (b & (1u << BTN_HOME)) { page = 0u; dirty = 1; }
        if (b & (1u << BTN_GLO)) { p.filter_bypass ^= 1u; dirty = 1; }
        if (b & (1u << BTN_SEL)) { p.filter_route ^= 1u; dirty = 1; }
        if (b & (1u << BTN_PLAY)) {
            p.roll_on ^= 1u;
            if (p.roll_on && p.rate_hz) ext_trigger(&kick, &p);
            if (!p.roll_on && !keys) ext_note_off(&kick, &p);
            dirty = 1;
        }
        fm1_led_key(BTN_ENV, page == 1u);
        fm1_led_key(BTN_EDIT, page == 2u);
        fm1_led_key(BTN_FX, page == 3u);
        fm1_led_key(BTN_LFO, page == 4u);
        fm1_led_key(BTN_SEQ, page == 5u);
        fm1_led_key(BTN_ARP, page == 6u);
        fm1_led_key(BTN_GLO, p.filter_bypass == 0u);
        fm1_led_key(BTN_SEL, p.filter_route != 0u);
        fm1_led_key(BTN_PLAY, p.roll_on != 0u);
        {
            int32_t shift = fm1_enc_take(ENC_PRESETS);
            if (shift) {
                uint32_t ix = bounded_add(active_preset, shift, 0u, EXT_PRESET_COUNT - 1u);
                if (ix != active_preset) {
                    load_preset(ix);
                    dirty = 1u;
                }
            }
        }
        for (k = 0; k < 4u; ++k) d[k] = fm1_enc_take(ENC_KNOB1 + k);
        if (d[0] || d[1] || d[2] || d[3]) {
            if (page == 0u) {
                p.rate_hz = bounded_add(p.rate_hz, d[0], 0u, 150u);
                p.tune_semitones = (int32_t)bounded_add((uint32_t)(p.tune_semitones + 24),
                                                        d[1], 0u, 48u) - 24;
                p.click_amount = bounded_add(p.click_amount, d[2] * 2, 0u, 127u);
                p.drive = bounded_add(p.drive, d[3] * 2, 0u, 127u);
            } else if (page == 1u) {
                p.start_hz = bounded_add(p.start_hz, d[0] * 80, 80u, 6000u);
                p.end_hz = bounded_add(p.end_hz, d[1] * 2, 20u, 260u);
                p.sweep_ms = bounded_add(p.sweep_ms, d[2] * 4, 3u, 350u);
                p.pitch_curve = bounded_add(p.pitch_curve, d[3] * 2, 0u, 127u);
            } else if (page == 2u) {
                p.fm_ratio_x2 = bounded_add(p.fm_ratio_x2, d[0], 1u, 16u);
                p.fm_depth = bounded_add(p.fm_depth, d[1] * 2, 0u, 127u);
                p.fm_feedback = bounded_add(p.fm_feedback, d[2] * 2, 0u, 127u);
                p.fm_decay_ms = bounded_add(p.fm_decay_ms, d[3] * 5, 5u, 500u);
            } else if (page == 4u) {
                p.shape_a = bounded_add(p.shape_a, d[0], 0u, 127u);
                p.shape_b = bounded_add(p.shape_b, d[1], 0u, 127u);
                p.b_mode = bounded_add(p.b_mode, d[2], 0u, 2u);
                p.b_detune_cents = (int32_t)bounded_add((uint32_t)(p.b_detune_cents + 100),
                                                       d[3] * 2, 0u, 200u) - 100;
            } else if (page == 5u) {
                p.level_a = bounded_add(p.level_a, d[0] * 2, 0u, 127u);
                p.level_b = bounded_add(p.level_b, d[1] * 2, 0u, 127u);
                p.sub_body = bounded_add(p.sub_body, d[2] * 2, 0u, 127u);
                p.output_trim = bounded_add(p.output_trim, d[3] * 2, 0u, 127u);
            } else if (page == 6u) {
                p.amp_attack_ms = bounded_add(p.amp_attack_ms, d[0], 0u, 100u);
                p.decay_ms = bounded_add(p.decay_ms, d[1] * 10, 10u, 1200u);
                p.amp_sustain = bounded_add(p.amp_sustain, d[2] * 2, 0u, 127u);
                p.amp_release_ms = bounded_add(p.amp_release_ms, d[3] * 10, 5u, 1000u);
            }
            if (page == 3u) {
                p.filter_cutoff = bounded_add(p.filter_cutoff, d[0], 0u, 127u);
                p.filter_res = bounded_add(p.filter_res, d[1], 0u, 127u);
                p.filter_env_amt = bounded_add(p.filter_env_amt, d[2], 0u, 127u);
                p.filter_env_ms = bounded_add(p.filter_env_ms, d[3] * 10, 5u, 1500u);
            }
            ext_recalc(&p);
            preset_edited = 1u;
            dirty = 1;
        }

        while (midi_get(&st, &d1, &d2)) {
            if ((st & 0xF0u) == 0x90u && d2) ext_trigger(&kick, &p);
            else if (((st & 0xF0u) == 0x80u ||
                     ((st & 0xF0u) == 0x90u && !d2)) && !p.roll_on)
                ext_note_off(&kick, &p);
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
