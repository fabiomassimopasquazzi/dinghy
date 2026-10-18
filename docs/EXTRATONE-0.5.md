# EXTRATONE CORE 0.5 — OSC SHAPE + deep FM

Experimental synthesizer firmware for **M-VAVE FM-1**. Builds on the proven v0.4.1
filter-routing fix. Earlier version branches and `main` are unchanged.

## Control pages

| Page (FM-1 button) | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| MAIN (HOME when returning) | RATE (0–150 Hz) | TUNE (-24..+24 st) | CLICK (0–127) | DRIVE (0–127) |
| ENV (ENV) | START (80–6000 Hz) | END (20–260 Hz) | SWEEP | DECAY |
| **OSC (LFO)** | **SHAPE A** | **SHAPE B** | **OSC MIX** | **SUB LEVEL** |
| **FM (EDIT)** | **RATIO** | **INDEX** | **FEEDBACK** | **FM DECAY** |
| LP24 (FX) | CUTOFF | RESONANCE | ENV AMT | ENV TIME |

### One knob per complete oscillator shape

SHAPE A and SHAPE B both use the same 0..127 morph path:

- 0 = pure sine.
- 1..32 = sine -> triangle.
- 32..64 = triangle -> saw.
- 64..96 = saw -> square.
- 96..127 = square -> progressively narrower PWM, down to about 5% duty.

The screen shows a numeric value and the current morph region for **both** shapes.
PWM uses a compensated low state to avoid introducing large DC offsets.
**The saw/square/pulse generators are not yet bandlimited**. At high carrier
frequency and/or large FM depth audible aliasing is possible: this is the next
quality/performance decision, not something hidden by the interface.

### OSC MIX and FM

OSC B is audible *and* modulates OSC A. OSC MIX 0 = only OSC A audible;
OSC MIX 127 = only OSC B audible, with intermediate parallel mixtures.
The clean sub is still separately level-adjustable.
INDEX 0 disables FM, and INDEX 127 reaches a much higher phase modulation depth
than the v0.4 engine (about four phase revolutions at peak modulation).
FEEDBACK drives the previous modulator sample back into OSC B phase.
These features create substantial tonal variety but extreme settings may alias.

No waveforms are samples; synthesis remains real time, integer-only audio path.

### Routing and special buttons

- **FX** filter page, **GLO** toggles LP24 bypass.
- **SEL** switches **FULL SERIAL** / **SUB SAFE** filter routing.
- **EDIT** FM page, **LFO** OSC page, **ENV** pitch envelopes page.
- **PLAY** toggles automatic retrigger. These page buttons toggle back to MAIN.

### MIDI channel 1 CCs

Preserved: 20 RATE, 21 START, 22 END, 23 SWEEP, 24 DECAY,
25 DRIVE, 26 FM INDEX (range now 0–127), 27 ROLL, 28 TUNE,
29 FM RATIO, 30 FM DECAY, 31 CLICK, 32 SUB, 33 CUTOFF,
34 RESONANCE, 35 FILTER ENV, 36 FILTER ENV TIME,
37 LP24 BYPASS, 38 FILTER ROUTE.

Added: **39 SHAPE A, 40 SHAPE B, 41 OSC MIX, 42 FEEDBACK**.

### First emulator listening test

1. Load v0.5 into the FM-1 emulator. Use ROLL OFF.
2. Press GLO to bypass the LP24 if it is active; set DRIVE low and SUB LEVEL 0.
3. Press EDIT and set INDEX 0, FEEDBACK 0 (so the source is unmodulated).
4. Press LFO and set OSC MIX 0; rotate SHAPE A slowly 0->127 while triggering
   the same kick. Each waveform region should sound distinct.
5. Set SHAPE A 0, SHAPE B ~105; bring OSC MIX from 0 to 127.
6. Return to EDIT, set OSC MIX to 0, raise INDEX to hear FM and then FEEDBACK.
7. Re-enable the LP24 and compare rich oscillator source with/without filter.

### Limitations

This release only delivers the **oscillator and FM redesign**. Multimode drive,
algorithm routing, real transient boost, retrigger intelligent modes and live
macros have **not** yet been implemented. Real-device CPU budget, aliasing and
live knob switching need physical validation; a successful emulator/build does
not prove hardware headroom.
