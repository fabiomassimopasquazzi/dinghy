# EXTRATONE CORE 1.3 — One good kick first

This version is an **isolated timbre experiment**, based on v1.2. It does
not add controls or change keyboard RATE, PITCH ENV, oscillator shapes,
FM algorithms, sub level, filter topology, HIT engine or AHDR timing.
Older branches, including v1.2, remain unchanged for A/B listening.

## Exactly what changed

**v1.2 signal path:**

`OSC A + OSC B -> amplitude AHDR -> hard DRIVE clip -> LP24 -> OUTPUT`

**v1.3 signal path:**

`OSC A + OSC B -> musical soft DRIVE -> amplitude AHDR -> LP24 -> OUTPUT`

SUB is still a separate *clean* oscillator, following its own direct
AHDR and existing FULL SERIAL/SUB SAFE LP24 route. HIT is still a separate
short transient and may bypass LP24. OUT TRIM remains downstream.

### DRIVE

DRIVE 0 is still perfectly clean; higher settings progressively
increase a fixed-point pre-gain and run through a **soft-knee** wave
shaper rather than v1.2's abrupt +/-30000 clamp.

- Before: pre-gain `1 + DRIVE^2 / (3*256)` (up to ~22x), then hard clip.
- After: pre-gain `1 + DRIVE^2 / (12*256)` (up to ~6.25x), then
  a continuous-first-derivative polynomial knee, asymptoting to a
  flat +/-26192 cap. The smooth transition begins at absolute 18000.
- Soft-clipping is symmetrical: no introduced DC offset.
- No extra DSP controls; the **existing DRIVE knob** is reused.

This is a simple, efficient **musical saturation curve**, not a
component-level analog circuit model. At very high DRIVE, high harmonics
and aliasing remain possible. The polynomial reduces sharp *corners*
compared with a hard clamp, but is not equivalent to oversampling.

### Envelope placement

The AHDR now scales OSC A+B **after** their shared saturation.
The saturation character stays more constant as the volume envelope
decays. The entire AHDR (Attack/Hold/Decay/Release) remains available.
The pitch envelope remains entirely independent of rate.

**IMPORTANT:** Moving the envelope after DRIVE changes the timbre
throughout all existing patches with DRIVE above zero. No old presets
have been deleted, but they will not be sonically identical to v1.2.
This is intentional for comparison; to go back, install v1.2.

## The one diagnostic preset

**P13 ANALOG KICK** (PRESETS encoder; select after P12):

| Parameter | Factory value |
|---|---:|
| OSC A SHAPE | 0 / SINE |
| LEVEL A | 115 |
| OSC B / SUB | 0 / 0 |
| OSC B MODE | LAYER (FM OFF) |
| HIT LEVEL | 0 |
| P START -> END | 220 -> 48 Hz |
| P SWEEP | 38 ms |
| P CURVE | 0 / EXP |
| BODY ATTACK / HOLD | 0 / 0 ms |
| AMP DECAY | 350 ms |
| DRIVE | 50 |
| LP24 | BYPASS |
| ROLL | OFF |
| OUT TRIM | 106 |

This patch intentionally has no 'secret bass booster'. The audition is
of one pitch-swept oscillator and its own musical saturation alone.
It is a **starting hypothesis**, not a claim to reproduce the
SSF Entity Ultra-Kick's coupled resonators or analog stages.

## Listening procedure

1. Load P13, listen to individual notes first, not extratone.
2. On MAIN (HOME), vary DRIVE through **0, 20, 35, 50, 65**.
3. Compare the tone and depth while keeping output level matched by
   **OUT TRIM**; a louder kick is not automatically a better kick.
4. Try small changes to P START (170–300Hz) and P SWEEP
   (25–60ms), leaving the rest unchanged.
5. Play slower ROLL to evaluate repeated attacks; then try 150Hz
   and higher only *after* deciding whether the single kick is good.
6. To A/B the *same parameter combination* in 1.2, load P1 SINE
   INIT, manually set the parameter values from the P13 table, then
   adjust OUT TRIM to similar perceived loudness. Switch firmware
   versions; do not compare at significantly different volume.

The new voice might feel smoother and better integrated yet still be
less deep than the Ultra-Kick; that would suggest inspecting *the resonant
core* next instead of adding more DRIVE or parameters.

## Tests

The C host test suite includes `test_analog_kick.c` to verify:
- Bound, symmetry, monotonicity and knee continuity of the new clipper.
- P13 is an isolated OSC A sinusoidal kick and decays to silence.
- Distortion character does not change as AHDR decay falls (because AHDR
  now acts AFTER the waveshaper).
- DRIVE 0 remains linear for the separate A/B mixer.
- Body DRIVE does not alter the independent clean SUB.
- Existing routing, filter, transient, phase and note-rate tests pass.

Native tests and cross-compilation **cannot certify** sound quality on
the physical device, CPU headroom or that aliasing is inaudible.
The following version, if needed, should specifically evaluate
high-rate phase-continuous retrigger behavior. No retrigger changes
are bundled in this version.
