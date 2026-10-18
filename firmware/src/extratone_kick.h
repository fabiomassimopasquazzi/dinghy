/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 0.4.1: serial LP24 routing, optional sub-preserving split.
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
    uint32_t filter_cutoff;  /* 0..127 logarithmic frequency 30..12000 Hz */
    uint32_t filter_res;     /* 0..127 feedback amount */
    uint32_t filter_env_amt; /* 0..127 peak cutoff modulation */
    uint32_t filter_env_ms;  /* 5..1500 ms */
    uint32_t filter_bypass;  /* 0 = LP24 enabled, 1 = compare without it */
    uint32_t filter_span;    /* cached envelope span in LUT indices */
    uint32_t filter_res_q12; /* cached nonlinear feedback gain */
    uint32_t filter_env_coef;/* cached decay, Q16 */
    uint32_t filter_route;   /* 0 = FULL SERIAL (all audio), 1 = SUB SAFE parallel sub */
} ext_settings_t;

typedef struct {
    uint32_t carrier_phase, mod_phase;
    uint32_t pitch_q8, amplitude_q15;
    uint32_t retrigger_clock_q16;
    uint32_t triggers;
    uint32_t fm_env_q15, click_env_q15, sub_phase, noise_state;
    int32_t lp24[4];          /* four one-pole states; zero latency, no audio buffers */
    uint32_t filter_env_q15, filter_coeff_q15, filter_frames;
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

/* 128 logarithmically spaced cutoffs, 30..12000 Hz at 44.1 kHz.
 * Values are 1-exp(-2*pi*f/fs) in Q15, computed offline.
 * Updating only once per 16 frames avoids float/division in audio ISR.
 */
/* Readable cutoff values used by LCD UI, same logarithmic table. */
static const uint16_t EXT_LP24_HZ[128] = {
    30u, 31u, 33u, 35u, 36u, 38u, 40u, 42u,
    44u, 46u, 48u, 50u, 53u, 55u, 58u, 61u,
    64u, 67u, 70u, 74u, 77u, 81u, 85u, 89u,
    93u, 98u, 102u, 107u, 112u, 118u, 124u, 129u,
    136u, 142u, 149u, 156u, 164u, 172u, 180u, 189u,
    198u, 208u, 218u, 228u, 239u, 251u, 263u, 275u,
    289u, 303u, 317u, 333u, 349u, 366u, 383u, 402u,
    421u, 442u, 463u, 485u, 509u, 533u, 559u, 586u,
    614u, 644u, 675u, 708u, 742u, 778u, 815u, 855u,
    896u, 939u, 985u, 1032u, 1082u, 1134u, 1189u, 1247u,
    1307u, 1370u, 1436u, 1505u, 1578u, 1654u, 1734u, 1818u,
    1906u, 1998u, 2095u, 2196u, 2302u, 2413u, 2530u, 2652u,
    2780u, 2914u, 3055u, 3203u, 3357u, 3519u, 3689u, 3868u,
    4055u, 4250u, 4456u, 4671u, 4897u, 5133u, 5381u, 5641u,
    5914u, 6199u, 6499u, 6813u, 7142u, 7487u, 7848u, 8228u,
    8625u, 9042u, 9478u, 9936u, 10416u, 10920u, 11447u, 12000u
};

static const uint16_t EXT_LP24_COEFF_Q15[128] = {
    140u, 146u, 154u, 161u, 169u, 177u, 185u, 194u,
    204u, 213u, 224u, 234u, 246u, 258u, 270u, 283u,
    297u, 311u, 326u, 341u, 358u, 375u, 393u, 412u,
    432u, 452u, 474u, 497u, 521u, 546u, 572u, 599u,
    628u, 658u, 689u, 722u, 757u, 793u, 830u, 870u,
    911u, 955u, 1000u, 1048u, 1098u, 1150u, 1204u, 1261u,
    1321u, 1383u, 1449u, 1517u, 1588u, 1663u, 1741u, 1823u,
    1909u, 1998u, 2091u, 2189u, 2291u, 2397u, 2509u, 2625u,
    2746u, 2873u, 3005u, 3143u, 3287u, 3437u, 3594u, 3757u,
    3927u, 4104u, 4289u, 4481u, 4682u, 4890u, 5107u, 5332u,
    5567u, 5810u, 6063u, 6326u, 6599u, 6881u, 7174u, 7478u,
    7793u, 8118u, 8455u, 8803u, 9162u, 9533u, 9916u, 10310u,
    10717u, 11134u, 11564u, 12005u, 12458u, 12922u, 13397u, 13882u,
    14379u, 14885u, 15400u, 15925u, 16458u, 16998u, 17546u, 18099u,
    18658u, 19220u, 19786u, 20354u, 20923u, 21491u, 22057u, 22621u,
    23179u, 23732u, 24277u, 24813u, 25339u, 25853u, 26354u, 26840u
};

static int32_t ext_filter_softclip(int32_t x)
{
    /* Nonlinear feedback saturation. Bounded for any resonance setting. */
    if (x > 60000) x = 60000;
    if (x < -60000) x = -60000;
    if (x > 16000) return 16000 + (x - 16000) / 4;
    if (x < -16000) return -16000 + (x + 16000) / 4;
    return x;
}

/* A resonant cascade of four one-pole low-passes (24 dB/oct asymptotically).
 * This is NOT an analog ladder circuit emulation. Saturation is within the
 * feedback path; a fixed-point bound makes high resonance safe.
 */
static int32_t ext_filter_lp24(ext_kick_t *s, const ext_settings_t *p, int32_t x)
{
    int32_t in, feedback;
    uint32_t i, g;
    if (p->filter_bypass) return x;
    if (!(s->filter_frames++ & 15u)) {
        uint32_t index = p->filter_cutoff
                       + ((p->filter_span * s->filter_env_q15) >> 15);
        if (index > 127u) index = 127u;
        s->filter_coeff_q15 = EXT_LP24_COEFF_Q15[index];
    }
    g = s->filter_coeff_q15;
    feedback = (s->lp24[3] * (int32_t)p->filter_res_q12) >> 12;
    /* More resonance = more input drive into the nonlinear feedback loop. */
    in = ext_filter_softclip(((x - feedback) * (int32_t)(256u + p->filter_res)) >> 8);
    for (i = 0u; i < 4u; ++i) {
        int32_t delta = in - s->lp24[i];
        s->lp24[i] += (int32_t)((delta * (int32_t)g) >> 15);
        in = s->lp24[i];
    }
    return s->lp24[3];
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
    if (p->filter_cutoff > 127u) p->filter_cutoff = 127u;
    if (p->filter_res > 127u) p->filter_res = 127u;
    if (p->filter_env_amt > 127u) p->filter_env_amt = 127u;
    p->filter_span = ((127u - p->filter_cutoff) * p->filter_env_amt + 63u) / 127u;
    p->filter_res_q12 = p->filter_res * 132u;  /* 0..4.09x feedback */
    p->filter_env_coef = 10268u / (p->filter_env_ms ? p->filter_env_ms : 1u);
    if (p->filter_env_coef > 32767u) p->filter_env_coef = 32767u;
}

static void ext_trigger(ext_kick_t *s, const ext_settings_t *p)
{
    s->carrier_phase = 0;
    s->mod_phase = 0;
    s->pitch_q8 = p->tuned_start_hz << 8;
    s->amplitude_q15 = 30000;
    s->fm_env_q15 = 32767u;
    s->click_env_q15 = 32767u;
    s->filter_env_q15 = 32767u;
    s->filter_frames = 0u;
    s->triggers++;
    s->noise_state ^= (s->triggers * 2654435761u);
    if (!s->noise_state) s->noise_state = 1u;
}

static int32_t ext_tick(ext_kick_t *s, const ext_settings_t *p)
{
    uint32_t target, diff, fall, step, level;
    int32_t mod, carrier, sample, gain, sub, click_signal, main_signal;
    int32_t dry_main_gain;
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
    if (!s->amplitude_q15 && s->click_env_q15 == 0u
        && s->lp24[0] == 0 && s->lp24[1] == 0
        && s->lp24[2] == 0 && s->lp24[3] == 0) return 0;

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

    /* FM body has already passed DRIVE above. The clean sine sub runs
     * independently, without restarting its phase at each trigger.
     */
    s->sub_phase += (p->tuned_end_hz << 8) * 380u;
    sub = (ext_sine(s->sub_phase) * (int32_t)s->amplitude_q15) >> 15;
    dry_main_gain = (int32_t)(127u - (p->sub_body * 3u / 4u));
    main_signal = (sample * dry_main_gain) / 127;

    /* The noise transient MUST follow the chosen filter routing too.
     * This is a synthesized click, no sample playback.
     */
    click_signal = 0;
    if (s->click_env_q15 > 4u) {
        noise = s->noise_state;
        noise ^= noise << 13;
        noise ^= noise >> 17;
        noise ^= noise << 5;
        s->noise_state = noise;
        click_signal = ((((int32_t)(noise >> 16) - 32768)
                       * (int32_t)s->click_env_q15) >> 15)
                       * (int32_t)p->click_amount / 127;
        s->click_env_q15 = (s->click_env_q15 * 31200u) >> 15;
    } else s->click_env_q15 = 0;

    /* FULL SERIAL: filter the entire kick (FM body + SUB + CLICK).
     * SUB SAFE: filter FM body + CLICK; route only the clean sine around it.
     * LP24 bypass, controlled by GLO, still bypasses ALL filtering.
     */
    if (p->filter_route == 0u) {
        sample = main_signal + (sub * (int32_t)p->sub_body) / 127 + click_signal;
        sample = ext_filter_lp24(s, p, sample);
    } else {
        sample = ext_filter_lp24(s, p, main_signal + click_signal);
        sample += (sub * (int32_t)p->sub_body) / 127;
    }
    if (sample > 30000) sample = 30000;
    if (sample < -30000) sample = -30000;

    fall = (s->filter_env_q15 * p->filter_env_coef) >> 16;
    if (!fall && s->filter_env_q15) fall = 1u;
    s->filter_env_q15 = s->filter_env_q15 > fall ? s->filter_env_q15 - fall : 0u;

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
