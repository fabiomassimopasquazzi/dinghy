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

/* Regression for v0.8 complaint: removing SUB must not make A/B louder.
 * Compare complete rendered PCM sample by sample. With LP24 bypassed,
 * a no-SUB voice + isolated SUB must equal the mixed voice, regardless
 * of how SUB LEVEL is moved, even with the existing DRIVE enabled.
 * Keep OUT TRIM at 55 to leave headroom and avoid final clipping.
 */
static int check_sub_independence(void)
{
    static const uint32_t subs[] = {1u, 32u, 75u, 127u};
    uint32_t mode, si, frame;
    for (mode = 0u; mode < 3u; ++mode) {
        for (si = 0u; si < sizeof(subs)/sizeof(subs[0]); ++si) {
            ext_settings_t base = EXT_PRESETS[3].settings;
            ext_settings_t mixed, isolated;
            ext_kick_t a = {0}, b = {0}, c = {0};
            base.b_mode = mode;
            base.shape_a = 67u;
            base.shape_b = 104u;
            base.level_a = 106u;
            base.level_b = 82u;
            base.sub_body = 0u;
            base.drive = 36u;
            base.click_amount = 0u;
            base.filter_bypass = 1u;
            base.output_trim = 55u;
            mixed = base;
            mixed.sub_body = subs[si];
            isolated = mixed;
            isolated.level_a = 0u;
            isolated.level_b = 0u;
            ext_recalc(&base);
            ext_recalc(&mixed);
            ext_recalc(&isolated);
            ext_trigger(&a, &base);
            ext_trigger(&b, &mixed);
            ext_trigger(&c, &isolated);
            for (frame = 0u; frame < 10000u; ++frame) {
                int32_t no_sub = ext_tick(&a, &base);
                int32_t with_sub = ext_tick(&b, &mixed);
                int32_t sub_only = ext_tick(&c, &isolated);
                int32_t error = (with_sub - no_sub) - sub_only;
                if (error < -2 || error > 2) {
                    fprintf(stderr,
                      "SUB alters OSC output: mode=%u level=%u frame=%u delta=%d\\n",
                      (unsigned)mode,(unsigned)subs[si],(unsigned)frame,(int)error);
                    return 35;
                }
            }
        }
    }
    /* Changing LEVEL B must not renormalize OSC A either. */
    for (mode = 0u; mode < 3u; mode += 2u) {
        ext_settings_t base = EXT_PRESETS[1].settings, mixed, only_b;
        ext_kick_t a = {0}, b = {0}, c = {0};
        base.b_mode = mode;
        base.level_a = 110u;
        base.level_b = 0u;
        base.sub_body = 0u;
        base.drive = 0u;
        base.click_amount = 0u;
        base.filter_bypass = 1u;
        base.output_trim = 50u;
        mixed = base; mixed.level_b = 127u;
        only_b = mixed; only_b.level_a = 0u;
        ext_recalc(&base);
        ext_recalc(&mixed);
        ext_recalc(&only_b);
        ext_trigger(&a, &base);
        ext_trigger(&b, &mixed);
        ext_trigger(&c, &only_b);
        for (frame = 0u; frame < 8000u; ++frame) {
            int32_t error = (ext_tick(&b,&mixed)-ext_tick(&a,&base))
                                - ext_tick(&c,&only_b);
            if (error < -2 || error > 2) {
                fprintf(stderr,"LEVEL B changes A: mode=%u frame=%u delta=%d\\n",
                        (unsigned)mode,(unsigned)frame,(int)error);
                return 36;
            }
        }
    }
    /* Absolute stress, including all three channels at 127 and DRIVE max.
     * Output can limit at the final stage but must never exceed PCM bounds.
     */
    {
        ext_settings_t p = EXT_PRESETS[3].settings;
        ext_kick_t v = {0};
        p.b_mode = 2u;
        p.level_a = 127u;
        p.level_b = 127u;
        p.sub_body = 127u;
        p.drive = 127u;
        p.output_trim = 127u;
        p.filter_bypass = 1u;
        ext_recalc(&p);
        ext_trigger(&v, &p);
        for (frame = 0u; frame < 10000u; ++frame) {
            int32_t x = ext_tick(&v, &p);
            if (x < -30000 || x > 30000) return 37;
        }
    }
    return 0;
}

/* EXTRATONE 1.0: transient tests in the real full audio signal path.
 * Isolate the attack by muting all OSC/SUB levels without any mixer
 * renormalization. Check LEVEL, TIME, COLOR, and filter/direct routing.
 */
