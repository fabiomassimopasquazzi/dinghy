# EXTRATONE CORE 0.4.1 — fixed LP24 routing

This is an experimental firmware build for the **FM-1 emulator first**. It has not
been measured or verified on a physical FM-1. The original Dinghy `main` branch
and EXTRATONE 0.3 remain unchanged.

## New: resonant low-pass filter

- A **four-pole low-pass** (~24 dB/oct asymptotic slope) based on four cascaded
  first-order sections, with a nonlinear resonant feedback path.
- **Not** a detailed analog ladder emulation: high-resonance character and cutoff
  curve must be evaluated by ear and against CPU timing on the device.
- 128 logarithmically spaced cutoff positions from approximately **30–12,000 Hz**.
- A falling cutoff envelope triggered by each kick.
- **FULL SERIAL (default):** FM body, clean sub and transient are mixed **before**
  the LP24, so cutoff/resonance affect the **whole kick**. This fixes the v0.4
  problem where most of the audible bass/click bypassed the filter.
- **SUB SAFE (optional):** sub bypasses the LP24, while FM body and click are
  filtered. Useful for extreme filter settings but inherently less dramatic.
- No additional parallel unfiltered path remains in FULL SERIAL mode.
- No lookahead, no convolution, no multiband processing or audio-buffer delay.
- Filter calculations are all integer / fixed-point, coefficients refresh every
  16 frames. No float or 64-bit division in the audio interrupt.
- Output is limited to the audio engine's usual 30k headroom.

## FM-1 controls

| Page | Button | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| MAIN | default / return from page | RATE | TUNE | FM | DRIVE |
| ENV | ENV | START | END | SWEEP | DECAY |
| TIMBRE | EDIT | FM RATIO | FM DECAY | CLICK | SUB |
| **LP24** | **FX** | **CUTOFF (Hz)** | **RESONANCE** | **ENV AMT** | **ENV TIME** |

- **FX** opens/closes the LP24 page; **GLO** enables/bypasses the LP24 regardless of page.
- **SEL** toggles **FULL SERIAL** (default) vs **SUB SAFE** routing.
- **PLAY** switches rolling retrigger on/off.
- The LP24 screen displays `LP24 FULL SERIAL`, `LP24 SUB SAFE`, or `LP24 BYPASS`.
- USB-MIDI CCs (channel 1): cutoff **33**, resonance **34**, envelope amount **35**,
  envelope time **36**, bypass **37** (0 = enabled, 127 = bypass), routing
  **38** (0 = FULL SERIAL, 127 = SUB SAFE).

## Audible A/B test in the emulator

1. Load the v0.4 firmware from the successful device-firmware GitHub Action.
2. Turn **ROLL OFF** and tap a key.
3. Press **FX** to view LP24. Start with cutoff around 1 kHz and resonance around 50.
4. Increase ENV AMT and tap the key again: the filter should open briefly and
   descend after the attack.
5. Press **GLO** to bypass the filter, then press it again to enable it.
6. Raise **SUB BODY** on EDIT if desired. The independent sub bypasses the LP24.

## Limitations / next blocks

This change deliberately implements **only the filter block**, not the full
proposed new synthesis architecture. It does not yet implement the revamped
oscillators, true transient boosting, waveform-selectable drive or algorithm
routing. They should be introduced and evaluated in separate, reversible steps.

Passing a Linux-host test or cross-compiling the `.fwsc` **does not establish**
real FM-1 CPU headroom, exact analog-style response or stable live switching.
A device test and cycle-budget measurements are still required before deploying
the prototype to physical hardware.
