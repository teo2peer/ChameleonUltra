# SPDX-License-Identifier: GPL-2.0-or-later
"""Host keyboard-layout mappings for keyboard script text.

Layout facts are derived from QMK's GPL-2.0-or-later ``keymap_extras`` headers:
https://github.com/qmk/qmk_firmware/tree/master/quantum/keymap_extras
"""

LSHIFT = 0x02
RALT = 0x40

SPACE = 0x2C
ENTER = 0x28
TAB = 0x2B
NUHS = 0x32
NUBS = 0x64

LAYOUT_CHOICES = ("us", "uk", "es", "de", "fr", "it", "pt")


def _tap(usage, modifier=0):
    return ((modifier, usage),)


def _letters(order):
    mapping = {}
    for character, usage_character in zip("abcdefghijklmnopqrstuvwxyz", order):
        usage = 0x04 + ord(usage_character) - ord("a")
        mapping[character] = _tap(usage)
        mapping[character.upper()] = _tap(usage, LSHIFT)
    return mapping


def _layout(order, digits, punctuation):
    mapping = _letters(order)
    mapping.update(
        {
            character: _tap(usage, modifier)
            for character, (modifier, usage) in digits.items()
        }
    )
    mapping.update(
        {
            character: _tap(usage, modifier)
            for character, (modifier, usage) in punctuation.items()
        }
    )
    mapping.update({" ": _tap(SPACE), "\n": _tap(ENTER), "\t": _tap(TAB)})
    return mapping


def _dead(mapping, dead_tap, combinations):
    for composed, base in combinations.items():
        mapping[composed] = (dead_tap, mapping[base][0])


_DIGITS = {
    **{str(number): (0, 0x1D + number) for number in range(1, 10)},
    "0": (0, 0x27),
}

_US_PUNCTUATION = {
    "-": (0, 0x2D),
    "_": (LSHIFT, 0x2D),
    "=": (0, 0x2E),
    "+": (LSHIFT, 0x2E),
    "[": (0, 0x2F),
    "{": (LSHIFT, 0x2F),
    "]": (0, 0x30),
    "}": (LSHIFT, 0x30),
    "\\": (0, 0x31),
    "|": (LSHIFT, 0x31),
    ";": (0, 0x33),
    ":": (LSHIFT, 0x33),
    "'": (0, 0x34),
    '"': (LSHIFT, 0x34),
    "`": (0, 0x35),
    "~": (LSHIFT, 0x35),
    ",": (0, 0x36),
    "<": (LSHIFT, 0x36),
    ".": (0, 0x37),
    ">": (LSHIFT, 0x37),
    "/": (0, 0x38),
    "?": (LSHIFT, 0x38),
    "!": (LSHIFT, 0x1E),
    "@": (LSHIFT, 0x1F),
    "#": (LSHIFT, 0x20),
    "$": (LSHIFT, 0x21),
    "%": (LSHIFT, 0x22),
    "^": (LSHIFT, 0x23),
    "&": (LSHIFT, 0x24),
    "*": (LSHIFT, 0x25),
    "(": (LSHIFT, 0x26),
    ")": (LSHIFT, 0x27),
}

_UK_PUNCTUATION = {
    **_US_PUNCTUATION,
    '"': (LSHIFT, 0x1F),
    "@": (LSHIFT, 0x34),
    "#": (0, NUHS),
    "~": (LSHIFT, NUHS),
    "\\": (0, NUBS),
    "|": (LSHIFT, NUBS),
}

_ES_PUNCTUATION = {
    "!": (LSHIFT, 0x1E),
    '"': (LSHIFT, 0x1F),
    "#": (RALT, 0x20),
    "$": (LSHIFT, 0x21),
    "%": (LSHIFT, 0x22),
    "&": (LSHIFT, 0x23),
    "'": (0, 0x2D),
    "(": (LSHIFT, 0x25),
    ")": (LSHIFT, 0x26),
    "*": (LSHIFT, 0x30),
    "+": (0, 0x30),
    ",": (0, 0x36),
    "-": (0, 0x38),
    ".": (0, 0x37),
    "/": (LSHIFT, 0x24),
    ":": (LSHIFT, 0x37),
    ";": (LSHIFT, 0x36),
    "<": (0, NUBS),
    "=": (LSHIFT, 0x27),
    ">": (LSHIFT, NUBS),
    "?": (LSHIFT, 0x2D),
    "@": (RALT, 0x1F),
    "[": (RALT, 0x2F),
    "\\": (RALT, 0x35),
    "]": (RALT, 0x30),
    "_": (LSHIFT, 0x38),
    "{": (RALT, 0x34),
    "|": (RALT, 0x1E),
    "}": (RALT, NUHS),
    "~": (RALT, 0x21),
}

