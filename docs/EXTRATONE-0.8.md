# EXTRATONE CORE 0.8 — finite AHDR one-shot

Builds on v0.7, preserves the OSC LAYER/MOD/BOTH and independent
pitch envelope, LP24, seven factory presets, and earlier version branches.

## AMP AHDR (button ARP)

| Encoder | Name | Range | Meaning |
| --- | --- | --- | --- |
| 1 | **A ATTACK** | 0–100 ms | Raise amplitude to full level |
| 2 | **D DECAY** | 10–1200 ms | Amplitude falls **all the way to zero** |
| 3 | **H HOLD** | 0–1000 ms | Stay at full level for a **fixed time** after attack |
| 4 | **R RELEASE** | 5–1000 ms | Fade out when explicitly cut/stopped |

The physical encoder order is A, D, H, R for continuity with the previous
v0.7 page; the actual envelope order is **A → H → D → silence**.
There is **no automatic GATE/HOLD NOTE mode**.
Even HOLD = 1 ms and HOLD = 1000 ms eventually fade to zero.
A key held for a long time does not sustain the sound.

Normal keyboard and MIDI note-offs deliberately **do not truncate** a
percussive one-shot. Retriggering by playing again creates a new one-shot.
The RELEASE segment is entered only on an explicit interruption:

- **REC button** = **CUT** of the current voice (also stops ROLL).
- **MIDI CC52 >= 64** (channel 1) = CUT.
- Stopping ROLL with **PLAY** triggers the same fade-out if no physical
  note is held at that time.

A true gated continuous-note mode may be offered later as a separate,
explicit mode; not as an automatic consequence of HOLD > 0.

## PITCH ENV (button ENV)

Still independent: START/END frequency, SWEEP time, EXP↔LINEAR CURVE.
The pitch settles to END but the sound is *not sustained indefinitely*;
the AHDR amplitude eventually reaches zero.

## Factory presets

- P1–P6 remain percussive one-shots with HOLD=0 ms.
- **P7 AHDR HOLD** has A 40ms, H 320ms, D 340ms, R 320ms. **Hold a key
  down**: the sound still fades to silence after A+H+D. Release does
  not retrigger or change it.
- PRESETS encoder changes patches. Main CC49 now means HOLD TIME
  (0..1000ms) instead of SUSTAIN; users with Faderfox mappings should
  relabel this control. CCs 48/50 still ATTACK/RELEASE.

## Listening test

1. Load P7 AHDR HOLD and hold one note. You hear a short rise, a
   **finite plateau**, then a gradual fall to complete silence.
2. Set H HOLD to 1 ms: it still fades to zero; the old v0.7 behavior
   that switched to a sustained note is gone.
3. Set H HOLD to 1000 ms: it plays 1 s at full level then decays.
4. Retrigger P7, press REC: the current sound fades at R RELEASE speed.
5. Load P1 SINE INIT; with ROLL OFF, a kick sounds and stops by itself.

## Limits

This release focuses on the envelope and UI semantics; it does not add
an advanced transient shaper, anti-aliased waves or upgraded distortion.
Host regression tests and compilation cannot demonstrate CPU headroom or
prove subjective sound quality on real FM-1 hardware.