static int test_transient_engine(void)
{
    ext_settings_t base = EXT_PRESETS[0].settings, change;
    ext_kick_t a = {0}, b = {0};
    uint32_t i;
    uint64_t difference = 0u, direct_energy = 0u, filtered_energy = 0u;
    base.rate_hz = 0u;
    base.roll_on = 0u;
    base.level_a = 0u;
    base.level_b = 0u;
    base.sub_body = 0u;
    base.drive = 0u;
    base.output_trim = 70u;
    base.click_amount = 112u;
    base.transient_ms = 20u;
    base.transient_color = 0u;
    base.transient_route = 1u;
    base.filter_bypass = 1u;
    ext_recalc(&base);
    ext_trigger(&a, &base);
    for (i = 0u; i < 2000u; ++i) {
        (void)ext_tick(&a, &base);
    }
    if (a.click_env_q15 != 0u) {
        fprintf(stderr, "Transient did not finish in expected time\\n");
        return 40;
    }
    /* Turning HIT LEVEL off must restore exact silence. */
    change = base;
    change.click_amount = 0u;
    if (rendered_energy(change) != 0u) return 41;
    /* Short HIT has stopped by 5ms; 30ms HIT must still be active. */
    change = base; change.transient_ms = 2u;
    ext_recalc(&change);
    a = (ext_kick_t){0}; ext_trigger(&a, &change);
    for (i=0u;i<240u;++i) (void)ext_tick(&a, &change);
    if (a.click_env_q15 != 0u) return 42;
    change.transient_ms = 30u;
    ext_recalc(&change);
    b = (ext_kick_t){0}; ext_trigger(&b, &change);
    for (i=0u;i<240u;++i) (void)ext_tick(&b, &change);
    if (b.click_env_q15 == 0u) return 43;

    /* COLOR must alter the PCM signal itself at same level and duration. */
    base.transient_color = 0u;
    change = base; change.transient_color = 127u;
    ext_recalc(&base); ext_recalc(&change);
    ext_trigger(&a, &base); ext_trigger(&b, &change);
    for (i=0u;i<1900u;++i) {
        int32_t d = ext_tick(&a, &base) - ext_tick(&b, &change);
        difference += (uint32_t)(d < 0 ? -d : d);
    }
    if (difference < 250000u) {
        fprintf(stderr, "Transient COLOR change had no PCM impact\\n");
        return 44;
    }

    /* PRE LP24 must be clearly attenuated with cutoff 30Hz.
     * DIRECT must retain the attack on the exact same OSC-free signal.
     */
    base.transient_color = 127u;
    base.filter_cutoff = 0u;
    base.filter_env_amt = 0u;
    base.filter_res = 0u;
    base.filter_bypass = 0u;
    base.transient_route = 1u;
    change = base; change.transient_route = 0u;
    ext_recalc(&base); ext_recalc(&change);
    a = (ext_kick_t){0}; b = (ext_kick_t){0};
    ext_trigger(&a, &base); ext_trigger(&b, &change);
    for (i=0u;i<1900u;++i) {
        int32_t x = ext_tick(&a,&base), y=ext_tick(&b,&change);
        if (x < -30000 || x > 30000 || y < -30000 || y > 30000)
            return 45;
        direct_energy += (uint32_t)(x < 0 ? -x : x);
        filtered_energy += (uint32_t)(y < 0 ? -y : y);
    }
    if (direct_energy < 100000u || direct_energy < filtered_energy*4u) {
        fprintf(stderr, "Transient direct/pre LP24 ineffective: %llu / %llu\\n",
            (unsigned long long)direct_energy,
            (unsigned long long)filtered_energy);
        return 46;
    }

    /* Silence after complete decay, even at longest transient time. */
    base.filter_bypass = 1u;
    base.transient_ms = 40u;
    ext_recalc(&base);
    a = (ext_kick_t){0};
    ext_trigger(&a, &base);
    for (i=0u;i<2500u;++i) (void)ext_tick(&a, &base);
    if (a.click_env_q15 != 0u || ext_tick(&a, &base) != 0)
        return 47;
    return 0;
}

static int check_tuning(void)
{
    static const int32_t semis[] = {-24, -12, 0, 12, 24};
    static const uint32_t expected_sub[] = {10u, 20u, 40u, 80u, 160u};
    static const uint32_t expected_start[] = {250u, 500u, 1000u, 2000u, 4000u};
    /* Designated initialization is mandatory when adding DSP parameters:
     * positional field initializers are fragile across engine versions.
     */
    ext_settings_t p = {
        .rate_hz = 0u, .start_hz = 1000u, .end_hz = 40u,
        .sweep_ms = 35u, .decay_ms = 175u, .fm_depth = 44u,
        .drive = 35u, .tune_semitones = 0,
        .fm_ratio_x2 = 4u, .fm_decay_ms = 90u,
        .click_amount = 40u, .transient_ms = 8u,
        .transient_color = 104u, .transient_route = 1u,
        .sub_body = 70u, .filter_cutoff = 78u, .filter_res = 48u,
        .filter_env_amt = 75u, .filter_env_ms = 90u,
        .filter_route = 0u, .filter_bypass = 0u,
        .level_a = 110u, .level_b = 0u, .output_trim = 127u,
        .amp_attack_ms = 0u, .amp_hold_ms = 0u,
        .amp_release_ms = 180u
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
    {
        int mix_error = check_sub_independence();
        if (mix_error) return mix_error;
    }
    {
        int err = test_transient_engine();
        if (err) return err;
    }
    puts("EXTRATONE CORE 1.0 transient COLOR/TIME/ROUTE/LEVEL tests: OK");
    return 0;
}
int main(void) { return check_tuning(); }