_DE_PUNCTUATION = {
    "!": (LSHIFT, 0x1E),
    '"': (LSHIFT, 0x1F),
    "#": (0, NUHS),
    "$": (LSHIFT, 0x21),
    "%": (LSHIFT, 0x22),
    "&": (LSHIFT, 0x23),
    "'": (LSHIFT, NUHS),
    "(": (LSHIFT, 0x25),
    ")": (LSHIFT, 0x26),
    "*": (LSHIFT, 0x30),
    "+": (0, 0x30),
    ",": (0, 0x36),
    "-": (0, 0x38),
    ".": (0, 0x37),
    "/": (LSHIFT, 0x24),
    ":": (LSHIFT, 0x37),
    ";": (LSHIFT, 0x36),
    "<": (0, NUBS),
    "=": (LSHIFT, 0x27),
    ">": (LSHIFT, NUBS),
    "?": (LSHIFT, 0x2D),
    "@": (RALT, 0x14),
    "[": (RALT, 0x25),
    "\\": (RALT, 0x2D),
    "]": (RALT, 0x26),
    "_": (LSHIFT, 0x38),
    "{": (RALT, 0x24),
    "|": (RALT, NUBS),
    "}": (RALT, 0x27),
    "~": (RALT, 0x30),
}

_FR_DIGITS = {str(number): (LSHIFT, 0x1D + number) for number in range(1, 10)}
_FR_DIGITS["0"] = (LSHIFT, 0x27)
_FR_PUNCTUATION = {
    "!": (0, 0x38),
    '"': (0, 0x20),
    "#": (RALT, 0x20),
    "$": (0, 0x30),
    "%": (LSHIFT, 0x34),
    "&": (0, 0x1E),
    "'": (0, 0x21),
    "(": (0, 0x22),
    ")": (0, 0x2D),
    "*": (0, NUHS),
    "+": (LSHIFT, 0x2E),
    ",": (0, 0x10),
    "-": (0, 0x23),
    ".": (LSHIFT, 0x36),
    "/": (LSHIFT, 0x37),
    ":": (0, 0x37),
    ";": (0, 0x36),
    "<": (0, NUBS),
    "=": (0, 0x2E),
    ">": (LSHIFT, NUBS),
    "?": (LSHIFT, 0x10),
    "@": (RALT, 0x27),
    "[": (RALT, 0x22),
    "\\": (RALT, 0x25),
    "]": (RALT, 0x2D),
    "_": (0, 0x25),
    "{": (RALT, 0x21),
    "|": (RALT, 0x23),
    "}": (RALT, 0x2E),
}

_IT_PUNCTUATION = {
    "!": (LSHIFT, 0x1E),
    '"': (LSHIFT, 0x1F),
    "#": (RALT, 0x34),
    "$": (LSHIFT, 0x21),
    "%": (LSHIFT, 0x22),
    "&": (LSHIFT, 0x23),
    "'": (0, 0x2D),
    "(": (LSHIFT, 0x25),
    ")": (LSHIFT, 0x26),
    "*": (LSHIFT, 0x30),
    "+": (0, 0x30),
    ",": (0, 0x36),
    "-": (0, 0x38),
    ".": (0, 0x37),
    "/": (LSHIFT, 0x24),
    ":": (LSHIFT, 0x37),
    ";": (LSHIFT, 0x36),
    "<": (0, NUBS),
    "=": (LSHIFT, 0x27),
    ">": (LSHIFT, NUBS),
    "?": (LSHIFT, 0x2D),
    "@": (RALT, 0x33),
    "[": (RALT, 0x2F),
    "\\": (0, 0x35),
    "]": (RALT, 0x30),
    "_": (LSHIFT, 0x38),
    "{": (RALT | LSHIFT, 0x2F),
    "|": (LSHIFT, 0x35),
    "}": (RALT | LSHIFT, 0x30),
    "^": (LSHIFT, 0x2E),
}

_PT_PUNCTUATION = {
    "!": (LSHIFT, 0x1E),
    '"': (LSHIFT, 0x1F),
    "#": (LSHIFT, 0x20),
    "$": (LSHIFT, 0x21),
    "%": (LSHIFT, 0x22),
    "&": (LSHIFT, 0x23),
    "'": (0, 0x2D),
    "(": (LSHIFT, 0x25),
    ")": (LSHIFT, 0x26),
    "*": (LSHIFT, 0x2F),
    "+": (0, 0x2F),
    ",": (0, 0x36),
    "-": (0, 0x38),
    ".": (0, 0x37),
    "/": (LSHIFT, 0x24),
    ":": (LSHIFT, 0x37),
    ";": (LSHIFT, 0x36),
    "<": (0, NUBS),
    "=": (LSHIFT, 0x27),
    ">": (LSHIFT, NUBS),
    "?": (LSHIFT, 0x2D),
    "@": (RALT, 0x1F),
    "[": (RALT, 0x25),
    "\\": (0, 0x35),
    "]": (RALT, 0x26),
    "_": (LSHIFT, 0x38),
    "{": (RALT, 0x24),
    "|": (LSHIFT, 0x35),
    "}": (RALT, 0x27),
}


