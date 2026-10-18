/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone native tests for EXTRATONE kick tuning and LP24, no FM-1 needed.
 */
#include <stdio.h>
#include <stdint.h>
#include "../firmware/src/extratone_kick.h"

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
static uint64_t wave_energy(ext_settings_t p, uint32_t shape_a, uint32_t mix,
                            uint32_t index, uint32_t feedback)
{
    ext_kick_t k = {0};
    uint64_t acc = 0u;
    uint32_t i;
    p.rate_hz = 0u; p.roll_on = 0u;
    p.start_hz = 180u; p.end_hz = 180u;
    p.tune_semitones = 0; p.decay_ms = 700u;
    p.drive = 0u; p.click_amount = 0u; p.sub_body = 0u;
    p.filter_bypass = 1u;
    p.shape_a = shape_a; p.shape_b = 108u;
    p.osc_mix = mix; p.fm_depth = index;
    p.fm_feedback = feedback; p.fm_ratio_x2 = 6u;
    ext_recalc(&p); ext_trigger(&k, &p);
    for (i = 0u; i < 4000u; ++i) {
        int32_t y = ext_tick(&k, &p);
        if (y < -30000 || y > 30000) return 0u;
        acc += (uint32_t)(y < 0 ? -y : y);
    }
    return acc;
}
static uint64_t wave_difference(ext_settings_t p, uint32_t mix_a, uint32_t mix_b,
                                 uint32_t index_a, uint32_t index_b,
                                 uint32_t fb_a, uint32_t fb_b)
{
    ext_kick_t a = {0}, b = {0};
    uint64_t total = 0u;
    uint32_t i;
    p.rate_hz = 0u; p.roll_on = 0u;
    p.start_hz = 160u; p.end_hz = 160u;
    p.decay_ms = 900u; p.tune_semitones = 0;
    p.drive = 0u; p.sub_body = 0u; p.click_amount = 0u;
    p.filter_bypass = 1u;
    p.shape_a = 0u; p.shape_b = 108u;
    p.fm_ratio_x2 = 6u;
    ext_recalc(&p);
    ext_settings_t q = p;
    p.osc_mix = mix_a; p.fm_depth = index_a; p.fm_feedback = fb_a;
    q.osc_mix = mix_b; q.fm_depth = index_b; q.fm_feedback = fb_b;
    ext_trigger(&a, &p);
    ext_trigger(&b, &q);
    for (i = 0u; i < 5000u; ++i) {
        int32_t d = ext_tick(&a, &p) - ext_tick(&b, &q);
        total += (uint32_t)(d < 0 ? -d : d);
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
        0u, 0u, 0u, 0u
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
    if (wave_difference(p, 0u, 127u, 0u, 0u, 0u, 0u) < 2000000u ||
        wave_difference(p, 0u, 0u, 0u, 127u, 0u, 0u) < 2000000u ||
        wave_difference(p, 0u, 0u, 95u, 95u, 0u, 115u) < 2000000u) {
        fprintf(stderr, "OSC MIX, FM INDEX or FEEDBACK has little signal effect\\n");
        return 12;
    }
    if (!wave_energy(p, 55u, 34u, 127u, 127u)) return 13;
    puts("EXTRATONE CORE 0.5 morph/FM routing/feedback tests: OK");
    return 0;
}
int main(void) { return check_tuning(); }
