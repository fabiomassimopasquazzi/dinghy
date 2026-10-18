/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.2: sample-free FM kick with full pitch envelopes.
 * No samples, no floating point, no allocation, no 64-bit division.
 */
#ifndef EXTRATONE_KICK_H
#define EXTRATONE_KICK_H
#include <stdint.h>
#define EXT_FS 44100u

typedef struct {
    uint32_t rate_hz;     /* 0..150 Hz, independent of master tempo */
    uint32_t start_hz;    /* 80..6000 Hz, pitch envelope start */
    uint32_t end_hz;      /* 20..260 Hz, final fundamental */
    uint32_t sweep_ms;    /* 3..350 ms, pitch drop */
    uint32_t decay_ms;    /* 20..1200 ms, amplitude envelope */
    uint32_t fm_depth;    /* 0..20000, phase modulation by 2:1 oscillator */
    uint32_t drive;       /* 0..127, up to ~22x nonlinear pre-gain */
    uint32_t roll_on;     /* PLAY starts/stops independent retriggering */
    uint32_t period_q16;  /* precomputed, no divisions in audio ISR */
    uint32_t sweep_coef;  /* precomputed exponential envelope */
    uint32_t decay_coef;  /* precomputed amplitude envelope */
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

/* Recalculate whenever parameters change in the main/UI loop. */
static void ext_recalc(ext_settings_t *p)
{
    uint32_t r = p->rate_hz > 150u ? 150u : p->rate_hz;
    p->period_q16 = r ? (EXT_FS << 16) / r : 0u;
    p->sweep_coef = 6818u / (p->sweep_ms ? p->sweep_ms : 1u);
    if (p->sweep_coef > 2272u) p->sweep_coef = 2272u;
    p->decay_coef = 10268u / (p->decay_ms ? p->decay_ms : 1u);
    if (p->decay_coef > 32767u) p->decay_coef = 32767u;
}

static void ext_trigger(ext_kick_t *s, const ext_settings_t *p)
{
    s->carrier_phase = 0;
    s->mod_phase = 0;
    s->pitch_q8 = p->start_hz << 8;
    s->amplitude_q15 = 30000;
    s->triggers++;
}

static int32_t ext_tick(ext_kick_t *s, const ext_settings_t *p)
{
    uint32_t target, diff, fall, step, level;
    int32_t mod, carrier, sample, gain;
    if (p->roll_on && p->period_q16) {
        s->retrigger_clock_q16 += 65536u;
        if (s->retrigger_clock_q16 >= p->period_q16) {
            s->retrigger_clock_q16 -= p->period_q16;
            ext_trigger(s, p);
        }
    } else {
        s->retrigger_clock_q16 = 0;
    }
    if (!s->amplitude_q15) return 0;

    /* Sweeps reach ~99% of target difference in sweep_ms. */
    target = p->end_hz << 8;
    if (s->pitch_q8 > target) {
        diff = s->pitch_q8 - target;
        fall = ((diff >> 8) * p->sweep_coef) >> 8;
        if (!fall) fall = 1;
        s->pitch_q8 = diff > fall ? s->pitch_q8 - fall : target;
    } else if (s->pitch_q8 < target) {
        diff = target - s->pitch_q8;
        fall = ((diff >> 8) * p->sweep_coef) >> 8;
        if (!fall) fall = 1;
        s->pitch_q8 = diff > fall ? s->pitch_q8 + fall : target;
    }

    /* Approximation of 2^32 / (44100 * 256). */
    step = s->pitch_q8 * 380u;
    s->carrier_phase += step;
    s->mod_phase += step * 2u;
    mod = ext_sine(s->mod_phase);
    carrier = ext_sine(s->carrier_phase + (uint32_t)(mod * (int32_t)p->fm_depth * 2));
    sample = (carrier * (int32_t)s->amplitude_q15) >> 15;

    /* Up to ~22x pre-gain into a 30k hard clipper. */
    level = p->drive > 127u ? 127u : p->drive;
    gain = (int32_t)(256u + level * level / 3u);
    sample = (sample * gain) >> 8;
    if (sample > 30000) sample = 30000;
    if (sample < -30000) sample = -30000;

    /* Exponential amplitude decay, ~-60 dB at decay_ms. */
    fall = (s->amplitude_q15 * p->decay_coef) >> 16;
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
