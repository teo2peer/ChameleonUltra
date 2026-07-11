#!/usr/bin/env python3
"""Regression tests for the .key / .dic key importers (load_key_file / load_dic_file).

These previously mis-parsed their input: load_dic_file was a no-op stub (so
`--import-dic` silently loaded nothing) and load_key_file decoded a binary .key
file as UTF-8 text. Both now parse into 6-byte keys, the inverse of the
--export-key / --export-dic writers.
"""
import contextlib
import io
import os
import sys
import tempfile
import unittest
from types import SimpleNamespace

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_cli_unit as u  # noqa: E402


def _quiet(fn, *args):
    with contextlib.redirect_stdout(io.StringIO()):
        return fn(*args)


def _write_temp(mode, suffix, payload):
    with tempfile.NamedTemporaryFile(mode, suffix=suffix, delete=False) as f:
        f.write(payload)
        return f.name


class TestDicLoading(unittest.TestCase):
    def test_parses_hex_lines_skipping_comments_and_blanks(self):
        path = _write_temp(
            "w", ".dic",
            "FFFFFFFFFFFF\n# a comment\n\nA0A1A2A3A4A5\nnot-a-key\n001122334455\n",
        )
        try:
            keys = set()
            _quiet(u.load_dic_file, SimpleNamespace(name=path), keys)
        finally:
            os.unlink(path)
        self.assertEqual(
            keys,
            {
                bytes.fromhex("FFFFFFFFFFFF"),
                bytes.fromhex("A0A1A2A3A4A5"),
                bytes.fromhex("001122334455"),
            },
        )
        for k in keys:
            self.assertEqual(len(k), 6)

    def test_returns_existing_set_when_file_empty(self):
        path = _write_temp("w", ".dic", "\n#only comments\n")
        try:
            keys = {bytes.fromhex("112233445566")}
            _quiet(u.load_dic_file, SimpleNamespace(name=path), keys)
        finally:
            os.unlink(path)
        self.assertEqual(keys, {bytes.fromhex("112233445566")})


class TestKeyLoading(unittest.TestCase):
    def test_parses_binary_6_byte_chunks(self):
        raw = bytes.fromhex("FFFFFFFFFFFF") + bytes.fromhex("A0A1A2A3A4A5")
        path = _write_temp("wb", ".key", raw)
        try:
            keys = set()
            _quiet(u.load_key_file, SimpleNamespace(name=path), keys)
        finally:
            os.unlink(path)
        self.assertEqual(
            keys,
            {bytes.fromhex("FFFFFFFFFFFF"), bytes.fromhex("A0A1A2A3A4A5")},
        )

    def test_rejects_length_not_multiple_of_six(self):
        path = _write_temp("wb", ".key", b"12345")  # 5 bytes
        try:
            keys = set()
            _quiet(u.load_key_file, SimpleNamespace(name=path), keys)
        finally:
            os.unlink(path)
        self.assertEqual(keys, set())


if __name__ == "__main__":
    unittest.main()
