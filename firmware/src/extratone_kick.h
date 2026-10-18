/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.1: integer-only two-oscillator FM/phase-modulation kick.
 * No samples, no floating point, no allocation, no 64-bit division.
 */
#ifndef EXTRATONE_KICK_H
#define EXTRATONE_KICK_H
#include <stdint.h>
#define EXT_FS 44100u

typedef struct {
    uint32_t rate_hz;       /* 0 (one-shot) or 1..150 Hz; independent of musical BPM */
    uint32_t start_hz;      /* 80..900 Hz */
    uint32_t end_hz;        /* 30..150 Hz */
    uint32_t sweep_shift;   /* 6..12: smaller = faster downward pitch sweep */
    uint32_t decay_shift;   /* 8..12: smaller = shorter sound */
    uint32_t fm_depth;      /* 0..16000, modulator/carrier ratio = 2:1 */
    uint32_t drive;         /* 0..127 */
} ext_settings_t;

typedef struct {
    uint32_t carrier_phase, mod_phase;
    uint32_t pitch_q8, amplitude_q15;
    uint32_t retrigger_clock_q16;
    uint32_t triggers;
} ext_kick_t;

static int32_t ext_sine(uint32_t phase)
{
    int32_t x = (int32_t)phase >> 16;
    int32_t ax = x < 0 ? -x : x;
    int32_t y = (x * (32768 - ax)) >> 13;
    int32_t ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

static void ext_trigger(ext_kick_t *s, const ext_settings_t *p)
{
    s->carrier_phase = 0;
    s->mod_phase = 0;
    s->pitch_q8 = p->start_hz << 8;
    s->amplitude_q15 = 28000;
    s->triggers++;
}

static int32_t ext_tick(ext_kick_t *s, const ext_settings_t *p)
{
    uint32_t target, difference, fall, step;
    int32_t mod, carrier, sample;
    if (p->rate_hz) {
        uint32_t rate = p->rate_hz > 150u ? 150u : p->rate_hz;
        uint32_t period_q16 = (EXT_FS << 16) / rate;
        s->retrigger_clock_q16 += 65536u;
        if (s->retrigger_clock_q16 >= period_q16) {
            s->retrigger_clock_q16 -= period_q16;
            ext_trigger(s, p);
        }
    } else {
        s->retrigger_clock_q16 = 0;
    }
    if (!s->amplitude_q15)
        return 0;

    target = p->end_hz << 8;
    if (s->pitch_q8 > target) {
        difference = s->pitch_q8 - target;
        fall = difference >> p->sweep_shift;
        if (!fall) fall = 1;
        s->pitch_q8 -= fall;
    } else {
        s->pitch_q8 = target;
    }
    /* 2^32 / (44100 * 256) ~= 380.435, no 64-bit arithmetic per sample. */
    step = s->pitch_q8 * 380u;
    s->carrier_phase += step;
    s->mod_phase += step * 2u;
    mod = ext_sine(s->mod_phase);
    /* Phase modulation: 2x sine modulator, integer modulation index. */
    carrier = ext_sine(s->carrier_phase + (uint32_t)(mod * (int32_t)p->fm_depth * 2));
    sample = (carrier * (int32_t)s->amplitude_q15) >> 15;
    sample = (sample * (int32_t)(256u + p->drive * 12u)) >> 8;
    if (sample > 30000) sample = 30000;
    if (sample < -30000) sample = -30000;

    fall = s->amplitude_q15 >> p->decay_shift;
    if (!fall) fall = 1;
    s->amplitude_q15 = s->amplitude_q15 > fall ? s->amplitude_q15 - fall : 0;
    return sample;
}

static void ext_audio(ext_kick_t *s, const ext_settings_t *p, int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; ++i) {
        int32_t sample = ext_tick(s, p);
        out[2u * i] += sample;
        out[2u * i + 1u] += sample;
    }
}
#endif
