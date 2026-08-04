# Indice de firmas mantenidas del firmware

Generado por `firmware/tools/generate_deep_analysis_signatures.py`.
Incluye definiciones de funciones C mantenidas en application, common,
bootloader y tests/stubs. Excluye Nordic SDK, prototypes sin cuerpo, macros,
artefactos de build y herramientas host de `software/`.

- Archivos C/H inspeccionados: 182
- Archivos con definiciones: 86
- Funciones definidas: 1422
- Regenerar: `python3 firmware/tools/generate_deep_analysis_signatures.py`

## `firmware/application/src/app_cmd.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 48 | static | `change_slot_auto` | `static bool change_slot_auto(uint8_t slot_new)` | Implementa change slot auto. |
| 67 | static | `cmd_parse_hf14a_anticoll` | `static bool cmd_parse_hf14a_anticoll(uint16_t length, const uint8_t *data, cmd_hf14a_anticoll_t *parsed)` | Implementa cmd parse hf14a anticoll. |
| 88 | static | `cmd_processor_get_app_version` | `static data_frame_tx_t *cmd_processor_get_app_version(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get app version. |
| 99 | static | `cmd_processor_get_git_version` | `static data_frame_tx_t *cmd_processor_get_git_version(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get git version. |
| 104 | static | `cmd_processor_get_device_model` | `static data_frame_tx_t *cmd_processor_get_device_model(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device model. |
| 110 | static | `cmd_processor_change_device_mode` | `static data_frame_tx_t *cmd_processor_change_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando change device mode. |
| 130 | static | `cmd_processor_get_device_mode` | `static data_frame_tx_t *cmd_processor_get_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device mode. |
| 135 | static | `cmd_processor_enter_bootloader` | `static data_frame_tx_t *cmd_processor_enter_bootloader(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando enter bootloader. |
| 152 | static | `cmd_processor_get_device_chip_id` | `static data_frame_tx_t *cmd_processor_get_device_chip_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device chip id. |
| 162 | static | `cmd_processor_get_device_address` | `static data_frame_tx_t *cmd_processor_get_device_address(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device address. |
| 177 | static | `cmd_processor_save_settings` | `static data_frame_tx_t *cmd_processor_save_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando save settings. |
| 183 | static | `cmd_processor_reset_settings` | `static data_frame_tx_t *cmd_processor_reset_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando reset settings. |
| 190 | static | `cmd_processor_get_device_settings` | `static data_frame_tx_t *cmd_processor_get_device_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device settings. |
| 207 | static | `cmd_processor_set_animation_mode` | `static data_frame_tx_t *cmd_processor_set_animation_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set animation mode. |
| 215 | static | `cmd_processor_get_animation_mode` | `static data_frame_tx_t *cmd_processor_get_animation_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get animation mode. |
| 220 | static | `cmd_processor_get_battery_info` | `static data_frame_tx_t *cmd_processor_get_battery_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get battery info. |
| 230 | static | `cmd_processor_get_button_press_config` | `static data_frame_tx_t *cmd_processor_get_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get button press config. |
| 238 | static | `cmd_processor_set_button_press_config` | `static data_frame_tx_t *cmd_processor_set_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set button press config. |
| 246 | static | `cmd_processor_get_long_button_press_config` | `static data_frame_tx_t *cmd_processor_get_long_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get long button press config. |
| 254 | static | `cmd_processor_set_long_button_press_config` | `static data_frame_tx_t *cmd_processor_set_long_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set long button press config. |
| 262 | static | `cmd_processor_get_sleep_timeout` | `static data_frame_tx_t *cmd_processor_get_sleep_timeout(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get sleep timeout. |
| 267 | static | `cmd_processor_set_sleep_timeout` | `static data_frame_tx_t *cmd_processor_set_sleep_timeout(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set sleep timeout. |
| 275 | static | `cmd_processor_get_ble_pairing_enable` | `static data_frame_tx_t *cmd_processor_get_ble_pairing_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get ble pairing enable. |
| 280 | static | `cmd_processor_set_ble_pairing_enable` | `static data_frame_tx_t *cmd_processor_set_ble_pairing_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set ble pairing enable. |
| 290 | static | `cmd_processor_hf14a_scan` | `static data_frame_tx_t *cmd_processor_hf14a_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a scan. |
| 312 | static | `cmd_processor_mf1_detect_support` | `static data_frame_tx_t *cmd_processor_mf1_detect_support(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 detect support. |
| 317 | static | `cmd_processor_mf1_detect_prng` | `static data_frame_tx_t *cmd_processor_mf1_detect_prng(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 detect prng. |
| 335 | static | `cmd_processor_mf1_static_nested_acquire` | `static data_frame_tx_t *cmd_processor_mf1_static_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 static nested acquire. |
| 350 | static | `cmd_processor_mf1_darkside_acquire` | `static data_frame_tx_t *cmd_processor_mf1_darkside_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 darkside acquire. |
| 369 | static | `cmd_processor_mf1_detect_nt_dist` | `static data_frame_tx_t *cmd_processor_mf1_detect_nt_dist(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 detect nt dist. |
| 394 | static | `cmd_processor_mf1_nested_acquire` | `static data_frame_tx_t *cmd_processor_mf1_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 nested acquire. |
| 409 | static | `cmd_processor_mf1_enc_nested_acquire` | `static data_frame_tx_t *cmd_processor_mf1_enc_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 enc nested acquire. |
| 447 | static | `cmd_processor_mf1_auth_one_key_block` | `static data_frame_tx_t *cmd_processor_mf1_auth_one_key_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 auth one key block. |
| 463 | static | `cmd_processor_mf1_check_keys_of_sectors` | `static data_frame_tx_t *cmd_processor_mf1_check_keys_of_sectors(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 check keys of sectors. |
| 481 | static | `cmd_processor_mf1_check_keys_on_block` | `static data_frame_tx_t *cmd_processor_mf1_check_keys_on_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 check keys on block. |
| 499 | static | `cmd_processor_mf1_hardnested_nonces_acquire` | `static data_frame_tx_t *cmd_processor_mf1_hardnested_nonces_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 hardnested nonces acquire. |
| 540 | static | `cmd_processor_mf1_read_one_block` | `static data_frame_tx_t *cmd_processor_mf1_read_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 read one block. |
| 568 | static | `cmd_processor_mf1_read_blocks` | `static data_frame_tx_t *cmd_processor_mf1_read_blocks(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 read blocks. |
| 607 | static | `cmd_processor_mf1_write_one_block` | `static data_frame_tx_t *cmd_processor_mf1_write_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 write one block. |
| 629 | static | `cmd_processor_hf14a_set_field_on` | `static data_frame_tx_t *cmd_processor_hf14a_set_field_on(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a set field on. |
| 644 | static | `cmd_processor_hf14a_set_field_off` | `static data_frame_tx_t *cmd_processor_hf14a_set_field_off(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a set field off. |
| 663 | static | `cmd_processor_hf14a_raw` | `static data_frame_tx_t *cmd_processor_hf14a_raw(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a raw. |
| 717 | static | `cmd_processor_hf14a_get_config` | `static data_frame_tx_t *cmd_processor_hf14a_get_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a get config. |
| 722 | static | `cmd_processor_hf14a_set_config` | `static data_frame_tx_t *cmd_processor_hf14a_set_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a set config. |
| 732 | static | `cmd_processor_mf1_manipulate_value_block` | `static data_frame_tx_t *cmd_processor_mf1_manipulate_value_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 manipulate value block. |
| 786 | static | `cmd_processor_em410x_scan` | `static data_frame_tx_t *cmd_processor_em410x_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em410x scan. |
| 799 | static | `cmd_processor_em410x_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_em410x_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em410x write to t55xx. |
| 817 | static | `cmd_processor_em410x_electra_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_em410x_electra_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em410x electra write to t55xx. |
| 835 | static | `cmd_processor_hidprox_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_hidprox_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hidprox write to t55xx. |
| 859 | static | `cmd_processor_hidprox_scan` | `static data_frame_tx_t *cmd_processor_hidprox_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hidprox scan. |
| 872 | static | `cmd_processor_ioprox_scan` | `static data_frame_tx_t *cmd_processor_ioprox_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox scan. |
| 883 | static | `cmd_processor_ioprox_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_ioprox_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox write to t55xx. |
| 917 | static | `cmd_processor_ioprox_decode_raw` | `static data_frame_tx_t *cmd_processor_ioprox_decode_raw(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox decode raw. |
| 938 | static | `cmd_processor_ioprox_compose_id` | `static data_frame_tx_t *cmd_processor_ioprox_compose_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox compose id. |
| 951 | static | `cmd_processor_viking_scan` | `static data_frame_tx_t *cmd_processor_viking_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando viking scan. |
| 960 | static | `cmd_processor_pac_scan` | `static data_frame_tx_t *cmd_processor_pac_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando pac scan. |
| 969 | static | `cmd_processor_viking_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_viking_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando viking write to t55xx. |
| 988 | static | `cmd_processor_pac_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_pac_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando pac write to t55xx. |
| 1005 | static | `cmd_processor_jablotron_scan` | `static data_frame_tx_t *cmd_processor_jablotron_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando jablotron scan. |
| 1014 | static | `cmd_processor_jablotron_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_jablotron_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando jablotron write to t55xx. |
| 1031 | static | `cmd_processor_lf_t55xx_write` | `static data_frame_tx_t *cmd_processor_lf_t55xx_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando lf t55xx write. |
| 1066 | static | `cmd_processor_generic_read` | `static data_frame_tx_t *cmd_processor_generic_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando generic read. |
| 1092 | static | `cmd_processor_set_active_slot` | `static data_frame_tx_t *cmd_processor_set_active_slot(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set active slot. |
| 1100 | static | `cmd_processor_set_slot_tag_type` | `static data_frame_tx_t *cmd_processor_set_slot_tag_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set slot tag type. |
| 1118 | static | `cmd_processor_delete_slot_sense_type` | `static data_frame_tx_t *cmd_processor_delete_slot_sense_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando delete slot sense type. |
| 1135 | static | `cmd_processor_set_slot_data_default` | `static data_frame_tx_t *cmd_processor_set_slot_data_default(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set slot data default. |
| 1161 | static | `cmd_processor_set_slot_enable` | `static data_frame_tx_t *cmd_processor_set_slot_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set slot enable. |
| 1202 | static | `cmd_processor_slot_data_config_save` | `static data_frame_tx_t *cmd_processor_slot_data_config_save(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando slot data config save. |
| 1208 | static | `cmd_processor_get_active_slot` | `static data_frame_tx_t *cmd_processor_get_active_slot(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get active slot. |
| 1213 | static | `cmd_processor_get_slot_info` | `static data_frame_tx_t *cmd_processor_get_slot_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get slot info. |
| 1229 | static | `cmd_processor_wipe_fds` | `static data_frame_tx_t *cmd_processor_wipe_fds(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando wipe fds. |
| 1237 | static | `get_active_em410x_type` | `static bool get_active_em410x_type(tag_specific_type_t *tag_type_out, uint16_t *id_size_out)` | Obtiene  active em410x type. |
| 1249 | static | `set_lf_emulator_id` | `static data_frame_tx_t *set_lf_emulator_id(uint16_t cmd, tag_specific_type_t tag_type, uint16_t expected_length, uint16_t length, const uint8_t *data)` | Configura  lf emulator id. |
| 1277 | static | `cmd_processor_em410x_set_emu_id` | `static data_frame_tx_t *cmd_processor_em410x_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em410x set emu id. |
| 1286 | static | `cmd_processor_em410x_get_emu_id` | `static data_frame_tx_t *cmd_processor_em410x_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em410x get emu id. |
| 1300 | static | `cmd_processor_hidprox_set_emu_id` | `static data_frame_tx_t *cmd_processor_hidprox_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hidprox set emu id. |
| 1305 | static | `cmd_processor_hidprox_get_emu_id` | `static data_frame_tx_t *cmd_processor_hidprox_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hidprox get emu id. |
| 1316 | static | `cmd_processor_ioprox_set_emu_id` | `static data_frame_tx_t *cmd_processor_ioprox_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox set emu id. |
| 1321 | static | `cmd_processor_idteck_set_emu_id` | `static data_frame_tx_t *cmd_processor_idteck_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando idteck set emu id. |
| 1326 | static | `cmd_processor_idteck_get_emu_id` | `static data_frame_tx_t *cmd_processor_idteck_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando idteck get emu id. |
| 1341 | static | `cmd_processor_idteck_write_to_t55xx` | `static data_frame_tx_t *cmd_processor_idteck_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando idteck write to t55xx. |
| 1361 | static | `cmd_processor_ioprox_get_emu_id` | `static data_frame_tx_t *cmd_processor_ioprox_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ioprox get emu id. |
| 1372 | static | `cmd_processor_viking_set_emu_id` | `static data_frame_tx_t *cmd_processor_viking_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando viking set emu id. |
| 1377 | static | `cmd_processor_viking_get_emu_id` | `static data_frame_tx_t *cmd_processor_viking_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando viking get emu id. |
| 1388 | static | `cmd_processor_pac_set_emu_id` | `static data_frame_tx_t *cmd_processor_pac_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando pac set emu id. |
| 1393 | static | `cmd_processor_pac_get_emu_id` | `static data_frame_tx_t *cmd_processor_pac_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando pac get emu id. |
| 1404 | static | `cmd_processor_jablotron_set_emu_id` | `static data_frame_tx_t *cmd_processor_jablotron_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando jablotron set emu id. |
| 1409 | static | `cmd_processor_jablotron_get_emu_id` | `static data_frame_tx_t *cmd_processor_jablotron_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando jablotron get emu id. |
| 1420 | static | `is_mf1_emulator_type` | `static bool is_mf1_emulator_type(tag_specific_type_t type)` | Comprueba mf1 emulator type. |
| 1425 | static | `is_mf0_ntag_emulator_type` | `static bool is_mf0_ntag_emulator_type(tag_specific_type_t type)` | Comprueba mf0 ntag emulator type. |
| 1442 | static | `before_hf_emulator_loaded` | `static data_frame_tx_t *before_hf_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before hf emulator loaded. |
| 1451 | static | `before_mf1_emulator_loaded` | `static data_frame_tx_t *before_mf1_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before mf1 emulator loaded. |
| 1461 | static | `before_mf0_ntag_emulator_loaded` | `static data_frame_tx_t *before_mf0_ntag_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before mf0 ntag emulator loaded. |
| 1472 | static | `before_hf14a_4_emulator_loaded` | `static data_frame_tx_t *before_hf14a_4_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before hf14a 4 emulator loaded. |
| 1480 | static | `get_coll_res_data` | `static nfc_tag_14a_coll_res_reference_t *get_coll_res_data(bool write)` | Obtiene  coll res data. |
| 1516 | static | `cmd_processor_hf14a_get_anti_coll_data` | `static data_frame_tx_t *cmd_processor_hf14a_get_anti_coll_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a get anti coll data. |
| 1551 | static | `cmd_processor_mf1_set_detection_enable` | `static data_frame_tx_t *cmd_processor_mf1_set_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set detection enable. |
| 1567 | static | `cmd_processor_mf1_get_detection_enable` | `static data_frame_tx_t *cmd_processor_mf1_get_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get detection enable. |
| 1572 | static | `cmd_processor_mf1_get_detection_count` | `static data_frame_tx_t *cmd_processor_mf1_get_detection_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get detection count. |
| 1581 | static | `cmd_processor_mf1_get_detection_log` | `static data_frame_tx_t *cmd_processor_mf1_get_detection_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get detection log. |
| 1599 | static | `cmd_processor_mf1_write_emu_block_data` | `static data_frame_tx_t *cmd_processor_mf1_write_emu_block_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 write emu block data. |
| 1618 | static | `cmd_processor_mf1_read_emu_block_data` | `static data_frame_tx_t *cmd_processor_mf1_read_emu_block_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 read emu block data. |
| 1634 | static | `cmd_processor_mf0_ntag_write_emu_page_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_write_emu_page_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag write emu page data. |
| 1674 | static | `cmd_processor_mf0_ntag_get_emu_page_count` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_emu_page_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get emu page count. |
| 1690 | static | `cmd_processor_mf0_ntag_read_emu_page_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_read_emu_page_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag read emu page data. |
| 1727 | static | `cmd_processor_mf0_ntag_get_version_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_version_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get version data. |
| 1734 | static | `cmd_processor_mf0_ntag_set_version_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_version_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set version data. |
| 1744 | static | `cmd_processor_mf0_ntag_get_signature_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_signature_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get signature data. |
| 1751 | static | `cmd_processor_mf0_ntag_set_signature_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_signature_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set signature data. |
| 1761 | static | `cmd_processor_mf0_ntag_get_counter_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_counter_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get counter data. |
| 1778 | static | `cmd_processor_mf0_ntag_set_counter_data` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_counter_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set counter data. |
| 1796 | static | `cmd_processor_mf0_ntag_reset_auth_cnt` | `static data_frame_tx_t *cmd_processor_mf0_ntag_reset_auth_cnt(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag reset auth cnt. |
| 1808 | static | `cmd_processor_hf14a_set_anti_coll_data` | `static data_frame_tx_t *cmd_processor_hf14a_set_anti_coll_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a set anti coll data. |
| 1832 | static | `cmd_processor_set_slot_tag_nick` | `static data_frame_tx_t *cmd_processor_set_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set slot tag nick. |
| 1855 | static | `cmd_processor_get_slot_tag_nick` | `static data_frame_tx_t *cmd_processor_get_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get slot tag nick. |
| 1877 | static | `cmd_processor_get_all_slot_nicks` | `static data_frame_tx_t *cmd_processor_get_all_slot_nicks(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get all slot nicks. |
| 1918 | static | `cmd_processor_delete_slot_tag_nick` | `static data_frame_tx_t *cmd_processor_delete_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando delete slot tag nick. |
| 1937 | static | `cmd_processor_mf1_get_emulator_config` | `static data_frame_tx_t *cmd_processor_mf1_get_emulator_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get emulator config. |
| 1947 | static | `cmd_processor_mf1_get_prng_type` | `static data_frame_tx_t *cmd_processor_mf1_get_prng_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get prng type. |
| 1952 | static | `cmd_processor_mf1_set_prng_type` | `static data_frame_tx_t *cmd_processor_mf1_set_prng_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set prng type. |
| 1960 | static | `cmd_processor_mf1_set_random_uid_mode` | `static data_frame_tx_t *cmd_processor_mf1_set_random_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set random uid mode. |
| 1968 | static | `cmd_processor_mf1_get_random_uid_mode` | `static data_frame_tx_t *cmd_processor_mf1_get_random_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get random uid mode. |
| 1973 | static | `cmd_processor_mf1_set_reader_keys_anim` | `static data_frame_tx_t *cmd_processor_mf1_set_reader_keys_anim(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set reader keys anim. |
| 1981 | static | `cmd_processor_mf1_get_gen1a_mode` | `static data_frame_tx_t *cmd_processor_mf1_get_gen1a_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get gen1a mode. |
| 1986 | static | `cmd_processor_mf1_set_gen1a_mode` | `static data_frame_tx_t *cmd_processor_mf1_set_gen1a_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set gen1a mode. |
| 1994 | static | `cmd_processor_mf1_get_gen2_mode` | `static data_frame_tx_t *cmd_processor_mf1_get_gen2_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get gen2 mode. |
| 1999 | static | `cmd_processor_mf1_set_gen2_mode` | `static data_frame_tx_t *cmd_processor_mf1_set_gen2_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set gen2 mode. |
| 2007 | static | `cmd_processor_mf1_get_block_anti_coll_mode` | `static data_frame_tx_t *cmd_processor_mf1_get_block_anti_coll_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get block anti coll mode. |
| 2012 | static | `cmd_processor_mf1_set_block_anti_coll_mode` | `static data_frame_tx_t *cmd_processor_mf1_set_block_anti_coll_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set block anti coll mode. |
| 2020 | static | `cmd_processor_mf1_get_write_mode` | `static data_frame_tx_t *cmd_processor_mf1_get_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get write mode. |
| 2025 | static | `cmd_processor_mf1_set_write_mode` | `static data_frame_tx_t *cmd_processor_mf1_set_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set write mode. |
| 2033 | static | `cmd_processor_mf1_get_field_off_do_reset` | `static data_frame_tx_t *cmd_processor_mf1_get_field_off_do_reset(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 get field off do reset. |
| 2038 | static | `cmd_processor_mf1_set_field_off_do_reset` | `static data_frame_tx_t *cmd_processor_mf1_set_field_off_do_reset(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf1 set field off do reset. |
| 2046 | static | `cmd_processor_get_enabled_slots` | `static data_frame_tx_t *cmd_processor_get_enabled_slots(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get enabled slots. |
| 2058 | static | `cmd_processor_get_ble_connect_key` | `static data_frame_tx_t *cmd_processor_get_ble_connect_key(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get ble connect key. |
| 2062 | static | `cmd_processor_set_ble_connect_key` | `static data_frame_tx_t *cmd_processor_set_ble_connect_key(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando set ble connect key. |
| 2082 | static | `cmd_processor_delete_all_ble_bonds` | `static data_frame_tx_t *cmd_processor_delete_all_ble_bonds(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando delete all ble bonds. |
| 2096 | static | `before_reader_run` | `static data_frame_tx_t *before_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before reader run. |
| 2109 | static | `before_hf_reader_run` | `static data_frame_tx_t *before_hf_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa before hf reader run. |
| 2123 | static | `after_hf_reader_run` | `static data_frame_tx_t *after_hf_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Implementa after hf reader run. |
| 2133 | static | `cmd_processor_mf0_ntag_get_uid_mode` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get uid mode. |
| 2142 | static | `cmd_processor_mf0_ntag_set_uid_mode` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set uid mode. |
| 2151 | static | `cmd_processor_mf0_ntag_get_write_mode` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get write mode. |
| 2156 | static | `cmd_processor_mf0_ntag_set_write_mode` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set write mode. |
| 2164 | static | `cmd_processor_mf0_ntag_set_detection_enable` | `static data_frame_tx_t *cmd_processor_mf0_ntag_set_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag set detection enable. |
| 2173 | static | `cmd_processor_mf0_ntag_get_detection_enable` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get detection enable. |
| 2178 | static | `cmd_processor_mf0_ntag_get_detection_count` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get detection count. |
| 2187 | static | `cmd_processor_mf0_ntag_get_detection_log` | `static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 ntag get detection log. |
| 2204 | static | `cmd_processor_mf0_get_emulator_config` | `static data_frame_tx_t *cmd_processor_mf0_get_emulator_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando mf0 get emulator config. |
| 2217 | static | `cmd_processor_em4x05_scan` | `static data_frame_tx_t *cmd_processor_em4x05_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando em4x05 scan. |
| 2236 | static | `cmd_processor_lf_sniff` | `static data_frame_tx_t *cmd_processor_lf_sniff(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando lf sniff. |
| 2284 | static | `auth_trace_store` | `static void auth_trace_store(const uint8_t *data, uint16_t szBits, bool is_tx)` | Implementa auth trace store. |
| 2306 | static | `auth_trace_emit_anticoll` | `static void auth_trace_emit_anticoll(const picc_14a_tag_t *tag)` | Implementa auth trace emit anticoll. |
| 2376 | static | `auth_trace_do_auth` | `static uint8_t auth_trace_do_auth(picc_14a_tag_t *tag, uint8_t type, uint8_t blockNo, const uint8_t *key6)` | Implementa auth trace do auth. |
| 2450 | static | `cmd_processor_hf14a_auth_trace` | `static data_frame_tx_t *cmd_processor_hf14a_auth_trace(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a auth trace. |
| 2524 | static | `hf14a_sniff_store` | `static void hf14a_sniff_store(const uint8_t *data, uint16_t szBits, bool is_tx)` | Implementa hf14a sniff store. |
| 2534 | static | `hf14a_sniff_frame_cb` | `static void hf14a_sniff_frame_cb(const uint8_t *data, uint16_t szBits)` | Implementa hf14a sniff frame cb. |
| 2540 | static | `hf14a_sniff_tx_frame_cb` | `static void hf14a_sniff_tx_frame_cb(const uint8_t *data, uint16_t szBits)` | Implementa hf14a sniff tx frame cb. |
| 2545 | static | `cmd_processor_hf14a_sniff` | `static data_frame_tx_t *cmd_processor_hf14a_sniff(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a sniff. |
| 2608 | static | `cmd_processor_hf14a_4_apdu_recv` | `static data_frame_tx_t *cmd_processor_hf14a_4_apdu_recv(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 apdu recv. |
| 2623 | static | `cmd_processor_hf14a_4_apdu_send` | `static data_frame_tx_t *cmd_processor_hf14a_4_apdu_send(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 apdu send. |
| 2637 | static | `cmd_processor_hf14a_4_set_anti_coll` | `static data_frame_tx_t *cmd_processor_hf14a_4_set_anti_coll(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 set anti coll. |
| 2661 | static | `cmd_processor_hf14a_4_static_resp` | `static data_frame_tx_t *cmd_processor_hf14a_4_static_resp(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 static resp. |
| 2688 | static | `cmd_processor_hf14a_scan_keep` | `static data_frame_tx_t *cmd_processor_hf14a_scan_keep(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a scan keep. |
| 2719 | static | `cmd_processor_hf14a_4_reader_apdu` | `static data_frame_tx_t *cmd_processor_hf14a_4_reader_apdu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 reader apdu. |
| 2771 | static | `iso_dep_session_failure_status` | `static uint16_t iso_dep_session_failure_status(const iso_dep_result_t *result)` | Implementa iso dep session failure status. |
| 2789 | static | `cmd_processor_hf14a_4_reader_session_start` | `static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_start( uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 reader session start. |
| 2830 | static | `cmd_processor_hf14a_4_reader_session_exchange` | `static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_exchange( uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 reader session exchange. |
| 2861 | static | `cmd_processor_hf14a_4_reader_session_stop` | `static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_stop( uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 reader session stop. |
| 2882 | static | `tcl_apdu_` | `static bool tcl_apdu_( const uint8_t *apdu, uint8_t apdu_sz, uint8_t **rdata_ptr, uint16_t *rlen_ptr, uint8_t *abuf, uint8_t *rbuf, uint8_t *chain_buf, uint16_t *rbits_p, uint8_t *blk_p)` | Implementa tcl apdu . |
| 2904 | static | `emv_dol_put` | `static void emv_dol_put(uint8_t *dst, uint8_t len, const uint8_t *src, uint8_t src_len)` | Implementa emv dol put. |
| 2911 | static | `emv_fill_dol` | `static uint8_t emv_fill_dol(const uint8_t *dol, uint8_t dol_len, uint8_t *out, uint8_t out_cap, const uint8_t amount[6])` | Implementa emv fill dol. |
| 2975 | static | `emv_dol_value_len` | `static uint8_t emv_dol_value_len(const uint8_t *dol, uint8_t dol_len, uint8_t out_cap)` | Implementa emv dol value len. |
| 2993 | static | `emv_find_tlv_value` | `static bool emv_find_tlv_value(const uint8_t *d, uint16_t dl, uint32_t wanted, uint8_t *out, uint8_t *out_len, uint8_t out_cap, uint8_t depth)` | Implementa emv find tlv value. |
| 3035 | static | `emv_find_cdol1` | `static bool emv_find_cdol1(const uint8_t *d, uint16_t dl, uint8_t *out, uint8_t *out_len, uint8_t depth)` | Implementa emv find cdol1. |
| 3069 | static | `emv_has_card_identity_tlv` | `static bool emv_has_card_identity_tlv(const uint8_t *d, uint16_t dl, uint8_t depth)` | Implementa emv has card identity tlv. |
| 3118 | static | `cmd_processor_hf14a_4_emv_scan` | `static data_frame_tx_t *cmd_processor_hf14a_4_emv_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 emv scan. |
| 3527 | static | `cmd_processor_hf14a_4_desfire_scan` | `static data_frame_tx_t *cmd_processor_hf14a_4_desfire_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 desfire scan. |
| 3658 | static | `cmd_processor_hf14a_4_debug_counters` | `static data_frame_tx_t *cmd_processor_hf14a_4_debug_counters(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 debug counters. |
| 3667 | static | `cmd_processor_hf14a_4_emv_trace_start` | `static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 emv trace start. |
| 3675 | static | `cmd_processor_hf14a_4_emv_trace_meta` | `static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_meta(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 emv trace meta. |
| 3683 | static | `cmd_processor_hf14a_4_emv_trace_get` | `static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando hf14a 4 emv trace get. |
| 3923 | exportada | `cmd_processor_get_device_capabilities` | `data_frame_tx_t *cmd_processor_get_device_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando get device capabilities. |
| 3940 | static | `auto_response_data` | `static void auto_response_data(data_frame_tx_t *resp)` | Implementa auto response data. |
| 3958 | exportada | `on_data_frame_received` | `void on_data_frame_received(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Atiende data frame received. |

## `firmware/application/src/app_cmd.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 18 | static | `cmd_payload_exact` | `static inline bool cmd_payload_exact(uint16_t length, const uint8_t *data, uint16_t expected)` | Implementa cmd payload exact. |
| 22 | static | `cmd_payload_empty` | `static inline bool cmd_payload_empty(uint16_t length)` | Implementa cmd payload empty. |
| 26 | static | `cmd_parse_bool` | `static inline bool cmd_parse_bool(uint8_t value, bool *out)` | Implementa cmd parse bool. |
| 32 | static | `cmd_read_u16be` | `static inline uint16_t cmd_read_u16be(const uint8_t *data)` | Implementa cmd read u16be. |
| 36 | static | `cmd_read_u32be` | `static inline uint32_t cmd_read_u32be(const uint8_t *data)` | Implementa cmd read u32be. |
| 41 | static | `cmd_parse_count_u8` | `static inline bool cmd_parse_count_u8(uint16_t length, uint16_t offset, uint8_t item_size, uint8_t *count)` | Implementa cmd parse count u8. |

## `firmware/application/src/app_cmd_ble.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 14 | static | `cmd_ble_radio_is_on` | `static bool cmd_ble_radio_is_on(void)` | Implementa cmd ble radio is on. |
| 19 | exportada | `cmd_processor_ble_scan_start` | `data_frame_tx_t *cmd_processor_ble_scan_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble scan start. |
| 31 | exportada | `cmd_processor_ble_scan_stop` | `data_frame_tx_t *cmd_processor_ble_scan_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble scan stop. |
| 37 | exportada | `cmd_processor_ble_scan_get_count` | `data_frame_tx_t *cmd_processor_ble_scan_get_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble scan get count. |
| 43 | exportada | `cmd_processor_ble_scan_get_results` | `data_frame_tx_t *cmd_processor_ble_scan_get_results(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble scan get results. |
| 50 | exportada | `cmd_processor_ble_advertising_set` | `data_frame_tx_t *cmd_processor_ble_advertising_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble advertising set. |
| 75 | exportada | `cmd_processor_ble_advertising_get` | `data_frame_tx_t *cmd_processor_ble_advertising_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble advertising get. |
| 81 | exportada | `cmd_processor_ble_link_probe` | `data_frame_tx_t *cmd_processor_ble_link_probe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble link probe. |
| 94 | exportada | `cmd_processor_ble_connect` | `data_frame_tx_t *cmd_processor_ble_connect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble connect. |
| 105 | exportada | `cmd_processor_ble_disconnect` | `data_frame_tx_t *cmd_processor_ble_disconnect(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble disconnect. |
| 111 | exportada | `cmd_processor_ble_central_state` | `data_frame_tx_t *cmd_processor_ble_central_state(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble central state. |
| 118 | exportada | `cmd_processor_ble_gatt_discover` | `data_frame_tx_t *cmd_processor_ble_gatt_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble gatt discover. |
| 124 | exportada | `cmd_processor_ble_gatt_get_chars` | `data_frame_tx_t *cmd_processor_ble_gatt_get_chars(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble gatt get chars. |
| 131 | exportada | `cmd_processor_ble_fuzz_start` | `data_frame_tx_t *cmd_processor_ble_fuzz_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble fuzz start. |
| 143 | exportada | `cmd_processor_ble_fuzz_stop` | `data_frame_tx_t *cmd_processor_ble_fuzz_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble fuzz stop. |
| 149 | exportada | `cmd_processor_ble_fuzz_get_log` | `data_frame_tx_t *cmd_processor_ble_fuzz_get_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble fuzz get log. |
| 156 | exportada | `cmd_processor_ble_gatt_read` | `data_frame_tx_t *cmd_processor_ble_gatt_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble gatt read. |
| 165 | exportada | `cmd_processor_ble_gatt_get_read` | `data_frame_tx_t *cmd_processor_ble_gatt_get_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble gatt get read. |
| 172 | exportada | `cmd_processor_ble_subscribe` | `data_frame_tx_t *cmd_processor_ble_subscribe(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble subscribe. |
| 182 | exportada | `cmd_processor_ble_get_notifications` | `data_frame_tx_t *cmd_processor_ble_get_notifications(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get notifications. |
| 189 | exportada | `cmd_processor_ble_find_cccd` | `data_frame_tx_t *cmd_processor_ble_find_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble find cccd. |
| 199 | exportada | `cmd_processor_ble_get_cccd` | `data_frame_tx_t *cmd_processor_ble_get_cccd(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get cccd. |
| 206 | exportada | `cmd_processor_ble_gatt_write` | `data_frame_tx_t *cmd_processor_ble_gatt_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble gatt write. |
| 222 | exportada | `cmd_processor_ble_get_write` | `data_frame_tx_t *cmd_processor_ble_get_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get write. |
| 229 | exportada | `cmd_processor_ble_get_mtu` | `data_frame_tx_t *cmd_processor_ble_get_mtu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get mtu. |
| 236 | exportada | `cmd_processor_ble_desc_discover` | `data_frame_tx_t *cmd_processor_ble_desc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble desc discover. |
| 242 | exportada | `cmd_processor_ble_desc_get` | `data_frame_tx_t *cmd_processor_ble_desc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble desc get. |
| 249 | exportada | `cmd_processor_ble_svc_discover` | `data_frame_tx_t *cmd_processor_ble_svc_discover(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble svc discover. |
| 255 | exportada | `cmd_processor_ble_svc_get` | `data_frame_tx_t *cmd_processor_ble_svc_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble svc get. |
| 262 | exportada | `cmd_processor_ble_device_info` | `data_frame_tx_t *cmd_processor_ble_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble device info. |
| 268 | exportada | `cmd_processor_ble_get_device_info` | `data_frame_tx_t *cmd_processor_ble_get_device_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get device info. |
| 275 | exportada | `cmd_processor_ble_set_addr` | `data_frame_tx_t *cmd_processor_ble_set_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble set addr. |
| 291 | exportada | `cmd_processor_ble_get_addr` | `data_frame_tx_t *cmd_processor_ble_get_addr(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble get addr. |
| 301 | exportada | `cmd_processor_ble_radio_set` | `data_frame_tx_t *cmd_processor_ble_radio_set(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble radio set. |
| 316 | exportada | `cmd_processor_ble_radio_get` | `data_frame_tx_t *cmd_processor_ble_radio_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble radio get. |
| 326 | exportada | `cmd_processor_ble_flood_start` | `data_frame_tx_t *cmd_processor_ble_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble flood start. |
| 368 | exportada | `cmd_processor_ble_flood_stop` | `data_frame_tx_t *cmd_processor_ble_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble flood stop. |
| 378 | exportada | `cmd_processor_ble_flood_count` | `data_frame_tx_t *cmd_processor_ble_flood_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble flood count. |
| 389 | exportada | `cmd_processor_ble_kick` | `data_frame_tx_t *cmd_processor_ble_kick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble kick. |
| 415 | exportada | `cmd_processor_ble_adv_flood_start` | `data_frame_tx_t *cmd_processor_ble_adv_flood_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble adv flood start. |
| 439 | exportada | `cmd_processor_ble_adv_flood_stop` | `data_frame_tx_t *cmd_processor_ble_adv_flood_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble adv flood stop. |
| 446 | static | `adv_lab_error_status` | `static uint16_t adv_lab_error_status(uint32_t error)` | Implementa adv lab error status. |
| 459 | exportada | `cmd_processor_ble_adv_lab_start` | `data_frame_tx_t *cmd_processor_ble_adv_lab_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble adv lab start. |
| 486 | exportada | `cmd_processor_ble_adv_lab_status` | `data_frame_tx_t *cmd_processor_ble_adv_lab_status(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble adv lab status. |
| 494 | exportada | `cmd_processor_ble_adv_lab_stop` | `data_frame_tx_t *cmd_processor_ble_adv_lab_stop(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando ble adv lab stop. |

## `firmware/application/src/app_cmd_keyboard.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 14 | static | `put_u16be` | `static void put_u16be(uint8_t *data, uint16_t value)` | Implementa put u16be. |
| 19 | static | `put_u32be` | `static void put_u32be(uint8_t *data, uint32_t value)` | Implementa put u32be. |
| 26 | static | `result_status` | `static uint16_t result_status(keyboard_payload_result_t result)` | Implementa result status. |
| 43 | static | `command_authorized` | `static bool command_authorized(void)` | Implementa command authorized. |
| 50 | exportada | `cmd_before_keyboard` | `data_frame_tx_t *cmd_before_keyboard(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Prepara el comando keyboard. |
| 58 | exportada | `cmd_processor_keyboard_upload_begin` | `data_frame_tx_t *cmd_processor_keyboard_upload_begin(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard upload begin. |
| 79 | exportada | `cmd_processor_keyboard_upload_chunk` | `data_frame_tx_t *cmd_processor_keyboard_upload_chunk(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard upload chunk. |
| 99 | exportada | `cmd_processor_keyboard_upload_commit` | `data_frame_tx_t *cmd_processor_keyboard_upload_commit(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard upload commit. |
| 120 | exportada | `cmd_processor_keyboard_run` | `data_frame_tx_t *cmd_processor_keyboard_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard run. |
| 141 | exportada | `cmd_processor_keyboard_cancel` | `data_frame_tx_t *cmd_processor_keyboard_cancel(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard cancel. |
| 150 | exportada | `cmd_processor_keyboard_get_status` | `data_frame_tx_t *cmd_processor_keyboard_get_status(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard get status. |
| 173 | exportada | `cmd_processor_keyboard_clear` | `data_frame_tx_t *cmd_processor_keyboard_clear(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard clear. |
| 182 | exportada | `cmd_processor_keyboard_set_temp_ble_name` | `data_frame_tx_t *cmd_processor_keyboard_set_temp_ble_name(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard set temp ble name. |
| 201 | exportada | `cmd_processor_keyboard_arm_ble` | `data_frame_tx_t *cmd_processor_keyboard_arm_ble(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Procesa el comando keyboard arm ble. |

## `firmware/application/src/app_main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 93 | exportada | `assert_nrf_callback` | `void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)` | Implementa assert nrf callback. |
| 100 | static | `app_timers_init` | `static void app_timers_init(void)` | Implementa app timers init. |
| 107 | static | `log_init` | `static void log_init(void)` | Implementa log init. |
| 116 | static | `power_management_init` | `static void power_management_init(void)` | Implementa power management init. |
| 124 | exportada | `rng_drv_and_srand_init` | `void rng_drv_and_srand_init(void)` | Implementa rng drv and srand init. |
| 154 | static | `gpio_te_init` | `static void gpio_te_init(void)` | Implementa gpio te init. |
| 161 | static | `field_generator_rainbow_loop` | `static void field_generator_rainbow_loop(void)` | Implementa field generator rainbow loop. |
| 191 | static | `button_pin_handler` | `static void button_pin_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)` | Implementa button pin handler. |
| 205 | static | `timer_button_event_handle` | `static void timer_button_event_handle(void *arg)` | Implementa timer button event handle. |
| 279 | static | `button_init` | `static void button_init(void)` | Implementa button init. |
| 302 | static | `system_off_enter` | `static void system_off_enter(void)` | Implementa system off enter. |
| 462 | static | `check_wakeup_src` | `static void check_wakeup_src(void)` | Implementa check wakeup src. |
| 595 | static | `cycle_slot` | `static void cycle_slot(bool dec)` | Implementa cycle slot. |
| 620 | static | `show_battery` | `static void show_battery(void)` | Implementa show battery. |
| 652 | static | `offline_status_blink_color` | `static void offline_status_blink_color(uint8_t blink_color)` | Implementa offline status blink color. |
| 674 | static | `offline_status_error` | `static void offline_status_error(void)` | Implementa offline status error. |
| 678 | static | `offline_status_ok` | `static void offline_status_ok(void)` | Implementa offline status ok. |
| 682 | static | `btn_fn_copy_lf` | `static void btn_fn_copy_lf(uint8_t slot, tag_specific_type_t type)` | Implementa btn fn copy lf. |
| 785 | static | `btn_fn_copy_hf` | `static void btn_fn_copy_hf(uint8_t slot, tag_specific_type_t type)` | Implementa btn fn copy hf. |
| 864 | static | `btn_fn_copy_ic_uid` | `static void btn_fn_copy_ic_uid(void)` | Implementa btn fn copy ic uid. |
| 895 | static | `btn_fn_toggle_reader_keys` | `static void btn_fn_toggle_reader_keys(void)` | Implementa btn fn toggle reader keys. |
| 921 | static | `btn_fn_ble_restart` | `static void btn_fn_ble_restart(void)` | Implementa btn fn ble restart. |
| 939 | static | `run_button_function_by_settings` | `static void run_button_function_by_settings(settings_button_function_t sbf)` | Implementa run button function by settings. |
| 1024 | static | `button_press_process` | `static void button_press_process(void)` | Implementa button press process. |
| 1067 | static | `blink_usb_led_status` | `static void blink_usb_led_status(void)` | Implementa blink usb led status. |
| 1104 | static | `lesc_event_process` | `static void lesc_event_process(void)` | Implementa lesc event process. |
| 1112 | static | `ble_passkey_init` | `static void ble_passkey_init(void)` | Implementa ble passkey init. |
| 1118 | exportada | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/application/src/ble_central.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 293 | static | `char_props_to_byte` | `static uint8_t char_props_to_byte(ble_gatt_char_props_t p)` | Implementa char props to byte. |
| 305 | static | `xorshift32` | `static uint32_t xorshift32(void)` | Implementa xorshift32. |
| 325 | static | `fuzz_build_payload` | `static void fuzz_build_payload(uint16_t iteration, uint8_t *buf, uint8_t *out_len)` | Implementa fuzz build payload. |
| 375 | static | `gatt_write_retryable` | `static bool gatt_write_retryable(ret_code_t err)` | Implementa gatt write retryable. |
| 380 | static | `timer_start` | `static ret_code_t timer_start(ble_timer_owner_t owner, uint32_t ticks)` | Implementa timer start. |
| 392 | static | `timer_stop` | `static void timer_stop(ble_timer_owner_t owner)` | Implementa timer stop. |
| 399 | static | `gatt_begin` | `static bool gatt_begin(ble_gatt_op_t op)` | Implementa gatt begin. |
| 407 | static | `gatt_finish` | `static void gatt_finish(ble_gatt_op_t op)` | Implementa gatt finish. |
| 413 | static | `reset_link_async_state` | `static void reset_link_async_state(void)` | Implementa reset link async state. |
| 435 | static | `prepare_target_connect` | `static ret_code_t prepare_target_connect(void)` | Implementa prepare target connect. |
| 441 | static | `fail_pending_gatt` | `static void fail_pending_gatt(uint8_t reason, bool fail_probe)` | Implementa fail pending gatt. |
| 481 | static | `continue_char_discovery` | `static void continue_char_discovery(uint16_t next_handle)` | Implementa continue char discovery. |
| 498 | static | `devinfo_read_next` | `static void devinfo_read_next(void)` | Implementa devinfo read next. |
| 516 | static | `probe_reset_batch_state` | `static void probe_reset_batch_state(void)` | Implementa probe reset batch state. |
| 525 | static | `probe_record_current` | `static void probe_record_current(uint8_t connect_status, uint8_t probe_result, uint8_t disconnect_reason)` | Implementa probe record current. |
| 541 | static | `probe_connect_target` | `static uint32_t probe_connect_target(uint8_t addr_type, const uint8_t *addr, int8_t rssi)` | Implementa probe connect target. |
| 580 | static | `probe_load_scan_targets` | `static uint32_t probe_load_scan_targets(void)` | Implementa probe load scan targets. |
| 597 | static | `probe_global_begin` | `static uint32_t probe_global_begin(void)` | Implementa probe global begin. |
| 630 | static | `probe_global_start_next` | `static uint32_t probe_global_start_next(void)` | Implementa probe global start next. |
| 647 | static | `probe_global_finish_current` | `static void probe_global_finish_current(uint8_t connect_status, uint8_t probe_result, uint8_t disconnect_reason)` | Implementa probe global finish current. |
| 657 | static | `ble_central_evt_handler` | `static void ble_central_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)` | Implementa ble central evt handler. |
| 1022 | static | `fuzz_timer_handler` | `static void fuzz_timer_handler(void *p_context)` | Implementa fuzz timer handler. |
| 1122 | exportada | `ble_central_init` | `void ble_central_init(void)` | Implementa ble central init. |
| 1127 | exportada | `ble_central_connect` | `uint32_t ble_central_connect(uint8_t addr_type, const uint8_t *addr)` | Implementa ble central connect. |
| 1164 | exportada | `ble_central_disconnect` | `uint32_t ble_central_disconnect(void)` | Implementa ble central disconnect. |
| 1199 | exportada | `ble_central_link_probe` | `uint32_t ble_central_link_probe(uint8_t global_mode)` | Implementa ble central link probe. |
| 1227 | exportada | `ble_central_gatt_discover` | `uint32_t ble_central_gatt_discover(void)` | Implementa ble central gatt discover. |
| 1245 | exportada | `ble_central_get_char_count` | `uint8_t ble_central_get_char_count(void)` | Implementa ble central get char count. |
| 1250 | exportada | `ble_central_desc_discover` | `uint32_t ble_central_desc_discover(void)` | Implementa ble central desc discover. |
| 1268 | exportada | `ble_central_copy_descs` | `uint16_t ble_central_copy_descs(uint8_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy descs. |
| 1291 | exportada | `ble_central_svc_discover` | `uint32_t ble_central_svc_discover(void)` | Implementa ble central svc discover. |
| 1308 | exportada | `ble_central_copy_svcs` | `uint16_t ble_central_copy_svcs(uint8_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy svcs. |
| 1336 | exportada | `ble_central_devinfo_start` | `uint32_t ble_central_devinfo_start(void)` | Implementa ble central devinfo start. |
| 1367 | exportada | `ble_central_copy_devinfo` | `uint16_t ble_central_copy_devinfo(uint8_t *out, uint16_t out_cap)` | Implementa ble central copy devinfo. |
| 1392 | exportada | `ble_central_gatt_read` | `uint32_t ble_central_gatt_read(uint16_t value_handle)` | Implementa ble central gatt read. |
| 1416 | exportada | `ble_central_gatt_write` | `uint32_t ble_central_gatt_write(uint16_t value_handle, const uint8_t *data, uint8_t len)` | Implementa ble central gatt write. |
| 1452 | exportada | `ble_central_get_write_result` | `uint16_t ble_central_get_write_result(uint8_t *out, uint16_t out_cap)` | Implementa ble central get write result. |
| 1462 | exportada | `ble_central_mtu` | `uint16_t ble_central_mtu(void)` | Implementa ble central mtu. |
| 1469 | exportada | `ble_central_write_max` | `uint16_t ble_central_write_max(void)` | Implementa ble central write max. |
| 1477 | exportada | `ble_central_copy_read` | `uint16_t ble_central_copy_read(uint8_t *out, uint16_t out_cap)` | Implementa ble central copy read. |
| 1497 | exportada | `ble_central_find_cccd` | `uint32_t ble_central_find_cccd(uint16_t value_handle)` | Implementa ble central find cccd. |
| 1518 | exportada | `ble_central_get_cccd` | `uint16_t ble_central_get_cccd(uint8_t *out, uint16_t out_cap)` | Implementa ble central get cccd. |
| 1529 | exportada | `ble_central_subscribe` | `uint32_t ble_central_subscribe(uint16_t cccd_handle, uint8_t mode)` | Implementa ble central subscribe. |
| 1561 | exportada | `ble_central_notif_count` | `uint16_t ble_central_notif_count(void)` | Implementa ble central notif count. |
| 1565 | exportada | `ble_central_copy_notifs` | `uint16_t ble_central_copy_notifs(uint16_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy notifs. |
| 1583 | exportada | `ble_central_copy_chars` | `uint16_t ble_central_copy_chars(uint8_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy chars. |
| 1600 | exportada | `ble_central_fuzz_start` | `uint32_t ble_central_fuzz_start(uint16_t value_handle, uint16_t max_iterations, uint16_t interval_ms)` | Implementa ble central fuzz start. |
| 1647 | exportada | `ble_central_fuzz_stop` | `uint32_t ble_central_fuzz_stop(void)` | Implementa ble central fuzz stop. |
| 1663 | exportada | `ble_central_flood_start` | `uint32_t ble_central_flood_start(uint16_t value_handle, uint8_t payload_size, uint16_t max_iterations, uint16_t interval_ms)` | Implementa ble central flood start. |
| 1711 | exportada | `ble_central_flood_stop` | `uint32_t ble_central_flood_stop(void)` | Implementa ble central flood stop. |
| 1724 | exportada | `ble_central_flood_count` | `uint32_t ble_central_flood_count(void)` | Implementa ble central flood count. |
| 1730 | exportada | `ble_central_kick` | `uint32_t ble_central_kick(uint8_t cycles)` | Implementa ble central kick. |
| 1749 | static | `scan_buffer_reset_state` | `static void scan_buffer_reset_state(void)` | Implementa scan buffer reset state. |
| 1760 | static | `scan_buffer_stop` | `static void scan_buffer_stop(bool disconnect_link)` | Implementa scan buffer stop. |
| 1782 | static | `scan_buffer_start_next_peer` | `static void scan_buffer_start_next_peer(void)` | Implementa scan buffer start next peer. |
| 1815 | static | `scan_buffer_disconnect_current` | `static void scan_buffer_disconnect_current(void)` | Implementa scan buffer disconnect current. |
| 1833 | static | `scan_buffer_timer_handler` | `static void scan_buffer_timer_handler(void *p_context)` | Implementa scan buffer timer handler. |
| 1950 | exportada | `ble_central_kick_scan_buffer` | `uint32_t ble_central_kick_scan_buffer(uint8_t cycles)` | Implementa ble central kick scan buffer. |
| 1998 | exportada | `ble_central_flood_scan_buffer` | `uint32_t ble_central_flood_scan_buffer(uint16_t value_handle, uint8_t payload_size, uint16_t max_iterations, uint16_t interval_ms)` | Implementa ble central flood scan buffer. |
| 2055 | exportada | `ble_central_get_state` | `uint16_t ble_central_get_state(uint8_t *out, uint16_t out_cap)` | Implementa ble central get state. |
| 2087 | exportada | `ble_central_copy_probe_log` | `uint16_t ble_central_copy_probe_log(uint8_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy probe log. |
| 2106 | exportada | `ble_central_copy_log` | `uint16_t ble_central_copy_log(uint16_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble central copy log. |
| 2128 | exportada | `ble_central_is_connected` | `bool ble_central_is_connected(void)` | Implementa ble central is connected. |
| 2132 | exportada | `ble_central_is_connecting` | `bool ble_central_is_connecting(void)` | Implementa ble central is connecting. |

## `firmware/application/src/ble_main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 176 | static | `nus_response_ready` | `static bool nus_response_ready(void)` | Implementa nus response ready. |
| 182 | static | `next_free_buf_index` | `static uint32_t next_free_buf_index(void)` | Implementa next free buf index. |
| 193 | exportada | `set_ble_connect_key` | `void set_ble_connect_key(uint8_t *key)` | Configura  ble connect key. |
| 206 | static | `gap_params_init` | `static void gap_params_init(void)` | Implementa gap params init. |
| 236 | static | `on_bas_evt` | `static void on_bas_evt(ble_bas_t *p_bas, ble_bas_evt_t *p_evt)` | Atiende bas evt. |
| 257 | static | `nus_tx_clear` | `static void nus_tx_clear(void)` | Implementa nus tx clear. |
| 270 | static | `nus_tx_send` | `static void nus_tx_send(void)` | Implementa nus tx send. |
| 352 | static | `nus_rx_resume` | `static void nus_rx_resume(void)` | Implementa nus rx resume. |
| 366 | static | `nus_rx_overrun` | `static void nus_rx_overrun(void)` | Implementa nus rx overrun. |
| 380 | static | `nus_data_handler` | `static void nus_data_handler(ble_nus_evt_t *p_evt)` | Implementa nus data handler. |
| 418 | exportada | `nus_data_response_try` | `uint32_t nus_data_response_try(const uint8_t *p_data, uint16_t length)` | Implementa nus data response try. |
| 454 | exportada | `nus_data_response` | `void nus_data_response(uint8_t *p_data, uint16_t length)` | Implementa nus data response. |
| 463 | exportada | `is_nus_working` | `bool is_nus_working(void)` | Comprueba nus working. |
| 467 | exportada | `is_nus_tx_idle` | `bool is_nus_tx_idle(void)` | Comprueba nus tx idle. |
| 483 | static | `nrf_qwr_error_handler` | `static void nrf_qwr_error_handler(uint32_t nrf_error)` | Implementa nrf qwr error handler. |
| 487 | exportada | `map` | `__INLINE uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max)` | Implementa map. |
| 492 | exportada | `BATVOL2PERCENT` | `uint32_t BATVOL2PERCENT(uint16_t VOL)` | Implementa BATVOL2PERCENT. |
| 536 | static | `services_init` | `static void services_init(void)` | Implementa services init. |
| 599 | static | `on_conn_params_evt` | `static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)` | Atiende conn params evt. |
| 612 | static | `conn_params_error_handler` | `static void conn_params_error_handler(uint32_t nrf_error)` | Implementa conn params error handler. |
| 618 | static | `conn_params_init` | `static void conn_params_init(void)` | Implementa conn params init. |
| 643 | static | `on_adv_evt` | `static void on_adv_evt(ble_adv_evt_t ble_adv_evt)` | Atiende adv evt. |
| 663 | static | `ble_evt_handler` | `static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)` | Implementa ble evt handler. |
| 831 | static | `ble_stack_init` | `static void ble_stack_init(void)` | Implementa ble stack init. |
| 852 | exportada | `gatt_evt_handler` | `void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt)` | Implementa gatt evt handler. |
| 864 | exportada | `gatt_init` | `void gatt_init(void)` | Implementa gatt init. |
| 882 | exportada | `ble_link_mtu` | `uint16_t ble_link_mtu(uint16_t conn_handle)` | Implementa ble link mtu. |
| 889 | static | `advertising_init` | `static void advertising_init(void)` | Implementa advertising init. |
| 913 | static | `valid_utf8_name` | `static bool valid_utf8_name(const uint8_t *name, uint8_t length)` | Implementa valid utf8 name. |
| 951 | static | `normal_advertising_data_update` | `static uint32_t normal_advertising_data_update(void)` | Implementa normal advertising data update. |
| 992 | exportada | `ble_peripheral_name_set_temporary` | `uint32_t ble_peripheral_name_set_temporary(const uint8_t *name, uint8_t length)` | Implementa ble peripheral name set temporary. |
| 1025 | exportada | `ble_peripheral_name_get` | `uint8_t ble_peripheral_name_get(uint8_t *name)` | Implementa ble peripheral name get. |
| 1030 | exportada | `ble_keyboard_advertising_start` | `uint32_t ble_keyboard_advertising_start(void)` | Implementa ble keyboard advertising start. |
| 1050 | exportada | `delete_bonds_all` | `void delete_bonds_all(void)` | Implementa delete bonds all. |
| 1063 | static | `whitelist_set` | `static void whitelist_set(pm_peer_id_list_skip_t skip)` | Implementa whitelist set. |
| 1078 | exportada | `advertising_start` | `void advertising_start(bool erase_bonds)` | Implementa advertising start. |
| 1114 | exportada | `advertising_stop` | `void advertising_stop(void)` | Implementa advertising stop. |
| 1126 | exportada | `is_ble_advertising` | `bool is_ble_advertising(void)` | Comprueba ble advertising. |
| 1131 | exportada | `ble_command_link_authorized` | `bool ble_command_link_authorized(void)` | Implementa ble command link authorized. |
| 1136 | exportada | `ble_keyboard_link_authorized` | `bool ble_keyboard_link_authorized(void)` | Implementa ble keyboard link authorized. |
| 1146 | static | `pm_evt_handler` | `static void pm_evt_handler(pm_evt_t const *p_evt)` | Implementa pm evt handler. |
| 1189 | static | `peer_manager_init` | `static void peer_manager_init(void)` | Implementa peer manager init. |
| 1223 | exportada | `saadc_event_handler` | `void saadc_event_handler(nrfx_saadc_evt_t const *p_event)` | Implementa saadc event handler. |
| 1235 | static | `adc_configure` | `static void adc_configure(void)` | Implementa adc configure. |
| 1255 | static | `battery_level_meas_timeout_handler` | `static void battery_level_meas_timeout_handler(void *p_context)` | Implementa battery level meas timeout handler. |
| 1295 | exportada | `create_battery_timer` | `void create_battery_timer(void)` | Implementa create battery timer. |
| 1321 | static | `restore_address_gap_procedures` | `static void restore_address_gap_procedures(bool advertising, bool flooding, bool scanning, uint8_t scan_mode, uint8_t flood_fill, uint16_t flood_interval)` | Implementa restore address gap procedures. |
| 1338 | static | `cache_original_address` | `static void cache_original_address(void)` | Implementa cache original address. |
| 1362 | exportada | `ble_addr_set` | `uint32_t ble_addr_set(uint8_t mode, const uint8_t *addr_le)` | Implementa ble addr set. |
| 1463 | exportada | `ble_addr_get` | `uint32_t ble_addr_get(uint8_t *addr_type, uint8_t *addr_out)` | Implementa ble addr get. |
| 1483 | exportada | `ble_radio_set` | `uint32_t ble_radio_set(uint8_t on)` | Implementa ble radio set. |
| 1545 | exportada | `ble_radio_get` | `uint32_t ble_radio_get(uint8_t *out)` | Implementa ble radio get. |
| 1557 | exportada | `ble_adv_flood_start` | `uint32_t ble_adv_flood_start(uint8_t fill_byte, uint16_t interval_ms)` | Implementa ble adv flood start. |
| 1628 | exportada | `ble_adv_flood_stop` | `uint32_t ble_adv_flood_stop(void)` | Implementa ble adv flood stop. |
| 1642 | static | `adv_lab_data_valid` | `static bool adv_lab_data_valid(const uint8_t *data, uint8_t length, bool scan_response, bool *has_name)` | Implementa adv lab data valid. |
| 1664 | static | `adv_lab_build_buffer` | `static void adv_lab_build_buffer(uint8_t buffer, uint8_t name_index)` | Implementa adv lab build buffer. |
| 1688 | static | `adv_lab_lengths` | `static void adv_lab_lengths(uint8_t *adv_length, uint8_t *scan_length)` | Implementa adv lab lengths. |
| 1698 | static | `adv_lab_disable_normal_modes` | `static void adv_lab_disable_normal_modes(void)` | Implementa adv lab disable normal modes. |
| 1705 | static | `adv_lab_restore_modes` | `static void adv_lab_restore_modes(bool allow_disconnect_restart)` | Implementa adv lab restore modes. |
| 1712 | static | `adv_lab_finish` | `static uint32_t adv_lab_finish(uint8_t reason, bool restore_normal)` | Implementa adv lab finish. |
| 1735 | exportada | `ble_adv_lab_is_active` | `bool ble_adv_lab_is_active(void)` | Implementa ble adv lab is active. |
| 1739 | exportada | `ble_adv_lab_start` | `uint32_t ble_adv_lab_start(uint8_t profile, uint8_t mode, uint8_t name_target, uint16_t interval_units, uint16_t rotation_ms, uint16_t duration_units, uint8_t max_adv_events, const uint8_t *adv_data, uint8_t adv_length, const uint8_t *scan_data, uint8_t scan_length, const uint8_t *names, uint16_t names_length, uint8_t name_count)` | Implementa ble adv lab start. |
| 1876 | exportada | `ble_adv_lab_process` | `void ble_adv_lab_process(void)` | Implementa ble adv lab process. |
| 1917 | exportada | `ble_adv_lab_stop` | `uint32_t ble_adv_lab_stop(void)` | Implementa ble adv lab stop. |
| 1928 | exportada | `ble_adv_lab_get_status` | `uint16_t ble_adv_lab_get_status(uint8_t *out, uint16_t max_length)` | Implementa ble adv lab get status. |
| 1963 | exportada | `ble_slave_init` | `void ble_slave_init(void)` | Implementa ble slave init. |
| 1976 | exportada | `register_lf_adc_callback` | `void register_lf_adc_callback(lf_adc_callback_t cb)` | Implementa register lf adc callback. |
| 1999 | exportada | `unregister_lf_adc_callback` | `void unregister_lf_adc_callback(void)` | Implementa unregister lf adc callback. |

## `firmware/application/src/ble_scan.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 39 | static | `ble_scan_take_snapshot` | `static uint8_t ble_scan_take_snapshot(void)` | Implementa ble scan take snapshot. |
| 49 | static | `ble_scan_record_update` | `static void ble_scan_record_update(const ble_gap_evt_adv_report_t *report)` | Implementa ble scan record update. |
| 87 | exportada | `ble_scan_start` | `uint32_t ble_scan_start(uint8_t active)` | Implementa ble scan start. |
| 105 | exportada | `ble_scan_stop` | `uint32_t ble_scan_stop(void)` | Implementa ble scan stop. |
| 116 | exportada | `is_ble_scanning` | `bool is_ble_scanning(void)` | Comprueba ble scanning. |
| 120 | exportada | `ble_scan_get_mode` | `uint8_t ble_scan_get_mode(void)` | Implementa ble scan get mode. |
| 124 | exportada | `ble_scan_mark_inactive` | `void ble_scan_mark_inactive(void)` | Implementa ble scan mark inactive. |
| 130 | exportada | `ble_scan_get_count` | `uint8_t ble_scan_get_count(void)` | Implementa ble scan get count. |
| 138 | exportada | `ble_scan_copy_records` | `uint16_t ble_scan_copy_records(uint8_t start_index, uint8_t *out, uint16_t out_cap)` | Implementa ble scan copy records. |
| 162 | exportada | `ble_scan_copy_addresses` | `uint8_t ble_scan_copy_addresses(ble_scan_addr_t *out, uint8_t out_cap)` | Implementa ble scan copy addresses. |
| 197 | exportada | `ble_scan_on_adv_report` | `void ble_scan_on_adv_report(const ble_gap_evt_adv_report_t *report)` | Implementa ble scan on adv report. |

## `firmware/application/src/bsp/bsp_delay.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 8 | exportada | `bsp_delay_ms` | `void bsp_delay_ms(uint16_t nms)` | Implementa bsp delay ms. |
| 14 | exportada | `bsp_delay_us` | `void bsp_delay_us(uint32_t nus)` | Implementa bsp delay us. |

## `firmware/application/src/bsp/bsp_time.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 28 | exportada | `bsp_obtain_timer` | `autotimer *bsp_obtain_timer(uint32_t start_value)` | Implementa bsp obtain timer. |
| 43 | exportada | `bsp_set_timer` | `inline uint8_t bsp_set_timer(autotimer *timer, uint32_t start_value)` | Implementa bsp set timer. |
| 53 | exportada | `bsp_return_timer` | `inline void bsp_return_timer(autotimer *timer)` | Implementa bsp return timer. |
| 62 | exportada | `timer_app_callback` | `void timer_app_callback(void *arg)` | Implementa timer app callback. |
| 72 | exportada | `bsp_timer_init` | `void bsp_timer_init(void)` | Implementa bsp timer init. |
| 82 | exportada | `bsp_timer_uninit` | `void bsp_timer_uninit(void)` | Implementa bsp timer uninit. |
| 88 | exportada | `bsp_timer_start` | `void bsp_timer_start(void)` | Implementa bsp timer start. |
| 100 | exportada | `bsp_timer_stop` | `void bsp_timer_stop(void)` | Implementa bsp timer stop. |

## `firmware/application/src/bsp/bsp_wdt.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 7 | static | `wdt_event_handler` | `static void wdt_event_handler(void)` | Implementa wdt event handler. |
| 15 | exportada | `bsp_wdt_init` | `void bsp_wdt_init(void)` | Implementa bsp wdt init. |
| 27 | exportada | `bsp_wdt_feed` | `void bsp_wdt_feed(void)` | Implementa bsp wdt feed. |

## `firmware/application/src/device_settings_payload_internal.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 10 | static | `device_settings_payload_v6` | `static inline void device_settings_payload_v6( uint8_t output[DEVICE_SETTINGS_V6_PAYLOAD_SIZE], uint8_t version, uint8_t animation, uint8_t button_a, uint8_t button_b, uint8_t long_button_a, uint8_t long_button_b, uint8_t pairing_enabled, const uint8_t pairing_key[DEVICE_SETTINGS_V6_PAIRING_KEY_SIZE], uint32_t sleep_timeout_ms )` | Implementa device settings payload v6. |

## `firmware/application/src/keyboard_hid.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 79 | static | `usb_context` | `static app_usbd_hid_kbd_ctx_t *usb_context(void)` | Implementa usb context. |
| 83 | static | `reports_equal` | `static bool reports_equal(uint8_t const *left, uint8_t const *right)` | Implementa reports equal. |
| 87 | static | `normalize_busy_error` | `static uint32_t normalize_busy_error(uint32_t error)` | Implementa normalize busy error. |
| 91 | static | `usb_transport_available` | `static bool usb_transport_available(void)` | Implementa usb transport available. |
| 99 | static | `usb_transfer_pending` | `static bool usb_transfer_pending(void)` | Implementa usb transfer pending. |
| 104 | exportada | `keyboard_hid_usb_ready` | `bool keyboard_hid_usb_ready(void)` | Implementa keyboard hid usb ready. |
| 111 | static | `ble_transport_available` | `static bool ble_transport_available(void)` | Implementa ble transport available. |
| 139 | exportada | `keyboard_hid_ble_ready` | `bool keyboard_hid_ble_ready(void)` | Implementa keyboard hid ble ready. |
| 145 | exportada | `keyboard_hid_outputs_ready` | `bool keyboard_hid_outputs_ready(uint8_t outputs)` | Implementa keyboard hid outputs ready. |
| 159 | static | `usb_submit_desired` | `static uint32_t usb_submit_desired(void)` | Implementa usb submit desired. |
| 191 | static | `ble_submit_desired` | `static uint32_t ble_submit_desired(void)` | Implementa ble submit desired. |
| 223 | exportada | `keyboard_hid_send_report` | `uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers, uint8_t usage)` | Implementa keyboard hid send report. |
| 265 | exportada | `keyboard_hid_abort` | `void keyboard_hid_abort(void)` | Implementa keyboard hid abort. |
| 272 | static | `usb_hid_event_handler` | `static void usb_hid_event_handler(app_usbd_class_inst_t const *instance, app_usbd_hid_user_event_t event)` | Implementa usb hid event handler. |
| 294 | exportada | `keyboard_hid_usb_class_instance` | `app_usbd_class_inst_t const *keyboard_hid_usb_class_instance(void)` | Implementa keyboard hid usb class instance. |
| 298 | exportada | `keyboard_hid_usb_reset` | `void keyboard_hid_usb_reset(void)` | Implementa keyboard hid usb reset. |
| 308 | static | `ble_hid_error_handler` | `static void ble_hid_error_handler(uint32_t error)` | Implementa ble hid error handler. |
| 312 | static | `ble_hid_event_handler` | `static void ble_hid_event_handler(ble_hids_t *hids, ble_hids_evt_t *event)` | Implementa ble hid event handler. |
| 359 | exportada | `keyboard_hid_ble_init` | `void keyboard_hid_ble_init(void)` | Implementa keyboard hid ble init. |
| 400 | static | `ble_state_clear` | `static void ble_state_clear(void)` | Implementa ble state clear. |
| 414 | exportada | `keyboard_hid_process` | `void keyboard_hid_process(void)` | Implementa keyboard hid process. |
| 443 | exportada | `keyboard_hid_on_ble_evt` | `void keyboard_hid_on_ble_evt(ble_evt_t const *event)` | Implementa keyboard hid on ble evt. |

## `firmware/application/src/keyboard_payload.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 60 | static | `read_u16be` | `static uint16_t read_u16be(const uint8_t *data)` | Lee  u16be. |
| 64 | static | `crc32_ieee` | `static uint32_t crc32_ieee(const uint8_t *data, uint16_t length)` | Implementa crc32 ieee. |
| 75 | static | `allocate_id` | `static uint32_t allocate_id(void)` | Implementa allocate id. |
| 83 | static | `deadline_reached` | `static bool deadline_reached(uint32_t now, uint32_t deadline)` | Implementa deadline reached. |
| 88 | static | `finish` | `static void finish(keyboard_payload_state_t state, keyboard_payload_error_t error)` | Implementa finish. |
| 99 | static | `finish_cancel_request` | `static bool finish_cancel_request(void)` | Implementa finish cancel request. |
| 116 | static | `send_report` | `static bool send_report(uint8_t modifiers, uint8_t usage)` | Envia  report. |
| 131 | static | `validate` | `static keyboard_payload_error_t validate(const uint8_t *payload, uint16_t length)` | Valida validate. |
| 176 | static | `outputs_ready` | `static bool outputs_ready(uint32_t now)` | Implementa outputs ready. |
| 192 | exportada | `keyboard_payload_upload_begin` | `keyboard_payload_result_t keyboard_payload_upload_begin( uint16_t total, uint32_t crc32, data_frame_transport_t transport, uint32_t *upload_id, uint16_t *next_offset)` | Implementa keyboard payload upload begin. |
| 220 | exportada | `keyboard_payload_upload_chunk` | `keyboard_payload_result_t keyboard_payload_upload_chunk( uint32_t upload_id, uint16_t offset, const uint8_t *data, uint16_t length, data_frame_transport_t transport, uint16_t *next_offset)` | Implementa keyboard payload upload chunk. |
| 248 | exportada | `keyboard_payload_upload_commit` | `keyboard_payload_result_t keyboard_payload_upload_commit( uint32_t upload_id, data_frame_transport_t transport, uint32_t *commit_id, uint16_t *length, uint32_t *crc32)` | Implementa keyboard payload upload commit. |
| 298 | exportada | `keyboard_payload_run` | `keyboard_payload_result_t keyboard_payload_run( uint32_t commit_id, uint8_t outputs, data_frame_transport_t transport, uint32_t *run_id)` | Implementa keyboard payload run. |
| 332 | exportada | `keyboard_payload_arm_ble` | `keyboard_payload_result_t keyboard_payload_arm_ble( uint32_t commit_id, uint32_t *run_id)` | Implementa keyboard payload arm ble. |
| 357 | exportada | `keyboard_payload_cancel` | `void keyboard_payload_cancel(void)` | Implementa keyboard payload cancel. |
| 375 | exportada | `keyboard_payload_cancel_command_link` | `void keyboard_payload_cancel_command_link(void)` | Implementa keyboard payload cancel command link. |
| 382 | exportada | `keyboard_payload_command_link_alive` | `bool keyboard_payload_command_link_alive(void)` | Implementa keyboard payload command link alive. |
| 398 | exportada | `keyboard_payload_cancel_from_button` | `bool keyboard_payload_cancel_from_button(void)` | Implementa keyboard payload cancel from button. |
| 408 | exportada | `keyboard_payload_clear` | `keyboard_payload_result_t keyboard_payload_clear(void)` | Implementa keyboard payload clear. |
| 439 | exportada | `keyboard_payload_get_status` | `void keyboard_payload_get_status(keyboard_payload_status_t *status)` | Implementa keyboard payload get status. |
| 456 | exportada | `keyboard_payload_is_running` | `bool keyboard_payload_is_running(void)` | Implementa keyboard payload is running. |
| 461 | exportada | `keyboard_payload_is_armed` | `bool keyboard_payload_is_armed(void)` | Implementa keyboard payload is armed. |
| 465 | exportada | `keyboard_payload_process` | `void keyboard_payload_process(void)` | Implementa keyboard payload process. |

## `firmware/application/src/rfid/crc_utils.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 47 | exportada | `calc_14a_crc_lut` | `void calc_14a_crc_lut(const uint8_t *data, size_t length, uint8_t *output)` | Implementa calc 14a crc lut. |

## `firmware/application/src/rfid/hex_utils.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 11 | exportada | `num_to_bytes` | `void num_to_bytes(uint64_t n, uint8_t len, uint8_t *dest)` | Implementa num to bytes. |
| 25 | exportada | `bytes_to_num` | `uint64_t bytes_to_num(uint8_t *src, uint8_t len)` | Implementa bytes to num. |

## `firmware/application/src/rfid/mf1_crapto1.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 21 | exportada | `filter` | `uint32_t filter(uint32_t const x)` | Implementa filter. |
| 36 | exportada | `crypto1_init` | `void crypto1_init(struct Crypto1State *state, uint64_t key)` | Implementa crypto1 init. |
| 47 | exportada | `crypto1_deinit` | `void inline crypto1_deinit(struct Crypto1State *state)` | Implementa crypto1 deinit. |
| 53 | exportada | `crypto1_create` | `struct Crypto1State *crypto1_create(uint64_t key)` | Implementa crypto1 create. |
| 60 | exportada | `crypto1_destroy` | `void crypto1_destroy(struct Crypto1State *state)` | Implementa crypto1 destroy. |
| 65 | exportada | `crypto1_get_lfsr` | `void crypto1_get_lfsr(struct Crypto1State *state, uint64_t *lfsr)` | Implementa crypto1 get lfsr. |
| 72 | exportada | `crypto1_bit` | `uint8_t crypto1_bit(struct Crypto1State *s, uint8_t in, int is_encrypted)` | Implementa crypto1 bit. |
| 88 | exportada | `crypto1_byte` | `uint8_t crypto1_byte(struct Crypto1State *s, uint8_t in, int is_encrypted)` | Implementa crypto1 byte. |
| 100 | exportada | `crypto1_word` | `uint32_t crypto1_word(struct Crypto1State *s, uint32_t in, int is_encrypted)` | Implementa crypto1 word. |
| 147 | exportada | `prng_successor` | `uint32_t prng_successor(uint32_t x, uint32_t n)` | Implementa prng successor. |

## `firmware/application/src/rfid/mf1_crapto1.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 58 | static | `filter` | `static inline int filter(uint32_t const x)` | Implementa filter. |

## `firmware/application/src/rfid/mf1_crypto1.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 326 | exportada | `Crypto1GetState` | `void Crypto1GetState(uint8_t *pEven, uint8_t *pOdd)` | Implementa Crypto1GetState. |
| 348 | static | `Crypto1LFSRbyteFeedback` | `static uint8_t Crypto1LFSRbyteFeedback(uint8_t E0, uint8_t E1, uint8_t E2, uint8_t O0, uint8_t O1, uint8_t O2)` | Implementa Crypto1LFSRbyteFeedback. |
| 377 | static | `Crypto1LFSR` | `static void Crypto1LFSR(uint8_t In)` | Implementa Crypto1LFSR. |
| 419 | exportada | `Crypto1FilterOutput` | `uint8_t Crypto1FilterOutput(void)` | Implementa Crypto1FilterOutput. |
| 425 | exportada | `Crypto1Setup` | `void Crypto1Setup(uint8_t Key[6], uint8_t Uid[4], uint8_t CardNonce[4])` | Implementa Crypto1Setup. |
| 525 | exportada | `Crypto1SetupNested` | `void Crypto1SetupNested(uint8_t Key[6], uint8_t Uid[4], uint8_t CardNonce[4], uint8_t NonceParity[4], bool Decrypt)` | Implementa Crypto1SetupNested. |
| 634 | exportada | `Crypto1Auth` | `void Crypto1Auth(uint8_t EncryptedReaderNonce[NONCE_SIZE])` | Implementa Crypto1Auth. |
| 727 | exportada | `Crypto1Nibble` | `uint8_t Crypto1Nibble(void)` | Implementa Crypto1Nibble. |
| 777 | exportada | `Crypto1Byte` | `uint8_t Crypto1Byte(void)` | Implementa Crypto1Byte. |
| 857 | exportada | `Crypto1ByteArray` | `void Crypto1ByteArray(uint8_t *Buffer, uint8_t Count)` | Implementa Crypto1ByteArray. |
| 942 | exportada | `Crypto1ByteArrayWithParity` | `void Crypto1ByteArrayWithParity(uint8_t *Buffer, uint8_t *Parity, uint8_t Count)` | Implementa Crypto1ByteArrayWithParity. |
| 1034 | exportada | `Crypto1ByteArrayWithParityHasIn` | `void Crypto1ByteArrayWithParityHasIn(uint8_t *Buffer, uint8_t *Parity, uint8_t Count)` | Implementa Crypto1ByteArrayWithParityHasIn. |
| 1145 | exportada | `Crypto1PRNG` | `void Crypto1PRNG(uint8_t State[4], uint8_t ClockCount)` | Implementa Crypto1PRNG. |
| 1191 | exportada | `Crypto1FreePRNG` | `uint32_t Crypto1FreePRNG(uint32_t x, uint32_t n)` | Implementa Crypto1FreePRNG. |
| 1201 | exportada | `Crypto1EncryptWithParity` | `void Crypto1EncryptWithParity(uint8_t *Buffer, uint8_t BitCount)` | Implementa Crypto1EncryptWithParity. |
| 1212 | exportada | `Crypto1ReaderAuthWithParity` | `void Crypto1ReaderAuthWithParity(uint8_t PlainReaderAnswerWithParityBits[9])` | Implementa Crypto1ReaderAuthWithParity. |

## `firmware/application/src/rfid/nfctag/hf/crypto1_helper.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 4 | exportada | `mf_crypto1_decryptEx` | `void mf_crypto1_decryptEx(struct Crypto1State *pcs, uint8_t *data_in, int len, uint8_t *data_out)` | Implementa mf crypto1 decryptEx. |
| 19 | exportada | `mf_crypto1_decrypt` | `void mf_crypto1_decrypt(struct Crypto1State *pcs, uint8_t *data, int len)` | Implementa mf crypto1 decrypt. |
| 23 | exportada | `mf_crypto1_encryptEx` | `void mf_crypto1_encryptEx(struct Crypto1State *pcs, uint8_t *data_in, uint8_t *keystream, uint8_t *data_out, uint16_t len, uint8_t *par)` | Implementa mf crypto1 encryptEx. |
| 34 | exportada | `mf_crypto1_encrypt` | `void mf_crypto1_encrypt(struct Crypto1State *pcs, uint8_t *data, uint16_t len, uint8_t *par)` | Implementa mf crypto1 encrypt. |
| 38 | exportada | `mf_crypto1_encrypt4bit` | `uint8_t mf_crypto1_encrypt4bit(struct Crypto1State *pcs, uint8_t data)` | Implementa mf crypto1 encrypt4bit. |

## `firmware/application/src/rfid/nfctag/hf/nfc_14a.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 70 | exportada | `nfc_tag_14a_set_sniff_cb` | `void nfc_tag_14a_set_sniff_cb(nfc_tag_14a_sniff_cb_t cb)` | Implementa nfc tag 14a set sniff cb. |
| 74 | exportada | `nfc_tag_14a_clear_sniff_cb` | `void nfc_tag_14a_clear_sniff_cb(void)` | Implementa nfc tag 14a clear sniff cb. |
| 81 | exportada | `nfc_tag_14a_set_tx_sniff_cb` | `void nfc_tag_14a_set_tx_sniff_cb(nfc_tag_14a_tx_sniff_cb_t cb)` | Implementa nfc tag 14a set tx sniff cb. |
| 85 | exportada | `nfc_tag_14a_clear_tx_sniff_cb` | `void nfc_tag_14a_clear_tx_sniff_cb(void)` | Implementa nfc tag 14a clear tx sniff cb. |
| 93 | exportada | `nfc_tag_14a_set_sniff_passive` | `void nfc_tag_14a_set_sniff_passive(bool passive)` | Implementa nfc tag 14a set sniff passive. |
| 107 | exportada | `nfc_tag_14a_create_bcc` | `void nfc_tag_14a_create_bcc(uint8_t *pbtData, size_t szLen, uint8_t *pbtBcc)` | Implementa nfc tag 14a create bcc. |
| 121 | exportada | `nfc_tag_14a_append_bcc` | `inline void nfc_tag_14a_append_bcc(uint8_t *pbtData, size_t szLen)` | Implementa nfc tag 14a append bcc. |
| 130 | exportada | `nfc_tag_14a_append_crc` | `inline void nfc_tag_14a_append_crc(uint8_t *pbtData, size_t szLen)` | Implementa nfc tag 14a append crc. |
| 138 | exportada | `nfc_tag_14a_checks_crc` | `bool nfc_tag_14a_checks_crc(uint8_t *pbtData, size_t szLen)` | Implementa nfc tag 14a checks crc. |
| 159 | exportada | `nfc_tag_14a_wrap_frame` | `uint8_t nfc_tag_14a_wrap_frame(const uint8_t *pbtTx, const size_t szTxBits, const uint8_t *pbtTxPar, uint8_t *pbtFrame)` | Implementa nfc tag 14a wrap frame. |
| 221 | exportada | `nfc_tag_14a_unwrap_frame` | `size_t nfc_tag_14a_unwrap_frame(const uint8_t *pbtFrame, const size_t szFrameBits, uint8_t *pbtRx, uint8_t *pbtRxPar)` | Implementa nfc tag 14a unwrap frame. |
| 258 | exportada | `nfc_tag_14a_tx_bytes` | `void nfc_tag_14a_tx_bytes(uint8_t *data, uint32_t bytes, bool appendCrc)` | Implementa nfc tag 14a tx bytes. |
| 285 | exportada | `nfc_tag_14a_tx_bits` | `void nfc_tag_14a_tx_bits(uint8_t *data, uint32_t bits)` | Implementa nfc tag 14a tx bits. |
| 298 | exportada | `nfc_tag_14a_tx_nbit` | `void nfc_tag_14a_tx_nbit(uint8_t data, uint32_t bits)` | Implementa nfc tag 14a tx nbit. |
| 308 | exportada | `nfc_tag_14a_data_process` | `void nfc_tag_14a_data_process(uint8_t *p_data)` | Implementa nfc tag 14a data process. |
| 574 | static | `nrf_nfct_reset` | `static inline void nrf_nfct_reset(void)` | Implementa nrf nfct reset. |
| 609 | static | `nfc_fdt_reset` | `static inline void nfc_fdt_reset(void)` | Implementa nfc fdt reset. |
| 621 | exportada | `nfc_tag_14a_event_callback` | `void nfc_tag_14a_event_callback(nrfx_nfct_evt_t const *p_event)` | Implementa nfc tag 14a event callback. |
| 739 | exportada | `nfc_tag_14a_set_state` | `void nfc_tag_14a_set_state(nfc_tag_14a_state_t state)` | Implementa nfc tag 14a set state. |
| 747 | exportada | `nfc_tag_14a_set_handler` | `void nfc_tag_14a_set_handler(nfc_tag_14a_handler_t *handler)` | Implementa nfc tag 14a set handler. |
| 765 | exportada | `nfc_tag_14a_sense_switch` | `void nfc_tag_14a_sense_switch(bool enable)` | Implementa nfc tag 14a sense switch. |
| 787 | exportada | `is_valid_uid_size` | `bool is_valid_uid_size(uint8_t uid_length)` | Comprueba valid uid size. |
| 793 | exportada | `nfc_tag_14a_set_reset_enable` | `void nfc_tag_14a_set_reset_enable(bool enable)` | Implementa nfc tag 14a set reset enable. |
| 797 | exportada | `nfc_tag_14a_is_reset_enable` | `bool nfc_tag_14a_is_reset_enable()` | Implementa nfc tag 14a is reset enable. |

## `firmware/application/src/rfid/nfctag/hf/nfc_14a_4.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 40 | static | `is_iblock` | `static inline bool is_iblock(uint8_t pcb)` | Comprueba iblock. |
| 43 | static | `is_rblock` | `static inline bool is_rblock(uint8_t pcb)` | Comprueba rblock. |
| 46 | static | `is_sblock` | `static inline bool is_sblock(uint8_t pcb)` | Comprueba sblock. |
| 115 | exportada | `nfc_tag_14a_4_add_static_response` | `void nfc_tag_14a_4_add_static_response(const uint8_t *cmd, uint8_t cmd_len, const uint8_t *resp, uint16_t resp_len)` | Implementa nfc tag 14a 4 add static response. |
| 149 | exportada | `nfc_tag_14a_4_clear_static_responses` | `void nfc_tag_14a_4_clear_static_responses(void)` | Implementa nfc tag 14a 4 clear static responses. |
| 157 | static | `find_static_response` | `static bool find_static_response(const uint8_t *apdu, uint16_t apdu_len, uint8_t **resp_out, uint16_t *resp_len_out)` | Implementa find static response. |
| 186 | static | `send_frame` | `static void send_frame(const uint8_t *data, uint16_t len)` | Envia  frame. |
| 193 | static | `resend_last_reply` | `static void resend_last_reply(void)` | Implementa resend last reply. |
| 199 | static | `parse_cid` | `static bool parse_cid(uint8_t pcb, const uint8_t *data, uint16_t len, uint8_t *offset)` | Interpreta  cid. |
| 213 | static | `send_rblock` | `static void send_rblock(bool nak, uint8_t block_num)` | Envia  rblock. |
| 223 | static | `response_inf_capacity` | `static uint16_t response_inf_capacity(void)` | Implementa response inf capacity. |
| 230 | static | `send_next_iblock` | `static void send_next_iblock(void)` | Envia  next iblock. |
| 259 | static | `start_response` | `static void start_response(void)` | Inicia  response. |
| 266 | static | `send_wtx` | `static void send_wtx(void)` | Envia  wtx. |
| 280 | static | `nfc_tag_14a_4_state_handler` | `static void nfc_tag_14a_4_state_handler(uint8_t *data, uint16_t szBits)` | Implementa nfc tag 14a 4 state handler. |
| 440 | exportada | `nfc_tag_14a_4_get_pending_apdu` | `bool nfc_tag_14a_4_get_pending_apdu(uint8_t *buf, uint16_t *length)` | Implementa nfc tag 14a 4 get pending apdu. |
| 454 | exportada | `nfc_tag_14a_4_set_response` | `void nfc_tag_14a_4_set_response(const uint8_t *data, uint16_t length)` | Implementa nfc tag 14a 4 set response. |
| 470 | exportada | `nfc_tag_14a_4_reset_handler` | `void nfc_tag_14a_4_reset_handler(void)` | Implementa nfc tag 14a 4 reset handler. |
| 494 | exportada | `nfc_tag_14a_4_activate` | `void nfc_tag_14a_4_activate(uint8_t fsdi, uint8_t cid)` | Implementa nfc tag 14a 4 activate. |
| 518 | exportada | `nfc_tag_14a_4_get_debug_counters` | `void nfc_tag_14a_4_get_debug_counters(uint8_t *rx, uint8_t *tx, uint8_t *last_pcb, uint8_t *last_match)` | Implementa nfc tag 14a 4 get debug counters. |
| 531 | exportada | `nfc_tag_14a_4_get_coll_res` | `nfc_tag_14a_coll_res_reference_t *nfc_tag_14a_4_get_coll_res(void)` | Implementa nfc tag 14a 4 get coll res. |
| 545 | exportada | `nfc_tag_14a_4_data_loadcb` | `int nfc_tag_14a_4_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag 14a 4 data loadcb. |
| 584 | exportada | `nfc_tag_14a_4_data_savecb` | `int nfc_tag_14a_4_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag 14a 4 data savecb. |
| 588 | exportada | `nfc_tag_14a_4_data_factory` | `bool nfc_tag_14a_4_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa nfc tag 14a 4 data factory. |

## `firmware/application/src/rfid/nfctag/hf/nfc_14a_frame_internal.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 9 | static | `nfc_tag_14a_unwrap_frame_checked` | `static inline size_t nfc_tag_14a_unwrap_frame_checked( const uint8_t *frame, size_t frame_bits, uint8_t *rx, uint8_t *rx_parity )` | Implementa nfc tag 14a unwrap frame checked. |

## `firmware/application/src/rfid/nfctag/hf/nfc_mf0_ntag.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 149 | static | `ensure_auth_log_valid` | `static void ensure_auth_log_valid(void)` | Implementa ensure auth log valid. |
| 157 | exportada | `nfc_tag_mf0_ntag_get_nr_pages_by_tag_type` | `int nfc_tag_mf0_ntag_get_nr_pages_by_tag_type(tag_specific_type_t tag_type)` | Implementa nfc tag mf0 ntag get nr pages by tag type. |
| 196 | static | `get_nr_pages_by_tag_type` | `static int get_nr_pages_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  nr pages by tag type. |
| 202 | static | `get_total_pages_by_tag_type` | `static int get_total_pages_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  total pages by tag type. |
| 241 | static | `get_first_cfg_page_by_tag_type` | `static int get_first_cfg_page_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  first cfg page by tag type. |
| 274 | static | `get_block_max_by_tag_type` | `static int get_block_max_by_tag_type(tag_specific_type_t tag_type, bool read)` | Obtiene  block max by tag type. |
| 289 | static | `is_ntag_type` | `static bool is_ntag_type(tag_specific_type_t type)` | Comprueba ntag type. |
| 302 | static | `is_ntag` | `static bool is_ntag(void)` | Comprueba ntag. |
| 306 | exportada | `get_version_page_by_tag_type` | `int get_version_page_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  version page by tag type. |
| 341 | exportada | `get_signature_page_by_tag_type` | `int get_signature_page_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  signature page by tag type. |
| 374 | exportada | `nfc_tag_mf0_ntag_get_version_data` | `uint8_t *nfc_tag_mf0_ntag_get_version_data()` | Implementa nfc tag mf0 ntag get version data. |
| 381 | exportada | `nfc_tag_mf0_ntag_get_signature_data` | `uint8_t *nfc_tag_mf0_ntag_get_signature_data()` | Implementa nfc tag mf0 ntag get signature data. |
| 388 | static | `handle_get_version_command` | `static void handle_get_version_command()` | Gestiona  get version command. |
| 401 | static | `handle_read_sig_command` | `static void handle_read_sig_command()` | Gestiona  read sig command. |
| 414 | static | `mirror_size_for_mode` | `static int mirror_size_for_mode(uint8_t mirror_mode)` | Implementa mirror size for mode. |
| 428 | static | `get_user_data_end_by_tag_type` | `static int get_user_data_end_by_tag_type(tag_specific_type_t type)` | Obtiene  user data end by tag type. |
| 467 | static | `get_counter_data_by_index` | `static uint8_t *get_counter_data_by_index(uint8_t index, bool external)` | Obtiene  counter data by index. |
| 518 | exportada | `nfc_tag_mf0_ntag_get_counter_data_by_index` | `uint8_t *nfc_tag_mf0_ntag_get_counter_data_by_index(uint8_t index)` | Implementa nfc tag mf0 ntag get counter data by index. |
| 523 | static | `handle_any_read` | `static void handle_any_read(uint8_t block_num, uint8_t block_cnt, uint8_t block_max)` | Gestiona  any read. |
| 640 | static | `handle_read_command` | `static void handle_read_command(uint8_t block_num)` | Gestiona  read command. |
| 655 | static | `handle_fast_read_command` | `static void handle_fast_read_command(uint8_t block_num, uint8_t end_block_num)` | Gestiona  fast read command. |
| 687 | static | `check_ro_lock_on_page` | `static bool check_ro_lock_on_page(int block_num)` | Implementa check ro lock on page. |
| 848 | static | `handle_write_command` | `static int handle_write_command(uint8_t block_num, uint8_t *p_data)` | Gestiona  write command. |
| 923 | static | `handle_read_cnt_command` | `static void handle_read_cnt_command(uint8_t index)` | Gestiona  read cnt command. |
| 946 | static | `handle_incr_cnt_command` | `static void handle_incr_cnt_command(uint8_t block_num, uint8_t *p_data)` | Gestiona  incr cnt command. |
| 989 | static | `handle_pwd_auth_command` | `static void handle_pwd_auth_command(uint8_t *p_data)` | Gestiona  pwd auth command. |
| 1032 | static | `handle_check_tearing_event` | `static void handle_check_tearing_event(int index)` | Gestiona  check tearing event. |
| 1053 | static | `handle_vcsl_command` | `static void handle_vcsl_command(uint16_t szDataBits)` | Gestiona  vcsl command. |
| 1073 | static | `nfc_tag_mf0_ntag_state_handler` | `static void nfc_tag_mf0_ntag_state_handler(uint8_t *p_data, uint16_t szDataBits)` | Implementa nfc tag mf0 ntag state handler. |
| 1161 | exportada | `nfc_tag_mf0_ntag_get_coll_res` | `nfc_tag_14a_coll_res_reference_t *nfc_tag_mf0_ntag_get_coll_res()` | Implementa nfc tag mf0 ntag get coll res. |
| 1172 | static | `nfc_tag_mf0_ntag_reset_handler` | `static void nfc_tag_mf0_ntag_reset_handler()` | Implementa nfc tag mf0 ntag reset handler. |
| 1178 | static | `get_information_size_by_tag_type` | `static int get_information_size_by_tag_type(tag_specific_type_t type)` | Obtiene  information size by tag type. |
| 1187 | exportada | `nfc_tag_mf0_ntag_data_savecb` | `int nfc_tag_mf0_ntag_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf0 ntag data savecb. |
| 1206 | exportada | `nfc_tag_mf0_ntag_data_save_failcb` | `void nfc_tag_mf0_ntag_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf0 ntag data save failcb. |
| 1214 | exportada | `nfc_tag_mf0_ntag_data_loadcb` | `int nfc_tag_mf0_ntag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf0 ntag data loadcb. |
| 1250 | exportada | `nfc_tag_mf0_ntag_data_factory` | `bool nfc_tag_mf0_ntag_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa nfc tag mf0 ntag data factory. |
| 1394 | exportada | `nfc_tag_mf0_ntag_get_uid_mode` | `int nfc_tag_mf0_ntag_get_uid_mode()` | Implementa nfc tag mf0 ntag get uid mode. |
| 1400 | exportada | `nfc_tag_mf0_ntag_set_uid_mode` | `bool nfc_tag_mf0_ntag_set_uid_mode(bool enabled)` | Implementa nfc tag mf0 ntag set uid mode. |
| 1407 | exportada | `nfc_tag_mf0_ntag_set_write_mode` | `void nfc_tag_mf0_ntag_set_write_mode(nfc_tag_mf0_ntag_write_mode_t write_mode)` | Implementa nfc tag mf0 ntag set write mode. |
| 1416 | exportada | `nfc_tag_mf0_ntag_get_write_mode` | `nfc_tag_mf0_ntag_write_mode_t nfc_tag_mf0_ntag_get_write_mode(void)` | Implementa nfc tag mf0 ntag get write mode. |
| 1423 | exportada | `mf0_get_auth_log` | `nfc_tag_mf0_ntag_auth_log_t *mf0_get_auth_log(uint32_t *count)` | Implementa mf0 get auth log. |
| 1429 | exportada | `nfc_tag_mf0_ntag_set_detection_enable` | `void nfc_tag_mf0_ntag_set_detection_enable(bool enable)` | Implementa nfc tag mf0 ntag set detection enable. |
| 1434 | exportada | `nfc_tag_mf0_ntag_is_detection_enable` | `bool nfc_tag_mf0_ntag_is_detection_enable(void)` | Implementa nfc tag mf0 ntag is detection enable. |
| 1439 | exportada | `nfc_tag_mf0_ntag_detection_log_clear` | `void nfc_tag_mf0_ntag_detection_log_clear(void)` | Implementa nfc tag mf0 ntag detection log clear. |
| 1444 | exportada | `nfc_tag_mf0_ntag_detection_log_count` | `uint32_t nfc_tag_mf0_ntag_detection_log_count(void)` | Implementa nfc tag mf0 ntag detection log count. |

## `firmware/application/src/rfid/nfctag/hf/nfc_mf1.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 239 | static | `ensure_auth_log_valid` | `static void ensure_auth_log_valid(void)` | Implementa ensure auth log valid. |
| 247 | static | `block_to_sector` | `static uint8_t block_to_sector(uint8_t block)` | Implementa block to sector. |
| 251 | static | `sector_trailer` | `static uint8_t sector_trailer(uint8_t sector)` | Implementa sector trailer. |
| 255 | static | `is_sector_trailer` | `static bool is_sector_trailer(uint8_t block)` | Comprueba sector trailer. |
| 260 | static | `is_authenticated_block` | `static bool is_authenticated_block(uint8_t block)` | Comprueba authenticated block. |
| 266 | exportada | `GetAccessCondition` | `uint8_t GetAccessCondition(uint8_t Block)` | Implementa GetAccessCondition. |
| 314 | static | `data_permissions` | `static uint8_t data_permissions(uint8_t block)` | Implementa data permissions. |
| 319 | static | `trailer_permissions` | `static uint8_t trailer_permissions(uint8_t block)` | Implementa trailer permissions. |
| 324 | exportada | `CheckValueIntegrity` | `bool CheckValueIntegrity(uint8_t *Block)` | Implementa CheckValueIntegrity. |
| 339 | exportada | `ValueFromBlock` | `void ValueFromBlock(uint32_t *Value, uint8_t *Block)` | Implementa ValueFromBlock. |
| 347 | exportada | `ValueToBlock` | `void ValueToBlock(uint8_t *Block, uint32_t Value)` | Implementa ValueToBlock. |
| 368 | exportada | `nfc_tag_mf1_prng_seed` | `void nfc_tag_mf1_prng_seed(uint32_t seed)` | Implementa nfc tag mf1 prng seed. |
| 372 | exportada | `nfc_tag_mf1_set_prng_type` | `void nfc_tag_mf1_set_prng_type(uint8_t type)` | Implementa nfc tag mf1 set prng type. |
| 378 | exportada | `nfc_tag_mf1_get_prng_type` | `uint8_t nfc_tag_mf1_get_prng_type(void)` | Implementa nfc tag mf1 get prng type. |
| 390 | exportada | `nfc_tag_mf1_random_nonce` | `void nfc_tag_mf1_random_nonce(uint8_t nonce[4], bool isNested)` | Implementa nfc tag mf1 random nonce. |
| 422 | exportada | `append_mf1_auth_log_step1` | `void append_mf1_auth_log_step1(bool isKeyB, bool isNested, uint8_t block, uint8_t *nonce)` | Implementa append mf1 auth log step1. |
| 445 | exportada | `append_mf1_auth_log_step2` | `void append_mf1_auth_log_step2(uint8_t *nr, uint8_t *ar)` | Implementa append mf1 auth log step2. |
| 464 | exportada | `append_mf1_auth_log_step3` | `void append_mf1_auth_log_step3(bool is_auth_success)` | Implementa append mf1 auth log step3. |
| 481 | exportada | `mf1_get_auth_log` | `nfc_tag_mf1_auth_log_t *mf1_get_auth_log(uint32_t *count)` | Implementa mf1 get auth log. |
| 489 | static | `get_block_max_by_tag_type` | `static int get_block_max_by_tag_type(tag_specific_type_t tag_type)` | Obtiene  block max by tag type. |
| 509 | static | `check_block_max_overflow` | `static bool check_block_max_overflow(uint8_t block)` | Implementa check block max overflow. |
| 515 | exportada | `mf1_prng_by_bytes` | `void mf1_prng_by_bytes(uint8_t *nonces, uint32_t n)` | Implementa mf1 prng by bytes. |
| 522 | exportada | `mf1_response_4bit_auto_encrypt` | `void mf1_response_4bit_auto_encrypt(uint8_t value)` | Implementa mf1 response 4bit auto encrypt. |
| 535 | exportada | `nfc_tag_mf1_state_handler` | `void nfc_tag_mf1_state_handler(uint8_t *p_data, uint16_t szDataBits)` | Implementa nfc tag mf1 state handler. |
| 1181 | exportada | `get_mifare_coll_res` | `nfc_tag_14a_coll_res_reference_t *get_mifare_coll_res()` | Obtiene  mifare coll res. |
| 1204 | exportada | `get_saved_mifare_coll_res` | `nfc_tag_14a_coll_res_reference_t *get_saved_mifare_coll_res()` | Obtiene  saved mifare coll res. |
| 1218 | static | `nfc_tag_mf1_regen_random_uid` | `static void nfc_tag_mf1_regen_random_uid(void)` | Implementa nfc tag mf1 regen random uid. |
| 1237 | exportada | `nfc_tag_mf1_reset_handler` | `void nfc_tag_mf1_reset_handler()` | Implementa nfc tag mf1 reset handler. |
| 1262 | static | `get_information_size_by_tag_type` | `static int get_information_size_by_tag_type(tag_specific_type_t type)` | Obtiene  information size by tag type. |
| 1271 | exportada | `nfc_tag_mf1_data_savecb` | `int nfc_tag_mf1_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data savecb. |
| 1288 | exportada | `nfc_tag_mf1_data_save_failcb` | `void nfc_tag_mf1_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data save failcb. |
| 1300 | exportada | `nfc_tag_mf1_data_loadcb` | `int nfc_tag_mf1_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data loadcb. |
| 1332 | exportada | `nfc_tag_mf1_data_factory` | `bool nfc_tag_mf1_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa nfc tag mf1 data factory. |
| 1395 | exportada | `nfc_tag_mf1_set_detection_enable` | `void nfc_tag_mf1_set_detection_enable(bool enable)` | Implementa nfc tag mf1 set detection enable. |
| 1400 | exportada | `nfc_tag_mf1_is_detection_enable` | `bool nfc_tag_mf1_is_detection_enable(void)` | Implementa nfc tag mf1 is detection enable. |
| 1405 | exportada | `nfc_tag_mf1_set_random_uid_mode` | `void nfc_tag_mf1_set_random_uid_mode(bool enable)` | Implementa nfc tag mf1 set random uid mode. |
| 1417 | exportada | `nfc_tag_mf1_is_random_uid_mode` | `bool nfc_tag_mf1_is_random_uid_mode(void)` | Implementa nfc tag mf1 is random uid mode. |
| 1422 | exportada | `nfc_tag_mf1_detection_log_clear` | `void nfc_tag_mf1_detection_log_clear(void)` | Implementa nfc tag mf1 detection log clear. |
| 1428 | exportada | `nfc_tag_mf1_detection_log_count` | `uint32_t nfc_tag_mf1_detection_log_count(void)` | Implementa nfc tag mf1 detection log count. |
| 1434 | exportada | `nfc_tag_mf1_set_gen1a_magic_mode` | `void nfc_tag_mf1_set_gen1a_magic_mode(bool enable)` | Implementa nfc tag mf1 set gen1a magic mode. |
| 1439 | exportada | `nfc_tag_mf1_is_gen1a_magic_mode` | `bool nfc_tag_mf1_is_gen1a_magic_mode(void)` | Implementa nfc tag mf1 is gen1a magic mode. |
| 1444 | exportada | `nfc_tag_mf1_set_gen2_magic_mode` | `void nfc_tag_mf1_set_gen2_magic_mode(bool enable)` | Implementa nfc tag mf1 set gen2 magic mode. |
| 1449 | exportada | `nfc_tag_mf1_is_gen2_magic_mode` | `bool nfc_tag_mf1_is_gen2_magic_mode(void)` | Implementa nfc tag mf1 is gen2 magic mode. |
| 1454 | exportada | `nfc_tag_mf1_set_use_mf1_coll_res` | `void nfc_tag_mf1_set_use_mf1_coll_res(bool enable)` | Implementa nfc tag mf1 set use mf1 coll res. |
| 1459 | exportada | `nfc_tag_mf1_is_use_mf1_coll_res` | `bool nfc_tag_mf1_is_use_mf1_coll_res(void)` | Implementa nfc tag mf1 is use mf1 coll res. |
| 1464 | exportada | `nfc_tag_mf1_set_write_mode` | `void nfc_tag_mf1_set_write_mode(nfc_tag_mf1_write_mode_t write_mode)` | Implementa nfc tag mf1 set write mode. |
| 1472 | exportada | `nfc_tag_mf1_get_write_mode` | `nfc_tag_mf1_write_mode_t nfc_tag_mf1_get_write_mode(void)` | Implementa nfc tag mf1 get write mode. |
| 1476 | exportada | `nfc_tag_mf1_set_field_off_do_reset` | `void nfc_tag_mf1_set_field_off_do_reset(bool enable)` | Implementa nfc tag mf1 set field off do reset. |
| 1480 | exportada | `nfc_tag_mf1_is_field_off_do_reset` | `bool nfc_tag_mf1_is_field_off_do_reset(void)` | Implementa nfc tag mf1 is field off do reset. |

## `firmware/application/src/rfid/nfctag/hf/ntag_mirror_internal.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 13 | static | `ntag_mirror_hex_digit` | `static inline uint8_t ntag_mirror_hex_digit(uint8_t value)` | Implementa ntag mirror hex digit. |
| 17 | static | `ntag_mirror_bytes_to_hex` | `static inline void ntag_mirror_bytes_to_hex( const uint8_t *bytes, size_t length, uint8_t *output )` | Implementa ntag mirror bytes to hex. |
| 28 | static | `ntag_mirror_render_uid` | `static inline void ntag_mirror_render_uid(const uint8_t uid[7], uint8_t output[NTAG_MIRROR_UID_SIZE])` | Implementa ntag mirror render uid. |
| 32 | static | `ntag_mirror_render_counter` | `static inline void ntag_mirror_render_counter( const uint8_t counter[3], uint8_t output[NTAG_MIRROR_COUNTER_SIZE] )` | Implementa ntag mirror render counter. |
| 39 | static | `ntag_mirror_render_uid_counter` | `static inline void ntag_mirror_render_uid_counter( const uint8_t uid[7], const uint8_t counter[3], uint8_t output[NTAG_MIRROR_UID_COUNTER_SIZE] )` | Implementa ntag mirror render uid counter. |
| 49 | static | `ntag_mirror_overlay_page` | `static inline size_t ntag_mirror_overlay_page( uint8_t page[NTAG_MIRROR_PAGE_SIZE], uint8_t page_number, uint8_t first_page, uint8_t first_page_offset, const uint8_t *mirror, size_t mirror_size )` | Implementa ntag mirror overlay page. |

## `firmware/application/src/rfid/nfctag/lf/lf_tag_em.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 49 | static | `lf_field_lost` | `static void lf_field_lost(void)` | Implementa lf field lost. |
| 64 | exportada | `is_lf_field_exists` | `bool is_lf_field_exists(void)` | Comprueba lf field exists. |
| 79 | static | `lpcomp_event_handler` | `static void lpcomp_event_handler(nrf_lpcomp_event_t event)` | Implementa lpcomp event handler. |
| 114 | static | `lpcomp_init` | `static void lpcomp_init(void)` | Implementa lpcomp init. |
| 125 | static | `pwm_handler` | `static void pwm_handler(nrfx_pwm_evt_type_t event_type)` | Implementa pwm handler. |
| 132 | exportada | `lf_tag_emulation_process` | `void lf_tag_emulation_process(void)` | Implementa lf tag emulation process. |
| 153 | static | `pwm_init` | `static void pwm_init(void)` | Implementa pwm init. |
| 175 | static | `lf_sense_enable` | `static bool lf_sense_enable(void)` | Implementa lf sense enable. |
| 220 | static | `lf_sense_disable` | `static void lf_sense_disable(void)` | Implementa lf sense disable. |
| 234 | static | `lf_em410x_id_size` | `static uint16_t lf_em410x_id_size(tag_specific_type_t type)` | Implementa lf em410x id size. |
| 241 | exportada | `lf_tag_125khz_sense_switch` | `void lf_tag_125khz_sense_switch(bool enable)` | Implementa lf tag 125khz sense switch. |
| 258 | static | `lf_load_protocol` | `static bool lf_load_protocol(tag_specific_type_t type, const protocol *p, uint8_t *data)` | Implementa lf load protocol. |
| 289 | exportada | `lf_tag_data_loadcb` | `int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag data loadcb. |
| 361 | exportada | `lf_tag_em410x_data_savecb` | `int lf_tag_em410x_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag em410x data savecb. |
| 378 | exportada | `lf_tag_hidprox_data_savecb` | `int lf_tag_hidprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag hidprox data savecb. |
| 389 | exportada | `lf_tag_ioprox_data_savecb` | `int lf_tag_ioprox_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag ioprox data savecb. |
| 400 | exportada | `lf_tag_viking_data_savecb` | `int lf_tag_viking_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag viking data savecb. |
| 406 | exportada | `lf_tag_data_factory` | `bool lf_tag_data_factory(uint8_t slot, tag_specific_type_t tag_type, uint8_t *tag_id, uint16_t length)` | Implementa lf tag data factory. |
| 426 | exportada | `lf_tag_em410x_data_factory` | `bool lf_tag_em410x_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag em410x data factory. |
| 447 | exportada | `lf_tag_hidprox_data_factory` | `bool lf_tag_hidprox_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag hidprox data factory. |
| 458 | exportada | `lf_tag_ioprox_data_factory` | `bool lf_tag_ioprox_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag ioprox data factory. |
| 470 | exportada | `lf_tag_viking_data_factory` | `bool lf_tag_viking_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag viking data factory. |
| 476 | exportada | `lf_tag_pac_data_savecb` | `int lf_tag_pac_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag pac data savecb. |
| 480 | exportada | `lf_tag_pac_data_factory` | `bool lf_tag_pac_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag pac data factory. |
| 486 | exportada | `lf_tag_jablotron_data_savecb` | `int lf_tag_jablotron_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag jablotron data savecb. |
| 490 | exportada | `lf_tag_jablotron_data_factory` | `bool lf_tag_jablotron_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag jablotron data factory. |
| 497 | exportada | `lf_tag_idteck_data_savecb` | `int lf_tag_idteck_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag idteck data savecb. |
| 502 | exportada | `lf_tag_idteck_data_factory` | `bool lf_tag_idteck_data_factory(uint8_t slot, tag_specific_type_t tag_type)` | Implementa lf tag idteck data factory. |

## `firmware/application/src/rfid/nfctag/lf/protocols/em410x.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 77 | exportada | `em410x_raw_data` | `uint64_t em410x_raw_data(uint8_t *uid)` | Implementa em410x raw data. |
| 100 | exportada | `em410x_raw_epilogue` | `uint64_t em410x_raw_epilogue(uint8_t *uid)` | Implementa em410x raw epilogue. |
| 111 | exportada | `em410x_get_time` | `bool em410x_get_time(uint16_t divisor, uint8_t interval, uint8_t base)` | Implementa em410x get time. |
| 116 | exportada | `em410x_period` | `uint8_t em410x_period(uint16_t divisor, uint8_t interval)` | Implementa em410x period. |
| 129 | exportada | `em410x_64_period` | `uint8_t em410x_64_period(uint8_t interval)` | Implementa em410x 64 period. |
| 133 | exportada | `em410x_32_period` | `uint8_t em410x_32_period(uint8_t interval)` | Implementa em410x 32 period. |
| 137 | exportada | `em410x_16_period` | `uint8_t em410x_16_period(uint8_t interval)` | Implementa em410x 16 period. |
| 141 | static | `em410x_alloc` | `static em410x_codec *em410x_alloc(period rp)` | Implementa em410x alloc. |
| 155 | exportada | `em410x_64_alloc` | `em410x_codec *em410x_64_alloc(void)` | Implementa em410x 64 alloc. |
| 159 | exportada | `em410x_32_alloc` | `em410x_codec *em410x_32_alloc(void)` | Implementa em410x 32 alloc. |
| 163 | exportada | `em410x_16_alloc` | `em410x_codec *em410x_16_alloc(void)` | Implementa em410x 16 alloc. |
| 167 | exportada | `em410x_free` | `void em410x_free(em410x_codec *d)` | Implementa em410x free. |
| 175 | exportada | `em410x_get_data` | `uint8_t *em410x_get_data(em410x_codec *d)` | Implementa em410x get data. |
| 177 | exportada | `em410x_decoder_start` | `void em410x_decoder_start(em410x_codec *d, uint8_t format)` | Implementa em410x decoder start. |
| 186 | exportada | `em410x_decode_feed` | `bool em410x_decode_feed(em410x_codec *d, bool bit)` | Implementa em410x decode feed. |
| 233 | exportada | `em410x_decoder_feed` | `bool em410x_decoder_feed(em410x_codec *d, uint16_t interval)` | Implementa em410x decoder feed. |
| 253 | exportada | `em410x_electra_decoder_start` | `void em410x_electra_decoder_start(em410x_codec *d, uint8_t format)` | Implementa em410x electra decoder start. |
| 257 | static | `em410x_electra_decode_feed` | `static bool em410x_electra_decode_feed(em410x_codec *d, bool bit)` | Implementa em410x electra decode feed. |
| 310 | exportada | `em410x_electra_decoder_feed` | `bool em410x_electra_decoder_feed(em410x_codec *d, uint16_t interval)` | Implementa em410x electra decoder feed. |
| 326 | exportada | `em410x_modulator` | `const nrf_pwm_sequence_t *em410x_modulator(em410x_codec *d, uint8_t *buf)` | Implementa em410x modulator. |
| 339 | exportada | `em410x_electra_modulator` | `const nrf_pwm_sequence_t *em410x_electra_modulator(em410x_codec *d, uint8_t *buf)` | Implementa em410x electra modulator. |
| 419 | exportada | `em410x_t55xx_writer` | `uint8_t em410x_t55xx_writer(uint8_t *uid, uint32_t *blks)` | Implementa em410x t55xx writer. |
| 427 | exportada | `em410x_electra_t55xx_writer` | `uint8_t em410x_electra_t55xx_writer(uint8_t *uid, uint32_t *blks)` | Implementa em410x electra t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/hidprox.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 35 | exportada | `decoder_reset` | `void decoder_reset(hidprox_codec *d)` | Implementa decoder reset. |
| 43 | exportada | `hidprox_decoder_start` | `void hidprox_decoder_start(hidprox_codec *d, uint8_t format_hint)` | Implementa hidprox decoder start. |
| 49 | exportada | `hidprox_codec_alloc` | `hidprox_codec *hidprox_codec_alloc(void)` | Implementa hidprox codec alloc. |
| 63 | exportada | `hidprox_codec_free` | `void hidprox_codec_free(hidprox_codec *d)` | Implementa hidprox codec free. |
| 76 | static | `hidprox_codec_get_length` | `static uint8_t hidprox_codec_get_length(hidprox_codec *d)` | Implementa hidprox codec get length. |
| 90 | exportada | `hidprox_get_data` | `uint8_t *hidprox_get_data(hidprox_codec *d)` | Implementa hidprox get data. |
| 103 | exportada | `hidprox_decode_feed` | `bool hidprox_decode_feed(hidprox_codec *d, bool bit)` | Implementa hidprox decode feed. |
| 153 | exportada | `hidprox_decoder_feed` | `bool hidprox_decoder_feed(hidprox_codec *d, uint16_t val)` | Implementa hidprox decoder feed. |
| 161 | exportada | `hidprox_raw_data` | `void hidprox_raw_data(wiegand_card_t *card, uint32_t *hi, uint32_t *mid, uint32_t *bot)` | Implementa hidprox raw data. |
| 189 | exportada | `hidprox_modulator` | `const nrf_pwm_sequence_t *hidprox_modulator(hidprox_codec *d, uint8_t *buf)` | Implementa hidprox modulator. |
| 244 | exportada | `hidprox_t55xx_writer` | `uint8_t hidprox_t55xx_writer(wiegand_card_t *card, uint32_t *blks)` | Implementa hidprox t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/idteck.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 31 | static | `idteck_alloc` | `static idteck_codec *idteck_alloc(void)` | Implementa idteck alloc. |
| 40 | static | `idteck_free` | `static void idteck_free(idteck_codec *d)` | Implementa idteck free. |
| 44 | static | `idteck_get_data` | `static uint8_t *idteck_get_data(idteck_codec *d)` | Implementa idteck get data. |
| 53 | static | `idteck_decoder_start` | `static void idteck_decoder_start(idteck_codec *d, uint8_t format)` | Implementa idteck decoder start. |
| 58 | static | `idteck_decoder_feed` | `static bool idteck_decoder_feed(idteck_codec *d, uint16_t val)` | Implementa idteck decoder feed. |
| 67 | static | `idteck_modulator` | `static const nrf_pwm_sequence_t *idteck_modulator(idteck_codec *d, uint8_t *buf)` | Implementa idteck modulator. |
| 92 | exportada | `idteck_t55xx_writer` | `uint8_t idteck_t55xx_writer(uint8_t *uid, uint32_t *blks)` | Implementa idteck t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/ioprox.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 30 | exportada | `ioprox_reset_bits` | `void ioprox_reset_bits(ioprox_codec_t *d)` | Implementa ioprox reset bits. |
| 34 | static | `push_bit` | `static inline void push_bit(ioprox_codec_t *d, uint8_t bit)` | Implementa push bit. |
| 44 | static | `get_bit_inv` | `static inline uint8_t get_bit_inv(const uint8_t *bits, uint16_t pos, bool inv)` | Obtiene  bit inv. |
| 50 | static | `bytebits_to_byte` | `static inline uint32_t bytebits_to_byte(const uint8_t *bits, uint16_t len)` | Implementa bytebits to byte. |
| 59 | static | `bytebits_to_u8_msb_inv` | `static inline uint8_t bytebits_to_u8_msb_inv(const uint8_t *bits, uint16_t start_pos, bool inv)` | Implementa bytebits to u8 msb inv. |
| 68 | exportada | `ioprox_raw8_to_bits` | `bool ioprox_raw8_to_bits(const uint8_t *raw8, ioprox_codec_t *d)` | Implementa ioprox raw8 to bits. |
| 83 | static | `ioprox_checksum5` | `static inline uint8_t ioprox_checksum5(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)` | Implementa ioprox checksum5. |
| 90 | static | `preamble_match` | `static bool preamble_match(const uint8_t *d, uint16_t off, bool inv)` | Implementa preamble match. |
| 115 | static | `decode_and_pack` | `static bool decode_and_pack(ioprox_codec_t *d, uint16_t idx, bool inv)` | Implementa decode and pack. |
| 160 | exportada | `ioprox_decode_raw_to_data` | `bool ioprox_decode_raw_to_data(const uint8_t *raw8, uint8_t *output)` | Implementa ioprox decode raw to data. |
| 175 | static | `write_bits_msb` | `static void write_bits_msb(uint8_t *bits, uint16_t pos, uint8_t v)` | Escribe  bits msb. |
| 184 | exportada | `ioprox_encode_params_to_data` | `bool ioprox_encode_params_to_data(uint8_t version, uint8_t facility, uint16_t number, uint8_t *output)` | Implementa ioprox encode params to data. |
| 236 | static | `scan_tail` | `static bool scan_tail(ioprox_codec_t *d)` | Implementa scan tail. |
| 285 | static | `ioprox_codec_alloc` | `static void *ioprox_codec_alloc(void)` | Implementa ioprox codec alloc. |
| 297 | static | `ioprox_codec_free` | `static void ioprox_codec_free(void *codec)` | Implementa ioprox codec free. |
| 307 | static | `ioprox_get_data` | `static uint8_t *ioprox_get_data(void *codec)` | Implementa ioprox get data. |
| 312 | static | `ioprox_decoder_start` | `static void ioprox_decoder_start(void *codec, uint8_t format_hint)` | Implementa ioprox decoder start. |
| 320 | static | `ioprox_decoder_feed` | `static bool ioprox_decoder_feed(void *codec, uint16_t val)` | Implementa ioprox decoder feed. |
| 347 | static | `ioprox_emit_bit` | `static inline void ioprox_emit_bit(int *k, bool bit)` | Implementa ioprox emit bit. |
| 364 | exportada | `ioprox_modulator` | `const nrf_pwm_sequence_t *ioprox_modulator(ioprox_codec_t *d, uint8_t *buf)` | Implementa ioprox modulator. |
| 401 | exportada | `ioprox_t55xx_writer` | `uint8_t ioprox_t55xx_writer(uint8_t *buf, uint32_t *blks)` | Implementa ioprox t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/jablotron.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 65 | static | `jablotron_raw_data` | `static uint64_t jablotron_raw_data(uint8_t *uid)` | Implementa jablotron raw data. |
| 81 | static | `jablotron_get_time` | `static bool jablotron_get_time(uint8_t interval, uint8_t base)` | Implementa jablotron get time. |
| 86 | static | `jablotron_period` | `static uint8_t jablotron_period(uint8_t interval)` | Implementa jablotron period. |
| 99 | static | `jablotron_alloc` | `static jablotron_codec *jablotron_alloc(void)` | Implementa jablotron alloc. |
| 113 | static | `jablotron_free` | `static void jablotron_free(jablotron_codec *d)` | Implementa jablotron free. |
| 121 | static | `jablotron_get_data` | `static uint8_t *jablotron_get_data(jablotron_codec *d)` | Implementa jablotron get data. |
| 125 | static | `jablotron_decoder_start` | `static void jablotron_decoder_start(jablotron_codec *d, uint8_t format)` | Implementa jablotron decoder start. |
| 132 | static | `jablotron_decode_feed` | `static bool jablotron_decode_feed(jablotron_codec *d, bool bit)` | Implementa jablotron decode feed. |
| 171 | static | `jablotron_decoder_feed` | `static bool jablotron_decoder_feed(jablotron_codec *d, uint16_t interval)` | Implementa jablotron decoder feed. |
| 209 | static | `jablotron_modulator` | `static const nrf_pwm_sequence_t *jablotron_modulator(jablotron_codec *d, uint8_t *buf)` | Implementa jablotron modulator. |
| 258 | exportada | `jablotron_data_valid` | `bool jablotron_data_valid(const uint8_t *uid)` | Implementa jablotron data valid. |
| 262 | exportada | `jablotron_t55xx_writer` | `uint8_t jablotron_t55xx_writer(uint8_t *uid, uint32_t *blks)` | Implementa jablotron t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/pac.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 83 | static | `shift_bit` | `static void shift_bit(pac_codec *d, bool bit)` | Implementa shift bit. |
| 90 | static | `get_bit` | `static bool get_bit(pac_codec *d, uint16_t pos)` | Obtiene  bit. |
| 99 | static | `decode_uart_byte` | `static int decode_uart_byte(pac_codec *d, uint16_t start, bool inverted)` | Implementa decode uart byte. |
| 131 | static | `try_decode_frame` | `static bool try_decode_frame(pac_codec *d, bool inverted)` | Implementa try decode frame. |
| 169 | static | `pac_process_interval` | `static bool pac_process_interval(pac_codec *d, uint32_t interval)` | Implementa pac process interval. |
| 196 | static | `pac_alloc` | `static pac_codec *pac_alloc(void)` | Implementa pac alloc. |
| 201 | static | `pac_free` | `static void pac_free(pac_codec *d)` | Implementa pac free. |
| 205 | static | `pac_get_data` | `static uint8_t *pac_get_data(pac_codec *d)` | Implementa pac get data. |
| 209 | static | `pac_decoder_start` | `static void pac_decoder_start(pac_codec *d, uint8_t format)` | Implementa pac decoder start. |
| 219 | static | `pac_decoder_feed` | `static bool pac_decoder_feed(pac_codec *d, uint16_t raw_sample)` | Implementa pac decoder feed. |
| 309 | static | `pac_build_bitstream` | `static void pac_build_bitstream(const uint8_t *card_id, uint8_t *bits_out)` | Implementa pac build bitstream. |
| 354 | exportada | `pac_data_valid` | `bool pac_data_valid(const uint8_t *data)` | Implementa pac data valid. |
| 366 | static | `pac_modulator` | `static const nrf_pwm_sequence_t *pac_modulator(pac_codec *d, uint8_t *buf)` | Implementa pac modulator. |
| 388 | exportada | `pac_t55xx_writer` | `uint8_t pac_t55xx_writer(uint8_t *data, uint32_t *blks)` | Implementa pac t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/viking.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 51 | static | `viking_raw_data` | `static uint64_t viking_raw_data(uint8_t *uid)` | Implementa viking raw data. |
| 64 | static | `viking_get_time` | `static bool viking_get_time(uint8_t interval, uint8_t base)` | Implementa viking get time. |
| 69 | static | `viking_period` | `static uint8_t viking_period(uint8_t interval)` | Implementa viking period. |
| 82 | static | `viking_alloc` | `static viking_codec *viking_alloc(void)` | Implementa viking alloc. |
| 96 | static | `viking_free` | `static void viking_free(viking_codec *d)` | Implementa viking free. |
| 104 | static | `viking_get_data` | `static uint8_t *viking_get_data(viking_codec *d)` | Implementa viking get data. |
| 108 | static | `viking_decoder_start` | `static void viking_decoder_start(viking_codec *d, uint8_t format)` | Implementa viking decoder start. |
| 115 | static | `viking_decode_feed` | `static bool viking_decode_feed(viking_codec *d, bool bit)` | Implementa viking decode feed. |
| 150 | static | `viking_decoder_feed` | `static bool viking_decoder_feed(viking_codec *d, uint16_t interval)` | Implementa viking decoder feed. |
| 192 | static | `viking_modulator` | `static const nrf_pwm_sequence_t *viking_modulator(viking_codec *d, uint8_t *buf)` | Implementa viking modulator. |
| 222 | exportada | `viking_t55xx_writer` | `uint8_t viking_t55xx_writer(uint8_t *uid, uint32_t *blks)` | Implementa viking t55xx writer. |

## `firmware/application/src/rfid/nfctag/lf/protocols/wiegand.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 41 | exportada | `wiegand_card_alloc` | `wiegand_card_t *wiegand_card_alloc()` | Implementa wiegand card alloc. |
| 49 | static | `get_nonlinear_fields` | `static uint64_t get_nonlinear_fields(uint64_t n, const uint8_t *map, size_t size)` | Obtiene  nonlinear fields. |
| 60 | static | `pack_nonlinear` | `static uint64_t pack_nonlinear( wiegand_card_t *card, const uint8_t *fc_map, size_t fc_map_size, const uint8_t *cn_map, size_t cn_map_size)` | Implementa pack nonlinear. |
| 71 | static | `unpack_nonlinear` | `static wiegand_card_t *unpack_nonlinear( uint64_t hi, uint64_t lo, const uint8_t *fc_map, size_t fc_map_size, const uint8_t *cn_map, size_t cn_map_size)` | Implementa unpack nonlinear. |
| 91 | static | `pack_h10301` | `static uint64_t pack_h10301(wiegand_card_t *card)` | Implementa pack h10301. |
| 106 | static | `unpack_h10301` | `static wiegand_card_t *unpack_h10301(uint64_t hi, uint64_t lo)` | Implementa unpack h10301. |
| 117 | static | `pack_ind26` | `static uint64_t pack_ind26(wiegand_card_t *card)` | Implementa pack ind26. |
| 136 | static | `unpack_ind26` | `static wiegand_card_t *unpack_ind26(uint64_t hi, uint64_t lo)` | Implementa unpack ind26. |
| 150 | static | `pack_ind27` | `static uint64_t pack_ind27(wiegand_card_t *card)` | Implementa pack ind27. |
| 157 | static | `unpack_ind27` | `static wiegand_card_t *unpack_ind27(uint64_t hi, uint64_t lo)` | Implementa unpack ind27. |
| 164 | static | `pack_indasc27` | `static uint64_t pack_indasc27(wiegand_card_t *card)` | Implementa pack indasc27. |
| 168 | static | `unpack_indasc27` | `static wiegand_card_t *unpack_indasc27(uint64_t hi, uint64_t lo)` | Implementa unpack indasc27. |
| 172 | static | `pack_tecom27` | `static uint64_t pack_tecom27(wiegand_card_t *card)` | Implementa pack tecom27. |
| 176 | static | `unpack_tecom27` | `static wiegand_card_t *unpack_tecom27(uint64_t hi, uint64_t lo)` | Implementa unpack tecom27. |
| 180 | static | `pack_2804w` | `static uint64_t pack_2804w(wiegand_card_t *card)` | Implementa pack 2804w. |
| 198 | static | `unpack_2804w` | `static wiegand_card_t *unpack_2804w(uint64_t hi, uint64_t lo)` | Implementa unpack 2804w. |
| 210 | static | `pack_ind29` | `static uint64_t pack_ind29(wiegand_card_t *card)` | Implementa pack ind29. |
| 217 | static | `unpack_ind29` | `static wiegand_card_t *unpack_ind29(uint64_t hi, uint64_t lo)` | Implementa unpack ind29. |
| 224 | static | `pack_atsw30` | `static uint64_t pack_atsw30(wiegand_card_t *card)` | Implementa pack atsw30. |
| 239 | static | `unpack_atsw30` | `static wiegand_card_t *unpack_atsw30(uint64_t hi, uint64_t lo)` | Implementa unpack atsw30. |
| 250 | static | `pack_adt31` | `static uint64_t pack_adt31(wiegand_card_t *card)` | Implementa pack adt31. |
| 259 | static | `unpack_adt31` | `static wiegand_card_t *unpack_adt31(uint64_t hi, uint64_t lo)` | Implementa unpack adt31. |
| 266 | static | `pack_hcp32` | `static uint64_t pack_hcp32(wiegand_card_t *card)` | Implementa pack hcp32. |
| 274 | static | `unpack_hcp32` | `static wiegand_card_t *unpack_hcp32(uint64_t hi, uint64_t lo)` | Implementa unpack hcp32. |
| 280 | static | `pack_hpp32` | `static uint64_t pack_hpp32(wiegand_card_t *card)` | Implementa pack hpp32. |
| 288 | static | `unpack_hpp32` | `static wiegand_card_t *unpack_hpp32(uint64_t hi, uint64_t lo)` | Implementa unpack hpp32. |
| 295 | static | `pack_kastle` | `static uint64_t pack_kastle(wiegand_card_t *card)` | Implementa pack kastle. |
| 311 | static | `unpack_kastle` | `static wiegand_card_t *unpack_kastle(uint64_t hi, uint64_t lo)` | Implementa unpack kastle. |
| 326 | static | `pack_kantech` | `static uint64_t pack_kantech(wiegand_card_t *card)` | Implementa pack kantech. |
| 335 | static | `unpack_kantech` | `static wiegand_card_t *unpack_kantech(uint64_t hi, uint64_t lo)` | Implementa unpack kantech. |
| 342 | static | `pack_wie32` | `static uint64_t pack_wie32(wiegand_card_t *card)` | Implementa pack wie32. |
| 350 | static | `unpack_wie32` | `static wiegand_card_t *unpack_wie32(uint64_t hi, uint64_t lo)` | Implementa unpack wie32. |
| 357 | static | `pack_d10202` | `static uint64_t pack_d10202(wiegand_card_t *card)` | Implementa pack d10202. |
| 372 | static | `unpack_d10202` | `static wiegand_card_t *unpack_d10202(uint64_t hi, uint64_t lo)` | Implementa unpack d10202. |
| 383 | static | `pack_h10306` | `static uint64_t pack_h10306(wiegand_card_t *card)` | Implementa pack h10306. |
| 398 | static | `unpack_h10306` | `static wiegand_card_t *unpack_h10306(uint64_t hi, uint64_t lo)` | Implementa unpack h10306. |
| 409 | static | `pack_n10002` | `static uint64_t pack_n10002(wiegand_card_t *card)` | Implementa pack n10002. |
| 424 | static | `unpack_n10002` | `static wiegand_card_t *unpack_n10002(uint64_t hi, uint64_t lo)` | Implementa unpack n10002. |
| 435 | static | `pack_optus` | `static uint64_t pack_optus(wiegand_card_t *card)` | Implementa pack optus. |
| 445 | static | `unpack_optus` | `static wiegand_card_t *unpack_optus(uint64_t hi, uint64_t lo)` | Implementa unpack optus. |
| 452 | static | `pack_smartpass` | `static uint64_t pack_smartpass(wiegand_card_t *card)` | Implementa pack smartpass. |
| 462 | static | `unpack_smartpass` | `static wiegand_card_t *unpack_smartpass(uint64_t hi, uint64_t lo)` | Implementa unpack smartpass. |
| 470 | static | `pack_bqt34` | `static uint64_t pack_bqt34(wiegand_card_t *card)` | Implementa pack bqt34. |
| 485 | static | `unpack_bqt34` | `static wiegand_card_t *unpack_bqt34(uint64_t hi, uint64_t lo)` | Implementa unpack bqt34. |
| 496 | static | `pack_c1k35s` | `static uint64_t pack_c1k35s(wiegand_card_t *card)` | Implementa pack c1k35s. |
| 514 | static | `unpack_c1k35s` | `static wiegand_card_t *unpack_c1k35s(uint64_t hi, uint64_t lo)` | Implementa unpack c1k35s. |
| 526 | static | `pack_c15001` | `static uint64_t pack_c15001(wiegand_card_t *card)` | Implementa pack c15001. |
| 545 | static | `unpack_c15001` | `static wiegand_card_t *unpack_c15001(uint64_t hi, uint64_t lo)` | Implementa unpack c15001. |
| 557 | static | `pack_actprox` | `static uint64_t pack_actprox(wiegand_card_t *card)` | Implementa pack actprox. |
| 576 | static | `unpack_actprox` | `static wiegand_card_t *unpack_actprox(uint64_t hi, uint64_t lo)` | Implementa unpack actprox. |
| 588 | static | `pack_s12906` | `static uint64_t pack_s12906(wiegand_card_t *card)` | Implementa pack s12906. |
| 604 | static | `unpack_s12906` | `static wiegand_card_t *unpack_s12906(uint64_t hi, uint64_t lo)` | Implementa unpack s12906. |
| 616 | static | `pack_sie36` | `static uint64_t pack_sie36(wiegand_card_t *card)` | Implementa pack sie36. |
| 631 | static | `unpack_sie36` | `static wiegand_card_t *unpack_sie36(uint64_t hi, uint64_t lo)` | Implementa unpack sie36. |
| 642 | static | `pack_h10320` | `static uint64_t pack_h10320(wiegand_card_t *card)` | Implementa pack h10320. |
| 666 | static | `unpack_h10320` | `static wiegand_card_t *unpack_h10320(uint64_t hi, uint64_t lo)` | Implementa unpack h10320. |
| 695 | static | `pack_h10302` | `static uint64_t pack_h10302(wiegand_card_t *card)` | Implementa pack h10302. |
| 709 | static | `unpack_h10302` | `static wiegand_card_t *unpack_h10302(uint64_t hi, uint64_t lo)` | Implementa unpack h10302. |
| 719 | static | `pack_h10304` | `static uint64_t pack_h10304(wiegand_card_t *card)` | Implementa pack h10304. |
| 734 | static | `unpack_h10304` | `static wiegand_card_t *unpack_h10304(uint64_t hi, uint64_t lo)` | Implementa unpack h10304. |
| 745 | static | `pack_p10004` | `static uint64_t pack_p10004(wiegand_card_t *card)` | Implementa pack p10004. |
| 755 | static | `unpack_p10004` | `static wiegand_card_t *unpack_p10004(uint64_t hi, uint64_t lo)` | Implementa unpack p10004. |
| 763 | static | `pack_hgeneric37` | `static uint64_t pack_hgeneric37(wiegand_card_t *card)` | Implementa pack hgeneric37. |
| 783 | static | `unpack_hgeneric37` | `static wiegand_card_t *unpack_hgeneric37(uint64_t hi, uint64_t lo)` | Implementa unpack hgeneric37. |
| 797 | static | `pack_mdi37` | `static uint64_t pack_mdi37(wiegand_card_t *card)` | Implementa pack mdi37. |
| 812 | static | `unpack_mdi37` | `static wiegand_card_t *unpack_mdi37(uint64_t hi, uint64_t lo)` | Implementa unpack mdi37. |
| 862 | exportada | `pack` | `uint64_t pack(wiegand_card_t *card)` | Implementa pack. |
| 875 | exportada | `unpack` | `wiegand_card_t *unpack(uint8_t format_hint, uint8_t length, uint64_t hi, uint64_t lo)` | Implementa unpack. |

## `firmware/application/src/rfid/nfctag/lf/utils/circular_buffer.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 6 | static | `cb_configure` | `static bool cb_configure(circular_buffer *cb, void *storage, size_t capacity, size_t sz, bool owns_buffer)` | Implementa cb configure. |
| 24 | exportada | `cb_init` | `bool cb_init(circular_buffer *cb, size_t capacity, size_t sz)` | Implementa cb init. |
| 37 | exportada | `cb_init_static` | `bool cb_init_static(circular_buffer *cb, void *storage, size_t capacity, size_t sz)` | Implementa cb init static. |
| 41 | exportada | `cb_free` | `void cb_free(circular_buffer *cb)` | Implementa cb free. |
| 54 | exportada | `cb_push_back` | `bool cb_push_back(circular_buffer *cb, const void *item)` | Implementa cb push back. |
| 73 | exportada | `cb_pop_front` | `bool cb_pop_front(circular_buffer *cb, void *item)` | Implementa cb pop front. |
| 91 | exportada | `cb_dropped` | `size_t cb_dropped(const circular_buffer *cb)` | Implementa cb dropped. |

## `firmware/application/src/rfid/nfctag/lf/utils/diphase.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 6 | exportada | `diphase_reset` | `void diphase_reset(diphase *d)` | Implementa diphase reset. |
| 37 | exportada | `diphase_feed` | `void diphase_feed(diphase *d, uint8_t interval, bool *bits, int8_t *bitlen)` | Implementa diphase feed. |

## `firmware/application/src/rfid/nfctag/lf/utils/fskdemod.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 10 | exportada | `goertzel_mag` | `float goertzel_mag(float coef, uint16_t samples[], int n)` | Implementa goertzel mag. |
| 24 | exportada | `fsk_free` | `void fsk_free(fsk_t *m)` | Implementa fsk free. |
| 34 | exportada | `fsk_feed` | `bool fsk_feed(fsk_t *m, uint16_t sample, bool *bit)` | Implementa fsk feed. |
| 56 | exportada | `fsk_alloc` | `fsk_t *fsk_alloc(uint8_t bitrate)` | Implementa fsk alloc. |

## `firmware/application/src/rfid/nfctag/lf/utils/manchester.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 6 | exportada | `manchester_reset` | `void manchester_reset(manchester *m)` | Implementa manchester reset. |
| 12 | exportada | `manchester_feed` | `void manchester_feed(manchester *m, uint8_t interval, bool *bits, int8_t *bitlen)` | Implementa manchester feed. |

## `firmware/application/src/rfid/nfctag/lf/utils/psk1.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 5 | static | `read_bit_msb_first` | `static inline bool read_bit_msb_first(const uint8_t *frame_bytes, size_t bit_idx)` | Lee  bit msb first. |
| 12 | static | `read_last_bit` | `static inline bool read_last_bit(const uint8_t *frame_bytes, size_t bit_count)` | Lee  last bit. |
| 17 | exportada | `lf_psk1_build_sequence` | `size_t lf_psk1_build_sequence(const uint8_t *frame_bytes, size_t bit_count, nrf_pwm_values_wave_form_t *out_buf, size_t out_capacity)` | Implementa lf psk1 build sequence. |

## `firmware/application/src/rfid/nfctag/tag_emulation.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 40 | exportada | `is_tag_specific_type_valid` | `bool is_tag_specific_type_valid(tag_specific_type_t tag_type)` | Comprueba tag specific type valid. |
| 84 | static | `loaded_owner` | `static loaded_tag_owner_t *loaded_owner(tag_sense_type_t sense_type)` | Carga ed owner. |
| 90 | static | `loaded_owner_matches` | `static bool loaded_owner_matches(uint8_t slot, tag_specific_type_t type)` | Carga ed owner matches. |
| 118 | exportada | `tag_emulation_is_active_type_loaded` | `bool tag_emulation_is_active_type_loaded(tag_specific_type_t tag_type)` | Implementa tag emulation is active type loaded. |
| 172 | static | `slot_config_types_valid` | `static bool slot_config_types_valid(const tag_slot_config_t *config)` | Implementa slot config types valid. |
| 187 | static | `get_data_loadcb_from_tag_type` | `static tag_datas_loadcb_t get_data_loadcb_from_tag_type(tag_specific_type_t type)` | Obtiene  data loadcb from tag type. |
| 199 | static | `get_data_savecb_from_tag_type` | `static tag_datas_savecb_t get_data_savecb_from_tag_type(tag_specific_type_t type)` | Obtiene  data savecb from tag type. |
| 208 | static | `get_data_save_failcb_from_tag_type` | `static tag_datas_save_failcb_t get_data_save_failcb_from_tag_type(tag_specific_type_t type)` | Obtiene  data save failcb from tag type. |
| 220 | static | `get_data_factory_from_tag_type` | `static tag_datas_factory_t get_data_factory_from_tag_type(tag_specific_type_t type)` | Obtiene  data factory from tag type. |
| 232 | exportada | `get_sense_type_from_tag_type` | `tag_sense_type_t get_sense_type_from_tag_type(tag_specific_type_t type)` | Obtiene  sense type from tag type. |
| 244 | exportada | `get_buffer_by_tag_type` | `tag_data_buffer_t *get_buffer_by_tag_type(tag_specific_type_t type)` | Obtiene  buffer by tag type. |
| 260 | exportada | `tag_emulation_load_by_buffer` | `bool tag_emulation_load_by_buffer(tag_specific_type_t tag_type, bool update_crc)` | Implementa tag emulation load by buffer. |
| 312 | static | `load_data_by_tag_type` | `static void load_data_by_tag_type(uint8_t slot, tag_specific_type_t tag_type)` | Carga  data by tag type. |
| 355 | static | `save_data_by_tag_type` | `static bool save_data_by_tag_type(uint8_t slot, tag_specific_type_t tag_type)` | Guarda  data by tag type. |
| 416 | static | `delete_data_by_tag_type` | `static bool delete_data_by_tag_type(uint8_t slot, tag_sense_type_t sense_type)` | Implementa delete data by tag type. |
| 432 | exportada | `tag_emulation_load_data` | `void tag_emulation_load_data(void)` | Implementa tag emulation load data. |
| 441 | static | `tag_emulation_save_data` | `static bool tag_emulation_save_data(void)` | Implementa tag emulation save data. |
| 454 | exportada | `tag_emulation_get_specific_types_by_slot` | `void tag_emulation_get_specific_types_by_slot(uint8_t slot, tag_slot_specific_type_t *tag_types)` | Implementa tag emulation get specific types by slot. |
| 463 | exportada | `tag_emulation_delete_data` | `bool tag_emulation_delete_data(uint8_t slot, tag_sense_type_t sense_type)` | Implementa tag emulation delete data. |
| 495 | exportada | `tag_emulation_factory_data` | `bool tag_emulation_factory_data(uint8_t slot, tag_specific_type_t tag_type)` | Implementa tag emulation factory data. |
| 522 | static | `tag_emulation_sense_switch_all` | `static void tag_emulation_sense_switch_all(bool enable)` | Implementa tag emulation sense switch all. |
| 544 | exportada | `tag_emulation_sense_switch` | `void tag_emulation_sense_switch(tag_sense_type_t type, bool enable)` | Implementa tag emulation sense switch. |
| 570 | static | `tag_emulation_migrate_slot_config_v0_to_v8` | `static void tag_emulation_migrate_slot_config_v0_to_v8(void)` | Implementa tag emulation migrate slot config v0 to v8. |
| 602 | static | `tag_emulation_migrate_slot_config` | `static void tag_emulation_migrate_slot_config(void)` | Implementa tag emulation migrate slot config. |
| 631 | static | `tag_emulation_load_config` | `static void tag_emulation_load_config(void)` | Implementa tag emulation load config. |
| 665 | static | `tag_emulation_save_config` | `static bool tag_emulation_save_config(void)` | Implementa tag emulation save config. |
| 688 | exportada | `tag_emulation_sense_run` | `void tag_emulation_sense_run(void)` | Implementa tag emulation sense run. |
| 696 | exportada | `tag_emulation_sense_end` | `void tag_emulation_sense_end(void)` | Implementa tag emulation sense end. |
| 705 | exportada | `tag_emulation_init` | `void tag_emulation_init(void)` | Implementa tag emulation init. |
| 713 | exportada | `tag_emulation_save` | `bool tag_emulation_save(void)` | Implementa tag emulation save. |
| 722 | exportada | `tag_emulation_get_slot` | `uint8_t tag_emulation_get_slot(void)` | Implementa tag emulation get slot. |
| 729 | exportada | `tag_emulation_set_slot` | `void tag_emulation_set_slot(uint8_t index)` | Implementa tag emulation set slot. |
| 738 | exportada | `tag_emulation_change_slot` | `bool tag_emulation_change_slot(uint8_t index, bool sense_disable)` | Implementa tag emulation change slot. |
| 761 | exportada | `is_slot_enabled` | `bool is_slot_enabled(uint8_t slot, tag_sense_type_t sense_type)` | Comprueba slot enabled. |
| 775 | exportada | `tag_emulation_slot_set_enable` | `void tag_emulation_slot_set_enable(uint8_t slot, tag_sense_type_t sense_type, bool enable)` | Implementa tag emulation slot set enable. |
| 789 | exportada | `tag_emulation_slot_find_next` | `uint8_t tag_emulation_slot_find_next(uint8_t slot_now)` | Implementa tag emulation slot find next. |
| 805 | exportada | `tag_emulation_slot_find_prev` | `uint8_t tag_emulation_slot_find_prev(uint8_t slot_now)` | Implementa tag emulation slot find prev. |
| 822 | exportada | `tag_emulation_change_type` | `void tag_emulation_change_type(uint8_t slot, tag_specific_type_t tag_type)` | Implementa tag emulation change type. |
| 850 | exportada | `tag_emulation_factory_init` | `void tag_emulation_factory_init(void)` | Implementa tag emulation factory init. |

## `firmware/application/src/rfid/nfctag/tag_persistence.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 11 | static | `get_fds_map_by_slot_auto_inc_id` | `static void get_fds_map_by_slot_auto_inc_id(uint16_t id, uint8_t slot, tag_sense_type_t sense_type, fds_slot_record_map_t *map)` | Obtiene  fds map by slot auto inc id. |
| 22 | exportada | `get_fds_map_by_slot_sense_type_for_dump` | `void get_fds_map_by_slot_sense_type_for_dump(uint8_t slot, tag_sense_type_t sense_type, fds_slot_record_map_t *map)` | Obtiene  fds map by slot sense type for dump. |
| 29 | exportada | `get_fds_map_by_slot_sense_type_for_nick` | `void get_fds_map_by_slot_sense_type_for_nick(uint8_t slot, tag_sense_type_t sense_type, fds_slot_record_map_t *map)` | Obtiene  fds map by slot sense type for nick. |

## `firmware/application/src/rfid/parity.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 18 | static | `oddparity8` | `static inline uint8_t oddparity8(const uint8_t x)` | Implementa oddparity8. |
| 22 | static | `evenparity8` | `static inline uint8_t evenparity8(const uint8_t x)` | Implementa evenparity8. |
| 26 | static | `evenparity32` | `static inline uint8_t evenparity32(uint32_t x)` | Implementa evenparity32. |
| 36 | static | `oddparity32` | `static inline uint8_t oddparity32(uint32_t x)` | Implementa oddparity32. |

## `firmware/application/src/rfid/reader/hf/emv_trace.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 133 | static | `terminal_profile_valid` | `static bool terminal_profile_valid(uint8_t profile)` | Implementa terminal profile valid. |
| 139 | static | `terminal_profile_select` | `static void terminal_profile_select(uint8_t profile)` | Implementa terminal profile select. |
| 159 | static | `polling_profile_values` | `static void polling_profile_values(uint8_t *retries, uint8_t *delay_ms, uint8_t *timeout_ms)` | Implementa polling profile values. |
| 180 | static | `put_u16` | `static void put_u16(uint8_t *out, uint16_t value)` | Implementa put u16. |
| 185 | static | `put_u32` | `static void put_u32(uint8_t *out, uint32_t value)` | Implementa put u32. |
| 192 | static | `get_u16` | `static uint16_t get_u16(const uint8_t *in)` | Obtiene  u16. |
| 196 | static | `get_u32` | `static uint32_t get_u32(const uint8_t *in)` | Obtiene  u32. |
| 201 | static | `elapsed_ms` | `static uint32_t elapsed_ms(void)` | Implementa elapsed ms. |
| 205 | static | `budget_available` | `static bool budget_available(void)` | Implementa budget available. |
| 214 | static | `append_record` | `static bool append_record(uint8_t type, uint8_t stage, uint8_t app, uint8_t attempt, uint8_t record_flags, uint16_t status, const uint8_t *payload, uint16_t payload_len, bool rf_record)` | Implementa append record. |
| 255 | static | `rf_trace_callback` | `static void rf_trace_callback(bool tx, const uint8_t *data, uint16_t bit_length, uint8_t status)` | Implementa rf trace callback. |
| 270 | static | `tlv_next` | `static bool tlv_next(const uint8_t *data, uint16_t length, uint16_t *offset, tlv_item_t *item)` | Implementa tlv next. |
| 302 | static | `tag_equals` | `static bool tag_equals(const tlv_item_t *item, uint32_t tag)` | Implementa tag equals. |
| 308 | static | `find_tlv` | `static bool find_tlv(const uint8_t *data, uint16_t length, uint32_t wanted, const uint8_t **value, uint16_t *value_len, uint8_t depth)` | Implementa find tlv. |
| 325 | static | `aid_exists` | `static bool aid_exists(const emv_aid_t *aids, uint8_t count, const uint8_t *aid, uint8_t aid_len)` | Implementa aid exists. |
| 333 | static | `collect_templates` | `static void collect_templates(const uint8_t *data, uint16_t length, emv_aid_t *aids, uint8_t *count, uint8_t depth)` | Implementa collect templates. |
| 363 | static | `sort_aids` | `static void sort_aids(emv_aid_t *aids, uint8_t count)` | Implementa sort aids. |
| 380 | static | `aid_scheme` | `static emv_scheme_t aid_scheme(const emv_aid_t *aid)` | Implementa aid scheme. |
| 387 | static | `sweep_order` | `static void sweep_order(const emv_aid_t *aid, uint8_t *profiles)` | Implementa sweep order. |
| 421 | static | `activate` | `static bool activate(picc_14a_tag_t *tag, bool poll)` | Implementa activate. |
| 453 | static | `record_apdu` | `static void record_apdu(const uint8_t *command, uint16_t command_len, const uint8_t *response, uint16_t response_len, const iso_dep_result_t *exchange)` | Implementa record apdu. |
| 473 | static | `raw_exchange` | `static bool raw_exchange(const uint8_t *command, uint16_t command_len, uint8_t *response, uint16_t *response_len)` | Implementa raw exchange. |
| 487 | static | `exchange_apdu` | `static bool exchange_apdu(const uint8_t *command, uint16_t command_len, uint8_t *logical, uint16_t *logical_len)` | Implementa exchange apdu. |
| 527 | static | `sw_success` | `static bool sw_success(const uint8_t *response, uint16_t length)` | Implementa sw success. |
| 531 | static | `fill_dol` | `static uint16_t fill_dol(const uint8_t *dol, uint16_t dol_len, uint8_t *out, uint16_t capacity)` | Implementa fill dol. |
| 594 | static | `select_application` | `static bool select_application(const emv_aid_t *aid, uint8_t *response, uint16_t *response_len)` | Implementa select application. |
| 606 | static | `record_application` | `static void record_application(const emv_aid_t *aid)` | Implementa record application. |
| 615 | static | `probe_direct_aids` | `static void probe_direct_aids(emv_aid_t *aids, uint8_t *count)` | Implementa probe direct aids. |
| 639 | static | `get_standard_data` | `static void get_standard_data(void)` | Obtiene  standard data. |
| 651 | static | `read_transaction_log` | `static void read_transaction_log(const uint8_t *fci, uint16_t fci_len)` | Lee  transaction log. |
| 674 | static | `scan_records` | `static uint8_t scan_records(bool grid, uint8_t stage, uint8_t *cdol, uint16_t *cdol_len)` | Implementa scan records. |
| 707 | static | `read_afl` | `static uint8_t read_afl(const uint8_t *gpo, uint16_t gpo_len, uint8_t *cdol, uint16_t *cdol_len)` | Lee  afl. |
| 758 | static | `profile_retry_status` | `static bool profile_retry_status(const uint8_t *response, uint16_t response_len)` | Implementa profile retry status. |
| 765 | static | `process_application` | `static bool process_application(const emv_aid_t *aid, bool maximum, picc_14a_tag_t *tag)` | Procesa  application. |
| 888 | static | `trace_crc32` | `static uint32_t trace_crc32(void)` | Implementa trace crc32. |
| 899 | static | `parse_request` | `static bool parse_request(const uint8_t *data, uint16_t length)` | Interpreta  request. |
| 947 | exportada | `emv_trace_start` | `uint16_t emv_trace_start(const uint8_t *request, uint16_t request_length, uint8_t *response, uint16_t response_capacity, uint16_t *status)` | Implementa emv trace start. |
| 1069 | exportada | `emv_trace_meta` | `uint16_t emv_trace_meta(const uint8_t *request, uint16_t request_length, uint8_t *response, uint16_t response_capacity, uint16_t *status)` | Implementa emv trace meta. |
| 1107 | exportada | `emv_trace_get` | `uint16_t emv_trace_get(const uint8_t *request, uint16_t request_length, uint8_t *response, uint16_t response_capacity, uint16_t *status)` | Implementa emv trace get. |

## `firmware/application/src/rfid/reader/hf/emv_trace_internal.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 8 | static | `emv_trace_same_uid` | `static inline bool emv_trace_same_uid( const uint8_t *expected, uint8_t expected_length, const uint8_t *observed, uint8_t observed_length )` | Implementa emv trace same uid. |

## `firmware/application/src/rfid/reader/hf/iso_dep_reader.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 22 | static | `fwt_ms_from_ats` | `static uint16_t fwt_ms_from_ats(const picc_14a_tag_t *tag)` | Implementa fwt ms from ats. |
| 42 | exportada | `iso_dep_reader_init` | `void iso_dep_reader_init(iso_dep_reader_t *reader, const picc_14a_tag_t *tag)` | Implementa iso dep reader init. |
| 57 | static | `crc_valid` | `static bool crc_valid(const uint8_t *frame, uint16_t length)` | Implementa crc valid. |
| 64 | static | `is_i_block` | `static bool is_i_block(uint8_t pcb)` | Comprueba i block. |
| 68 | static | `is_r_ack` | `static bool is_r_ack(uint8_t pcb)` | Comprueba r ack. |
| 72 | static | `is_r_nak` | `static bool is_r_nak(uint8_t pcb)` | Comprueba r nak. |
| 76 | static | `is_wtx` | `static bool is_wtx(uint8_t pcb)` | Comprueba wtx. |
| 80 | static | `transceive_frame` | `static uint8_t transceive_frame(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa transceive frame. |
| 99 | static | `request_response_retransmission` | `static uint8_t request_response_retransmission(const iso_dep_reader_t *reader, uint8_t block_num, uint8_t *rx, uint16_t *rx_len)` | Implementa request response retransmission. |
| 113 | static | `parse_cid` | `static bool parse_cid(const iso_dep_reader_t *reader, const uint8_t *frame, uint16_t frame_len, uint8_t *offset)` | Interpreta  cid. |
| 124 | static | `iso_dep_reader_transceive_impl` | `static bool iso_dep_reader_transceive_impl(iso_dep_reader_t *reader, const uint8_t *apdu, uint16_t apdu_len, uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result)` | Implementa iso dep reader transceive impl. |
| 340 | exportada | `iso_dep_reader_transceive` | `bool iso_dep_reader_transceive(iso_dep_reader_t *reader, const uint8_t *apdu, uint16_t apdu_len, uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result)` | Implementa iso dep reader transceive. |
| 351 | exportada | `iso_dep_reader_deselect` | `void iso_dep_reader_deselect(iso_dep_reader_t *reader)` | Implementa iso dep reader deselect. |

## `firmware/application/src/rfid/reader/hf/iso_dep_session.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 16 | static | `allocate_session_id` | `static uint32_t allocate_session_id(void)` | Implementa allocate session id. |
| 22 | exportada | `iso_dep_session_is_active` | `bool iso_dep_session_is_active(void)` | Implementa iso dep session is active. |
| 26 | exportada | `iso_dep_session_abort` | `void iso_dep_session_abort(void)` | Implementa iso dep session abort. |
| 34 | static | `iso_dep_session_start_common` | `static uint8_t iso_dep_session_start_common(picc_14a_tag_t *tag, uint32_t *session_id)` | Implementa iso dep session start common. |
| 68 | exportada | `iso_dep_session_start` | `uint8_t iso_dep_session_start(picc_14a_tag_t *tag, uint32_t *session_id)` | Implementa iso dep session start. |
| 73 | exportada | `iso_dep_session_start_apple_transit` | `uint8_t iso_dep_session_start_apple_transit(picc_14a_tag_t *tag, uint32_t *session_id)` | Implementa iso dep session start apple transit. |
| 100 | exportada | `iso_dep_session_exchange` | `iso_dep_session_exchange_status_t iso_dep_session_exchange( uint32_t session_id, const uint8_t *apdu, uint16_t apdu_len, uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result)` | Implementa iso dep session exchange. |
| 118 | exportada | `iso_dep_session_stop` | `bool iso_dep_session_stop(uint32_t session_id)` | Implementa iso dep session stop. |
| 125 | exportada | `iso_dep_session_process` | `void iso_dep_session_process(void)` | Implementa iso dep session process. |

## `firmware/application/src/rfid/reader/hf/mf1_key_access_internal.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 11 | static | `mf1_copy_nonzero_key` | `static inline bool mf1_copy_nonzero_key( uint8_t destination[MF1_KEY_ACCESS_SIZE], const uint8_t source[MF1_KEY_ACCESS_SIZE] )` | Implementa mf1 copy nonzero key. |

## `firmware/application/src/rfid/reader/hf/mf1_toolbox.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 34 | static | `nonce_distance` | `static void nonce_distance(uint32_t *msb, uint32_t *lsb)` | Implementa nonce distance. |
| 68 | static | `check_lfsr_prng` | `static bool check_lfsr_prng(uint32_t nonce)` | Implementa check lfsr prng. |
| 81 | static | `reset_radio_field_with_delay` | `static inline void reset_radio_field_with_delay(void)` | Implementa reset radio field with delay. |
| 98 | static | `send_cmd` | `static uint8_t send_cmd(struct Crypto1State *pcs, uint8_t encrypted, uint8_t cmd, uint8_t data, uint8_t *status, uint8_t *answer, uint8_t *answer_parity, uint16_t answer_max_bit)` | Envia  cmd. |
| 175 | exportada | `authex` | `int authex(struct Crypto1State *pcs, uint32_t uid, uint8_t blockNo, uint8_t keyType, uint64_t ui64Key, uint8_t isNested, uint32_t *ntptr)` | Implementa authex. |
| 263 | static | `darkside_select_nonces` | `static uint8_t darkside_select_nonces(picc_14a_tag_t *tag, uint8_t block, uint8_t keytype, uint32_t *nt, mf1_darkside_status_t *darkside_status)` | Implementa darkside select nonces. |
| 342 | exportada | `darkside_recover_key` | `uint8_t darkside_recover_key(uint8_t targetBlk, uint8_t targetTyp, uint8_t firstRecover, uint8_t ntSyncMax, DarksideCore_t *dc, mf1_darkside_status_t *darkside_status)` | Implementa darkside recover key. |
| 570 | exportada | `antenna_switch_delay` | `void antenna_switch_delay(uint32_t delay_ms)` | Implementa antenna switch delay. |
| 580 | exportada | `check_tag_response_nt` | `uint8_t check_tag_response_nt(picc_14a_tag_t *tag, uint32_t *nt)` | Implementa check tag response nt. |
| 614 | exportada | `check_std_mifare_nt_support` | `uint8_t check_std_mifare_nt_support(void)` | Implementa check std mifare nt support. |
| 632 | exportada | `check_static_prng` | `uint8_t check_static_prng(bool *is_static)` | Implementa check static prng. |
| 668 | exportada | `check_prng_type` | `uint8_t check_prng_type(mf1_prng_type_t *prng_type)` | Implementa check prng type. |
| 726 | static | `measure_nonces` | `static uint32_t measure_nonces(uint32_t from, uint32_t to)` | Implementa measure nonces. |
| 742 | exportada | `measure_median` | `uint32_t measure_median(uint32_t *src, uint32_t length)` | Implementa measure median. |
| 782 | static | `measure_distance` | `static uint8_t measure_distance(uint64_t u64Key, uint8_t block, uint8_t type, uint32_t *distance)` | Implementa measure distance. |
| 838 | static | `nested_recover_core` | `static uint8_t nested_recover_core(mf1_nested_core_t *pnc, uint64_t keyKnown, uint8_t blkKnown, uint8_t typKnown, uint8_t targetBlock, uint8_t targetType)` | Implementa nested recover core. |
| 884 | exportada | `nested_recover_key` | `uint8_t nested_recover_key(uint64_t keyKnown, uint8_t blkKnown, uint8_t typKnown, uint8_t targetBlock, uint8_t targetType, mf1_nested_core_t ncs[SETS_NR])` | Implementa nested recover key. |
| 917 | exportada | `nested_distance_detect` | `uint8_t nested_distance_detect(uint8_t block, uint8_t type, uint8_t *key, uint8_t *uid, uint32_t *distance)` | Implementa nested distance detect. |
| 946 | exportada | `static_nested_recover_core` | `uint8_t static_nested_recover_core(uint8_t *p_nt1, uint8_t *p_nt2, uint64_t keyKnown, uint8_t blkKnown, uint8_t typKnown, uint8_t targetBlock, uint8_t targetType, uint8_t nestedAgain)` | Implementa static nested recover core. |
| 991 | exportada | `static_nested_recover_key` | `uint8_t static_nested_recover_key(uint64_t keyKnown, uint8_t blkKnown, uint8_t typKnown, uint8_t targetBlock, uint8_t targetType, mf1_static_nested_core_t *sncs)` | Implementa static nested recover key. |
| 1014 | exportada | `auth_key_use_522_hw` | `uint16_t auth_key_use_522_hw(uint8_t block, uint8_t type, uint8_t *key)` | Implementa auth key use 522 hw. |
| 1023 | exportada | `mf1_toolbox_antenna_restart` | `inline void mf1_toolbox_antenna_restart()` | Implementa mf1 toolbox antenna restart. |
| 1029 | exportada | `mf1_toolbox_report_healthy` | `inline void mf1_toolbox_report_healthy()` | Implementa mf1 toolbox report healthy. |
| 1034 | exportada | `mf1_toolbox_check_keys_of_sectors` | `uint16_t mf1_toolbox_check_keys_of_sectors( mf1_toolbox_check_keys_of_sectors_in_t *in, mf1_toolbox_check_keys_of_sectors_out_t *out )` | Implementa mf1 toolbox check keys of sectors. |
| 1119 | exportada | `mf1_hardnested_nonces_acquire` | `uint8_t mf1_hardnested_nonces_acquire(bool slow, uint8_t blkKnown, uint8_t typKnown, uint64_t keyKnown, uint8_t targetBlk, uint8_t targetTyp, uint8_t *nonces, uint16_t noncesMax, uint8_t *num_nonces)` | Implementa mf1 hardnested nonces acquire. |
| 1203 | exportada | `mf1_static_encrypted_nonces_acquire` | `uint8_t mf1_static_encrypted_nonces_acquire(uint64_t keyKnown, uint8_t sector_count, uint8_t starting_sector, uint8_t sector_data[40][sizeof(mf1_static_nonce_sector_t)], uint8_t *sectors_acquired, uint32_t *cardUid)` | Implementa mf1 static encrypted nonces acquire. |
| 1293 | exportada | `mf1_toolbox_check_keys_on_block` | `uint16_t mf1_toolbox_check_keys_on_block( mf1_toolbox_check_keys_on_block_in_t *in, mf1_toolbox_check_keys_on_block_out_t *out )` | Implementa mf1 toolbox check keys on block. |

## `firmware/application/src/rfid/reader/hf/rc522.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 46 | exportada | `pcd_14a_reader_trace_set` | `void pcd_14a_reader_trace_set(pcd_14a_trace_cb_t callback)` | Implementa pcd 14a reader trace set. |
| 50 | exportada | `pcd_14a_reader_trace_clear` | `void pcd_14a_reader_trace_clear(void)` | Implementa pcd 14a reader trace clear. |
| 55 | exportada | `pcd_14a_reader_polling_annotation_set` | `bool pcd_14a_reader_polling_annotation_set(const uint8_t *frame, uint8_t length)` | Implementa pcd 14a reader polling annotation set. |
| 62 | exportada | `pcd_14a_reader_polling_annotation_timing_set` | `bool pcd_14a_reader_polling_annotation_timing_set(uint8_t retries, uint8_t delay_ms, uint8_t timeout_ms)` | Implementa pcd 14a reader polling annotation timing set. |
| 73 | exportada | `pcd_14a_reader_polling_annotation_clear` | `void pcd_14a_reader_polling_annotation_clear(void)` | Implementa pcd 14a reader polling annotation clear. |
| 99 | static | `spi_wait_ready` | `static bool spi_wait_ready(void)` | Implementa spi wait ready. |
| 117 | exportada | `read_register_single` | `uint8_t read_register_single(uint8_t Address)` | Lee  register single. |
| 145 | exportada | `read_register_buffer` | `void read_register_buffer(uint8_t Address, uint8_t *pInBuffer, uint8_t len)` | Lee  register buffer. |
| 180 | exportada | `write_register_single` | `void ONCE_OPT write_register_single(uint8_t Address, uint8_t value)` | Escribe  register single. |
| 208 | exportada | `write_register_buffer` | `void write_register_buffer(uint8_t Address, uint8_t *values, uint8_t len)` | Escribe  register buffer. |
| 243 | exportada | `set_register_mask` | `inline void set_register_mask(uint8_t reg, uint8_t mask)` | Configura  register mask. |
| 252 | exportada | `clear_register_mask` | `inline void clear_register_mask(uint8_t reg, uint8_t mask)` | Implementa clear register mask. |
| 260 | exportada | `pcd_14a_reader_init` | `void pcd_14a_reader_init(void)` | Implementa pcd 14a reader init. |
| 290 | exportada | `pcd_14a_reader_reset` | `void pcd_14a_reader_reset(void)` | Implementa pcd 14a reader reset. |
| 319 | exportada | `pcd_14a_reader_uninit` | `void pcd_14a_reader_uninit(void)` | Implementa pcd 14a reader uninit. |
| 333 | exportada | `pcd_14a_reader_timeout_set` | `void pcd_14a_reader_timeout_set(uint16_t timeout_ms)` | Implementa pcd 14a reader timeout set. |
| 342 | exportada | `pcd_14a_reader_timeout_get` | `uint16_t pcd_14a_reader_timeout_get()` | Implementa pcd 14a reader timeout get. |
| 355 | exportada | `pcd_14a_reader_bytes_transfer` | `uint8_t pcd_14a_reader_bytes_transfer(uint8_t Command, uint8_t *pIn, uint8_t InLenByte, uint8_t *pOut, uint16_t *pOutLenBit, uint16_t maxOutLenBit)` | Implementa pcd 14a reader bytes transfer. |
| 511 | exportada | `pcd_14a_reader_bits_transfer` | `uint8_t pcd_14a_reader_bits_transfer(uint8_t *pTx, uint16_t szTxBits, uint8_t *pTxPar, uint8_t *pRx, uint8_t *pRxPar, uint16_t *pRxLenBit, uint16_t szRxLenBitMax)` | Implementa pcd 14a reader bits transfer. |
| 624 | exportada | `pcd_14a_reader_bytes_transfer_flags` | `uint8_t pcd_14a_reader_bytes_transfer_flags(uint8_t Command, uint8_t *pIn, uint8_t InLenByte, uint8_t *pOut, uint16_t *pOutLenBit, uint16_t maxOutLenBit, uint32_t flags)` | Implementa pcd 14a reader bytes transfer flags. |
| 765 | exportada | `pcd_14a_reader_fast_select` | `uint8_t pcd_14a_reader_fast_select(picc_14a_tag_t *tag)` | Implementa pcd 14a reader fast select. |
| 818 | exportada | `pcd_14a_reader_scan_once` | `uint8_t pcd_14a_reader_scan_once(picc_14a_tag_t *tag)` | Implementa pcd 14a reader scan once. |
| 987 | exportada | `pcd_14a_reader_scan_auto` | `uint8_t pcd_14a_reader_scan_auto(picc_14a_tag_t *tag)` | Implementa pcd 14a reader scan auto. |
| 1012 | exportada | `pcd_14a_reader_ats_request` | `uint8_t pcd_14a_reader_ats_request(uint8_t *pAts, uint16_t *szAts, uint16_t szAtsBitMax)` | Implementa pcd 14a reader ats request. |
| 1038 | exportada | `pcd_14a_reader_atqa_request` | `uint8_t pcd_14a_reader_atqa_request(uint8_t *resp, uint8_t *resp_par, uint16_t resp_max_bit)` | Implementa pcd 14a reader atqa request. |
| 1090 | exportada | `pcd_14a_reader_gen1a_unlock` | `uint8_t pcd_14a_reader_gen1a_unlock(void)` | Implementa pcd 14a reader gen1a unlock. |
| 1128 | exportada | `pcd_14a_reader_gen1a_uplock` | `uint8_t pcd_14a_reader_gen1a_uplock(void)` | Implementa pcd 14a reader gen1a uplock. |
| 1169 | exportada | `pcd_14a_reader_mf1_auth` | `uint16_t pcd_14a_reader_mf1_auth(picc_14a_tag_t *tag, uint8_t type, uint8_t addr, uint8_t *pKey)` | Implementa pcd 14a reader mf1 auth. |
| 1191 | exportada | `pcd_14a_reader_mf1_unauth` | `void pcd_14a_reader_mf1_unauth(void)` | Implementa pcd 14a reader mf1 unauth. |
| 1202 | exportada | `pcd_14a_reader_mf1_read_by_cmd` | `uint16_t pcd_14a_reader_mf1_read_by_cmd(uint8_t cmd, uint8_t addr, uint8_t *p)` | Implementa pcd 14a reader mf1 read by cmd. |
| 1239 | exportada | `pcd_14a_reader_mf1_read` | `uint16_t pcd_14a_reader_mf1_read(uint8_t addr, uint8_t *p)` | Implementa pcd 14a reader mf1 read. |
| 1252 | exportada | `pcd_14a_reader_mf1_write_by_cmd` | `uint8_t pcd_14a_reader_mf1_write_by_cmd(uint8_t cmd, uint8_t addr, uint8_t *p)` | Implementa pcd 14a reader mf1 write by cmd. |
| 1304 | exportada | `pcd_14a_reader_mf1_write` | `uint8_t pcd_14a_reader_mf1_write(uint8_t addr, uint8_t *p)` | Implementa pcd 14a reader mf1 write. |
| 1318 | exportada | `pcd_14a_reader_mf1_manipulate_value_block` | `uint8_t pcd_14a_reader_mf1_manipulate_value_block(uint8_t operator, uint8_t addr, int32_t operand)` | Implementa pcd 14a reader mf1 manipulate value block. |
| 1377 | exportada | `pcd_14a_reader_mf1_transfer_value_block` | `uint8_t pcd_14a_reader_mf1_transfer_value_block(uint8_t addr)` | Implementa pcd 14a reader mf1 transfer value block. |
| 1403 | exportada | `pcd_14a_reader_halt_tag` | `uint8_t pcd_14a_reader_halt_tag(void)` | Implementa pcd 14a reader halt tag. |
| 1417 | exportada | `pcd_14a_reader_fast_halt_tag` | `void pcd_14a_reader_fast_halt_tag(void)` | Implementa pcd 14a reader fast halt tag. |
| 1429 | exportada | `pcd_14a_reader_calc_crc` | `void pcd_14a_reader_calc_crc(uint8_t *pbtData, size_t szLen, uint8_t *pbtCrc)` | Implementa pcd 14a reader calc crc. |
| 1464 | exportada | `pcd_14a_reader_antenna_on` | `inline void pcd_14a_reader_antenna_on(void)` | Implementa pcd 14a reader antenna on. |
| 1473 | exportada | `pcd_14a_reader_antenna_off` | `inline void pcd_14a_reader_antenna_off(void)` | Implementa pcd 14a reader antenna off. |
| 1482 | exportada | `pcd_14a_reader_parity_on` | `inline void pcd_14a_reader_parity_on(void)` | Implementa pcd 14a reader parity on. |
| 1489 | exportada | `pcd_14a_reader_parity_off` | `inline void pcd_14a_reader_parity_off(void)` | Implementa pcd 14a reader parity off. |
| 1503 | exportada | `cascade_to_cmd` | `uint8_t cascade_to_cmd(uint8_t cascade)` | Implementa cascade to cmd. |
| 1533 | exportada | `get_4byte_tag_uid` | `uint8_t *get_4byte_tag_uid(picc_14a_tag_t *tag, uint8_t *pUid)` | Obtiene  4byte tag uid. |
| 1564 | exportada | `get_u32_tag_uid` | `uint32_t get_u32_tag_uid(picc_14a_tag_t *tag)` | Obtiene  u32 tag uid. |
| 1575 | exportada | `crc_14a_calculate` | `inline void crc_14a_calculate(uint8_t *pbtData, size_t szLen, uint8_t *pbtCrc)` | Implementa crc 14a calculate. |
| 1596 | exportada | `crc_14a_append` | `inline void crc_14a_append(uint8_t *pbtData, size_t szLen)` | Implementa crc 14a append. |
| 1619 | exportada | `pcd_14a_reader_crc_computer` | `inline void pcd_14a_reader_crc_computer(uint8_t use522CalcCRC)` | Implementa pcd 14a reader crc computer. |
| 1637 | exportada | `pcd_14a_reader_raw_cmd` | `uint8_t pcd_14a_reader_raw_cmd(bool openRFField, bool waitResp, bool appendCrc, bool autoSelect, bool keepField, bool checkCrc, uint16_t waitRespTimeout, uint16_t szDataSendBits, uint8_t *pDataSend, uint8_t *pDataRecv, uint16_t *pszDataRecv, uint16_t szDataRecvBitMax)` | Implementa pcd 14a reader raw cmd. |
| 1766 | exportada | `set_hf14a_config` | `void set_hf14a_config(const hf14a_config_t *hc)` | Configura  hf14a config. |
| 1784 | exportada | `get_hf14a_config` | `hf14a_config_t *get_hf14a_config(void)` | Obtiene  hf14a config. |

## `firmware/application/src/rfid/reader/lf/lf_125khz_radio.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 34 | static | `lf_125khz_gpio_handler` | `static void lf_125khz_gpio_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)` | Implementa lf 125khz gpio handler. |
| 41 | static | `gpiote_init` | `static void gpiote_init(void)` | Implementa gpiote init. |
| 52 | exportada | `start_lf_125khz_radio` | `void start_lf_125khz_radio(void)` | Inicia  lf 125khz radio. |
| 60 | exportada | `stop_lf_125khz_radio` | `void stop_lf_125khz_radio(void)` | Detiene  lf 125khz radio. |
| 65 | static | `pwm_init` | `static void pwm_init(void)` | Implementa pwm init. |
| 82 | static | `pwm_timer_counter_init` | `static void pwm_timer_counter_init(void)` | Implementa pwm timer counter init. |
| 93 | static | `pwm_timer_count_ppi_init` | `static void pwm_timer_count_ppi_init(void)` | Implementa pwm timer count ppi init. |
| 107 | static | `pwm_saadc_sample_ppi_init` | `static void pwm_saadc_sample_ppi_init(void)` | Implementa pwm saadc sample ppi init. |
| 120 | exportada | `lf_125khz_radio_saadc_enable` | `void lf_125khz_radio_saadc_enable(lf_adc_callback_t cb)` | Implementa lf 125khz radio saadc enable. |
| 128 | exportada | `lf_125khz_radio_saadc_disable` | `void lf_125khz_radio_saadc_disable(void)` | Implementa lf 125khz radio saadc disable. |
| 136 | exportada | `lf_125khz_radio_gpiote_enable` | `void lf_125khz_radio_gpiote_enable(void)` | Implementa lf 125khz radio gpiote enable. |
| 146 | exportada | `lf_125khz_radio_gpiote_disable` | `void lf_125khz_radio_gpiote_disable(void)` | Implementa lf 125khz radio gpiote disable. |
| 157 | exportada | `lf_125khz_radio_init` | `void lf_125khz_radio_init(void)` | Implementa lf 125khz radio init. |
| 168 | exportada | `lf_125khz_radio_uninit` | `void lf_125khz_radio_uninit(void)` | Implementa lf 125khz radio uninit. |

## `firmware/application/src/rfid/reader/lf/lf_em410x_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 25 | exportada | `gpio_int0_cb` | `void gpio_int0_cb(void)` | Implementa gpio int0 cb. |
| 37 | static | `init_em410x_hw` | `static void init_em410x_hw(void)` | Inicializa  em410x hw. |
| 42 | static | `uninit_em410x_hw` | `static void uninit_em410x_hw(void)` | Libera  em410x hw. |
| 47 | exportada | `em410x_read` | `bool em410x_read(uint8_t *data, uint32_t timeout_ms)` | Implementa em410x read. |

## `firmware/application/src/rfid/reader/lf/lf_em4x05_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 10 | exportada | `em4x05_read` | `bool em4x05_read(em4x05_data_t *out, uint32_t timeout_ms)` | Implementa em4x05 read. |
| 18 | exportada | `scan_em4x05` | `uint8_t scan_em4x05(em4x05_data_t *out)` | Implementa scan em4x05. |

## `firmware/application/src/rfid/reader/lf/lf_gap.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 29 | static | `field_off` | `static inline void field_off(void)` | Implementa field off. |
| 35 | static | `field_on` | `static inline void field_on(void)` | Implementa field on. |
| 40 | exportada | `lf_gap_send_start` | `void lf_gap_send_start(void)` | Implementa lf gap send start. |
| 46 | exportada | `lf_gap_send_bit` | `void lf_gap_send_bit(uint8_t bit)` | Implementa lf gap send bit. |
| 57 | exportada | `lf_gap_send_u32` | `void lf_gap_send_u32(uint32_t word)` | Implementa lf gap send u32. |
| 61 | exportada | `lf_gap_send_bits` | `void lf_gap_send_bits(uint32_t value, uint8_t nbits)` | Implementa lf gap send bits. |
| 67 | exportada | `lf_gap_detect` | `bool lf_gap_detect(uint32_t last_count, uint32_t *gap_tc)` | Implementa lf gap detect. |

## `firmware/application/src/rfid/reader/lf/lf_hidprox_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 25 | static | `saadc_cb` | `static void saadc_cb(nrf_saadc_value_t *vals, size_t size)` | Implementa saadc cb. |
| 37 | static | `init_hidprox_hw` | `static void init_hidprox_hw(void)` | Inicializa  hidprox hw. |
| 41 | static | `uninit_hidprox_hw` | `static void uninit_hidprox_hw(void)` | Libera  hidprox hw. |
| 45 | exportada | `hidprox_read` | `bool hidprox_read(uint8_t *data, uint8_t format_hint, uint32_t timeout_ms)` | Implementa hidprox read. |

## `firmware/application/src/rfid/reader/lf/lf_ioprox_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 29 | static | `saadc_cb` | `static void saadc_cb(nrf_saadc_value_t *vals, size_t size)` | Implementa saadc cb. |
| 42 | static | `init_ioprox_hw` | `static void init_ioprox_hw(void)` | Inicializa  ioprox hw. |
| 68 | static | `uninit_ioprox_hw` | `static void uninit_ioprox_hw(void)` | Libera  ioprox hw. |
| 82 | exportada | `ioprox_read` | `bool ioprox_read(uint8_t *data, uint8_t format_hint, uint32_t timeout_ms)` | Implementa ioprox read. |

## `firmware/application/src/rfid/reader/lf/lf_jablotron_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 21 | static | `jablotron_gpio_int0_cb` | `static void jablotron_gpio_int0_cb(void)` | Implementa jablotron gpio int0 cb. |
| 33 | static | `init_jablotron_hw` | `static void init_jablotron_hw(void)` | Inicializa  jablotron hw. |
| 38 | static | `uninit_jablotron_hw` | `static void uninit_jablotron_hw(void)` | Libera  jablotron hw. |
| 43 | exportada | `jablotron_read` | `bool jablotron_read(uint8_t *data, uint32_t timeout_ms)` | Implementa jablotron read. |

## `firmware/application/src/rfid/reader/lf/lf_pac_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 25 | static | `pac_saadc_cb` | `static void pac_saadc_cb(nrf_saadc_value_t *vals, size_t size)` | Implementa pac saadc cb. |
| 37 | static | `init_pac_hw` | `static void init_pac_hw(void)` | Inicializa  pac hw. |
| 41 | static | `uninit_pac_hw` | `static void uninit_pac_hw(void)` | Libera  pac hw. |
| 45 | exportada | `pac_read` | `bool pac_read(uint8_t *data, uint32_t timeout_ms)` | Implementa pac read. |

## `firmware/application/src/rfid/reader/lf/lf_reader_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 9 | exportada | `register_rio_callback` | `void register_rio_callback(RIO_CALLBACK_S P)` | Implementa register rio callback. |
| 13 | exportada | `unregister_rio_callback` | `void unregister_rio_callback(void)` | Implementa unregister rio callback. |
| 18 | exportada | `gpio_int0_irq_handler` | `void gpio_int0_irq_handler(void)` | Implementa gpio int0 irq handler. |
| 25 | exportada | `get_lf_counter_value` | `uint32_t get_lf_counter_value(void)` | Obtiene  lf counter value. |
| 30 | exportada | `clear_lf_counter_value` | `void clear_lf_counter_value(void)` | Implementa clear lf counter value. |

## `firmware/application/src/rfid/reader/lf/lf_reader_generic.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 27 | static | `saadc_cb` | `static void saadc_cb(nrf_saadc_value_t *vals, size_t size)` | Implementa saadc cb. |
| 36 | static | `init_saadc_hw` | `static void init_saadc_hw(void)` | Inicializa  saadc hw. |
| 40 | static | `uninit_saadc_hw` | `static void uninit_saadc_hw(void)` | Libera  saadc hw. |
| 44 | exportada | `raw_read_to_buffer` | `bool raw_read_to_buffer(uint8_t *data, size_t maxlen, uint32_t timeout_ms, size_t *outlen)` | Implementa raw read to buffer. |

## `firmware/application/src/rfid/reader/lf/lf_reader_main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 30 | exportada | `scan_em410x` | `uint8_t scan_em410x(uint8_t *uid)` | Implementa scan em410x. |
| 40 | exportada | `scan_hidprox` | `uint8_t scan_hidprox(uint8_t *data, uint8_t format_hint)` | Implementa scan hidprox. |
| 52 | exportada | `scan_ioprox` | `uint8_t scan_ioprox(uint8_t *data, uint8_t format_hint)` | Implementa scan ioprox. |
| 65 | exportada | `decode_ioprox_raw` | `uint8_t decode_ioprox_raw(uint8_t *raw8, uint8_t *output)` | Implementa decode ioprox raw. |
| 80 | exportada | `encode_ioprox_params` | `uint8_t encode_ioprox_params(uint8_t ver, uint8_t fc, uint16_t cn, uint8_t *out)` | Implementa encode ioprox params. |
| 90 | exportada | `scan_pac` | `uint8_t scan_pac(uint8_t *card_id)` | Implementa scan pac. |
| 100 | exportada | `scan_viking` | `uint8_t scan_viking(uint8_t *uid)` | Implementa scan viking. |
| 110 | exportada | `scan_jablotron` | `uint8_t scan_jablotron(uint8_t *uid)` | Implementa scan jablotron. |
| 120 | static | `try_reset_t55xx_passwd` | `static ret_code_t try_reset_t55xx_passwd(uint32_t new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Implementa try reset t55xx passwd. |
| 132 | static | `write_t55xx` | `static uint8_t write_t55xx(uint32_t *blks, uint8_t blk_count, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  t55xx. |
| 152 | exportada | `write_em410x_to_t55xx` | `uint8_t write_em410x_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  em410x to t55xx. |
| 161 | exportada | `write_em410x_electra_to_t55xx` | `uint8_t write_em410x_electra_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  em410x electra to t55xx. |
| 173 | exportada | `write_hidprox_to_t55xx` | `uint8_t write_hidprox_to_t55xx(uint8_t format, uint32_t fc, uint64_t cn, uint32_t il, uint32_t oem, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  hidprox to t55xx. |
| 192 | exportada | `write_ioprox_to_t55xx` | `uint8_t write_ioprox_to_t55xx(uint8_t *card_data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  ioprox to t55xx. |
| 208 | exportada | `write_viking_to_t55xx` | `uint8_t write_viking_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  viking to t55xx. |
| 217 | exportada | `write_pac_to_t55xx` | `uint8_t write_pac_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  pac to t55xx. |
| 227 | exportada | `write_jablotron_to_t55xx` | `uint8_t write_jablotron_to_t55xx(uint8_t *uid, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  jablotron to t55xx. |
| 239 | exportada | `write_idteck_to_t55xx` | `uint8_t write_idteck_to_t55xx(uint8_t *data, uint8_t *new_passwd, uint8_t *old_passwds, uint8_t old_passwd_count)` | Escribe  idteck to t55xx. |
| 249 | exportada | `set_scan_tag_timeout` | `void set_scan_tag_timeout(uint32_t ms)` | Configura  scan tag timeout. |
| 268 | exportada | `lf_t55xx_write_block` | `uint8_t lf_t55xx_write_block(uint8_t block, uint32_t word, uint32_t passwd, bool use_passwd, bool page1)` | Implementa lf t55xx write block. |

## `firmware/application/src/rfid/reader/lf/lf_t55xx_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 27 | exportada | `t55xx_send_gap` | `void t55xx_send_gap(uint32_t nus)` | Implementa t55xx send gap. |
| 33 | exportada | `t55xx_tx_bit` | `void t55xx_tx_bit(uint8_t data)` | Implementa t55xx tx bit. |
| 42 | exportada | `t55xx_tx_uint32_t` | `void t55xx_tx_uint32_t(uint32_t data)` | Implementa t55xx tx uint32 t. |
| 49 | exportada | `t55xx_timeslot_callback` | `void t55xx_timeslot_callback()` | Implementa t55xx timeslot callback. |
| 92 | exportada | `t55xx_send_cmd` | `ret_code_t t55xx_send_cmd(uint8_t opcode, uint32_t *passwd, uint8_t lock_bit, uint32_t *data, uint8_t blk_addr)` | Implementa t55xx send cmd. |
| 131 | exportada | `t55xx_write_data` | `ret_code_t t55xx_write_data(uint32_t passwd, uint32_t *blks, uint8_t blk_count)` | Implementa t55xx write data. |
| 148 | exportada | `t55xx_reset_passwd` | `ret_code_t t55xx_reset_passwd(uint32_t old_passwd, uint32_t new_passwd)` | Implementa t55xx reset passwd. |

## `firmware/application/src/rfid/reader/lf/lf_viking_data.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 22 | static | `viking_gpio_int0_cb` | `static void viking_gpio_int0_cb(void)` | Implementa viking gpio int0 cb. |
| 34 | static | `init_viking_hw` | `static void init_viking_hw(void)` | Inicializa  viking hw. |
| 39 | static | `uninit_viking_hw` | `static void uninit_viking_hw(void)` | Libera  viking hw. |
| 44 | exportada | `viking_read` | `bool viking_read(uint8_t *data, uint32_t timeout_ms)` | Implementa viking read. |

## `firmware/application/src/rfid_main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 17 | exportada | `reader_mode_enter` | `void reader_mode_enter(void)` | Lee er mode enter. |
| 43 | exportada | `tag_mode_enter` | `void tag_mode_enter(void)` | Implementa tag mode enter. |
| 84 | exportada | `light_up_by_slot` | `void light_up_by_slot(void)` | Implementa light up by slot. |
| 100 | exportada | `apply_slot_change` | `void apply_slot_change(uint8_t slot_now, uint8_t slot_new)` | Implementa apply slot change. |
| 109 | exportada | `get_device_mode` | `device_mode_t get_device_mode(void)` | Obtiene  device mode. |
| 119 | exportada | `get_color_by_slot` | `uint8_t get_color_by_slot(uint8_t slot)` | Obtiene  color by slot. |

## `firmware/application/src/rgb_marquee.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 45 | exportada | `rgb_marquee_init` | `void rgb_marquee_init(void)` | Implementa rgb marquee init. |
| 49 | exportada | `rgb_marquee_stop` | `void rgb_marquee_stop(void)` | Implementa rgb marquee stop. |
| 57 | exportada | `rgb_marquee_reset` | `void rgb_marquee_reset(void)` | Implementa rgb marquee reset. |
| 63 | exportada | `get_pwmduty` | `uint16_t get_pwmduty(uint8_t light_level)` | Obtiene  pwmduty. |
| 69 | exportada | `rgb_marquee_usb_open_sweep` | `void rgb_marquee_usb_open_sweep(uint8_t color, uint8_t dir)` | Implementa rgb marquee usb open sweep. |
| 126 | exportada | `rgb_marquee_usb_open_symmetric` | `void rgb_marquee_usb_open_symmetric(uint8_t color)` | Implementa rgb marquee usb open symmetric. |
| 178 | exportada | `rgb_marquee_sweep_to` | `void rgb_marquee_sweep_to(uint8_t color, uint8_t dir, uint8_t end)` | Implementa rgb marquee sweep to. |
| 259 | static | `rgb_marquee_slot_switch_pwm_callback` | `static void rgb_marquee_slot_switch_pwm_callback(nrfx_pwm_evt_type_t event_type)` | Implementa rgb marquee slot switch pwm callback. |
| 264 | exportada | `rgb_marquee_slot_switch` | `void rgb_marquee_slot_switch(uint8_t led_down, uint8_t color_led_down, uint8_t led_up, uint8_t color_led_up)` | Implementa rgb marquee slot switch. |
| 328 | exportada | `rgb_marquee_sweep_fade` | `void rgb_marquee_sweep_fade(uint8_t color, uint8_t dir, uint8_t end, uint8_t start_light, uint8_t stop_light)` | Implementa rgb marquee sweep fade. |
| 402 | exportada | `rgb_marquee_sweep_from_to` | `void rgb_marquee_sweep_from_to(uint8_t color, uint8_t start, uint8_t stop)` | Implementa rgb marquee sweep from to. |
| 431 | exportada | `rgb_marquee_usb_idle_pwm_callback` | `void rgb_marquee_usb_idle_pwm_callback(nrfx_pwm_evt_type_t event_type)` | Implementa rgb marquee usb idle pwm callback. |
| 436 | exportada | `rgb_marquee_usb_idle` | `void rgb_marquee_usb_idle(void)` | Implementa rgb marquee usb idle. |
| 542 | exportada | `rgb_marquee_symmetric_out` | `void rgb_marquee_symmetric_out(uint8_t color, uint8_t slot)` | Implementa rgb marquee symmetric out. |
| 602 | exportada | `rgb_marquee_symmetric_in` | `void rgb_marquee_symmetric_in(uint8_t color, uint8_t slot)` | Implementa rgb marquee symmetric in. |
| 668 | exportada | `rgb_marquee_is_enabled` | `bool rgb_marquee_is_enabled(void)` | Implementa rgb marquee is enabled. |
| 680 | exportada | `rgb_marquee_set_reader_keys_anim` | `void rgb_marquee_set_reader_keys_anim(bool enable)` | Implementa rgb marquee set reader keys anim. |
| 692 | exportada | `rgb_marquee_is_reader_keys_anim` | `bool rgb_marquee_is_reader_keys_anim(void)` | Implementa rgb marquee is reader keys anim. |
| 705 | exportada | `rgb_marquee_reader_keys_loop` | `void rgb_marquee_reader_keys_loop(void)` | Implementa rgb marquee reader keys loop. |
| 747 | exportada | `rgb_marquee_set_ble_test_anim` | `void rgb_marquee_set_ble_test_anim(bool enable)` | Implementa rgb marquee set ble test anim. |
| 758 | exportada | `rgb_marquee_is_ble_test_anim` | `bool rgb_marquee_is_ble_test_anim(void)` | Implementa rgb marquee is ble test anim. |
| 770 | exportada | `rgb_marquee_ble_test_loop` | `void rgb_marquee_ble_test_loop(void)` | Implementa rgb marquee ble test loop. |
| 811 | exportada | `rgb_marquee_set_ble_active_anim` | `void rgb_marquee_set_ble_active_anim(bool enable)` | Implementa rgb marquee set ble active anim. |
| 827 | exportada | `rgb_marquee_is_ble_active_anim` | `bool rgb_marquee_is_ble_active_anim(void)` | Implementa rgb marquee is ble active anim. |
| 831 | exportada | `rgb_marquee_ble_active_loop` | `void rgb_marquee_ble_active_loop(void)` | Implementa rgb marquee ble active loop. |

## `firmware/application/src/settings.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 21 | static | `update_config_crc` | `static void update_config_crc(void)` | Implementa update config crc. |
| 26 | static | `config_did_change` | `static bool config_did_change(void)` | Implementa config did change. |
| 32 | static | `config_is_valid` | `static bool config_is_valid(uint16_t length)` | Implementa config is valid. |
| 65 | exportada | `settings_update_version_for_config` | `void settings_update_version_for_config(void)` | Configura tings update version for config. |
| 70 | exportada | `settings_init_button_press_config` | `void settings_init_button_press_config(void)` | Configura tings init button press config. |
| 76 | exportada | `settings_init_button_long_press_config` | `void settings_init_button_long_press_config(void)` | Configura tings init button long press config. |
| 82 | exportada | `settings_init_ble_connect_key_config` | `void settings_init_ble_connect_key_config(void)` | Configura tings init ble connect key config. |
| 88 | exportada | `settings_init_ble_pairing_enable_config` | `void settings_init_ble_pairing_enable_config(void)` | Configura tings init ble pairing enable config. |
| 93 | exportada | `settings_init_sleep_timeout_config` | `void settings_init_sleep_timeout_config(void)` | Configura tings init sleep timeout config. |
| 97 | exportada | `settings_init_config` | `void settings_init_config(void)` | Configura tings init config. |
| 108 | exportada | `settings_migrate` | `void settings_migrate(void)` | Configura tings migrate. |
| 143 | exportada | `settings_load_config` | `void settings_load_config(void)` | Configura tings load config. |
| 171 | exportada | `settings_save_config` | `uint8_t settings_save_config(void)` | Configura tings save config. |
| 194 | exportada | `settings_get_animation_config` | `uint8_t settings_get_animation_config()` | Configura tings get animation config. |
| 198 | exportada | `settings_set_animation_config` | `void settings_set_animation_config(uint8_t value)` | Configura tings set animation config. |
| 209 | exportada | `is_settings_button_type_valid` | `bool is_settings_button_type_valid(char type)` | Comprueba settings button type valid. |
| 227 | exportada | `settings_get_button_press_config` | `uint8_t settings_get_button_press_config(char which)` | Configura tings get button press config. |
| 252 | exportada | `settings_get_long_button_press_config` | `uint8_t settings_get_long_button_press_config(char which)` | Configura tings get long button press config. |
| 277 | exportada | `settings_set_button_press_config` | `void settings_set_button_press_config(char which, uint8_t value)` | Configura tings set button press config. |
| 302 | exportada | `settings_set_long_button_press_config` | `void settings_set_long_button_press_config(char which, uint8_t value)` | Configura tings set long button press config. |
| 321 | exportada | `settings_get_ble_connect_key` | `uint8_t *settings_get_ble_connect_key(void)` | Configura tings get ble connect key. |
| 330 | exportada | `settings_set_ble_connect_key` | `void settings_set_ble_connect_key(uint8_t *key)` | Configura tings set ble connect key. |
| 334 | exportada | `settings_set_ble_pairing_enable` | `void settings_set_ble_pairing_enable(bool enable)` | Configura tings set ble pairing enable. |
| 338 | exportada | `settings_get_ble_pairing_enable` | `bool settings_get_ble_pairing_enable(void)` | Configura tings get ble pairing enable. |
| 342 | exportada | `settings_get_ble_pairing_enable_first_load` | `bool settings_get_ble_pairing_enable_first_load(void)` | Configura tings get ble pairing enable first load. |
| 346 | exportada | `settings_get_sleep_timeout` | `uint32_t settings_get_sleep_timeout(void)` | Configura tings get sleep timeout. |
| 350 | exportada | `settings_set_sleep_timeout` | `void settings_set_sleep_timeout(uint8_t seconds)` | Configura tings set sleep timeout. |

## `firmware/application/src/usb_main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 61 | static | `usb_response_ready` | `static bool usb_response_ready(void)` | Implementa usb response ready. |
| 65 | static | `usb_tx_clear` | `static void usb_tx_clear(void)` | Implementa usb tx clear. |
| 71 | static | `usb_tx_start` | `static void usb_tx_start(void)` | Implementa usb tx start. |
| 86 | static | `usb_rx_arm` | `static void usb_rx_arm(void)` | Implementa usb rx arm. |
| 113 | static | `usb_rx_resume` | `static void usb_rx_resume(void)` | Implementa usb rx resume. |
| 129 | static | `cdc_acm_user_ev_handler` | `static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst, app_usbd_cdc_acm_user_event_t event)` | Implementa cdc acm user ev handler. |
| 176 | static | `usbd_user_ev_handler` | `static void usbd_user_ev_handler(app_usbd_event_type_t event)` | Implementa usbd user ev handler. |
| 247 | exportada | `usb_cdc_init` | `void usb_cdc_init(void)` | Implementa usb cdc init. |
| 269 | exportada | `usb_cdc_write_try` | `uint32_t usb_cdc_write_try(const void *p_buf, uint16_t length)` | Implementa usb cdc write try. |
| 288 | exportada | `usb_cdc_write` | `void usb_cdc_write(const void *p_buf, uint16_t length)` | Implementa usb cdc write. |
| 319 | exportada | `is_usb_working` | `bool is_usb_working(void)` | Comprueba usb working. |
| 323 | exportada | `is_usb_tx_idle` | `bool is_usb_tx_idle(void)` | Comprueba usb tx idle. |

## `firmware/application/src/utils/dataframe.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 50 | static | `compute_lrc` | `static uint8_t compute_lrc(const uint8_t *buf, uint16_t bufsize)` | Implementa compute lrc. |
| 58 | static | `decoder_for_transport` | `static data_frame_decoder_t *decoder_for_transport(data_frame_transport_t transport)` | Implementa decoder for transport. |
| 65 | static | `reserve_request` | `static data_frame_request_t *reserve_request(data_frame_transport_t transport, uint32_t generation)` | Implementa reserve request. |
| 83 | static | `queue_decoder` | `static bool queue_decoder(data_frame_transport_t transport, data_frame_decoder_t *decoder)` | Implementa queue decoder. |
| 112 | static | `decoder_prefix_valid` | `static bool decoder_prefix_valid(data_frame_decoder_t *decoder, bool *frame_complete)` | Implementa decoder prefix valid. |
| 151 | static | `decoder_resynchronize` | `static void decoder_resynchronize(data_frame_decoder_t *decoder)` | Implementa decoder resynchronize. |
| 176 | exportada | `data_frame_receive_from` | `uint16_t data_frame_receive_from(const uint8_t *data, uint16_t length, data_frame_transport_t transport)` | Implementa data frame receive from. |
| 212 | exportada | `data_frame_receive` | `void data_frame_receive(uint8_t *data, uint16_t length)` | Implementa data frame receive. |
| 219 | exportada | `data_frame_reset_transport` | `void data_frame_reset_transport(data_frame_transport_t transport)` | Implementa data frame reset transport. |
| 238 | exportada | `data_frame_set_flow_callback` | `void data_frame_set_flow_callback(data_frame_transport_t transport, void (*callback)(void))` | Implementa data frame set flow callback. |
| 244 | exportada | `data_frame_set_ready_callback` | `void data_frame_set_ready_callback(data_frame_transport_t transport, bool (*callback)(void))` | Implementa data frame set ready callback. |
| 250 | exportada | `data_frame_get_transport` | `data_frame_transport_t data_frame_get_transport(void)` | Implementa data frame get transport. |
| 254 | static | `oldest_request` | `static data_frame_request_t *oldest_request(void)` | Implementa oldest request. |
| 270 | exportada | `data_frame_process` | `void data_frame_process(void)` | Implementa data frame process. |
| 307 | exportada | `on_data_frame_complete` | `void on_data_frame_complete(data_frame_cbk_t callback)` | Atiende data frame complete. |
| 311 | exportada | `data_frame_make` | `data_frame_tx_t *data_frame_make(uint16_t cmd, uint16_t status, uint16_t data_length, uint8_t *data)` | Implementa data frame make. |

## `firmware/application/src/utils/delayed_reset.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 10 | static | `delayed_reset_event_handler` | `static void delayed_reset_event_handler(void *ctx)` | Implementa delayed reset event handler. |
| 19 | exportada | `delayed_reset` | `void delayed_reset(uint32_t delay)` | Implementa delayed reset. |

## `firmware/application/src/utils/fds_util.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 52 | static | `fds_set_last_error` | `static void fds_set_last_error(ret_code_t result)` | Implementa fds set last error. |
| 58 | exportada | `fds_util_last_error` | `ret_code_t fds_util_last_error(void)` | Implementa fds util last error. |
| 66 | exportada | `fds_util_is_ready` | `bool fds_util_is_ready(void)` | Implementa fds util is ready. |
| 74 | static | `fds_lock` | `static bool fds_lock(bool require_initialized)` | Implementa fds lock. |
| 98 | static | `fds_finish` | `static void fds_finish(ret_code_t result)` | Implementa fds finish. |
| 113 | static | `fds_prepare_wait` | `static void fds_prepare_wait(fds_sync_op_t operation, uint16_t file_id, uint16_t record_key, uint32_t record_id)` | Implementa fds prepare wait. |
| 127 | static | `fds_cancel_wait` | `static void fds_cancel_wait(void)` | Implementa fds cancel wait. |
| 134 | static | `fds_wait` | `static ret_code_t fds_wait(void)` | Implementa fds wait. |
| 168 | static | `fds_find_record` | `static ret_code_t fds_find_record(uint16_t id, uint16_t key, fds_record_desc_t *desc)` | Implementa fds find record. |
| 173 | static | `fds_gc_locked` | `static ret_code_t fds_gc_locked(void)` | Implementa fds gc locked. |
| 183 | static | `fds_write_record_locked` | `static ret_code_t fds_write_record_locked(uint16_t id, uint16_t key, uint16_t data_length_words)` | Implementa fds write record locked. |
| 211 | static | `fds_write_with_gc_locked` | `static ret_code_t fds_write_with_gc_locked(uint16_t id, uint16_t key, uint16_t data_length_words)` | Implementa fds write with gc locked. |
| 226 | exportada | `fds_is_exists` | `bool fds_is_exists(uint16_t id, uint16_t key)` | Implementa fds is exists. |
| 238 | exportada | `fds_read_sync` | `bool fds_read_sync(uint16_t id, uint16_t key, uint16_t *length, uint8_t *buffer)` | Implementa fds read sync. |
| 304 | exportada | `fds_write_sync` | `bool fds_write_sync(uint16_t id, uint16_t key, uint16_t length, void *buffer)` | Implementa fds write sync. |
| 331 | static | `fds_delete_record_locked` | `static ret_code_t fds_delete_record_locked(fds_record_desc_t *record_desc)` | Implementa fds delete record locked. |
| 346 | exportada | `fds_delete_sync` | `int fds_delete_sync(uint16_t id, uint16_t key)` | Implementa fds delete sync. |
| 373 | static | `is_peer_manager_record` | `static bool is_peer_manager_record(uint16_t id_or_key)` | Comprueba peer manager record. |
| 377 | static | `event_matches_locked` | `static bool event_matches_locked(fds_evt_t const *event)` | Implementa event matches locked. |
| 395 | static | `event_is_peer_manager_record` | `static bool event_is_peer_manager_record(fds_evt_t const *event)` | Implementa event is peer manager record. |
| 407 | static | `fds_evt_handler` | `static void fds_evt_handler(fds_evt_t const *event)` | Implementa fds evt handler. |
| 435 | exportada | `fds_util_init` | `void fds_util_init(void)` | Implementa fds util init. |
| 466 | exportada | `fds_gc_sync` | `bool fds_gc_sync(void)` | Implementa fds gc sync. |
| 475 | exportada | `fds_wipe` | `bool fds_wipe(void)` | Implementa fds wipe. |

## `firmware/application/src/utils/syssleep.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 22 | static | `timer_sleep_event_handle` | `static void timer_sleep_event_handle(void *arg)` | Implementa timer sleep event handle. |
| 29 | exportada | `sleep_timer_init` | `void sleep_timer_init(void)` | Implementa sleep timer init. |
| 39 | exportada | `sleep_timer_stop` | `void sleep_timer_stop()` | Implementa sleep timer stop. |
| 47 | exportada | `sleep_timer_start` | `void sleep_timer_start(uint32_t time_ms)` | Implementa sleep timer start. |
| 65 | exportada | `sleep_system_run` | `void sleep_system_run(void (*sysOffSleep)(), void (*sysOnSleep)())` | Implementa sleep system run. |

## `firmware/application/src/utils/timeslot.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 35 | static | `deadline_expired` | `static bool deadline_expired(uint32_t started, uint32_t timeout_ms)` | Implementa deadline expired. |
| 39 | static | `timeslot_soc_evt_handler` | `static void timeslot_soc_evt_handler(uint32_t evt_id, void *p_context)` | Implementa timeslot soc evt handler. |
| 64 | static | `radio_callback` | `static nrf_radio_signal_callback_return_param_t *radio_callback(uint8_t signal_type)` | Implementa radio callback. |
| 90 | exportada | `request_timeslot_timeout` | `ret_code_t request_timeslot_timeout(uint32_t time_us, timeslot_callback_t callback, uint32_t timeout_ms)` | Implementa request timeslot timeout. |
| 161 | exportada | `request_timeslot` | `ret_code_t request_timeslot(uint32_t time_us, timeslot_callback_t callback)` | Implementa request timeslot. |
| 165 | exportada | `timeslot_cancel` | `ret_code_t timeslot_cancel(void)` | Implementa timeslot cancel. |
| 175 | exportada | `timeslot_start` | `ret_code_t timeslot_start(uint32_t time_ms)` | Implementa timeslot start. |
| 180 | exportada | `timeslot_stop` | `ret_code_t timeslot_stop(void)` | Implementa timeslot stop. |

## `firmware/bootloader/src/main.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 68 | static | `on_error` | `static void on_error(void)` | Atiende error. |
| 82 | exportada | `app_error_handler` | `void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t *p_file_name)` | Implementa app error handler. |
| 88 | exportada | `app_error_fault_handler` | `void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)` | Implementa app error fault handler. |
| 94 | exportada | `app_error_handler_bare` | `void app_error_handler_bare(uint32_t error_code)` | Implementa app error handler bare. |
| 102 | exportada | `flash_led` | `void flash_led(void *p_event_data, uint16_t event_size)` | Implementa flash led. |
| 144 | static | `dfu_observer` | `static void dfu_observer(nrf_dfu_evt_type_t evt_type)` | Implementa dfu observer. |
| 176 | exportada | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/common/hw_connect.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 76 | exportada | `board_lite_high_voltage_set` | `void board_lite_high_voltage_set(void)` | Implementa board lite high voltage set. |
| 97 | exportada | `hw_connect_init` | `void hw_connect_init(void)` | Implementa hw connect init. |
| 195 | exportada | `hw_get_led_array` | `uint32_t *hw_get_led_array(void)` | Implementa hw get led array. |
| 199 | exportada | `hw_get_led_reversal_array` | `uint32_t *hw_get_led_reversal_array(void)` | Implementa hw get led reversal array. |
| 203 | exportada | `hw_get_rgb_array` | `uint32_t *hw_get_rgb_array(void)` | Implementa hw get rgb array. |
| 207 | exportada | `hw_get_device_type` | `chameleon_device_type_t hw_get_device_type(void)` | Implementa hw get device type. |
| 211 | exportada | `hw_get_version_code` | `uint8_t hw_get_version_code(void)` | Implementa hw get version code. |
| 216 | exportada | `init_leds` | `void init_leds(void)` | Inicializa  leds. |
| 241 | exportada | `set_slot_light_color` | `void set_slot_light_color(chameleon_rgb_type_t color)` | Configura  slot light color. |

## `firmware/common/libc_nano_stubs.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 14 | exportada | `_close` | `int _close(int file)` | Implementa close. |
| 19 | exportada | `_fstat` | `int _fstat(int file, struct stat *st)` | Implementa fstat. |
| 24 | exportada | `_getpid` | `int _getpid(void)` | Implementa getpid. |
| 28 | exportada | `_isatty` | `int _isatty(int file)` | Implementa isatty. |
| 33 | exportada | `_kill` | `int _kill(int pid, int sig)` | Implementa kill. |
| 38 | exportada | `_lseek` | `int _lseek(int file, int ptr, int dir)` | Implementa lseek. |
| 43 | exportada | `_read` | `int _read(int file, char *ptr, int len)` | Implementa read. |
| 48 | exportada | `_write` | `int _write(int file, char *ptr, int len)` | Implementa write. |

## `firmware/tests/stubs/bsp_wdt.h`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 3 | test/stub | `bsp_wdt_feed` | `static inline void bsp_wdt_feed(void)` | Implementa bsp wdt feed. |

## `firmware/tests/test_circular_buffer.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 6 | test/stub | `test_invalid_initialization` | `static void test_invalid_initialization(void)` | Ejecuta la prueba invalid initialization. |
| 15 | test/stub | `test_fifo_wrap_and_drop_count` | `static void test_fifo_wrap_and_drop_count(void)` | Ejecuta la prueba fifo wrap and drop count. |
| 44 | test/stub | `test_static_storage` | `static void test_static_storage(void)` | Ejecuta la prueba static storage. |
| 58 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_crc_utils.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 6 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_dataframe.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 20 | test/stub | `lrc` | `static uint8_t lrc(const uint8_t *data, size_t length)` | Implementa lrc. |
| 28 | test/stub | `make_frame` | `static size_t make_frame(uint8_t *out, uint16_t cmd, uint16_t status, const uint8_t *payload, uint16_t payload_length)` | Implementa make frame. |
| 46 | test/stub | `on_frame` | `static void on_frame(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data)` | Atiende frame. |
| 59 | test/stub | `reset_state` | `static void reset_state(void)` | Implementa reset state. |
| 69 | test/stub | `test_response_encoding` | `static void test_response_encoding(void)` | Ejecuta la prueba response encoding. |
| 84 | test/stub | `test_fragmented_and_interleaved_frames` | `static void test_fragmented_and_interleaved_frames(void)` | Ejecuta la prueba fragmented and interleaved frames. |
| 107 | test/stub | `test_malformed_input_resynchronizes` | `static void test_malformed_input_resynchronizes(void)` | Ejecuta la prueba malformed input resynchronizes. |
| 129 | test/stub | `test_queue_backpressure_preserves_order` | `static void test_queue_backpressure_preserves_order(void)` | Ejecuta la prueba queue backpressure preserves order. |
| 153 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_device_settings_payload.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 7 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_hf_memory_safety.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 11 | test/stub | `test_ntag_mirror_render` | `static void test_ntag_mirror_render(void)` | Ejecuta la prueba ntag mirror render. |
| 35 | test/stub | `test_ntag_mirror_overlay` | `static void test_ntag_mirror_overlay(void)` | Ejecuta la prueba ntag mirror overlay. |
| 78 | test/stub | `test_mf1_key_access` | `static void test_mf1_key_access(void)` | Ejecuta la prueba mf1 key access. |
| 103 | test/stub | `set_frame_bit` | `static void set_frame_bit(uint8_t *frame, size_t bit, uint8_t value)` | Configura  frame bit. |
| 107 | test/stub | `test_nfc_14a_unwrap_exact_groups` | `static void test_nfc_14a_unwrap_exact_groups(void)` | Ejecuta la prueba nfc 14a unwrap exact groups. |
| 159 | test/stub | `test_emv_uid_continuity` | `static void test_emv_uid_continuity(void)` | Ejecuta la prueba emv uid continuity. |
| 169 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_iso_dep_reader.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 16 | test/stub | `crc_14a_calculate` | `void crc_14a_calculate(uint8_t *data, size_t length, uint8_t *crc)` | Implementa crc 14a calculate. |
| 23 | test/stub | `crc_14a_append` | `void crc_14a_append(uint8_t *data, size_t length)` | Implementa crc 14a append. |
| 27 | test/stub | `write_register_single` | `void write_register_single(uint8_t address, uint8_t value)` | Escribe  register single. |
| 32 | test/stub | `pcd_14a_reader_timeout_get` | `uint16_t pcd_14a_reader_timeout_get(void)` | Implementa pcd 14a reader timeout get. |
| 36 | test/stub | `pcd_14a_reader_timeout_set` | `void pcd_14a_reader_timeout_set(uint16_t timeout_ms)` | Implementa pcd 14a reader timeout set. |
| 40 | test/stub | `pcd_14a_reader_bytes_transfer` | `uint8_t pcd_14a_reader_bytes_transfer(uint8_t command, uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_bits, uint16_t max_rx_bits)` | Implementa pcd 14a reader bytes transfer. |
| 51 | test/stub | `make_frame` | `static void make_frame(uint8_t *out, uint16_t *length, const uint8_t *body, uint8_t body_len)` | Implementa make frame. |
| 58 | test/stub | `chained_command_handler` | `static uint8_t chained_command_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa chained command handler. |
| 73 | test/stub | `chained_response_handler` | `static uint8_t chained_response_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa chained response handler. |
| 93 | test/stub | `wtx_handler` | `static uint8_t wtx_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa wtx handler. |
| 108 | test/stub | `repeated_wtx_handler` | `static uint8_t repeated_wtx_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa repeated wtx handler. |
| 126 | test/stub | `endless_wtx_handler` | `static uint8_t endless_wtx_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa endless wtx handler. |
| 139 | test/stub | `wtx_timeout_recovery_handler` | `static uint8_t wtx_timeout_recovery_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa wtx timeout recovery handler. |
| 162 | test/stub | `initial_timeout_recovery_handler` | `static uint8_t initial_timeout_recovery_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Inicializa ial timeout recovery handler. |
| 176 | test/stub | `bad_crc_handler` | `static uint8_t bad_crc_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa bad crc handler. |
| 186 | test/stub | `final_nak_handler` | `static uint8_t final_nak_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa final nak handler. |
| 200 | test/stub | `deselect_handler` | `static uint8_t deselect_handler(const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint16_t *rx_len)` | Implementa deselect handler. |
| 210 | test/stub | `new_reader` | `static iso_dep_reader_t new_reader(uint8_t fsci)` | Implementa new reader. |
| 220 | test/stub | `test_chained_command` | `static void test_chained_command(void)` | Ejecuta la prueba chained command. |
| 233 | test/stub | `test_chained_response_and_next_command` | `static void test_chained_response_and_next_command(void)` | Ejecuta la prueba chained response and next command. |
| 249 | test/stub | `test_wtx_and_crc_error` | `static void test_wtx_and_crc_error(void)` | Ejecuta la prueba wtx and crc error. |
| 274 | test/stub | `test_repeated_wtx_and_total_budget` | `static void test_repeated_wtx_and_total_budget(void)` | Ejecuta la prueba repeated wtx and total budget. |
| 297 | test/stub | `test_wtx_timeout_response_recovery` | `static void test_wtx_timeout_response_recovery(void)` | Ejecuta la prueba wtx timeout response recovery. |
| 321 | test/stub | `test_deselect` | `static void test_deselect(void)` | Ejecuta la prueba deselect. |
| 331 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_iso_dep_session.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 39 | test/stub | `app_timer_cnt_get` | `uint32_t app_timer_cnt_get(void)` | Implementa app timer cnt get. |
| 43 | test/stub | `app_timer_cnt_diff_compute` | `uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from)` | Implementa app timer cnt diff compute. |
| 47 | test/stub | `bsp_delay_ms` | `void bsp_delay_ms(uint16_t delay_ms)` | Implementa bsp delay ms. |
| 52 | test/stub | `pcd_14a_reader_reset` | `void pcd_14a_reader_reset(void)` | Implementa pcd 14a reader reset. |
| 56 | test/stub | `pcd_14a_reader_antenna_on` | `void pcd_14a_reader_antenna_on(void)` | Implementa pcd 14a reader antenna on. |
| 61 | test/stub | `pcd_14a_reader_antenna_off` | `void pcd_14a_reader_antenna_off(void)` | Implementa pcd 14a reader antenna off. |
| 66 | test/stub | `pcd_14a_reader_timeout_get` | `uint16_t pcd_14a_reader_timeout_get(void)` | Implementa pcd 14a reader timeout get. |
| 70 | test/stub | `pcd_14a_reader_timeout_set` | `void pcd_14a_reader_timeout_set(uint16_t timeout_ms)` | Implementa pcd 14a reader timeout set. |
| 74 | test/stub | `pcd_14a_reader_scan_auto` | `uint8_t pcd_14a_reader_scan_auto(picc_14a_tag_t *tag)` | Implementa pcd 14a reader scan auto. |
| 81 | test/stub | `pcd_14a_reader_polling_annotation_set` | `bool pcd_14a_reader_polling_annotation_set(const uint8_t *frame, uint8_t length)` | Implementa pcd 14a reader polling annotation set. |
| 91 | test/stub | `pcd_14a_reader_polling_annotation_timing_set` | `bool pcd_14a_reader_polling_annotation_timing_set(uint8_t retries, uint8_t delay_ms, uint8_t timeout_ms)` | Implementa pcd 14a reader polling annotation timing set. |
| 101 | test/stub | `pcd_14a_reader_polling_annotation_clear` | `void pcd_14a_reader_polling_annotation_clear(void)` | Implementa pcd 14a reader polling annotation clear. |
| 106 | test/stub | `iso_dep_reader_init` | `void iso_dep_reader_init(iso_dep_reader_t *reader, const picc_14a_tag_t *tag)` | Implementa iso dep reader init. |
| 113 | test/stub | `iso_dep_reader_transceive` | `bool iso_dep_reader_transceive(iso_dep_reader_t *reader, const uint8_t *apdu, uint16_t apdu_len, uint8_t *response, uint16_t response_capacity, iso_dep_result_t *result)` | Implementa iso dep reader transceive. |
| 135 | test/stub | `iso_dep_reader_deselect` | `void iso_dep_reader_deselect(iso_dep_reader_t *reader)` | Implementa iso dep reader deselect. |
| 140 | test/stub | `reset_fixture` | `static void reset_fixture(void)` | Implementa reset fixture. |
| 178 | test/stub | `start_session` | `static uint32_t start_session(picc_14a_tag_t *tag)` | Inicia  session. |
| 185 | test/stub | `test_start_and_persistent_exchange` | `static void test_start_and_persistent_exchange(void)` | Ejecuta la prueba start and persistent exchange. |
| 219 | test/stub | `test_failure_and_timeout_abort` | `static void test_failure_and_timeout_abort(void)` | Ejecuta la prueba failure and timeout abort. |
| 245 | test/stub | `test_start_replaces_session_and_rejects_bad_targets` | `static void test_start_replaces_session_and_rejects_bad_targets(void)` | Ejecuta la prueba start replaces session and rejects bad targets. |
| 265 | test/stub | `test_plain_start_clears_stale_annotation` | `static void test_plain_start_clears_stale_annotation(void)` | Ejecuta la prueba plain start clears stale annotation. |
| 278 | test/stub | `test_apple_transit_annotation_and_cleanup` | `static void test_apple_transit_annotation_and_cleanup(void)` | Ejecuta la prueba apple transit annotation and cleanup. |
| 318 | test/stub | `test_apple_transit_setup_and_parameter_failures_cleanup` | `static void test_apple_transit_setup_and_parameter_failures_cleanup(void)` | Ejecuta la prueba apple transit setup and parameter failures cleanup. |
| 347 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_keyboard_payload.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 30 | test/stub | `app_timer_cnt_get` | `uint32_t app_timer_cnt_get(void)` | Implementa app timer cnt get. |
| 34 | test/stub | `app_timer_cnt_diff_compute` | `uint32_t app_timer_cnt_diff_compute(uint32_t ticks_to, uint32_t ticks_from)` | Implementa app timer cnt diff compute. |
| 38 | test/stub | `is_usb_working` | `bool is_usb_working(void)` | Comprueba usb working. |
| 42 | test/stub | `is_usb_tx_idle` | `bool is_usb_tx_idle(void)` | Comprueba usb tx idle. |
| 46 | test/stub | `is_nus_tx_idle` | `bool is_nus_tx_idle(void)` | Comprueba nus tx idle. |
| 50 | test/stub | `ble_keyboard_link_authorized` | `bool ble_keyboard_link_authorized(void)` | Implementa ble keyboard link authorized. |
| 54 | test/stub | `keyboard_hid_ble_ready` | `bool keyboard_hid_ble_ready(void)` | Implementa keyboard hid ble ready. |
| 62 | test/stub | `ble_peripheral_name_set_temporary` | `uint32_t ble_peripheral_name_set_temporary(const uint8_t *name, uint8_t length)` | Implementa ble peripheral name set temporary. |
| 67 | test/stub | `sleep_timer_stop` | `void sleep_timer_stop(void)` | Implementa sleep timer stop. |
| 70 | test/stub | `keyboard_hid_outputs_ready` | `bool keyboard_hid_outputs_ready(uint8_t outputs)` | Implementa keyboard hid outputs ready. |
| 75 | test/stub | `keyboard_hid_send_report` | `uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers, uint8_t usage)` | Implementa keyboard hid send report. |
| 84 | test/stub | `keyboard_hid_abort` | `void keyboard_hid_abort(void)` | Implementa keyboard hid abort. |
| 88 | test/stub | `crc32_ieee` | `static uint32_t crc32_ieee(const uint8_t *data, size_t length)` | Implementa crc32 ieee. |
| 99 | test/stub | `reset_test` | `static void reset_test(void)` | Implementa reset test. |
| 119 | test/stub | `upload_program` | `static uint32_t upload_program(const uint8_t *program, uint16_t length, data_frame_transport_t transport)` | Implementa upload program. |
| 143 | test/stub | `test_usb_run_waits_for_command_response` | `static void test_usb_run_waits_for_command_response(void)` | Ejecuta la prueba usb run waits for command response. |
| 187 | test/stub | `test_run_tracks_its_command_transport` | `static void test_run_tracks_its_command_transport(void)` | Ejecuta la prueba run tracks its command transport. |
| 214 | test/stub | `test_upload_transport_crc_and_atomic_commit` | `static void test_upload_transport_crc_and_atomic_commit(void)` | Ejecuta la prueba upload transport crc and atomic commit. |
| 249 | test/stub | `test_run_rejects_unavailable_output_and_bad_transport` | `static void test_run_rejects_unavailable_output_and_bad_transport(void)` | Ejecuta la prueba run rejects unavailable output and bad transport. |
| 265 | test/stub | `test_shorter_commit_resets_execution_status` | `static void test_shorter_commit_resets_execution_status(void)` | Ejecuta la prueba shorter commit resets execution status. |
| 295 | test/stub | `test_ble_arm_waits_for_hid_and_runs_without_command_link` | `static void test_ble_arm_waits_for_hid_and_runs_without_command_link(void)` | Ejecuta la prueba ble arm waits for hid and runs without command link. |
| 330 | test/stub | `test_cancel_disarms_and_restores_name` | `static void test_cancel_disarms_and_restores_name(void)` | Ejecuta la prueba cancel disarms and restores name. |
| 347 | test/stub | `test_button_cancel_cannot_be_overwritten_by_armed_transition` | `static void test_button_cancel_cannot_be_overwritten_by_armed_transition(void)` | Ejecuta la prueba button cancel cannot be overwritten by armed transition. |
| 368 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_lf_helpers.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 11 | test/stub | `identity_period` | `static uint8_t identity_period(uint8_t interval)` | Implementa identity period. |
| 15 | test/stub | `test_line_decoders` | `static void test_line_decoders(void)` | Ejecuta la prueba line decoders. |
| 42 | test/stub | `check_wiegand_roundtrip` | `static void check_wiegand_roundtrip(wiegand_card_t card, uint8_t bits)` | Implementa check wiegand roundtrip. |
| 59 | test/stub | `test_wiegand_formats` | `static void test_wiegand_formats(void)` | Ejecuta la prueba wiegand formats. |
| 70 | test/stub | `test_jablotron_vectors_and_modulator` | `static void test_jablotron_vectors_and_modulator(void)` | Ejecuta la prueba jablotron vectors and modulator. |
| 110 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |

## `firmware/tests/test_tag_persistence.c`

| Linea | Ambito | Funcion | Firma | Descripcion |
|---:|---|---|---|---|
| 23 | test/stub | `set_write_results` | `static void set_write_results(const bool *results, size_t count)` | Configura  write results. |
| 30 | test/stub | `fds_write_sync` | `bool fds_write_sync(uint16_t id, uint16_t key, uint16_t length, void *buffer)` | Implementa fds write sync. |
| 43 | test/stub | `fds_read_sync` | `bool fds_read_sync(uint16_t id, uint16_t key, uint16_t *length, uint8_t *buffer)` | Implementa fds read sync. |
| 52 | test/stub | `fds_delete_sync` | `int fds_delete_sync(uint16_t id, uint16_t key)` | Implementa fds delete sync. |
| 58 | test/stub | `fds_is_exists` | `bool fds_is_exists(uint16_t id, uint16_t key)` | Implementa fds is exists. |
| 64 | test/stub | `fds_util_last_error` | `ret_code_t fds_util_last_error(void)` | Implementa fds util last error. |
| 68 | test/stub | `get_fds_map_by_slot_sense_type_for_dump` | `void get_fds_map_by_slot_sense_type_for_dump( uint8_t slot, tag_sense_type_t sense_type, fds_slot_record_map_t *map )` | Obtiene  fds map by slot sense type for dump. |
| 77 | test/stub | `get_fds_map_by_slot_sense_type_for_nick` | `void get_fds_map_by_slot_sense_type_for_nick( uint8_t slot, tag_sense_type_t sense_type, fds_slot_record_map_t *map )` | Obtiene  fds map by slot sense type for nick. |
| 85 | test/stub | `nfc_tag_14a_sense_switch` | `void nfc_tag_14a_sense_switch(bool enable)` | Implementa nfc tag 14a sense switch. |
| 89 | test/stub | `lf_tag_125khz_sense_switch` | `void lf_tag_125khz_sense_switch(bool enable)` | Implementa lf tag 125khz sense switch. |
| 93 | test/stub | `rgb_marquee_reset` | `void rgb_marquee_reset(void)` | Implementa rgb marquee reset. |
| 95 | test/stub | `nfc_tag_mf1_data_loadcb` | `int nfc_tag_mf1_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data loadcb. |
| 101 | test/stub | `nfc_tag_mf1_data_savecb` | `int nfc_tag_mf1_data_savecb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data savecb. |
| 113 | test/stub | `nfc_tag_mf1_data_save_failcb` | `void nfc_tag_mf1_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf1 data save failcb. |
| 136 | test/stub | `nfc_tag_mf0_ntag_data_save_failcb` | `DEFINE_TAG_CALLBACKS(nfc_tag_mf0_ntag, 16) DEFINE_TAG_CALLBACKS(nfc_tag_14a_4, 16) void nfc_tag_mf0_ntag_data_save_failcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa nfc tag mf0 ntag data save failcb. |
| 144 | test/stub | `nfc_tag_mf1_data_factory` | `bool nfc_tag_mf1_data_factory(uint8_t slot, tag_specific_type_t type)` | Implementa nfc tag mf1 data factory. |
| 150 | test/stub | `lf_tag_data_loadcb` | `int lf_tag_data_loadcb(tag_specific_type_t type, tag_data_buffer_t *buffer)` | Implementa lf tag data loadcb. |
| 168 | test/stub | `prepare_active_mf1` | `DEFINE_LF_CALLBACKS(lf_tag_em410x) DEFINE_LF_CALLBACKS(lf_tag_hidprox) DEFINE_LF_CALLBACKS(lf_tag_ioprox) DEFINE_LF_CALLBACKS(lf_tag_viking) DEFINE_LF_CALLBACKS(lf_tag_pac) DEFINE_LF_CALLBACKS(lf_tag_jablotron) DEFINE_LF_CALLBACKS(lf_tag_idteck) static tag_data_buffer_t *prepare_active_mf1(void)` | Implementa prepare active mf1. |
| 187 | test/stub | `test_failed_save_retries_and_commits_crc` | `static void test_failed_save_retries_and_commits_crc(void)` | Ejecuta la prueba failed save retries and commits crc. |
| 211 | test/stub | `test_failed_shadow_prepare_is_retried` | `static void test_failed_shadow_prepare_is_retried(void)` | Ejecuta la prueba failed shadow prepare is retried. |
| 240 | test/stub | `test_factory_data_requires_configured_type` | `static void test_factory_data_requires_configured_type(void)` | Ejecuta la prueba factory data requires configured type. |
| 247 | test/stub | `test_invalid_crc_baseline_forces_write` | `static void test_invalid_crc_baseline_forces_write(void)` | Ejecuta la prueba invalid crc baseline forces write. |
| 258 | test/stub | `test_failed_switch_and_delete_keep_owner` | `static void test_failed_switch_and_delete_keep_owner(void)` | Ejecuta la prueba failed switch and delete keep owner. |
| 284 | test/stub | `main` | `int main(void)` | Punto de entrada del ejecutable. |
