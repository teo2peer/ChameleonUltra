#!/usr/bin/env python3
"""Guard firmware/Python command, status, and dispatch-table mirrors.

Command IDs are also mirrored by a GUI in a separate repository. This test
enforces the complete bidirectional mirror available in this checkout and
checks the command sets compiled for both firmware variants.
"""
import os
import re
import sys
import unittest

CURRENT_DIR = os.path.split(os.path.abspath(__file__))[0]
REPO = CURRENT_DIR.rsplit(os.sep, 1)[0]
sys.path.append(REPO)

from chameleon_enum import Command, Status  # noqa: E402

DATA_CMD_H = os.path.normpath(
    os.path.join(REPO, "..", "..", "firmware", "application", "src", "data_cmd.h")
)
APP_STATUS_H = os.path.join(os.path.dirname(DATA_CMD_H), "app_status.h")
APP_CMD_C = os.path.join(os.path.dirname(DATA_CMD_H), "app_cmd.c")

# Defined for compatibility with existing clients but intentionally has no
# firmware handler. Any addition here needs a concrete protocol reason.
UNDISPATCHED_COMMANDS = {
    "EM4X05_READSNIFF": "reserved legacy command; no implementation exists",
}

# Unknown response codes remain integers in the client. Keep the firmware-only
# exceptions explicit until host-facing behavior needs named Status members.
FIRMWARE_ONLY_STATUSES = {
    "LF_TAG_LOGIN_REQUIRED": "reserved for authenticated LF tags",
    "MEM_ERR": "reported as a raw command failure by current host methods",
    "CREATE_RESPONSE_ERR": "reported as a raw command failure by current host methods",
    "CMD_ERR": "reported as a raw command failure by current host methods",
}


def _parse_defines(path, prefix):
    defines = {}
    with open(path) as f:
        for line in f:
            m = re.match(
                rf"\s*#define\s+{re.escape(prefix)}(\w+)\s+"
                r"\((0x[0-9a-fA-F]+|\d+)\)",
                line,
            )
            if m:
                defines[m.group(1)] = int(m.group(2), 0)
    return defines


def _parse_dispatch(path):
    variants = {"ultra": {}, "lite": {}}
    in_map = False
    ultra_only = False
    with open(path) as f:
        for line in f:
            if "static cmd_data_map_t m_data_cmd_map[]" in line:
                in_map = True
                continue
            if not in_map:
                continue
            if re.match(r"\s*#if\s+defined\(PROJECT_CHAMELEON_ULTRA\)", line):
                ultra_only = True
                continue
            if re.match(r"\s*#endif", line):
                ultra_only = False
                continue
            if re.match(r"\s*};", line):
                break
            row = re.search(
                r"\{\s*DATA_CMD_(\w+)\s*,\s*[^,]+,\s*([^,]+),", line
            )
            if row:
                name, processor = row.group(1), row.group(2).strip()
                if name in variants["ultra"]:
                    raise AssertionError(f"duplicate dispatch row for {name}")
                if processor == "NULL":
                    raise AssertionError(f"dispatch row for {name} has no processor")
                variants["ultra"][name] = processor
                if not ultra_only:
                    variants["lite"][name] = processor
    return variants


class TestCommandMirror(unittest.TestCase):
    def test_debug_counters_mirrored(self):
        self.assertEqual(int(Command.HF14A_4_DEBUG_COUNTERS), 6010)

    def test_emv_trace_commands_mirrored(self):
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_START), 6007)
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_META), 6008)
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_GET), 6009)

    def test_firmware_defines_match_enum(self):
        if not os.path.exists(DATA_CMD_H):
            self.skipTest("firmware data_cmd.h not available")
        defines = _parse_defines(DATA_CMD_H, "DATA_CMD_")
        self.assertIn("HF14A_4_DEBUG_COUNTERS", defines)
        python_commands = {name: int(value) for name, value in Command.__members__.items()}
        self.assertEqual(set(defines), set(python_commands))
        mismatches = sorted(
            (name, defines[name], python_commands[name])
            for name in defines
            if defines[name] != python_commands[name]
        )
        self.assertEqual(mismatches, [], f"data_cmd.h vs Command enum: {mismatches}")

    def test_status_defines_match_enum_or_allowlist(self):
        firmware_statuses = _parse_defines(APP_STATUS_H, "STATUS_")
        python_statuses = {name: int(value) for name, value in Status.__members__.items()}
        self.assertTrue(all(FIRMWARE_ONLY_STATUSES.values()))
        self.assertEqual(
            set(firmware_statuses) - set(python_statuses),
            set(FIRMWARE_ONLY_STATUSES),
        )
        self.assertEqual(set(python_statuses) - set(firmware_statuses), set())
        self.assertEqual(
            {
                name: (firmware_statuses[name], python_statuses[name])
                for name in firmware_statuses.keys() & python_statuses.keys()
                if firmware_statuses[name] != python_statuses[name]
            },
            {},
        )

    def test_dispatch_is_complete_for_ultra_and_guarded_for_lite(self):
        commands = _parse_defines(DATA_CMD_H, "DATA_CMD_")
        dispatch = _parse_dispatch(APP_CMD_C)
        dispatchable = set(commands) - set(UNDISPATCHED_COMMANDS)
        ultra_only = {
            name
            for name, value in commands.items()
            if 2000 <= value < 4000 or 6000 <= value < 7000
        }

        self.assertTrue(all(UNDISPATCHED_COMMANDS.values()))
        self.assertEqual(set(dispatch["ultra"]), dispatchable)
        self.assertEqual(set(dispatch["lite"]), dispatchable - ultra_only)
        self.assertEqual(
            set(dispatch["ultra"]) - set(dispatch["lite"]),
            ultra_only - set(UNDISPATCHED_COMMANDS),
        )


if __name__ == "__main__":
    unittest.main()
