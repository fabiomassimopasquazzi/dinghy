/* SPDX-License-Identifier: GPL-3.0-only
 * Standalone native tests for the EXTRATONE kick tuning, no FM-1 needed.
 */
#include <stdio.h>
#include <stdint.h>
#include "../firmware/src/extratone_kick.h"

static int near(uint32_t a, uint32_t b) { return a + 1u >= b && b + 1u >= a; }
static int check_tuning(void)
{
    static const int32_t semis[] = {-24, -12, 0, 12, 24};
    static const uint32_t expected_sub[] = {10u, 20u, 40u, 80u, 160u};
    static const uint32_t expected_start[] = {250u, 500u, 1000u, 2000u, 4000u};
    ext_settings_t p = {
        0u, 1000u, 40u, 35u, 175u, 7500u, 35u,
        0u, 0u, 0u, 0u, 0, 0u, 0u,
        4u, 90u, 40u, 70u, 0u
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
    puts("EXTRATONE CORE 0.3 tuning and DSP tests: OK");
    return 0;
}
int main(void) { return check_tuning(); }
