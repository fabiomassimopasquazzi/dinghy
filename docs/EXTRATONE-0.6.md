# EXTRATONE CORE 0.6 — LAYER / MOD / BOTH and independent mixer

Experimental FM-1 synthesis firmware. Starts from 0.5; older branches remain unchanged.

## New OSC page (LFO)

| KNOB | Control |
| --- | --- |
| 1 | SHAPE A: sine -> triangle -> saw -> square -> PWM |
| 2 | SHAPE B: sine -> triangle -> saw -> square -> PWM |
| 3 | **B MODE**: 0=LAYER, 1=MOD, 2=BOTH |
| 4 | **B DETUNE**: -100..+100 cents (shared pitch sweep plus offset) |

The displayed status indicates the active oscillator B role.

- **LAYER**: A + B are audible independently. FM is **fully disabled**,
  regardless of FM Index / Feedback / Ratio values. B runs the shared
  pitch drop plus fine detune; FM RATIO has no effect here.
- **MOD**: oscillator B modifies the phase of oscillator A; only A is
  audible directly. B level in MIX has no effect in this mode.
- **BOTH**: B phase-modulates A AND is audible as a second signal. B direct
  level in MIX and FM index are independently usable.

In MOD and BOTH, the B frequency follows `FM RATIO` and B DETUNE together.
B is frequency-limited to approximately 14 kHz to avoid integer overflow;
aliasing at extreme settings is not yet eliminated.

## Independent MIX page (SEQ)

| KNOB | Control |
| --- | --- |
| 1 | LEVEL A (0..127) |
| 2 | LEVEL B (0..127; inactive as audible source in MOD) |
| 3 | SUB LEVEL (0..127) |
| 4 | OUT TRIM (0..127) |

A and B have independent signal levels. A normalizer before the shared
DRIVE prevents simply increasing all mixer levels from forcing an
instant hard clip. A second normalized mix balances driven A/B body
against the independent clean sine sub. The `OUTPUT TRIM` controls final
gain, after FILTER and transient, without altering drive character.

**Pitch envelopes act on the oscillator frequency BEFORE audio mixing**
for A and B. The independent sub stays at the pitch envelope's END.
The amplitude envelope applies to both A and B; the sub also follows it.

## Main routing

- LFO opens/closes OSC page.
- SEQ opens/closes MIX page.
- EDIT opens/closes FM page; shows FM OFF while in LAYER.
- ENV is original pitch envelope page.
- FX selects LP24; SEL toggles full serial/safe sub routing.
- GLO bypasses filter; PLAY toggles roll.

SHAPE now aligns the phases of saw, triangle and square for a smoother
morph in the middle region. The first sine-to-triangle stretch remains
subtle because the waves are similar; it is not a new oscillator algorithm.

## MIDI CC (channel 1)

Existing CCs 20-40 unchanged except:
- 41 is now **LEVEL B** (formerly OSC MIX)
- 42 FM FEEDBACK
- **43 LEVEL A**
- **44 OUT TRIM**
- **45 B MODE** (0-42 LAYER; 43-85 MOD; 86-127 BOTH)
- **46 B DETUNE** (-100..+100 cents)

## What has NOT changed

CLICK still uses the v0.5 short noise transient. No enhanced transient
shaper, no multimodal distortion and no Algorithm selector are added here.
These are later separate blocks after validating the oscillator engine.

## First A/B test in emulator

1. Turn off ROLL, bypass LP24 with GLO, DRIVE to 0, and SUB to 0.
2. Set MIX LEVEL A / LEVEL B to 127, OUT TRIM 127.
3. OSC B MODE LAYER. Raise FM INDEX, change FM RATIO and FEEDBACK: **NO
   change in sound**. Alter DETUNE: clear beating when A and B are layered.
4. OSC B MODE MOD. Change LEVEL B in MIX: **no change in direct sound**.
   Raise INDEX: OSC A changes due to B's modulation.
5. OSC B MODE BOTH. Turn LEVEL B up/down: now direct B audio is heard,
   *while* INDEX still changes modulated A.

Firmware compiles and host tests cannot certify real-device CPU headroom.
At extreme morph positions and FM index, naive saw/PWM forms still alias.
