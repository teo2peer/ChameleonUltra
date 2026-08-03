#!/usr/bin/env python3
import os
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
config_path = CURRENT_DIR.rsplit(os.sep, 1)[0]
sys.path.append(config_path)

from chameleon_enum import Command  # noqa: E402
import chameleon_cli_unit as u       # noqa: E402


def _child(node, name):
    return next(c for c in node.children if c.name == name)


class FakeDeviceCom:
    def __init__(self, data_max_length):
        self.data_max_length = data_max_length


class FakeCMD:
    def __init__(self):
        self.writes = []  # list of (block_start, len(block_data))

    def mf1_write_emu_block_data(self, block_start, block_data):
        self.writes.append((block_start, len(block_data)))


class TestReaderKeysCommands(unittest.TestCase):
    def test_command_ids(self):
        # Must match the firmware data_cmd.h defines exactly.
        self.assertEqual(Command.MF1_SET_RANDOM_UID_MODE, 4042)
        self.assertEqual(Command.MF1_GET_RANDOM_UID_MODE, 4043)
        self.assertEqual(Command.MF1_SET_READER_KEYS_ANIM, 4044)
        self.assertEqual(Command.MF1_READER_KEYS_RESELECT, 4045)

    def test_cli_tree(self):
        node = _child(_child(_child(u.root, "hf"), "mf"), "readerkeys")
        names = sorted(c.name for c in node.children)
        self.assertEqual(names, ["extract", "start", "status", "stop"])
        # Every subcommand's parser must build (help_text is populated from it).
        for c in node.children:
            self.assertTrue(c.help_text)

    def test_write_emu_dump_single_frame(self):
        cmd = FakeCMD()
        # 1K dump (64 blocks) fits in one 4096-byte frame.
        write = u.write_emu_dump_to_slot
        write(cmd, FakeDeviceCom(4096), bytes(1024))
        self.assertEqual(cmd.writes, [(0, 1024)])

    def test_write_emu_dump_multi_frame_4k(self):
        cmd = FakeCMD()
        # 4K dump (256 blocks). One frame holds (4096-1)//16 = 255 blocks,
        # so it must split into 255 + 1 with no block dropped.
        u.write_emu_dump_to_slot(cmd, FakeDeviceCom(4096), bytes(4096))
        self.assertEqual(cmd.writes, [(0, 255 * 16), (255, 16)])
        total_blocks = sum(n // 16 for _, n in cmd.writes)
        self.assertEqual(total_blocks, 256)

    def test_write_emu_dump_unaligned_rejected(self):
        with self.assertRaises(Exception):
            u.write_emu_dump_to_slot(FakeCMD(), FakeDeviceCom(4096), bytes(20))


if __name__ == "__main__":
    unittest.main()
