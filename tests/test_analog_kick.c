/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 1.3: analog kick timbre regression.
 *
 * Test the C DSP actually compiled into the FM-1, not a reimplementation.
 * Float/64-bit in host tests only; the firmware audio ISR is fixed-point.
 */
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/extratone_kick.h"
#include "../firmware/src/extratone_presets.h"

static int fail(const char *message, int code)
{
    fprintf(stderr, "EXTRATONE 1.3: %s\n", message);
    return code;
}

int main(void)
{
    int32_t previous = -1;
    int32_t x;
    uint32_t j;
    ext_settings_t a, b, c;
    ext_kick_t sa={0}, sb={0}, sc={0};
    uint64_t shape_error = 0u, output_energy = 0u;
    uint32_t measured = 0u;

    /* Symmetric, bounded and smoothly sloping around the saturation
     * transition. Neither the old instant +/-30000 hard clip nor a
     * discontinuity at the knee is permitted.
     */
    for (x=-500000; x<=500000; x+=13) {
        int32_t y=ext_drive_softclip(x);
        if (y < -26192 || y > 26192) return fail("Softclip exceeds bounds",1);
        if (y != -ext_drive_softclip(-x)) return fail("Softclip not symmetric",2);
    }
    for (x=0; x<=50000; ++x) {
        int32_t y=ext_drive_softclip(x);
        if (y < previous || y-previous > 2) return fail("Softclip knee discontinuity",3);
        previous=y;
    }
    if (ext_drive_softclip(10000)!=10000 ||
        ext_drive_softclip(34384)!=26192 ||
        ext_drive_softclip(100000)!=26192)
        return fail("Softclip threshold/knee is wrong",4);

    /* Pure reference voice: just OSC A sine, independent from HIT,
     * SUB or FM, with an audible pitched drop and no auto ROLL.
     */
    if (EXT_PRESET_COUNT!=13u) return fail("Reference preset count",5);
    a=EXT_PRESETS[12].settings;
    if (a.start_hz!=220u || a.end_hz!=48u || a.shape_a!=0u ||
        a.click_amount!=0u || a.sub_body!=0u || a.level_b!=0u ||
        a.drive==0u || a.roll_on)
        return fail("P13 ANALOG KICK is not a dry single-sine kick",6);
    ext_recalc(&a);
    ext_trigger(&sa,&a);
    for (j=0u;j<44100u;++j) {
        int32_t sample=ext_tick(&sa,&a);
        if (sample > 30000 || sample < -30000)
            return fail("Output exceeding allowed headroom",7);
        if (j<15000u)output_energy+=(uint32_t)(sample<0?-sample:sample);
    }
    if (output_energy<2000000u || sa.amplitude_q15)
        return fail("Analog voice silent or hanging note",8);

    /* With pitch frozen, changing AMP DECAY is a pure gain change
     * AFTER the nonlinear DRIVE; shape must not drift as the
     * amplitude changes. This specifically fails for the old
     * pre-DRIVE envelope architecture.
     */
    a=EXT_PRESETS[12].settings;
    a.start_hz=70u; a.end_hz=70u; a.drive=92u;
    a.decay_ms=100u; a.output_trim=65u;
    b=a; b.decay_ms=900u;
    ext_recalc(&a);ext_recalc(&b);
    sa=(ext_kick_t){0};sb=(ext_kick_t){0};
    ext_trigger(&sa,&a);ext_trigger(&sb,&b);
    for(j=0u;j<15000u;++j) {
        int32_t x1=ext_tick(&sa,&a);
        int32_t x2=ext_tick(&sb,&b);
        if(j>2500u && j<7000u) {
            int64_t lhs=(int64_t)x1 * (int64_t)sb.amplitude_q15;
            int64_t rhs=(int64_t)x2 * (int64_t)sa.amplitude_q15;
            int64_t d=lhs-rhs;
            if(d<0)d=-d;
            shape_error+=(uint64_t)d;
            measured++;
        }
    }
    if(measured<1000u || shape_error/(uint64_t)measured>68000u)
        return fail("Envelope still changing the saturation character",9);

    /* At DRIVE=0 a constant A+B mix remains linear, preserving
     * compatibility with the independent-mixer tests from v0.9.
     */
    a=EXT_PRESETS[0].settings;
    a.start_hz=80u;a.end_hz=80u;
    a.decay_ms=700u;a.drive=0u;
    a.sub_body=0u;a.click_amount=0u;
    a.filter_bypass=1u;a.output_trim=55u;
    a.level_a=100u;a.level_b=0u;a.b_mode=0u;
    b=a;b.level_a=0u;b.level_b=100u;
    c=a;c.level_b=100u;
    ext_recalc(&a);ext_recalc(&b);ext_recalc(&c);
    sa=(ext_kick_t){0};sb=(ext_kick_t){0};sc=(ext_kick_t){0};
    ext_trigger(&sa,&a);ext_trigger(&sb,&b);ext_trigger(&sc,&c);
    for(j=0u;j<6000u;++j){
        int32_t e=ext_tick(&sc,&c)-ext_tick(&sa,&a)-ext_tick(&sb,&b);
        if(e<-3 || e>3)return fail("DRIVE zero altered A+B independent mixer",10);
    }

    /* The SUB should sound the same whether BODY DRIVE is zero or 127. */
    a=EXT_PRESETS[4].settings;
    a.level_a=0u;a.level_b=0u;a.click_amount=0u;
    a.sub_body=95u;a.filter_bypass=1u;
    a.drive=0u;
    b=a;b.drive=127u;
    ext_recalc(&a);ext_recalc(&b);
    sa=(ext_kick_t){0};sb=(ext_kick_t){0};
    ext_trigger(&sa,&a);ext_trigger(&sb,&b);
    for(j=0u;j<5000u;++j){
        if(ext_tick(&sa,&a)!=ext_tick(&sb,&b))
            return fail("BODY DRIVE now distorts the separate SUB",11);
    }

    puts("EXTRATONE 1.3 soft DRIVE / post-DRIVE AMP / clean SUB: OK");
    return 0;
}
