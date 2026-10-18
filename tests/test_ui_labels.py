"""Static firmware display + button mapping audit.

This checks that control labels and routes correspond to the firmware
parameters. It is not a substitute for a visual check in the FM-1 emulator.
"""
from pathlib import Path

src = Path("firmware/src/dinghy.c").read_text()
platform = Path("firmware/src/platform.c").read_text()

buttons = {
    "BTN_HOME": 0, "BTN_ENV": 1, "BTN_EDIT": 2,
    "BTN_FX": 3, "BTN_LFO": 4, "BTN_SEQ": 5,
    "BTN_ARP": 6, "BTN_OCT_UP": 7,
}
pages = [
    '"MAIN"', '"PITCH ENVELOPE"', '"FM ENGINE"',
    '"LP24 FILTER"', '"OSCILLATORS"',
    '"A/B/SUB MIXER"', '"AMP AHDR ENVELOPE"',
    '"TRANSIENT ENGINE"',
]
required = [
    *pages,
    '"ROLL RATE"', '"BODY ATTACK"', '"HIT LEVEL"',
    '"HIT TIME"', '"HIT COLOR"', '"ROUTE DIRECT"', '"ROUTE LP24"',
    '"P START"', '"P END"', '"P SWEEP"', '"P CURVE"',
    '"LEVEL A"', '"LEVEL B"', '"SUB LEVEL"', '"OUT TRIM"',
    '"FM OFF: LAYER"', '"LAYER: A+B"', '"MOD: B TO A FM"',
    '"BOTH: FM + B"', "KEY RATE ON", "KEY RATE OFF",
    '"AHDR BODY; HIT SEPARATE"', '"HIT=SINGLE ROLL=REPEAT"',
    'show_note_rate();', 'EXT_MIDI_HZ_Q8',
    'show_keyboard_octave();', 'ext_keyboard_rate_note', 
    'KEY OCT -12 ALG', "ENC_ALGORITHM",
    'hundredths/100u',
    'lcd_fill(0, 45, 240, 175, C_BLACK);',
]
for item in required:
    if item in ('EXT_MIDI_HZ_Q8', 'ext_keyboard_rate_note'):
        from_dsp = Path("firmware/src/extratone_kick.h").read_text()
        assert item in from_dsp, item
    else:
        assert item in src, f"Missing text/control: {item}"

for button, number in buttons.items():
    text = f'if (b & (1u << {button}))'
    assert text in src, f"{button} not wired"
    assert f'page == {number}u' in src or number == 0, button
    assert button in platform, f"{button} undefined"

assert "p.key_rate_mode ^= 1u;" in src
assert "if (page != 4u) draw_text_box(0, 198" in src
assert "line, C_ACC, 0);" in src
assert "p.b_mode, \"\");" not in src, "Numeric OSC mode displayed"
assert "AHDR: HOLD NOTE" not in src
assert "ADSR: HOLD NOTE" not in src
print("EXTRATONE CORE 1.1 UI labels and button routes: OK")
