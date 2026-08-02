"""Bounded compiler for Chameleon and common DuckyScript keyboard syntax."""

import json
import struct
import unicodedata

from keyboard_layout import LAYOUT_CHOICES, character_taps, logical_key_tap


MAX_PROGRAM_BYTES = 4096
MAX_TAPS = 1024
MAX_TOTAL_DELAY_MS = 60000

OP_END = 0x00
OP_DELAY = 0x01
OP_TAP = 0x02


class KeyboardScriptError(ValueError):
    """A keyboard script is invalid or exceeds a firmware limit."""


MODIFIERS = {
    "LCTRL": 0x01,
    "LSHIFT": 0x02,
    "LALT": 0x04,
    "LGUI": 0x08,
    "RCTRL": 0x10,
    "RSHIFT": 0x20,
    "RALT": 0x40,
    "RGUI": 0x80,
}

MODIFIER_ALIASES = {
    "CTRL": "LCTRL",
    "CONTROL": "LCTRL",
    "SHIFT": "LSHIFT",
    "ALT": "LALT",
    "GUI": "LGUI",
    "WINDOWS": "LGUI",
    "COMMAND": "LGUI",
    **{name: name for name in MODIFIERS},
}

NAMED_KEYS = {
    "ENTER": 0x28,
    "ESC": 0x29,
    "TAB": 0x2B,
    "SPACE": 0x2C,
    "BACKSPACE": 0x2A,
    "DELETE": 0x4C,
    "RIGHT": 0x4F,
    "LEFT": 0x50,
    "DOWN": 0x51,
    "UP": 0x52,
    "HOME": 0x4A,
    "END": 0x4D,
    "PAGEUP": 0x4B,
    "PAGEDOWN": 0x4E,
}
NAMED_KEYS.update({f"F{number}": 0x39 + number for number in range(1, 13)})

_PUNCTUATION_NAMES = {
    "MINUS": "-", "UNDERSCORE": "_", "EQUAL": "=", "PLUS": "+",
    "LEFTBRACKET": "[", "LEFTBRACE": "{",
    "RIGHTBRACKET": "]", "RIGHTBRACE": "}",
    "BACKSLASH": "\\", "PIPE": "|", "SEMICOLON": ";", "COLON": ":",
    "APOSTROPHE": "'", "QUOTE": '"', "GRAVE": "`", "TILDE": "~",
    "COMMA": ",", "LESS": "<", "PERIOD": ".", "GREATER": ">",
    "SLASH": "/", "QUESTION": "?", "EXCLAMATION": "!", "AT": "@",
    "HASH": "#", "DOLLAR": "$", "PERCENT": "%", "CARET": "^",
    "AMPERSAND": "&", "ASTERISK": "*", "LEFTPAREN": "(", "RIGHTPAREN": ")",
}


def key_to_hid(key, layout="us"):
    """Return ``(modifier, HID usage)`` for one supported key."""
    if not isinstance(key, str) or not key:
        raise KeyboardScriptError("key must not be empty")

    if len(key) == 1:
        if "a" <= key <= "z":
            raise KeyboardScriptError(f"unsupported key {key!r}")
        tap = logical_key_tap(layout, key)
        if tap is not None:
            return tap
    upper = key.upper()
    if upper in NAMED_KEYS and key == upper:
        return 0, NAMED_KEYS[upper]
    if upper in _PUNCTUATION_NAMES and key == upper:
        tap = logical_key_tap(layout, _PUNCTUATION_NAMES[upper])
        if tap is not None:
            return tap
    raise KeyboardScriptError(f"unsupported key {key!r}")


def _parse_chord(value, layout):
    if value.endswith("++"):
        parts = value[:-2].split("+") + ["+"]
    else:
        parts = value.split("+")
    if len(parts) < 2 or any(not part for part in parts):
        raise KeyboardScriptError(
            "CHORD requires one or more modifiers and exactly one key")

    modifier = 0
    seen = set()
    for name in parts[:-1]:
        canonical = MODIFIER_ALIASES.get(name)
        if canonical is None:
            raise KeyboardScriptError(f"invalid chord modifier {name!r}")
        if canonical in seen:
            raise KeyboardScriptError(f"duplicate chord modifier {name!r}")
        seen.add(canonical)
        modifier |= MODIFIERS[canonical]

    key_modifier, usage = key_to_hid(parts[-1], layout)
    return modifier | key_modifier, usage


