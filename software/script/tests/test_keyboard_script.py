#!/usr/bin/env python3
"""Golden and rejection tests for the keyboard script compiler."""

import json
import os
import sys
import unittest


CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

from keyboard_script import KeyboardScriptError, compile_script  # noqa: E402
from keyboard_layout import LAYOUT_CHOICES  # noqa: E402


def program_taps(program):
    """Decode an all-tap test program into modifier/usage pairs."""
    assert program[-1] == 0
    assert all(program[offset] == 2 for offset in range(0, len(program) - 1, 3))
    return [
        (program[offset + 1], program[offset + 2])
        for offset in range(0, len(program) - 1, 3)
    ]


class TestKeyboardCompilerGolden(unittest.TestCase):
    def test_text_uses_us_hid_mapping_and_ends_program(self):
        self.assertEqual(
            compile_script('TEXT "aA1! \\n\\t"'),
            bytes.fromhex("020004 020204 02001e 02021e 02002c 020028 02002b 00"),
        )

    def test_commands_comments_delay_keys_and_chords(self):
        source = """
            # ignored
            DELAY 1000
            KEY ENTER
            KEY ?
            CHORD LCTRL+LSHIFT+Z
            CHORD LCTRL+A
            CHORD RALT++
        """
        self.assertEqual(
            compile_script(source),
            bytes.fromhex("0103e8 020028 020238 02031d 020104 02422e 00"),
        )

    def test_all_named_ranges_are_available(self):
        program = compile_script(
            "KEY ESC\nKEY DELETE\nKEY LEFT\nKEY PAGEDOWN\nKEY F1\nKEY F12"
        )
        self.assertEqual(
            program, bytes.fromhex("020029 02004c 020050 02004e 02003a 020045 00")
        )

    def test_common_duckyscript_aliases(self):
        source = """REM standard syntax
GUI r
DELAY 500
STRING notepad
ENTER
CTRL SHIFT s
ALT d
STRINGLN done
"""
        self.assertEqual(
            compile_script(source),
            bytes.fromhex(
                "020815 0101f4 "
                "020011 020012 020017 020008 020013 020004 020007 "
                "020028 020316 020407 "
                "020007 020012 020011 020008 020028 00"
            ),
        )

    def test_ducky_string_accepts_shell_path_characters(self):
        program = compile_script(
            r"STRING C:\Users\%USERPROFILE%\AppData\run.bat&whoami"
        )
        self.assertEqual(program[-1], 0)
        self.assertGreater(len(program), 1)

    def test_layout_ascii_and_logical_key_positions(self):
        cases = {
            "us": ('TEXT "Az@|"', [(2, 0x04), (0, 0x1D), (2, 0x1F), (2, 0x31)]),
            "uk": ('TEXT "@#\\\\|"', [(2, 0x34), (0, 0x32), (0, 0x64), (2, 0x64)]),
            "es": (
                'TEXT "@[]{}\\\\|"',
                [
                    (64, 0x1F),
                    (64, 0x2F),
                    (64, 0x30),
                    (64, 0x34),
                    (64, 0x32),
                    (64, 0x35),
                    (64, 0x1E),
                ],
            ),
            "de": (
                'TEXT "yz@[]{}\\\\|"',
                [
                    (0, 0x1D),
                    (0, 0x1C),
                    (64, 0x14),
                    (64, 0x25),
                    (64, 0x26),
                    (64, 0x24),
                    (64, 0x27),
                    (64, 0x2D),
                    (64, 0x64),
                ],
            ),
            "fr": (
                'TEXT "azqmw0@[]{}\\\\|"',
                [
                    (0, 0x14),
                    (0, 0x1A),
                    (0, 0x04),
                    (0, 0x33),
                    (0, 0x1D),
                    (2, 0x27),
                    (64, 0x27),
                    (64, 0x22),
                    (64, 0x2D),
                    (64, 0x21),
                    (64, 0x2E),
                    (64, 0x25),
                    (64, 0x23),
                ],
            ),
            "it": (
                'TEXT "@#[]{}\\\\|^"',
                [
                    (64, 0x33),
                    (64, 0x34),
                    (64, 0x2F),
                    (64, 0x30),
                    (66, 0x2F),
                    (66, 0x30),
                    (0, 0x35),
                    (2, 0x35),
                    (2, 0x2E),
                ],
            ),
            "pt": (
                'TEXT "@#[]{}\\\\|"',
                [
                    (64, 0x1F),
                    (2, 0x20),
                    (64, 0x25),
                    (64, 0x26),
                    (64, 0x24),
                    (64, 0x27),
                    (0, 0x35),
                    (2, 0x35),
                ],
            ),
        }
        self.assertEqual(set(cases), set(LAYOUT_CHOICES))
        for layout, (source, expected) in cases.items():
            with self.subTest(layout=layout):
                self.assertEqual(program_taps(compile_script(source, layout)), expected)

        self.assertEqual(
            program_taps(compile_script("CTRL z\nKEY QUESTION", "fr")),
            [(1, 0x1A), (2, 0x10)],
        )
        self.assertEqual(program_taps(compile_script("CTRL z", "de")), [(1, 0x1C)])

    def test_printable_ascii_coverage(self):
        printable_ascii = "".join(chr(codepoint) for codepoint in range(0x20, 0x7F))
        for layout in LAYOUT_CHOICES:
            with self.subTest(layout=layout):
                compile_script("TEXT " + json.dumps(printable_ascii), layout)

    def test_spanish_national_characters_and_nfc(self):
        characters = "\u00f1\u00d1\u00e1\u00c9\u00fc\u00dc\u00bf\u00a1\u00e7\u00c7"
        self.assertEqual(
            program_taps(compile_script('TEXT "' + characters + '"', "es")),
            [
                (0, 0x33),
                (2, 0x33),
                (0, 0x34),
                (0, 0x04),
                (0, 0x34),
                (2, 0x08),
                (2, 0x34),
                (0, 0x18),
                (2, 0x34),
                (2, 0x18),
                (2, 0x2E),
                (0, 0x2E),
                (0, 0x32),
                (2, 0x32),
            ],
        )
        self.assertEqual(
            compile_script('TEXT "n\\u0303a\\u0301"', "es"),
            compile_script('TEXT "\u00f1\u00e1"', "es"),
        )
        self.assertEqual(
            compile_script("STRING ma\u00f1ana", "es"),
            compile_script('TEXT "ma\u00f1ana"', "es"),
        )

    def test_german_direct_and_dead_key_characters(self):
        self.assertEqual(
            program_taps(
                compile_script(
                    'TEXT "\u00e4\u00f6\u00fc\u00c4\u00d6\u00dc'
                    '\u00df\u00e1\u00c0\u00ea"',
                    "de",
                )
            ),
            [
                (0, 0x34),
                (0, 0x33),
                (0, 0x2F),
                (2, 0x34),
                (2, 0x33),
                (2, 0x2F),
                (0, 0x2D),
                (0, 0x2E),
                (0, 0x04),
                (2, 0x2E),
                (2, 0x04),
                (0, 0x35),
                (0, 0x08),
            ],
        )

    def test_french_direct_and_deterministic_dead_keys(self):
        self.assertEqual(
            program_taps(
                compile_script(
                    'TEXT "\u00e9\u00e8\u00e0\u00f9\u00e7\u00e2\u00ca\u00fc\u0178"',
                    "fr",
                )
            ),
            [
                (0, 0x1F),
                (0, 0x24),
                (0, 0x27),
                (0, 0x34),
                (0, 0x26),
                (0, 0x2F),
                (0, 0x14),
                (0, 0x2F),
                (2, 0x08),
                (2, 0x2F),
                (0, 0x18),
                (2, 0x2F),
                (2, 0x1C),
            ],
        )

    def test_italian_direct_national_characters(self):
        self.assertEqual(
            program_taps(
                compile_script(
                    'TEXT "\u00e0\u00e8\u00e9\u00ec\u00f2\u00f9\u00e7"', "it"
                )
            ),
            [
                (0, 0x34),
                (0, 0x2F),
                (2, 0x2F),
                (0, 0x2E),
                (0, 0x33),
                (0, 0x32),
                (2, 0x33),
            ],
        )

    def test_portuguese_compositions_and_symbols(self):
        self.assertEqual(
            program_taps(
                compile_script(
                    'TEXT "\u00e7\u00c7\u00e1\u00c0\u00ea\u00d5'
                    '\u00fc\u00dc\u00ba\u00aa\u00ab\u00bb"',
                    "pt",
                )
            ),
            [
                (0, 0x33),
                (2, 0x33),
                (0, 0x30),
                (0, 0x04),
                (2, 0x30),
                (2, 0x04),
                (2, 0x32),
                (0, 0x08),
                (0, 0x32),
                (2, 0x12),
                (64, 0x2F),
                (0, 0x18),
                (64, 0x2F),
                (2, 0x18),
                (0, 0x34),
                (2, 0x34),
                (0, 0x2E),
                (2, 0x2E),
            ],
        )

    def test_uk_national_symbols(self):
        self.assertEqual(
            program_taps(compile_script('TEXT "\u00a3\u20ac\u00ac\u00a6"', "uk")),
            [(2, 0x20), (64, 0x21), (2, 0x35), (64, 0x35)],
        )

    def test_all_requested_national_character_sets_compile(self):
        cases = {
            "es": (
                "\u00f1\u00d1\u00e1\u00e9\u00ed\u00f3\u00fa\u00c1\u00c9"
                "\u00cd\u00d3\u00da\u00fc\u00dc\u00bf\u00a1\u00e7\u00c7"
                "\u00ba\u00aa\u00b7\u20ac\u00ac"
            ),
            "de": (
                "\u00e4\u00f6\u00fc\u00c4\u00d6\u00dc\u00df\u00e1\u00c9"
                "\u00ec\u00d2\u00f9\u00c2\u00ea\u00ee\u00f4\u00db"
                "\u00b0\u00a7\u00b2\u00b3\u20ac\u00b5"
            ),
            "fr": (
                "\u00e9\u00e8\u00e0\u00f9\u00e7\u00e2\u00ea\u00ee\u00f4"
                "\u00fb\u00c2\u00ca\u00ce\u00d4\u00db\u00e4\u00eb\u00ef"
                "\u00f6\u00fc\u00c4\u00cb\u00cf\u00d6\u00dc"
                "\u00b2\u00b0\u00a3\u00b5\u00a7\u20ac"
            ),
            "it": (
                "\u00e0\u00e8\u00e9\u00ec\u00f2\u00f9\u00e7"
                "\u00a3\u00b0\u00a7\u20ac"
            ),
            "pt": (
                "\u00e7\u00c7\u00e1\u00c9\u00ed\u00d3\u00fa\u00e0\u00c0"
                "\u00e2\u00ca\u00f4\u00e3\u00d5\u00fc\u00dc\u00ba\u00aa"
                "\u00ab\u00bb\u00a3\u00a7\u20ac"
            ),
            "uk": "\u00a3\u20ac\u00ac\u00a6",
        }
        for layout, text in cases.items():
            with self.subTest(layout=layout):
                compile_script("TEXT " + json.dumps(text), layout)

    def test_dead_ascii_is_terminated_with_space(self):
        cases = {
            "es": ("^", [(2, 0x2F), (0, 0x2C)]),
            "de": ("`", [(2, 0x2E), (0, 0x2C)]),
            "fr": ("~", [(64, 0x1F), (0, 0x2C)]),
            "pt": ("^", [(2, 0x32), (0, 0x2C)]),
        }
        for layout, (character, expected) in cases.items():
            with self.subTest(layout=layout):
                source = "TEXT " + repr(character).replace("'", '"')
                self.assertEqual(program_taps(compile_script(source, layout)), expected)


