# EXTRATONE CORE 1.0 — real synthesized transient engine

Based on the tested 0.9 independent A/B/SUB mixer and finite AHDR.
No samples, allocation or float inside the real-time DSP. This is a
focused attack-engine update, **not yet the multimode distortion engine**.

## Why the old CLICK seemed ineffective

The original noise impulse decayed in about 0.46 ms and went through LP24;
at low cutoff the remaining attack was mostly filtered out. The new engine
replaces it with an independent, audible component with a clear duration.

## New dedicated TRANSIENT page

**Press OCT+ on FM-1** (BTN_OCT_UP) to open/close the attack page.
The original OCT+ page-shift function was not implemented in this fork.
The bottom of the LCD now reads `OCT+:HIT ARP:AHDR`.

| KNOB | Control | Range | Meaning |
| --- | --- | --- | --- |
| 1 | **HIT LEVEL** | 0..127 | Transient amplitude independent of OSC A/B/SUB |
| 2 | **HIT TIME** | 1..40 ms | Linear amplitude-envelope duration; finite |
| 3 | **HIT COLOR** | 0..127 | 0=pitched sine click, 127=bright highpass-noise click |
| 4 | **ROUTE** | PRE LP24 / DIRECT | Transient enters LP24 or is added after it |

The MAIN page's third knob is renamed **HIT**, and still adjusts the same
level as the first knob on the new page.

DSP: a phase-reset pitched sine from ~600 to 2886Hz blends with high-passed
synthesized noise (xorshift, **not** a sample); a precise 1–40 ms linear
envelope sets the click duration. Transient remains independent from
pitch sweep and amplitude AHDR. It can be heard with both oscillators and SUB
muted. Every retrigger starts a new transient. No hidden note sustain.

**PRE LP24** follows the existing FULL SERIAL / SUB SAFE body+sub filter
routing (in both, the transient is filtered). **DIRECT** sums the transient
after the filter, independent of cutoff. The final OUT TRIM and safety
clipping remain shared. If LP24 is globally bypassed (GLO), both routes
naturally sound alike.

Because the final stage is a common limiter, very loud combined settings
can still clip; to compare transients use conservative OUT TRIM. There is
no gain compensation coupling the attack to OSC A/B/SUB.

## MIDI CC channel 1

- CC31 = HIT LEVEL (same destination as previous CLICK).
- CC53 = HIT TIME, 1..40ms.
- CC54 = HIT COLOR, 0 tonal / 127 noise.
- CC55 = HIT ROUTE, 0..63 PRE LP24, 64..127 DIRECT.
- All earlier CCs, including CC52 CUT, remain supported.

## Presets (PRESETS encoder)

P1–P7 from 0.9 remain available with a sensible 8ms transient default;
for the dry P1 baseline HIT remains 0.

Three new diagnostic presets:

- **P8 TONAL HIT** — pronounced pitched transient, 22 ms, COLOR 0,
  with a little sub.
- **P9 NOISE SNAP** — bright, forceful noise attack, 7 ms, COLOR 127,
  with LP24 bypassed for easy comparison.
- **P10 FILTER HIT** — attack into very low LP24 cutoff. Set
  ROUTE PRE LP24 then DIRECT to hear why bypassing the filter matters.

The display now correctly supports two-digit preset numbers such as P10.

## First 1.0 emulator test

1. Select P8 and play one note. Press OCT+ and sweep HIT TIME 1..40ms.
2. Sweep HIT COLOR from 0 to 127. The pitched thud becomes a sharp noisy snap.
3. Compare P8 vs P9.
4. Select P10, leave FX LP24 **on**, press OCT+, switch ROUTE
   between PRE LP24 and DIRECT. DIRECT should be clearly sharper.
5. Press SEQ (MIX), mute A and B and SUB; with HIT > 0,
   **only the transient** should still sound.
6. Reduce HIT LEVEL to 0, play again: the attack component vanishes.

## Automated tests

Native host tests verify the transient ends after the requested time, that
LEVEL=0 is silent when the three oscillators are muted, that COLOR changes
the actual PCM waveform, that DIRECT passes significantly more attack
than PRE LP24 with cutoff at 30Hz, and that max duration still expires.
Existing LP24, FM, morph, mixer independence and AHDR regressions still run.

The firmware build verifies compilation, **not** CPU headroom or timbral
quality of the physical FM-1. The new color blend is not a complete
bandlimited physical transient model; assess high-rate aliasing by ear.
