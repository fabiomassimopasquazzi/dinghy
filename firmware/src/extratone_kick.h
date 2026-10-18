/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.3: sample-free FM kick with +/-24 semitone tuning.
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
    int32_t tune_semitones; /* -24..+24: whole kick frequency transposition */
    uint32_t tuned_start_hz; /* derived initial pitch in Hz */
    uint32_t tuned_end_hz;   /* derived sub fundamental in Hz */
    uint32_t fm_ratio_x2;    /* 1..16, modulator/carrier ratio = value / 2 */
    uint32_t fm_decay_ms;    /* 5..500 ms, separate FM modulation envelope */
    uint32_t click_amount;   /* 0..127, synthetic noise transient */
    uint32_t sub_body;       /* 0..127, parallel un-distorted sub oscillator */
    uint32_t fm_decay_coef;  /* precomputed envelope coefficient */
} ext_settings_t;

typedef struct {
    uint32_t carrier_phase, mod_phase;
    uint32_t pitch_q8, amplitude_q15;
    uint32_t retrigger_clock_q16;
    uint32_t triggers;
    uint32_t fm_env_q15, click_env_q15, sub_phase, noise_state;
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

/* Equal-tempered tuning ratios in Q16, indexed from -24 to +24 semitones.
 * Prepared offline: one integer multiplication per parameter change, none per audio frame.
 */
static const uint32_t EXT_TUNE_RATIO_Q16[49] = {
    16384u, 17358u, 18390u, 19484u, 20643u, 21870u, 23170u, 
    24548u, 26008u, 27554u, 29193u, 30929u, 32768u, 34716u, 
    36781u, 38968u, 41285u, 43740u, 46341u, 49097u, 52016u, 
    55109u, 58386u, 61858u, 65536u, 69433u, 73562u, 77936u, 
    82570u, 87480u, 92682u, 98193u, 104032u, 110218u, 116772u, 
    123715u, 131072u, 138866u, 147123u, 155872u, 165140u, 174960u, 
    185364u, 196386u, 208064u, 220436u, 233544u, 247431u, 262144u
};

static uint32_t ext_transpose_hz(uint32_t base_hz, int32_t semitones)
{
    uint32_t ratio, hz;
    if (semitones < -24) semitones = -24;
    if (semitones > 24) semitones = 24;
    ratio = EXT_TUNE_RATIO_Q16[(uint32_t)(semitones + 24)];
    /* base_hz <= 6000, ratio <= 4.0 Q16: fits in 32-bit multiplication */
    hz = (base_hz * ratio + 32768u) >> 16;
    /* Avoid carrier aliasing above most of the audio band at extreme settings. */
    if (hz > 14000u) hz = 14000u;
    if (hz < 1u) hz = 1u;
    return hz;
}

/* Recalculate whenever parameters change in the main/UI loop. */
static void ext_recalc(ext_settings_t *p)
{
    uint32_t r = p->rate_hz > 150u ? 150u : p->rate_hz;
    p->period_q16 = r ? (EXT_FS << 16) / r : 0u;
    p->sweep_coef = 6818u / (p->sweep_ms ? p->sweep_ms : 1u);
    if (p->sweep_coef > 2272u) p->sweep_coef = 2272u;
    p->tuned_start_hz = ext_transpose_hz(p->start_hz, p->tune_semitones);
    p->tuned_end_hz = ext_transpose_hz(p->end_hz, p->tune_semitones);
    p->decay_coef = 10268u / (p->decay_ms ? p->decay_ms : 1u);
    if (p->decay_coef > 32767u) p->decay_coef = 32767u;
    p->fm_decay_coef = 10268u / (p->fm_decay_ms ? p->fm_decay_ms : 1u);
    if (p->fm_decay_coef > 32767u) p->fm_decay_coef = 32767u;
}

static void ext_trigger(ext_kick_t *s, const ext_settings_t *p)
{
    s->carrier_phase = 0;
    s->mod_phase = 0;
    s->pitch_q8 = p->tuned_start_hz << 8;
    s->amplitude_q15 = 30000;
    s->fm_env_q15 = 32767u;
    s->click_env_q15 = 32767u;
    s->triggers++;
    s->noise_state ^= (s->triggers * 2654435761u);
    if (!s->noise_state) s->noise_state = 1u;
}

static int32_t ext_tick(ext_kick_t *s, const ext_settings_t *p)
{
    uint32_t target, diff, fall, step, level;
    int32_t mod, carrier, sample, gain, sub;
    uint32_t depth, noise;
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
    target = p->tuned_end_hz << 8;
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
    /* The phase naturally wraps at 2^32; fractional ratios in 0.5 steps. */
    s->mod_phase += (step >> 1) * p->fm_ratio_x2;
    mod = ext_sine(s->mod_phase);
    depth = (p->fm_depth * s->fm_env_q15) >> 15;
    carrier = ext_sine(s->carrier_phase + (uint32_t)(mod * (int32_t)depth * 2));
    sample = (carrier * (int32_t)s->amplitude_q15) >> 15;

    /* Up to ~22x pre-gain into a 30k hard clipper. */
    level = p->drive > 127u ? 127u : p->drive;
    gain = (int32_t)(256u + level * level / 3u);
    sample = (sample * gain) >> 8;
    if (sample > 30000) sample = 30000;
    if (sample < -30000) sample = -30000;

    /* A separate clean sub at tuned END Hz survives strong main-path drive.
       Free-running phase prevents full phase restarts on every retrigger. */
    s->sub_phase += (p->tuned_end_hz << 8) * 380u;
    sub = (ext_sine(s->sub_phase) * (int32_t)s->amplitude_q15) >> 15;
    sample = (sample * (int32_t)(127u - (p->sub_body * 3u / 4u))
            + sub * (int32_t)p->sub_body) / 127;

    /* Synthetic noise click (not a sample), with a short fixed decay. */
    if (s->click_env_q15 > 4u) {
        noise = s->noise_state;
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        s->noise_state = noise;
        sample += ((((int32_t)(noise >> 16) - 32768)
                  * (int32_t)s->click_env_q15) >> 15)
                  * (int32_t)p->click_amount / 127;
        s->click_env_q15 = (s->click_env_q15 * 31200u) >> 15;
    } else s->click_env_q15 = 0;

    if (sample > 30000) sample = 30000;
    if (sample < -30000) sample = -30000;

    fall = (s->fm_env_q15 * p->fm_decay_coef) >> 16;
    if (!fall && s->fm_env_q15) fall = 1u;
    s->fm_env_q15 = s->fm_env_q15 > fall ? s->fm_env_q15 - fall : 0u;

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
