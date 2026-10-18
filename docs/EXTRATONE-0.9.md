# EXTRATONE CORE 0.9 — Independent A/B/SUB Mixer

This is a focused correction based on EXTRATONE CORE 0.8.
No changes to oscillator algorithms, AHDR, pitch envelope, filter or sound presets.

## Problem fixed

In 0.8, the mixer recalculated the overall A/B level whenever SUB LEVEL
changed. Lowering SUB effectively **raised A/B**, so the kick sometimes became
louder when its suboscillator was turned down.

The 0.9 mixer **never calculates A or B gain from SUB LEVEL**:

- LEVEL A sets only OSC A's gain.
- LEVEL B sets only OSC B's direct audio gain (inactive in MOD mode).
- SUB LEVEL sets only the independent clean suboscillator's gain.
- OUT TRIM scales the combined output.
- OSC B may still modulate OSC A in MOD or BOTH; its *direct level* and
  its FM modulation are independent.

Each channel has a fixed maximum gain of approximately **0.5** of its
enveloped oscillator sample, with constant headroom per source. This is
different from v0.8 and may make **single-source presets quieter**.
The MASTER control still operates as before. All seven presets retain
their previous parameter values; they are **not** automatically renormalized.

The body A+B goes into DRIVE as before; the clean sub is summed afterward.
The existing LP24 FULL SERIAL/SUB SAFE routing is unchanged.

**Physical limits:** Once a signal passes through a *nonlinear* element
such as hard clipping, resonant saturation or the final output limiter,
adding SUB can still influence the **combined output**. Likewise, phase
cancellation can affect perceived loudness. Fixed independent mixer gains
guarantee that SUB cannot turn up A/B in the mixer, not that the overall
waveform will have identical peak/RMS loudness for every SUB setting.
OUT TRIM may need lowering for maximum A, B and SUB levels.

## Tests

Host regression tests directly compare rendered output waveforms:

- With LP24 bypassed and safe OUT TRIM, the difference between a mixed
  output at SUB LEVEL 1/32/75/127 and the same output at SUB LEVEL 0
  must equal an isolated SUB waveform (±2 integer rounding units).
- Repeat in **LAYER, MOD and BOTH** with some DRIVE.
- Changing LEVEL B must not rescale OSC A in LAYER or BOTH.
- At all three source levels 127 plus maximum DRIVE, bounded output
  remains in the signed Q15 safe range.
- Existing FM / LP24 routing / pitch / finite AHDR tests remain enabled.

## FM-1 UI

- Press **SEQ** for the MIX page, showing A, B, SUB, OUT TRIM.
- The page footer displays **MIX: INDEPENDENT GAIN**.
- LFO, EDIT, ENV, ARP, FX and PRESETS work as in 0.8.
- If desired for a clean A/B comparison, load **P2 DUAL LAYER**,
  bypass LP24, set DRIVE low, keep OUT TRIM conservative and move
  SUB LEVEL 0..127. The tonal addition should no longer turn up A/B.
- To check sub in isolation, make LEVEL A=0 and LEVEL B=0 and raise SUB.

Real FM-1 CPU headroom and subjective audio behavior require listening
tests; compilation and software regression are not a physical-device test.
