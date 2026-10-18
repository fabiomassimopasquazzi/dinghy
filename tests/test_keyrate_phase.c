/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE 1.1: test keyboard note frequencies, extratone clock,
 * phase-locked oscillators and actual 100ms attack.
 */
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/extratone_kick.h"
#include "../firmware/src/extratone_presets.h"

static int fail(const char *msg,int code)
{
    fprintf(stderr,"EXTRATONE 1.1: %s\n",msg);
    return code;
}
int main(void)
{
    ext_settings_t q = EXT_PRESETS[10].settings, p;
    ext_kick_t a={0},b={0};
    uint32_t i;
    uint64_t diff=0u;
    if (ext_note_hz_q8(57u) != 56320u ||
        ext_note_hz_q8(69u) != 112640u ||
        ext_note_hz_q8(81u) != 225280u)
        return fail("MIDI A3/A4/A5 frequency is wrong",1);
    if (ext_note_hz_q8(69u) != 2u*ext_note_hz_q8(57u))
        return fail("A4 is not an exact octave above A3",2);
    /* Two clean 220Hz sine oscillators with detune=0 must be perfectly
     * phase locked after every trigger. Any random low beating is a bug.
     */
    q.filter_bypass=1u; q.drive=0u; q.click_amount=0u;
    q.sub_body=0u; q.output_trim=70u;
    q.b_mode=0u; q.b_detune_cents=0;
    q.level_a=110u; q.level_b=0u;
    p=q; p.level_a=0u; p.level_b=110u;
    ext_recalc(&p);ext_recalc(&q);
    ext_trigger(&a,&q);ext_trigger(&b,&p);
    for(i=0u;i<30000u;++i){
        int32_t x=ext_tick(&a,&q), y=ext_tick(&b,&p);
        int32_t d=x-y;
        if(d < -1 || d > 1) return fail("Phase-locked A and B differ",3);
    }
    p.b_detune_cents=16;
    ext_recalc(&p);
    a=(ext_kick_t){0};b=(ext_kick_t){0};
    ext_trigger(&a,&q);ext_trigger(&b,&p);
    for(i=0u;i<30000u;++i){
        int32_t d=ext_tick(&a,&q)-ext_tick(&b,&p);
        diff+=(uint32_t)(d<0?-d:d);
    }
    if(diff < 400000u)
        return fail("Detune should introduce audible slow beating",4);
    /* Retrigger must reset the otherwise free-running SUB phase. */
    q.sub_body=80u;
    ext_recalc(&q);
    a=(ext_kick_t){0};
    ext_trigger(&a,&q);
    for(i=0u;i<800u;++i)(void)ext_tick(&a,&q);
    if(a.sub_phase==0u)return fail("Sub test did not run",5);
    ext_trigger(&a,&q);
    if(a.sub_phase || a.carrier_phase || a.mod_phase)
        return fail("SUB/OSC phase did not reset at trigger",6);

    /* One second of 220Hz and 440Hz *full-kick retriggers*. */
    q=EXT_PRESETS[0].settings;
    q.click_amount=0u; q.key_roll_on=1u; q.roll_on=0u;
    q.key_rate_q8=ext_note_hz_q8(57u);
    ext_recalc(&q);
    a=(ext_kick_t){0}; ext_trigger(&a,&q);
    for(i=0u;i<44100u;++i)(void)ext_tick(&a,&q);
    if(a.triggers < 220u || a.triggers > 222u)
        return fail("A3 did not generate 220 retriggers per second",7);
    q.key_rate_q8=ext_note_hz_q8(69u);
    ext_recalc(&q);
    a=(ext_kick_t){0}; ext_trigger(&a,&q);
    for(i=0u;i<44100u;++i)(void)ext_tick(&a,&q);
    if(a.triggers < 440u || a.triggers > 442u)
        return fail("A4 did not generate 440 retriggers per second",8);
    /* Manual knob still controls its own clock, without pressed keys. */
    q.key_roll_on=0u; q.key_rate_q8=0u;
    q.roll_on=1u; q.rate_hz=440u;
    ext_recalc(&q);
    a=(ext_kick_t){0}; ext_trigger(&a,&q);
    for(i=0u;i<44100u;++i)(void)ext_tick(&a,&q);
    if(a.triggers < 440u || a.triggers > 442u)
        return fail("Manual knob 440Hz roll is not accurate",9);
    q.rate_hz=1000u; ext_recalc(&q);
    a=(ext_kick_t){0}; ext_trigger(&a,&q);
    for(i=0u;i<44100u;++i)(void)ext_tick(&a,&q);
    if(a.triggers < 1000u || a.triggers > 1002u)
        return fail("Manual 1000Hz extratone roll is not accurate",10);
    /* Real audible body attack takes 100 ms when ROLL and HIT are off.
     * HIT remains a separately triggered transient, so mute it to test ADSR.
     */
    q=EXT_PRESETS[0].settings;
    q.roll_on=0u; q.key_roll_on=0u;
    q.amp_attack_ms=100u; q.click_amount=0u;
    q.amp_hold_ms=0u; q.decay_ms=900u;
    ext_recalc(&q);
    a=(ext_kick_t){0}; ext_trigger(&a,&q);
    if(a.amplitude_q15!=0u)return fail("Attack did not start at zero",11);
    for(i=0u;i<2205u;++i)(void)ext_tick(&a,&q);
    if(a.amplitude_q15 < 12000u || a.amplitude_q15 > 18000u)
        return fail("Attack is not near 50% at 50ms",12);
    for(i=0u;i<2205u;++i)(void)ext_tick(&a,&q);
    if(a.amplitude_q15 < 28000u)
        return fail("100ms Attack did not reach full level",13);
    puts("EXTRATONE 1.1 key note-rate, exact 220/440Hz, phase/attack: OK");
    return 0;
}
