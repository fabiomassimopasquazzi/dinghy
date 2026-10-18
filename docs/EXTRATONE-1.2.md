# EXTRATONE CORE 1.2 — Musical notes below 150Hz, with decimals

Based on tested EXTRATONE CORE 1.1. This update focuses ONLY on
keyboard control of the **kick repetition rate**, never on the carrier's
musical pitch.

## Musical rate below 150Hz

The existing MIDI-note equal-temperament lookup already held Q8 fractional
Hertz, but the 1.1 LCD rounded notes to integer Hz and the 27 physical
keys started at F3 (~174.61Hz). That made the range below 150Hz hard to
reach and impossible to read precisely.

**Now:** press **OCT-** to enable KEY RATE. The 27 physical keys default
one octave down (**KEY OCT -12**, selectable). On MAIN the line at the
bottom reads `KEY OCT -12 ALG` to show that the **ALG encoder** shifts the
physical keyboard by octaves. Rotate ALG while in KEY RATE to select:

- **-24 semitones:** physical key F3 sounds the rate F1 = 43.65Hz
- **-12 semitones (startup default):** physical F3 = F2 = 87.31Hz
- **0 semitones:** physical F3 = F3 = 174.61Hz
- **+12 semitones:** physical F3 = F4 = 349.23Hz

All 27 keys follow equal temperament, including the non-integer
frequency values below 150Hz. Example frequencies for KEY RATE:

| Note | Kick retrigger rate |
| ---- | ----: |
| F2 | 87.31Hz |
| A2 | 110.00Hz |
| C3 | 130.81Hz |
| C#3 | 138.59Hz |
| D3 | 146.83Hz |
| A3 | 220.00Hz |
| A4 | 440.00Hz |

Press/hold a physical key in KEY RATE mode; MAIN shows e.g.
`KEY D3 146.83 Hz`, never merely `147 Hz`. Audio timing is kept
in **Q8 fractional Hz**, not truncated or rounded to display.

The physical note is shifted for **rate calculation only**. The
MIDI note numbers coming from external MIDI controllers are never
octave-shifted, so a MIDI D3 still always gives 146.83Hz. Turning
ALG while a physical key is held updates the rate in real time.

## Timbre pitch remains on the knobs

The KEY RATE keyboard **never** changes P START / P END (PITCH ENV),
MAIN TUNE, oscillator frequencies, OSC B detune or FM RATIO. It controls
**only how many complete kicks are retriggered per second.**

This separation is explicitly checked in automated native tests:
changing the MIDI key rate must leave calculated OSC A start/end
frequencies unchanged while changing only the roll step.

- MAIN KNOB 1: **manual ROLL RATE** remains 0..1000Hz (PLAY starts roll).
- MAIN KNOB 2: **TUNE** still transposes the synth/kick tone in semitones.
- ENV page: P START and P END still set the sound's pitch sweep.
- ALG encoder: changes physical KEY RATE keyboard octave **only when KEY
  RATE mode is enabled**, not the oscillator tuning.
- OCT-: toggle KEY RATE mode; OCT+: open HIT engine.

At 100..440Hz the kick retriggers very frequently; for clearly distinct
pulses use fast BODY ATTACK and short HIT TIME. This does not imply an
accelerating sweep; rate is steady per held note.

## Regression tests

- MIDI 49 C#3 = Q8 138.59Hz, 50 D3 = Q8 146.83Hz,
  45 A2 = 110.00Hz, 41 F2 = 87.31Hz.
- With physical octave shift -12, F3 maps to F2 and D4 maps to D3.
- Ten seconds at D3 generates about **1468** retriggers, not 1470
  as it would if the clock were accidentally rounded to 147Hz.
- Changing key RATE D3→A4 leaves start/end oscillator pitch unchanged.
- 1.1 phase, mixer, HIT and AHDR regression tests are also run.
- Static LCD/control mapping audit checks the new octave and fractional
  display code. A visual emulation check is still recommended.

## Note on sound & implementation

The physical FM-1 has 27 keys F3–G5 and an ALG encoder. The octave
control is a performance convenience, not a modification to the
keyboard's MIDI note-on output, which retains its original unshifted
note number. Neither the oscillator pitch nor the MIDI output
transposes when changing KEY OCT.
