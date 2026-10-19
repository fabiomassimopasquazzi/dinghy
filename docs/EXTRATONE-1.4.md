# EXTRATONE CORE 1.4 — Clean retriggers without changing the kick

## The user's listening report

P13 ANALOG KICK in v1.3 has a pleasing individual kick, but rapid
retriggering sounds as if it clips. P13 is a **single monophonic voice**,
not many kicks summed. Even with its factory DRIVE 50 and OUT TRIM 106,
numeric PCM peak should remain well below +/-30000; the phase and amplitude
discontinuity created by resetting the same oscillator can sound like
clipping due to its broadband transient energy.

The exact subjective cause still requires an emulator/hardware A/B.
Actual final clipping remains possible on other patches combining
OSC A+B, SUB, HIT and large OUT TRIM.

## Focused change

At every overlapping kick retrigger, the existing engine still resets
the carrier/B/SUB phases and pitch+AHDR as in v1.3. However, an
**output continuity correction** replaces an instantaneous jump:

- Store the last delivered PCM sample.
- Render the first sample of the newly retriggered kick.
- Correct that sample to equal the previous output sample.
- Linearly fade the correction to zero over exactly **24 audio frames**
  (about 0.54 ms at 44.1kHz), after OUT TRIM.
- No second voice, sample buffers, oversampling, float or new DSP UI knobs.
- The correction is strictly per retrigger; **the first hit from silence
  is not faded**. The original A/B/FM/SUB/HIT and saturator timbre then
  reappears immediately as the correction fades.
- 24 frames remain below the approximately 44.1-frame period at 1000Hz,
  so each correction finishes before the next 1000Hz event.
- The final PCM clamp still ensures bounds +/-30000.

The software still has **one** retriggered oscillator voice. This is an
anti-click transition, not true polyphonic overlapping of individual
kick envelopes. It may soften some attack energy in an extratone run.
High-rate aliasing from saw/PWM waves and FM is still possible.

## Listen on FM-1 or emulator

1. Load P13 ANALOG KICK with original settings.
2. Run at 20–60Hz, then accelerate to 150, 220, 440, 1000Hz.
3. Compare the same P13 values on the earlier v1.3.
4. At higher RATE, listen for discontinuous *cracks* on each attack.
   The v1.4 should reduce them without dulling the first slow kick.
5. If the sound still clips, reduce OUT TRIM to ~80 as a diagnostic.
   If that helps at constant RATE, there may be real downstream overload;
   if only RATE changes the distortion, consider timbral alias/retrigger
   artifacts before introducing more output limiting.

## Tests

Native C tests check:
- Initial hit from silence remains unaltered.
- Immediate PCM sample after a manual retrigger equals the previous one.
- Smoothing correction completes within 24 samples.
- Smooth transitions at manual RATE 50,150,440,1000Hz and keyboard
  RATE A4=440Hz, maintaining existing trigger timing.
- Stress with sub+HIT+body at 440Hz remains in legal signed PCM bounds.
- All 1.3 saturation and 1.2 phase, keyboard and UI tests remain enabled.

This is deliberately a **single-issue release**, not a new kick synth.