_LAYOUTS = {
    "us": _layout("abcdefghijklmnopqrstuvwxyz", _DIGITS, _US_PUNCTUATION),
    "uk": _layout("abcdefghijklmnopqrstuvwxyz", _DIGITS, _UK_PUNCTUATION),
    "es": _layout("abcdefghijklmnopqrstuvwxyz", _DIGITS, _ES_PUNCTUATION),
    "de": _layout("abcdefghijklmnopqrstuvwxzy", _DIGITS, _DE_PUNCTUATION),
    "fr": _layout("abcdefghijklmnopqrstuvwxyz", _FR_DIGITS, _FR_PUNCTUATION),
    "it": _layout("abcdefghijklmnopqrstuvwxyz", _DIGITS, _IT_PUNCTUATION),
    "pt": _layout("abcdefghijklmnopqrstuvwxyz", _DIGITS, _PT_PUNCTUATION),
}

# Correct the five AZERTY letters that do not share their QWERTY HID usages.
for character, usage in {"a": 0x14, "z": 0x1A, "q": 0x04, "m": 0x33, "w": 0x1D}.items():
    _LAYOUTS["fr"][character] = _tap(usage)
    _LAYOUTS["fr"][character.upper()] = _tap(usage, LSHIFT)

# Dead ASCII symbols require Space so they cannot affect the following character.
for layout, character, tap in (
    ("es", "`", (0, 0x2F)),
    ("es", "^", (LSHIFT, 0x2F)),
    ("de", "`", (LSHIFT, 0x2E)),
    ("de", "^", (0, 0x35)),
    ("fr", "`", (RALT, 0x24)),
    ("fr", "^", (0, 0x2F)),
    ("fr", "~", (RALT, 0x1F)),
    ("pt", "`", (LSHIFT, 0x30)),
    ("pt", "^", (LSHIFT, NUHS)),
    ("pt", "~", (0, NUHS)),
):
    _LAYOUTS[layout][character] = (tap, (0, SPACE))

# Direct national characters.
_LAYOUTS["uk"].update(
    {
        "\u00a3": _tap(0x20, LSHIFT),
        "\u20ac": _tap(0x21, RALT),
        "\u00ac": _tap(0x35, LSHIFT),
        "\u00a6": _tap(0x35, RALT),
    }
)
_LAYOUTS["es"].update(
    {
        "\u00ba": _tap(0x35),
        "\u00aa": _tap(0x35, LSHIFT),
        "\u00b7": _tap(0x20, LSHIFT),
        "\u00f1": _tap(0x33),
        "\u00d1": _tap(0x33, LSHIFT),
        "\u00e7": _tap(NUHS),
        "\u00c7": _tap(NUHS, LSHIFT),
        "\u00bf": _tap(0x2E, LSHIFT),
        "\u00a1": _tap(0x2E),
        "\u20ac": _tap(0x22, RALT),
        "\u00ac": _tap(0x23, RALT),
    }
)
_LAYOUTS["de"].update(
    {
        "\u00e4": _tap(0x34),
        "\u00c4": _tap(0x34, LSHIFT),
        "\u00f6": _tap(0x33),
        "\u00d6": _tap(0x33, LSHIFT),
        "\u00fc": _tap(0x2F),
        "\u00dc": _tap(0x2F, LSHIFT),
        "\u00df": _tap(0x2D),
        "\u00b0": _tap(0x35, LSHIFT),
        "\u00a7": _tap(0x20, LSHIFT),
        "\u00b2": _tap(0x1F, RALT),
        "\u00b3": _tap(0x20, RALT),
        "\u20ac": _tap(0x08, RALT),
        "\u00b5": _tap(0x10, RALT),
    }
)
_LAYOUTS["fr"].update(
    {
        "\u00b2": _tap(0x35),
        "\u00b0": _tap(0x2D, LSHIFT),
        "\u00a3": _tap(0x30, LSHIFT),
        "\u00b5": _tap(NUHS, LSHIFT),
        "\u00a7": _tap(0x38, LSHIFT),
        "\u20ac": _tap(0x08, RALT),
        "\u00e9": _tap(0x1F),
        "\u00e8": _tap(0x24),
        "\u00e0": _tap(0x27),
        "\u00f9": _tap(0x34),
        "\u00e7": _tap(0x26),
    }
)
_LAYOUTS["it"].update(
    {
        "\u00a3": _tap(0x20, LSHIFT),
        "\u00b0": _tap(0x34, LSHIFT),
        "\u00a7": _tap(NUHS, LSHIFT),
        "\u20ac": _tap(0x08, RALT),
        "`": _tap(0x2D, RALT),
        "~": _tap(0x2E, RALT),
        "\u00e0": _tap(0x34),
        "\u00e8": _tap(0x2F),
        "\u00e9": _tap(0x2F, LSHIFT),
        "\u00ec": _tap(0x2E),
        "\u00f2": _tap(0x33),
        "\u00f9": _tap(NUHS),
        "\u00e7": _tap(0x33, LSHIFT),
    }
)
_LAYOUTS["pt"].update(
    {
        "\u00a3": _tap(0x20, RALT),
        "\u00a7": _tap(0x21, RALT),
        "\u20ac": _tap(0x08, RALT),
        "\u00e7": _tap(0x33),
        "\u00c7": _tap(0x33, LSHIFT),
        "\u00ba": _tap(0x34),
        "\u00aa": _tap(0x34, LSHIFT),
        "\u00ab": _tap(0x2E),
        "\u00bb": _tap(0x2E, LSHIFT),
    }
)


