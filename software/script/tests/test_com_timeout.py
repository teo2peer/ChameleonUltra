#!/usr/bin/env python3
"""Regression tests for ChameleonCom.thread_check_timeout.

Two defects are covered:
  * it iterated the live wait_response_map, which other threads mutate — a
    concurrent insert/delete could raise "dictionary changed size during
    iteration". It now iterates a snapshot.
  * a callback-mode timeout never removed its entry, so the callback re-fired on
    every ~100 ms loop forever. It now fires exactly once.
"""
import os
import sys
import time
import unittest
from unittest import mock

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
sys.path.append(CURRENT_DIR.rsplit(os.sep, 1)[0])

import chameleon_com  # noqa: E402


class _FakeCom:
    """Minimal stand-in exposing only what thread_check_timeout touches."""

    def __init__(self, wait_response_map, open_iters):
        self.wait_response_map = wait_response_map
        self._open_iters = open_iters

    def isOpen(self):
        if self._open_iters > 0:
            self._open_iters -= 1
            return True
        return False


def _run(wait_map, open_iters):
    fake = _FakeCom(wait_map, open_iters)
    # Skip the real 100 ms sleep between loops so the test is instant.
    with mock.patch.object(chameleon_com.time, "sleep", lambda *_: None):
        chameleon_com.ChameleonCom.thread_check_timeout(fake)


class TestCheckTimeout(unittest.TestCase):
    def test_callback_fires_once_and_entry_removed(self):
        calls = []
        wait_map = {
            0x1234: {
                "end_time": time.time() - 1,
                "callback": lambda cmd, a, b: calls.append(cmd),
            }
        }
        _run(wait_map, open_iters=3)  # three loops; callback must still fire once
        self.assertEqual(calls, [0x1234])
        self.assertNotIn(0x1234, wait_map)

    def test_sync_timeout_sets_flag(self):
        wait_map = {0x1111: {"end_time": time.time() - 1}}  # no callback => sync mode
        _run(wait_map, open_iters=1)
        self.assertTrue(wait_map[0x1111]["is_timeout"])

    def test_no_error_when_map_mutated_during_iteration(self):
        wait_map = {}

        def cb(cmd, a, b):
            # Insert during iteration — raises on the old live-dict loop.
            wait_map[0x9999] = {"end_time": time.time() + 100}

        for i in range(5):
            wait_map[i] = {"end_time": time.time() - 1, "callback": cb}
        _run(wait_map, open_iters=1)  # must not raise
        self.assertIn(0x9999, wait_map)


if __name__ == "__main__":
    unittest.main()
