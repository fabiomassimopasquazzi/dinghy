# EXTRATONE CORE 0.7 — named modes, pitch + ADSR, factory test presets

A reproducible diagnostic build based on v0.6. Earlier branches and `main` are unchanged.

## Three clearly labelled oscillator modes

Press **LFO** for OSC. KNOB 3 now shows the actual mode **name**, not "0/1/2":

- **LAYER: A+B** — two audible oscillators, **FM is completely disabled**.
- **MOD: B TO A FM** — B only modulates A; B is not directly audible.
- **BOTH: FM + B** — B modulates A and is also independently audible.

The screen indicates the current mode on every page, especially FM/EDIT,
so you never have to visit a different page to discover the current routing.

## Two separate envelopes

| Page | Button | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| **PITCH ENV** | **ENV** | P START (Hz) | P END (Hz) | P SWEEP (ms) | P CURVE (0 EXP to 127 LIN) |
| **AMP ADSR** | **ARP** | A ATTACK (0–100 ms) | D DECAY (10–1200 ms) | S SUSTAIN (0–127) | R RELEASE (5–1000 ms) |

PITCH ENV changes **oscillator frequency before the mixer**.
AMP ADSR changes **amplitude**.

A=0 gives an immediate attack. D and R are exponential amplitude segments.
When S>0, a held physical or MIDI note sustains its sound until key/note-off,
then R releases it. With S=0, a kick is a percussive one-shot:
a short key release does not prematurely truncate its decay.
For automatic ROLL, a new kick retriggers the ADSR; when ROLL stops,
the active envelope receives note-off.

The pitch curve interpolates between the original exponential falling pitch
and a linear frequency sweep. No additional oscillators or audio buffers.

## Seven ready-to-play factory presets

Turn the FM-1 **PRESETS encoder** (ENC_PRESETS, not the four KNOB encoders).
The new sound loads immediately; loading stops ROLL, clears the old voice
state, and opens a reproducible test setup. The top of the display shows
`P1 SINE INIT` or the appropriate name; an asterisk `*` indicates editing.
These are factory/demo presets only, not user-save slots.

| # | Name | Expected test |
| --- | --- | --- |
| 1 | **SINE INIT** | pure A sine, no FM, SUB, DRIVE or FILTER; use as baseline |
| 2 | **DUAL LAYER** | LAYER mode, audible A saw + B square, independent detune |
| 3 | **FM ATTACK** | MOD mode, high FM index and feedback; B is *not* directly audible |
| 4 | **BOTH HYBRID** | FM sound plus direct oscillator B and some SUB |
| 5 | **SUB PUNCH** | low sustained sub with downward pitch and punch |
| 6 | **FILTER ACID** | resonant LP24 and filter envelope demonstrate frequency shaping |
| 7 | **ADSR HOLD** | A 60ms / D 250ms / S 95/127 / R 320ms; hold and release note |

P1 is the default at every launch. It is deliberately dry and simple.
Turn the **PRESETS encoder** for the prepared diagnostic comparisons.

## Buttons and extra MIDI CC

- LFO: OSC mode and detune; EDIT: FM; ENV: PITCH; ARP: AMP ADSR.
- SEQ: MIX; FX: LP24; GLO: filter bypass; SEL: filter routing.
- HOME: return to MAIN; PLAY: toggle roll.
- CC 47: PITCH CURVE
- CC 48: AMP ATTACK
- CC 49: AMP SUSTAIN
- CC 50: AMP RELEASE
- CC 51: LOAD PRESET (0–127 maps to P1–P7)

Existing CCs retained, including 24 for AMP DECAY (previously DECAY).
Factory presets do not persist edits and there is no patch save operation yet.

## Audio testing

1. Load `P1 SINE INIT` and press a key. It is intentionally just a dry sine kick.
2. Turn PRESETS once to `P2 DUAL LAYER`. LFO shows `LAYER: A+B` and FM settings
   on EDIT should have NO effect. SEQ controls independent LEVEL A/B.
3. Turn to `P3 FM ATTACK`. LFO shows `MOD: B TO A FM` and EDIT Index and
   Feedback now change the sound. Turning MIX LEVEL B has no audible effect.
4. Turn to `P4 BOTH HYBRID`; in SEQ changing LEVEL B adds/removes a second
   audible oscillator, while INDEX still modulates A.
5. Compare SUB and FILTER presets.
6. Load `P7 ADSR HOLD`, hold a key, then release to hear the sustain and release.
   This behavior is intentionally different from the percussive P1–P6.
7. In PITCH ENV compare CURVE=0 vs 127 while leaving AMP ADSR unchanged.

## Scope

This patch improves *usability* and adds a real amp ADSR. It does **not** yet
add a new transient shaper, new distortion algorithms or anti-aliasing.
Host and cross-compilation results do **not** measure the physical CPU budget.