def _accented(layout, dead_tap, pairs):
    mapping = _LAYOUTS[layout]
    _dead(mapping, dead_tap, {composed: base for composed, base in pairs})


_accented(
    "es",
    (0, 0x34),
    zip("\u00e1\u00e9\u00ed\u00f3\u00fa\u00c1\u00c9\u00cd\u00d3\u00da", "aeiouAEIOU"),
)
_accented(
    "es",
    (LSHIFT, 0x34),
    zip("\u00e4\u00eb\u00ef\u00f6\u00fc\u00c4\u00cb\u00cf\u00d6\u00dc", "aeiouAEIOU"),
)

for dead_tap, composed in (
    (
        (0, 0x2E),
        "\u00e1\u00e9\u00ed\u00f3\u00fa\u00fd\u00c1\u00c9\u00cd\u00d3\u00da\u00dd",
    ),
    ((LSHIFT, 0x2E), "\u00e0\u00e8\u00ec\u00f2\u00f9\u00c0\u00c8\u00cc\u00d2\u00d9"),
    ((0, 0x35), "\u00e2\u00ea\u00ee\u00f4\u00fb\u00c2\u00ca\u00ce\u00d4\u00db"),
):
    bases = "aeiouyAEIOUY" if len(composed) == 12 else "aeiouAEIOU"
    _accented("de", dead_tap, zip(composed, bases))

for dead_tap, composed in (
    ((0, 0x2F), "\u00e2\u00ea\u00ee\u00f4\u00fb\u00c2\u00ca\u00ce\u00d4\u00db"),
    (
        (LSHIFT, 0x2F),
        "\u00e4\u00eb\u00ef\u00f6\u00fc\u00ff\u00c4\u00cb\u00cf\u00d6\u00dc\u0178",
    ),
):
    bases = "aeiouAEIOU" if len(composed) == 10 else "aeiouyAEIOUY"
    _accented("fr", dead_tap, zip(composed, bases))

for dead_tap, composed, bases in (
    (
        (0, 0x30),
        "\u00e1\u00e9\u00ed\u00f3\u00fa\u00fd\u00c1\u00c9\u00cd\u00d3\u00da\u00dd",
        "aeiouyAEIOUY",
    ),
    (
        (LSHIFT, 0x30),
        "\u00e0\u00e8\u00ec\u00f2\u00f9\u00c0\u00c8\u00cc\u00d2\u00d9",
        "aeiouAEIOU",
    ),
    (
        (LSHIFT, NUHS),
        "\u00e2\u00ea\u00ee\u00f4\u00fb\u00c2\u00ca\u00ce\u00d4\u00db",
        "aeiouAEIOU",
    ),
    ((0, NUHS), "\u00e3\u00f5\u00f1\u00c3\u00d5\u00d1", "aonAON"),
    (
        (RALT, 0x2F),
        "\u00e4\u00eb\u00ef\u00f6\u00fc\u00ff\u00c4\u00cb\u00cf\u00d6\u00dc\u0178",
        "aeiouyAEIOUY",
    ),
):
    _accented("pt", dead_tap, zip(composed, bases))


def character_taps(layout, character):
    """Return the tap sequence that types one text character, or ``None``."""
    mapping = _LAYOUTS.get(layout)
    return None if mapping is None else mapping.get(character)


def logical_key_tap(layout, character):
    """Return the single tap for a logical KEY/CHORD character, or ``None``."""
    mapping = _LAYOUTS.get(layout)
    if mapping is None:
        return None
    if "A" <= character <= "Z":
        taps = mapping.get(character.lower())
    else:
        taps = mapping.get(character)
    return taps[0] if taps else None
