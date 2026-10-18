# EXTRATONE CORE 1.1 — musical key-rate, phase and UI

This update follows 1.0. It does not change the independent A/B/SUB
mixer, FM modes, or the pitched/noise HIT transient generator.

## Why you heard slow beating

- Several older preset defaults had nonzero OSC B DETUNE (for example
  P2 16 cents). Two tones close in frequency naturally beat slowly.
- The SUB oscillator was not resetting phase with OSC A and B.
  In 1.1 SUB resets phase on each trigger, making transient starts
  consistent rather than unpredictable.
- The B oscillator's increment now preserves the fractional bits
  when at equal pitch, so zero detune/ratio 1 remains phase locked.
- Three former detuned defaults P2/P4/P6 now use DETUNE=0.
- **P11 PHASE LOCK**: equal sine waves at 220Hz, DETUNE 0;
  **P12 PHASE BEATS**: same setup but DETUNE +16 cents to
  demonstrate intentional beating. These are diagnostic, not
  evidence that all beating or cancellation is eliminated.
- Extremely high frequencies and PWM/FM can still alias.

## Keyboard controlling the frequency of *kick repetition*

This means the number of **complete kick triggers per second**, not
the sustained fundamental frequency of OSC A. A3 (MIDI 57) becomes
220 kicks per second, A4 (MIDI 69) becomes 440 kicks per second.
Every other MIDI semitone follows equal temperament and is accurate
to the MIDI-note Q8 table's precision.

- Press **OCT-** to turn **KEY RATE ON** / OFF.
- When KEY RATE ON, press/hold a key; its MIDI pitch determines the
  rolling **retrigger frequency**, while AHDR, PITCH and HIT retrigger
  with each kick. Releasing the active key stops keyboard-driven roll,
  allowing the final hit's one-shot envelope to decay normally.
- While KEY RATE is OFF, keys trigger one ordinary kick as in 1.0.
- The **MAIN RATE knob remains available** and controls the manual roll
  frequency. Press PLAY to start/stop manual roll.
- MAIN now supports manual **0–1000 Hz** instead of 0–150 Hz; after
  150 Hz the encoder moves in 10Hz steps. You can still get, e.g.,
  440 Hz by the knob.
- When a key is held, screen reads `KEY A3 220 Hz` or
  `KEY A4 440 Hz`; when not held, `ROLL RATE NNN Hz`.
- OCT+ still opens the HIT (transient) page; OCT- is only the KEY RATE
  mode toggle. Other pages are unchanged.

For MIDI external controllers: note-on sets the rate in KEY RATE
mode; note-off for the most recently held MIDI note ends the roll.
Normal one-shot keyboard mode is preserved.

**CC20 RATE** now uses 0–150Hz for values 0..64 and 150–1000Hz for
values 65..127. **CC56** toggles KEY RATE mode (value>=64 ON).

## ATTACK vs HIT vs ROLL

These are different modules in the 1.1 signal path:

- **BODY ATTACK** under ARP AHDR rises over up to 100ms for OSC A/B
  and SUB. The engine has been regression tested: starting at 0 it
  reaches about 50% at 50ms and 100% at 100ms if ROLL is OFF.
- **HIT** under OCT+ is the former CLICK, now its own transient engine
  and envelope. It bypasses the BODY amplitude AHDR: even with a
  100ms attack it still fires immediately. To listen to BODY ATTACK,
  set HIT LEVEL = 0 and PLAY OFF.
- **ROLL RATE** is the number of *whole kick events per second*.
  To get a machine-gun effect, press PLAY and increase RATE, or enable
  KEY RATE and press A3/A4. HIT TIME does **not** multiply the number
  of hits.
- At 440Hz there is only ~2.27ms between hits. If BODY ATTACK is
  set to 100ms, its envelope restarts on every hit, so the body
  cannot complete the attack. HIT can still sound. For fast extratone
  use BODY ATTACK 0–2ms, short HIT TIME and suitable MIX headroom.
- No automatic rate-acceleration envelope is included; RATE is steady
  while a key is held. This is a **separate future feature**.

## LCD/button audit

- Page labels and encoders: MAIN, PITCH ENV, FM, LP24 FILTER, OSC,
  MIX, AMP AHDR, HIT.
- Page buttons: HOME MAIN, ENV PITCH, EDIT FM, FX FILTER, LFO OSC,
  SEQ MIX, ARP AHDR, OCT+ HIT.
- OCT- toggles musical KEY RATE mode, PLAY toggles manual ROLL.
- OSC B mode is displayed as LAYER/MOD/BOTH rather than numbers.
- FM page shows `FM OFF: LAYER` when FM is bypassed.
- HIT page footer indicates `HIT=SINGLE ROLL=REPEAT`.
- The changing text region is redrawn with its previous content
  cleared, preventing stale text from previous parameter names.
- Screens, including note labels, are tested statically and firmware
  compilation confirms button IDs; the FM-1 emulator is needed for
  a final **visual** layout check on its 240x240 LCD.

## Test sequence

1. P11 PHASE LOCK vs P12 PHASE BEATS: hear the deliberate difference.
2. P1 with HIT=0, PLAY off: ARP BODY ATTACK 0 then 100ms;
   it should rise slowly only at 100ms.
3. Return BODY ATTACK to 0ms; HIT=65. Press PLAY and increase MAIN
   ROLL RATE into 100–440Hz: machine-gun attacks, becoming audio-rate.
4. Switch PLAY off; press OCT- to enable KEY RATE. Hold A3 (MIDI 57)
   for 220Hz, A4 (69) for 440Hz; release to stop. MAIN displays
   exact note and its frequency.
5. SEQ MIX still has independent A/B/SUB/OUT TRIM as in 0.9.

The tests confirm mathematical clock frequencies and synthesized
sample-envelope values; final subjective timbre and actual UI rendering
still need checking in the simulator/physical instrument.
