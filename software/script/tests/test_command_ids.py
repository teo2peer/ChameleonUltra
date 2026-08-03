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
BLE_MAIN_C = os.path.join(os.path.dirname(DATA_CMD_H), "ble_main.c")
HW_CONNECT_C = os.path.normpath(
    os.path.join(REPO, "..", "..", "firmware", "common", "hw_connect.c")
)
KEYBOARD_HID_C = os.path.join(os.path.dirname(DATA_CMD_H), "keyboard_hid.c")

# Defined for compatibility with existing clients but intentionally has no
# firmware handler. Any addition here needs a concrete protocol reason.
UNDISPATCHED_COMMANDS = {
    "EM4X05_READSNIFF": "reserved legacy command; no implementation exists",
    "HF_CAPTURE_EVENT": "unsolicited capture notification; hosts must not request it",
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


def _parse_dispatch_hooks(path):
    hooks = {}
    with open(path) as source:
        for line in source:
            row = re.search(
                r"\{\s*DATA_CMD_(\w+)\s*,\s*([^,]+),\s*([^,]+),", line
            )
            if row:
                name, before, processor = (value.strip() for value in row.groups())
                hooks[name] = (before, processor)
    return hooks


class TestCommandMirror(unittest.TestCase):
    def test_runtime_undercover_is_common_and_disconnect_scoped(self):
        self.assertEqual(int(Command.SET_RUNTIME_UNDERCOVER_MODE), 1053)
        dispatch = _parse_dispatch(APP_CMD_C)
        self.assertIn("SET_RUNTIME_UNDERCOVER_MODE", dispatch["ultra"])
        self.assertIn("SET_RUNTIME_UNDERCOVER_MODE", dispatch["lite"])
        with open(BLE_MAIN_C) as source:
            ble_source = source.read()
        self.assertGreaterEqual(ble_source.count("runtime_undercover_revoke();"), 2)
        with open(APP_CMD_C) as source:
            self.assertIn("enabled && !is_nus_working()", source.read())
        with open(HW_CONNECT_C) as source:
            hardware_source = source.read()
        self.assertIn("nrf_gpio_cfg_input(m_led_array[i]", hardware_source)
        self.assertIn("app_util_critical_region_enter", hardware_source)

    def test_active_slot_snapshot_is_common_command(self):
        self.assertEqual(int(Command.ACTIVE_SLOT_SNAPSHOT), 1050)
        if os.path.exists(APP_CMD_C):
            dispatch = _parse_dispatch(APP_CMD_C)
            self.assertIn("ACTIVE_SLOT_SNAPSHOT", dispatch["ultra"])
            self.assertIn("ACTIVE_SLOT_SNAPSHOT", dispatch["lite"])

    def test_debug_counters_mirrored(self):
        self.assertEqual(int(Command.HF14A_4_DEBUG_COUNTERS), 6010)

    def test_emv_trace_commands_mirrored(self):
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_START), 6007)
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_META), 6008)
        self.assertEqual(int(Command.HF14A_4_EMV_TRACE_GET), 6009)

    def test_iso_dep_reader_session_commands_mirrored(self):
        self.assertEqual(int(Command.HF14A_4_READER_SESSION_START), 6011)
        self.assertEqual(int(Command.HF14A_4_READER_SESSION_EXCHANGE), 6012)
        self.assertEqual(int(Command.HF14A_4_READER_SESSION_STOP), 6013)
        self.assertEqual(
            int(Command.HF14A_4_READER_SESSION_START_APPLE_TRANSIT), 6014)
        if os.path.exists(APP_CMD_C):
            dispatch = _parse_dispatch(APP_CMD_C)
            self.assertEqual(
                dispatch["ultra"]["HF14A_4_READER_SESSION_START_APPLE_TRANSIT"],
                dispatch["ultra"]["HF14A_4_READER_SESSION_START"],
            )
            self.assertNotIn(
                "HF14A_4_READER_SESSION_START_APPLE_TRANSIT", dispatch["lite"])

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

    def test_keyboard_commands_share_transport_authorization_hook(self):
        with open(APP_CMD_C) as source:
            app_cmd = source.read()
        for name in (
            "UPLOAD_BEGIN", "UPLOAD_CHUNK", "UPLOAD_COMMIT", "RUN",
            "CANCEL", "GET_STATUS", "CLEAR", "SET_TEMP_BLE_NAME", "ARM_BLE",
        ):
            with self.subTest(command=name):
                self.assertRegex(
                    app_cmd,
                    rf"\{{\s*DATA_CMD_KEYBOARD_{name}\s*,\s*"
                    r"cmd_before_keyboard\s*,",
                )

    def test_hf_emulator_commands_require_the_loaded_buffer_owner(self):
        hooks = _parse_dispatch_hooks(APP_CMD_C)
        generic = {
            "HF14A_GET_ANTI_COLL_DATA",
            "HF14A_SET_ANTI_COLL_DATA",
        }
        mf1 = {
            "MF1_WRITE_EMU_BLOCK_DATA",
            "MF1_SET_DETECTION_ENABLE",
            "MF1_GET_DETECTION_ENABLE",
            "MF1_READ_EMU_BLOCK_DATA",
            "MF1_GET_EMULATOR_CONFIG",
            "MF1_GET_PRNG_TYPE",
            "MF1_SET_PRNG_TYPE",
            "MF1_SET_RANDOM_UID_MODE",
            "MF1_GET_RANDOM_UID_MODE",
            "MF1_READER_KEYS_RESELECT",
            "MF1_GET_GEN1A_MODE",
            "MF1_SET_GEN1A_MODE",
            "MF1_GET_GEN2_MODE",
            "MF1_SET_GEN2_MODE",
            "MF1_GET_BLOCK_ANTI_COLL_MODE",
            "MF1_SET_BLOCK_ANTI_COLL_MODE",
            "MF1_GET_WRITE_MODE",
            "MF1_SET_WRITE_MODE",
            "MF1_GET_FIELD_OFF_DO_RESET",
            "MF1_SET_FIELD_OFF_DO_RESET",
        }
        retained = {
            "MF1_GET_DETECTION_COUNT",
            "MF1_GET_DETECTION_LOG",
            "MF0_NTAG_GET_DETECTION_COUNT",
            "MF0_NTAG_GET_DETECTION_LOG",
            "HF14A_4_DEBUG_COUNTERS",
        }
        mf0 = {
            name for name in hooks
            if name.startswith("MF0_NTAG_") and name not in retained
        }
        hf14a4 = {
            "HF14A_4_APDU_RECV",
            "HF14A_4_APDU_SEND",
            "HF14A_4_SET_ANTI_COLL",
            "HF14A_4_STATIC_RESP",
        }
        for names, expected in (
            (generic, "before_hf_emulator_loaded"),
            (mf1, "before_mf1_emulator_loaded"),
            (mf0, "before_mf0_ntag_emulator_loaded"),
            (hf14a4, "before_hf14a_4_emulator_loaded"),
        ):
            for name in names:
                with self.subTest(command=name):
                    self.assertEqual(hooks[name][0], expected)

        self.assertEqual(hooks["MF1_SET_READER_KEYS_ANIM"][0], "NULL")
        for name in retained:
            self.assertEqual(hooks[name][0], "NULL")
        for name in (
            "HF14A_4_READER_APDU",
            "HF14A_4_EMV_SCAN",
            "HF14A_4_DESFIRE_SCAN",
            "HF14A_4_EMV_TRACE_START",
            "HF14A_4_EMV_TRACE_META",
            "HF14A_4_EMV_TRACE_GET",
            "HF14A_4_READER_SESSION_START",
            "HF14A_4_READER_SESSION_EXCHANGE",
            "HF14A_4_READER_SESSION_STOP",
            "HF14A_4_READER_SESSION_START_APPLE_TRANSIT",
        ):
            with self.subTest(reader_command=name):
                self.assertNotEqual(
                    hooks[name][0], "before_hf14a_4_emulator_loaded")

    def test_keyboard_ble_integration_guards(self):
        with open(BLE_MAIN_C) as source:
            ble_main = source.read()
        with open(KEYBOARD_HID_C) as source:
            keyboard_hid = source.read()

        self.assertRegex(
            keyboard_hid,
            r"BLE_LINK_CTX_MANAGER_DEF\(m_hids_link_ctx_storage,\s*"
            r"NRF_SDH_BLE_TOTAL_LINK_COUNT,",
        )
        authorization = re.search(
            r"bool ble_keyboard_link_authorized\(void\)\s*\{(.*?)\n\}",
            ble_main,
            re.DOTALL,
        )
        self.assertIsNotNone(authorization)
        for check in (
            "ble_command_link_authorized()",
            "ble_conn_state_mitm_protected(m_conn_handle)",
            "ble_conn_state_lesc(m_conn_handle)",
        ):
            self.assertIn(check, authorization.group(1))
        self.assertIn("!m_nus_hvn_inflight", ble_main)
        self.assertRegex(
            ble_main,
            r"(?s)BLE_NUS_EVT_TX_RDY.*?m_nus_hvn_inflight\s*=\s*false",
        )


if __name__ == "__main__":
    unittest.main()
