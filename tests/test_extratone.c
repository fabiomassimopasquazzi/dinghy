/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone native tests for EXTRATONE kick tuning and LP24, no FM-1 needed.
 */
#include <stdio.h>
#include <stdint.h>
#include "../firmware/src/extratone_kick.h"
#include "../firmware/src/extratone_presets.h"

static int near(uint32_t a, uint32_t b) { return a + 1u >= b && b + 1u >= a; }
static uint64_t routing_energy(ext_settings_t p, uint32_t route, uint32_t cutoff)
{
    ext_kick_t k = {0};
    uint64_t total = 0;
    uint32_t i;
    p.rate_hz = 0u;
    p.roll_on = 0u;
    p.start_hz = 130u;
    p.end_hz = 130u;
    p.tune_semitones = 0;
    p.decay_ms = 900u;
    p.drive = 0u;
    p.fm_depth = 0u;
    p.click_amount = 0u;
    p.sub_body = 127u; /* emphasize the old parallel leakage */
    p.filter_cutoff = cutoff;
    p.filter_env_amt = 0u;
    p.filter_res = 0u;
    p.filter_bypass = 0u;
    p.filter_route = route;
    ext_recalc(&p);
    ext_trigger(&k, &p);
    for (i = 0u; i < 5500u; ++i) {
        int32_t y = ext_tick(&k, &p);
        if (i > 512u)
            total += (uint32_t)(y < 0 ? -y : y);
    }
    return total;
}

/* Tests exact wave-generation on its own and then the complete output path. */
static uint64_t shape_difference(uint32_t a, uint32_t b)
{
    uint64_t total = 0u;
    uint32_t n;
    for (n = 0u; n < 4096u; ++n) {
        uint32_t phase = n * 1048576u; /* one phase turn every 4096 samples */
        int32_t delta = ext_shape(phase, a) - ext_shape(phase, b);
        total += (uint32_t)(delta < 0 ? -delta : delta);
    }
    return total;
}
static void configure_voice(ext_settings_t *p)
{
    p->rate_hz = 0u;
    p->roll_on = 0u;
    p->start_hz = 150u;
    p->end_hz = 150u;
    p->tune_semitones = 0;
    p->decay_ms = 900u;
    p->drive = 0u;
    p->click_amount = 0u;
    p->sub_body = 0u;
    p->filter_bypass = 1u;
    p->output_trim = 127u;
    p->shape_a = 0u;
    p->shape_b = 110u;
    p->level_a = 127u;
    p->level_b = 127u;
    p->fm_ratio_x2 = 6u;
    p->fm_depth = 82u;
    p->fm_feedback = 56u;
    p->b_detune_cents = 0;
}
/* Compare actual output samples for different routing/controls. */
static uint64_t rendered_difference(ext_settings_t p, ext_settings_t q)
{
    ext_kick_t a = {0}, b = {0};
    uint64_t total = 0u;
    uint32_t i;
    ext_recalc(&p);
    ext_recalc(&q);
    ext_trigger(&a, &p);
    ext_trigger(&b, &q);
    for (i = 0u; i < 7000u; ++i) {
        int32_t x = ext_tick(&a, &p);
        int32_t y = ext_tick(&b, &q);
        int32_t d = x - y;
        if (x < -30000 || x > 30000 || y < -30000 || y > 30000) return 0u;
        total += (uint32_t)(d < 0 ? -d : d);
    }
    return total;
}
static uint64_t rendered_energy(ext_settings_t p)
{
    ext_kick_t s = {0};
    uint64_t total = 0u;
    uint32_t i;
    ext_recalc(&p);
    ext_trigger(&s, &p);
    for (i = 0u; i < 7000u; ++i) {
        int32_t x = ext_tick(&s, &p);
        total += (uint32_t)(x < 0 ? -x : x);
    }
    return total;
}