def _parse_ducky_chord(parts, layout):
    if len(parts) < 2:
        raise KeyboardScriptError(
            "modifier command requires one or more modifiers and exactly one key")
    modifier = 0
    seen = set()
    for name in parts[:-1]:
        canonical = MODIFIER_ALIASES.get(name.upper())
        if canonical is None:
            raise KeyboardScriptError(f"invalid modifier {name!r}")
        if canonical in seen:
            raise KeyboardScriptError(f"duplicate modifier {name!r}")
        seen.add(canonical)
        modifier |= MODIFIERS[canonical]
    key_modifier, usage = key_to_hid(parts[-1].upper(), layout)
    return modifier | key_modifier, usage


def compile_script(source, layout="us"):
    """Compile script source to the bounded firmware bytecode format."""
    if not isinstance(source, str):
        raise TypeError("source must be a string")
    if layout not in LAYOUT_CHOICES:
        raise KeyboardScriptError(
            f"unsupported keyboard layout {layout!r}; choose from "
            f"{', '.join(LAYOUT_CHOICES)}")

    program = bytearray()
    tap_count = 0
    total_delay = 0

    def add_tap(modifier, usage):
        nonlocal tap_count
        tap_count += 1
        if tap_count > MAX_TAPS:
            raise KeyboardScriptError(f"script exceeds {MAX_TAPS} taps")
        program.extend((OP_TAP, modifier, usage))

    def add_text(text, command):
        for character in unicodedata.normalize("NFC", text):
            taps = character_taps(layout, character)
            if taps is None:
                raise KeyboardScriptError(
                    f"{command} cannot type U+{ord(character):04X} "
                    f"on the {layout!r} layout")
            for tap in taps:
                add_tap(*tap)

    for line_number, raw_line in enumerate(source.splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        command, separator, argument = line.partition(" ")
        argument = argument.strip() if separator else ""
        if command == "REM":
            continue

        try:
            if command == "TEXT":
                if not argument or not argument.startswith('"'):
                    raise KeyboardScriptError("TEXT requires one JSON quoted string")
                try:
                    text = json.loads(argument)
                except (json.JSONDecodeError, UnicodeDecodeError) as exc:
                    raise KeyboardScriptError(
                        "TEXT requires one valid JSON quoted string") from exc
                if not isinstance(text, str):
                    raise KeyboardScriptError("TEXT value must be a JSON string")
                add_text(text, "TEXT")
            elif command in ("STRING", "STRINGLN"):
                add_text(argument, command)
                if command == "STRINGLN":
                    add_tap(*key_to_hid("ENTER", layout))
            elif command == "DELAY":
                if not argument.isascii() or not argument.isdecimal():
                    raise KeyboardScriptError("DELAY must be a decimal integer")
                delay = int(argument, 10)
                if not 1 <= delay <= 10000:
                    raise KeyboardScriptError("DELAY must be 1..10000 ms")
                total_delay += delay
                if total_delay > MAX_TOTAL_DELAY_MS:
                    raise KeyboardScriptError(
                        f"explicit delays exceed {MAX_TOTAL_DELAY_MS} ms")
                program.extend((OP_DELAY,))
                program.extend(struct.pack("!H", delay))
            elif command == "KEY":
                if not argument or any(character.isspace() for character in argument):
                    raise KeyboardScriptError("KEY requires exactly one named key")
                add_tap(*key_to_hid(argument, layout))
            elif command == "CHORD":
                if not argument or any(character.isspace() for character in argument):
                    raise KeyboardScriptError(
                        "CHORD requires modifiers joined with + and exactly one key")
                add_tap(*_parse_chord(argument, layout))
            elif command.upper() in MODIFIER_ALIASES:
                add_tap(*_parse_ducky_chord(
                    [command, *argument.split()], layout))
            elif not argument:
                add_tap(*key_to_hid(command.upper(), layout))
            else:
                raise KeyboardScriptError(f"unknown command {command!r}")
        except KeyboardScriptError as exc:
            raise KeyboardScriptError(f"line {line_number}: {exc}") from exc

        if len(program) + 1 > MAX_PROGRAM_BYTES:
            raise KeyboardScriptError(
                f"compiled program exceeds {MAX_PROGRAM_BYTES} bytes")

    program.append(OP_END)
    return bytes(program)


compile_keyboard_script = compile_script
