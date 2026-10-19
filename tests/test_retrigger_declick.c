/* SPDX-License-Identifier: GPL-3.0-only
 * EXTRATONE CORE 1.4: retrigger continuity regression suite.
 */
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/extratone_kick.h"
#include "../firmware/src/extratone_presets.h"

static int run_case(ext_settings_t p, uint32_t hz, int keyboard, int code)
{
    ext_kick_t voice={0};
    uint32_t frame, old_trigger=0u, checked=0u;
    uint32_t max_jump=0u;
    int32_t last=0, last_raw=0;
    p.roll_on=keyboard?0u:1u;
    p.key_roll_on=keyboard?1u:0u;
    p.rate_hz=hz;
    p.key_rate_q8=keyboard?ext_note_hz_q8(69u):0u;
    p.transient_route=1u;
    ext_recalc(&p);
    ext_trigger(&voice,&p);
    old_trigger=voice.triggers;
    for(frame=0u; frame<44100u; ++frame) {
        int32_t sample;
        uint32_t jump;
        sample=ext_tick(&voice,&p);
        if(sample > 30000 || sample < -30000)
            return code;
        if(frame && voice.triggers > old_trigger && last != 0) {
            if(sample != last) {
                fprintf(stderr, "Discontinuity rate=%u frame=%u prev=%d now=%d\n",
                    hz,frame,(int)last,(int)sample);
                return code+1;
            }
            if(voice.declick_left != EXT_DECLICK_FRAMES-1u)
                return code+2;
            ++checked;
        }
        if(frame) {
            int32_t d=sample-last;
            jump=(uint32_t)(d<0?-d:d);
            if(jump>max_jump)max_jump=jump;
        }
        last=sample;
        last_raw=sample;
        old_trigger=voice.triggers;
    }
    (void)last_raw;
    if(checked<15u) {
        fprintf(stderr,"No overlapping retriggers tested: rate=%u checked=%u\n",hz,checked);
        return code+3;
    }
    /* For the isolated P13 voice we expect no clipped flat tops
     * and sample continuity, not altered (musical) RATE accuracy.
     */
    if(p.click_amount==0u && p.level_b==0u && p.sub_body==0u && max_jump>4500u) {
        fprintf(stderr,"Excessive post-fix jump: %u\n",max_jump);
        return code+4;
    }
    if((hz==440u && !keyboard && (voice.triggers<440u||voice.triggers>442u))||
       (hz==1000u && (voice.triggers<1000u||voice.triggers>1002u)))
        return code+5;
    puts("Retrigger-rate continuity: OK");
    return 0;
}
int main(void)
{
    ext_settings_t p=EXT_PRESETS[12].settings;
    ext_kick_t voice={0}, first={0};
    uint32_t k;
    int error;

    if(EXT_DECLICK_FRAMES>=44u)
        return 1; /* 1000 Hz gives only 44.1 samples/trigger */
    p.roll_on=0u;p.key_roll_on=0u;
    ext_recalc(&p);
    ext_trigger(&voice,&p);
    first=voice;
    if(voice.declick_pending||voice.declick_left)
        return 2; /* initial hit starts unaltered */
    if(ext_tick(&voice,&p)!=ext_tick(&first,&p))
        return 3;

    /* Simulate a manual overlapping trigger while the previous sound
     * is nonzero: first new PCM sample must equal previous sample.
     */
    for(k=0u;k<1500u;++k) (void)ext_tick(&voice,&p);
    if(!voice.last_output)return 4;
    {
        int32_t before=voice.last_output;
        ext_trigger(&voice,&p);
        if(!voice.declick_pending)return 5;
        if(ext_tick(&voice,&p)!=before)return 6;
        for(k=1u;k<EXT_DECLICK_FRAMES;++k)
            (void)ext_tick(&voice,&p);
        if(voice.declick_left)return 7;
    }
    /* Isolated new analog kick at conventional and audio-rate roll. */
    error=run_case(p,50u,0,10);if(error)return error;
    error=run_case(p,150u,0,20);if(error)return error;
    error=run_case(p,440u,0,30);if(error)return error;
    error=run_case(p,1000u,0,40);if(error)return error;
    /* Same behavior from the musical KEY RATE clock. */
    error=run_case(p,440u,1,50);if(error)return error;

    /* Direct transient and SUB must not create a numeric PCM overflow
     * at high RATE even when the levels are large.
     */
    p=EXT_PRESETS[3].settings;
    p.click_amount=100u;p.sub_body=80u;p.drive=60u;p.output_trim=78u;
    error=run_case(p,440u,0,60);if(error)return error;

    puts("EXTRATONE CORE 1.4 24-frame retrigger de-click: OK");
    return 0;
}