class TestKeyboardCompilerRejection(unittest.TestCase):
    def assert_invalid(self, source, message=None):
        with self.assertRaises(KeyboardScriptError) as caught:
            compile_script(source)
        if message is not None:
            self.assertIn(message, str(caught.exception))

    def test_text_is_exact_json_and_ascii_only(self):
        invalid = [
            "TEXT hello",
            'TEXT "ok" trailing',
            "TEXT 12",
            'TEXT "caf\\u00e9"',
            'TEXT "bad\\rreturn"',
        ]
        for source in invalid:
            with self.subTest(source=source):
                self.assert_invalid(source, "line 1")

    def test_delay_syntax_range_and_aggregate_are_strict(self):
        for source in ("DELAY 0", "DELAY 10001", "DELAY +1", "DELAY 1.0"):
            with self.subTest(source=source):
                self.assert_invalid(source)
        self.assert_invalid("\n".join(["DELAY 10000"] * 7), "60000")

    def test_keys_and_chords_are_unambiguous(self):
        invalid = [
            "KEY enter",
            "KEY a",
            "KEY NOPE",
            "KEY A B",
            "CHORD A",
            "CHORD LCTRL+LCTRL+A",
            "CHORD LCTRL+A+B",
            "CTRL LCTRL A",
            "GUI",
        ]
        for source in invalid:
            with self.subTest(source=source):
                self.assert_invalid(source)

    def test_tap_and_program_limits(self):
        self.assertEqual(len(compile_script('TEXT "' + "a" * 1024 + '"')), 3073)
        self.assert_invalid('TEXT "' + "a" * 1025 + '"', "1024 taps")
        self.assert_invalid("\n".join(["DELAY 1"] * 1366), "4096 bytes")

    def test_unknown_commands_are_rejected_with_line(self):
        self.assert_invalid("\n# comment\nTYPE x", "line 3")

    def test_invalid_layout_is_rejected_before_compilation(self):
        with self.assertRaisesRegex(KeyboardScriptError, "unsupported keyboard layout"):
            compile_script('TEXT "ok"', "dvorak")

    def test_stateless_impossible_characters_are_rejected(self):
        cases = {
            "fr": "\u00c9\u00c8\u00c0\u00d9\u00c7",
            "it": "\u00c0\u00c8\u00c9\u00cc\u00d2\u00d9\u00c7",
            "us": "\u00e9",
        }
        for layout, characters in cases.items():
            for character in characters:
                with self.subTest(layout=layout, character=character):
                    with self.assertRaisesRegex(
                        KeyboardScriptError, "cannot type U\\+"
                    ):
                        compile_script('TEXT "' + character + '"', layout)

    def test_composed_characters_count_as_multiple_taps(self):
        self.assertEqual(
            len(compile_script('TEXT "' + "\u00e1" * 512 + '"', "es")), 3073
        )
        with self.assertRaisesRegex(KeyboardScriptError, "1024 taps"):
            compile_script('TEXT "' + "\u00e1" * 513 + '"', "es")


if __name__ == "__main__":
    unittest.main()