static int check_tuning(void)
{
    static const int32_t semis[] = {-24, -12, 0, 12, 24};
    static const uint32_t expected_sub[] = {10u, 20u, 40u, 80u, 160u};
    static const uint32_t expected_start[] = {250u, 500u, 1000u, 2000u, 4000u};
    ext_settings_t p = {
        0u, 1000u, 40u, 35u, 175u, 44u, 35u,
        0u, 0u, 0u, 0u, 0, 0u, 0u,
        4u, 90u, 40u, 70u, 0u,
        78u, 48u, 75u, 90u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        110u, 127u, 0u, 0, 0u, 0u, 0u, 0u, 0u
    };
    ext_kick_t k = {0};
    uint32_t i;
    for (i = 0; i < 5u; ++i) {
        p.tune_semitones = semis[i];
        ext_recalc(&p);
        if (!near(p.tuned_start_hz, expected_start[i]) || !near(p.tuned_end_hz, expected_sub[i])) {
            fprintf(stderr, "tune %+d: got START %u END %u\n",
                    (int)semis[i], p.tuned_start_hz, p.tuned_end_hz);
            return 1;
        }
        ext_trigger(&k, &p);
        if (k.pitch_q8 != p.tuned_start_hz << 8) return 2;
        (void)ext_tick(&k, &p);
    }
    p.tune_semitones = 0;
    p.end_hz = 55u;
    ext_recalc(&p);
    if (p.tuned_end_hz != 55u || p.tuned_start_hz != 1000u) return 3;
    p.tune_semitones = -12;
    ext_recalc(&p);
    if (!near(p.tuned_end_hz, 28u) || !near(p.tuned_start_hz, 500u)) return 4;
    /* Voice output should be nonzero and bounded even with very hard drive. */
    p.drive = 127u;
    p.click_amount = 127u;
    p.sub_body = 127u;
    p.fm_ratio_x2 = 16u;
    ext_recalc(&p);
    ext_trigger(&k, &p);
    for (i = 0; i < 5000u; ++i) {
        int32_t v = ext_tick(&k, &p);
        if (v < -30000 || v > 30000) return 5;
    }
    /* LP24 low/high cutoff, followed by a high-resonance stability stress test. */
    {
        ext_kick_t filt = {0};
        int32_t low_out = 0, high_out = 0;
        uint32_t j;
        p.filter_bypass = 0u;
        p.filter_res = 0u;
        p.filter_env_amt = 0u;
        p.filter_env_ms = 90u;
        p.filter_cutoff = 0u; /* ~30 Hz */
        ext_recalc(&p);
        for (j = 0u; j < 64u; ++j)
            low_out = ext_filter_lp24(&filt, &p, 12000);
        filt = (ext_kick_t){0};
        p.filter_cutoff = 127u; /* ~12000 Hz */
        ext_recalc(&p);
        for (j = 0u; j < 64u; ++j)
            high_out = ext_filter_lp24(&filt, &p, 12000);
        if (high_out < 8000 || low_out > 1000) {
            fprintf(stderr,"LP24 cutoff test failed: low=%d high=%d\n",
                    (int)low_out, (int)high_out);
            return 6;
        }
        /* Extreme feedback must remain safely bounded with nonlinear drive. */
        filt = (ext_kick_t){0};
        p.filter_cutoff = 99u;
        p.filter_res = 127u;
        ext_recalc(&p);
        for (j = 0u; j < 80000u; ++j) {
            int32_t x = (j % 137u) < 68u ? 32000 : -32000;
            int32_t y = ext_filter_lp24(&filt, &p, x);
            if (y < -30000 || y > 30000) {
                fprintf(stderr,"LP24 unstable at frame %u: %d\n",j,(int)y);
                return 7;
            }
        }
        p.filter_bypass = 1u;
        if (ext_filter_lp24(&filt, &p, 12345) != 12345) return 8;
    }
    /* Verify the actual complete signal path, not just a standalone LP stage.
     * At 30 Hz cutoff, a 130 Hz SUB MUST be strongly attenuated in FULL SERIAL;
     * the same SUB is intentionally preserved by the legacy SUB SAFE route.
     */
    {
        uint64_t full_low = routing_energy(p, 0u, 0u);
        uint64_t split_low = routing_energy(p, 1u, 0u);
        uint64_t full_open = routing_energy(p, 0u, 127u);
        if (!full_low || !split_low || !full_open ||
            full_low * 4u >= split_low || full_low * 4u >= full_open) {
            fprintf(stderr, "routing ineffective: FULL low %llu, SUB SAFE low %llu, FULL open %llu\\n",
                    (unsigned long long)full_low, (unsigned long long)split_low,
                    (unsigned long long)full_open);
            return 9;
        }
    }
    /* All four waveform corners and a narrow pulse MUST be different. */
    if (shape_difference(0u, 32u) < 2000000u ||
        shape_difference(32u, 64u) < 2000000u ||
        shape_difference(64u, 96u) < 2000000u ||
        shape_difference(96u, 127u) < 2000000u) {
        fprintf(stderr, "waveform morph endpoints are not audibly distinct\\n");
        return 10;
    }
    /* Avoid pulse DC offset; test PWM end with 4096 sample phase average. */
    {
        int64_t sum = 0;
        uint32_t j;
        for (j = 0; j < 4096u; ++j)
            sum += ext_shape(j * 1048576u, 127u);
        if (sum < -500000 || sum > 500000) {
            fprintf(stderr, "PWM has unwanted DC offset: %lld\\n", (long long)sum);
            return 11;
        }
    }
    /* LAYER excludes FM exactly, not only when INDEX=0.
       Changing FM INDEX / RATIO / FEEDBACK must not change any output sample.
     */
    {
        ext_settings_t a = p, b;
        configure_voice(&a);
        a.b_mode = 0u;  /* LAYER */
        a.level_b = 110u;
        b = a;
        b.fm_depth = 0u;
        b.fm_ratio_x2 = 16u;
        b.fm_feedback = 127u;
        if (rendered_difference(a, b) != 0u) {
            fprintf(stderr, "LAYER unexpectedly reacts to FM settings\\n");
            return 12;
        }

        /* OSC B pitch in LAYER is unrelated to FM RATIO; DETUNE matters. */
        b = a;
        b.b_detune_cents = 70;
        if (rendered_difference(a, b) < 1000000u) {
            fprintf(stderr, "B DETUNE does not affect the LAYER sound\\n");
            return 13;
        }

        /* True independent levels, with no hidden sound at all gains 0. */
        b = a; b.level_a = 0u; b.level_b = 0u; b.sub_body = 0u;
        if (rendered_energy(b) != 0u) {
            fprintf(stderr, "oscillator mixer not silent when all levels zero\\n");
            return 14;
        }
        b = a; b.output_trim = 0u;
        if (rendered_energy(b) != 0u) return 15;

        /* MOD: B is modulation only. Moving its mixer gain changes nothing. */
        a.b_mode = 1u;
        b = a; b.level_b = 0u;
        if (rendered_difference(a, b) != 0u) {
            fprintf(stderr, "MOD leaks direct oscillator B into mixer\\n");
            return 16;
        }
        /* MOD: oscillator B shapes the carrier when changing INDEX. */
        b = a; b.fm_depth = 0u;
        if (rendered_difference(a, b) < 1000000u) {
            fprintf(stderr, "MOD is not generating effective FM\\n");
            return 17;
        }
        b = a; b.fm_feedback = 0u;
        if (rendered_difference(a, b) < 1000000u) {
            fprintf(stderr, "MOD feedback has little audible effect\\n");
            return 18;
        }

        /* BOTH: B is audible AND the source of FM. */
        a.b_mode = 2u;
        b = a; b.level_b = 0u;
        if (rendered_difference(a, b) < 1000000u) {
            fprintf(stderr, "BOTH cannot independently mix oscillator B\\n");
            return 19;
        }
        b = a; b.fm_depth = 0u;
        if (rendered_difference(a, b) < 1000000u) {
            fprintf(stderr, "BOTH does not retain FM modulation\\n");
            return 20;
        }
    }
    /* All advertised factory presets must have usable output and stable defaults. */
    {
        uint32_t j;
        for (j = 0u; j < EXT_PRESET_COUNT; ++j) {
            ext_kick_t voice = {0};
            ext_settings_t preset = EXT_PRESETS[j].settings;
            uint64_t energy = 0u;
            uint32_t frame;
            if (preset.roll_on || preset.amp_release_ms < 5u ||
                preset.output_trim == 0u || preset.b_mode > 2u) return 21;
            ext_recalc(&preset);
            ext_trigger(&voice, &preset);
            for (frame = 0u; frame < 10000u; ++frame) {
                int32_t v = ext_tick(&voice, &preset);
                energy += (uint32_t)(v < 0 ? -v : v);
                if (v < -30000 || v > 30000) return 22;
            }
            if (energy < 100000u) {
                fprintf(stderr, "silent factory preset: %s\\n", EXT_PRESETS[j].name);
                return 23;
            }
        }
    }
    /* AHDR is always finite: a held note never produces sustain.
     * Verify the whole attack->hold->decay timeline without note-off.
     */
    {
        ext_settings_t p7 = EXT_PRESETS[6].settings;
        ext_kick_t v = {0};
        uint32_t j, rising;
        ext_recalc(&p7);
        ext_trigger(&v, &p7);
        if (v.amplitude_q15 != 0u || v.amp_stage != 1u) return 24;
        for (j = 0u; j < 1000u; ++j) (void)ext_tick(&v, &p7);
        rising = v.amplitude_q15;
        if (rising < 1000u || rising >= 30000u) return 25;
        for (j = 0u; j < 1400u; ++j) (void)ext_tick(&v, &p7);
        if (v.amp_stage != 2u || v.amplitude_q15 != 30000u) return 26;
        for (j = 0u; j < p7.amp_hold_frames + 15u; ++j)
            (void)ext_tick(&v, &p7);
        if (v.amp_stage != 3u || v.amplitude_q15 >= 30000u) return 27;
        for (j = 0u; j < 110000u; ++j) (void)ext_tick(&v, &p7);
        if (v.amp_stage != 0u || v.amplitude_q15 != 0u) {
            fprintf(stderr, "AHDR HOLD left stuck note after decay\\n");
            return 29;
        }
    }
    /* Even 1ms and maximum 1000ms of HOLD must end in silence. */
    {
        static const uint32_t holds[] = {0u, 1u, 1000u};
        uint32_t h;
        for (h = 0u; h < 3u; ++h) {
            ext_settings_t q = EXT_PRESETS[0].settings;
            ext_kick_t voice = {0};
            uint32_t j;
            q.amp_hold_ms = holds[h];
            ext_recalc(&q);
            ext_trigger(&voice, &q);
            if (q.amp_hold_frames && voice.amp_stage != 2u) return 30;
            for (j = 0u; j < 155000u; ++j) (void)ext_tick(&voice, &q);
            if (voice.amplitude_q15 || voice.amp_stage) {
                fprintf(stderr, "HOLD %u ms failed one-shot termination\\n",holds[h]);
                return 31;
            }
        }
    }
    /* RELEASE acts only on an explicit CUT / roll stop, even during HOLD. */
    {
        ext_settings_t q = EXT_PRESETS[6].settings;
        ext_kick_t voice = {0};
        uint32_t j;
        ext_recalc(&q);
        ext_trigger(&voice, &q);
        for (j = 0u; j < 3500u; ++j) (void)ext_tick(&voice, &q);
        if (voice.amp_stage != 2u) return 32;
        ext_note_off(&voice, &q); /* explicit CUT */
        if (voice.amp_stage != 4u) return 33;
        for (j = 0u; j < 60000u; ++j) (void)ext_tick(&voice, &q);
        if (voice.amplitude_q15 || voice.amp_stage) return 34;
    }
    {
        ext_settings_t linear = EXT_PRESETS[4].settings;
        ext_settings_t exp = linear;
        ext_kick_t x = {0}, y = {0};
        uint32_t j;
        linear.pitch_curve = 127u;
        exp.pitch_curve = 0u;
        ext_recalc(&linear);
        ext_recalc(&exp);
        ext_trigger(&x, &linear);
        ext_trigger(&y, &exp);
        for (j = 0; j < 1300u; ++j) {
            (void)ext_tick(&x, &linear);
            (void)ext_tick(&y, &exp);
        }
        if (x.pitch_q8 == y.pitch_q8 ||
            x.pitch_q8 < y.pitch_q8 + (100u << 8)) {
            fprintf(stderr, "PITCH EXP and LINEAR not distinguishable\\n");
            return 28;
        }
    }
    puts("EXTRATONE CORE 0.8 one-shot AHDR and pitch envelope: OK");
    return 0;
}
int main(void) { return check_tuning(); }
