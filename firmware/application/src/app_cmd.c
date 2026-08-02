#include <stdlib.h>

#include "fds_util.h"
#include "bsp_time.h"
#include "bsp_delay.h"
#include "usb_main.h"
#include "rfid_main.h"
#include "ble_main.h"
#include "rgb_marquee.h"
#include "syssleep.h"
#include "hex_utils.h"
#include "data_cmd.h"
#include "app_cmd.h"
#include "app_cmd_ble.h"
#include "app_cmd_keyboard.h"
#include "app_status.h"
#include "tag_persistence.h"
#include "nrf_pwr_mgmt.h"
#include "settings.h"
#include "device_settings_payload_internal.h"
#include "delayed_reset.h"
#include "netdata.h"
#include "app_timer.h"
#include "active_slot_snapshot_internal.h"
#if defined(PROJECT_CHAMELEON_ULTRA)
#include "bsp_wdt.h"
#include "lf_reader_generic.h"
#include "lf_em4x05_data.h"
#include "rc522.h"
#include "iso_dep_reader.h"
#include "iso_dep_session.h"
#include "emv_trace.h"
#include "mf1_crapto1.h"
#include "parity.h"
#include "hf_capture.h"
#endif
#include "nfc_14a.h"
#include "nfc_14a_4.h"

#define NRF_LOG_MODULE_NAME app_cmd
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

static active_slot_snapshot_transaction_t m_active_slot_snapshot;
static uint32_t m_last_snapshot_revision;

static uint16_t active_slot_snapshot_block_count(tag_specific_type_t type) {
    switch (type) {
        case TAG_TYPE_MIFARE_Mini: return 20u;
        case TAG_TYPE_MIFARE_1024: return 64u;
        case TAG_TYPE_MIFARE_2048: return 128u;
        case TAG_TYPE_MIFARE_4096: return 256u;
        default: return 0u;
    }
}

static uint32_t active_slot_snapshot_new_revision(void) {
    uint32_t revision;
    do {
        revision = ((uint32_t)(unsigned int)rand() << 16) ^
                   (uint32_t)(unsigned int)rand();
    } while (revision == 0u || revision == m_last_snapshot_revision);
    m_last_snapshot_revision = revision;
    return revision;
}

void app_cmd_active_slot_snapshot_process(void) {
    if (!m_active_slot_snapshot.active) return;
    uint32_t now = app_timer_cnt_get();
    uint32_t idle_elapsed = app_timer_cnt_diff_compute(
                                now, m_active_slot_snapshot.last_activity);
    uint32_t absolute_elapsed = app_timer_cnt_diff_compute(
                                    now, m_active_slot_snapshot.started_at);
    if (!active_slot_snapshot_transaction_expired(
                &m_active_slot_snapshot, idle_elapsed, absolute_elapsed,
                APP_TIMER_TICKS(ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS),
                APP_TIMER_TICKS(ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS))) return;

    active_slot_snapshot_transaction_clear(&m_active_slot_snapshot);
    tag_emulation_snapshot_release();
    NRF_LOG_WARNING("Active-slot snapshot lease expired.");
}

bool app_cmd_active_slot_snapshot_is_active(void) {
    return m_active_slot_snapshot.active;
}


static bool change_slot_auto(uint8_t slot_new) {
    uint8_t slot_now = tag_emulation_get_slot();
    device_mode_t mode = get_device_mode();
    if (!tag_emulation_change_slot(slot_new, mode != DEVICE_MODE_READER)) {
        return false;
    }
    apply_slot_change(slot_now, slot_new);
    return true;
}

typedef struct {
    uint8_t uid_len;
    uint16_t uid_offset;
    uint16_t atqa_offset;
    uint16_t sak_offset;
    uint16_t ats_offset;
    uint8_t ats_len;
} cmd_hf14a_anticoll_t;

static bool cmd_parse_hf14a_anticoll(uint16_t length, const uint8_t *data,
                                     cmd_hf14a_anticoll_t *parsed) {
    if (length < 5u || data == NULL || !is_valid_uid_size(data[0])) return false;

    uint8_t uid_len = data[0];
    uint16_t ats_len_offset = 1u + uid_len + 2u + 1u;
    if (ats_len_offset >= length) return false;

    uint8_t ats_len = data[ats_len_offset];
    uint16_t expected = ats_len_offset + 1u + ats_len;
    if (length != expected) return false;

    parsed->uid_len = uid_len;
    parsed->uid_offset = 1u;
    parsed->atqa_offset = 1u + uid_len;
    parsed->sak_offset = parsed->atqa_offset + 2u;
    parsed->ats_offset = ats_len_offset + 1u;
    parsed->ats_len = ats_len;
    return true;
}

static data_frame_tx_t *cmd_processor_get_app_version(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    struct {
        uint8_t version_major;
        uint8_t version_minor;
    } PACKED payload;
    payload.version_major = APP_FW_VER_MAJOR;
    payload.version_minor = APP_FW_VER_MINOR;
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}


static data_frame_tx_t *cmd_processor_get_git_version(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return data_frame_make(cmd, STATUS_SUCCESS, strlen(GIT_VERSION), (uint8_t *)GIT_VERSION);
}


static data_frame_tx_t *cmd_processor_get_device_model(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t resp_data = hw_get_device_type();
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(resp_data), &resp_data);
}


static data_frame_tx_t *cmd_processor_change_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 1) || (data[0] > 1)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
#if defined(PROJECT_CHAMELEON_ULTRA)
    if (hf_capture_is_active()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
#endif

    if (data[0] == 1) {
#if defined(PROJECT_CHAMELEON_ULTRA)
        reader_mode_enter();
        return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
#else
        return data_frame_make(cmd, STATUS_NOT_IMPLEMENTED, 0, NULL);
#endif
    } else {
#if defined(PROJECT_CHAMELEON_ULTRA)
        tag_mode_enter();
#endif
        return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
    }
}

static data_frame_tx_t *cmd_processor_get_device_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t resp_data = (get_device_mode() == DEVICE_MODE_READER);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(resp_data), &resp_data);
}

static data_frame_tx_t *cmd_processor_enter_bootloader(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // restart to boot
#define BOOTLOADER_DFU_GPREGRET_MASK            (0xB0)
#define BOOTLOADER_DFU_START_BIT_MASK           (0x01)
#define BOOTLOADER_DFU_START    (BOOTLOADER_DFU_GPREGRET_MASK |         BOOTLOADER_DFU_START_BIT_MASK)
    APP_ERROR_CHECK(sd_power_gpregret_clr(0, 0xffffffff));
    APP_ERROR_CHECK(sd_power_gpregret_set(0, BOOTLOADER_DFU_START));
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
    // Never into here...
    while (1) __NOP();
    // For the compiler to be happy...
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_device_chip_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    struct {
        uint32_t chip_HSW;
        uint32_t chip_LSW;
    } PACKED payload;
    payload.chip_LSW = U32HTONL(NRF_FICR->DEVICEID[0]);
    payload.chip_HSW = U32HTONL(NRF_FICR->DEVICEID[1]);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_get_device_address(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // The FICR value is a just a random number, with no knowledge
    // of the Bluetooth Specification requirements for random addresses.
    // So we need to set a Bluetooth LE random address as a static address.
    // See: https://github.com/zephyrproject-rtos/zephyr/blob/7b6b1328a0cb96fe313a5e2bfc57047471df236e/subsys/bluetooth/controller/hci/nordic/hci_vendor.c#L29

    struct {
        uint16_t device_address_HSW;
        uint32_t device_address_LSW;
    } PACKED payload;
    payload.device_address_LSW = U32HTONL(NRF_FICR->DEVICEADDR[0]);
    payload.device_address_HSW = U16HTONS(NRF_FICR->DEVICEADDR[1] | 0xC000);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_save_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    status = settings_save_config();
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_reset_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    settings_init_config();
    status = settings_save_config();
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_device_settings(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t settings[DEVICE_SETTINGS_V6_PAYLOAD_SIZE] = {};
    device_settings_payload_v6(
        settings,
        SETTINGS_CURRENT_VERSION,
        settings_get_animation_config(),
        settings_get_button_press_config('A'),
        settings_get_button_press_config('B'),
        settings_get_long_button_press_config('A'),
        settings_get_long_button_press_config('B'),
        settings_get_ble_pairing_enable(),
        settings_get_ble_connect_key(),
        settings_get_sleep_timeout()
    );
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(settings), settings);
}

static data_frame_tx_t *cmd_processor_set_animation_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 1) || (data[0] >= SettingsAnimationModeMAX)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    settings_set_animation_config(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_animation_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t animation_mode = settings_get_animation_config();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &animation_mode);
}

static data_frame_tx_t *cmd_processor_get_battery_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    struct {
        uint16_t voltage;
        uint8_t percent;
    } PACKED payload;
    payload.voltage = U16HTONS(batt_lvl_in_milli_volts);
    payload.percent = percentage_batt_lvl;
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_get_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 1) || (!is_settings_button_type_valid(data[0]))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t button_press_config = settings_get_button_press_config(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(button_press_config), &button_press_config);
}

static data_frame_tx_t *cmd_processor_set_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 2) || (!is_settings_button_type_valid(data[0]))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    settings_set_button_press_config(data[0], data[1]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_long_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 1) || (!is_settings_button_type_valid(data[0]))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t button_press_config = settings_get_long_button_press_config(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(button_press_config), &button_press_config);
}

static data_frame_tx_t *cmd_processor_set_long_button_press_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 2) || (!is_settings_button_type_valid(data[0]))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    settings_set_long_button_press_config(data[0], data[1]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_sleep_timeout(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t seconds = settings_get_sleep_timeout() / 1000U;
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &seconds);
}

static data_frame_tx_t *cmd_processor_set_sleep_timeout(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] < SETTINGS_SLEEP_TIMEOUT_MIN_S || data[0] > SETTINGS_SLEEP_TIMEOUT_MAX_S) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    settings_set_sleep_timeout(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_ble_pairing_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t is_enable = settings_get_ble_pairing_enable();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &is_enable);
}

static data_frame_tx_t *cmd_processor_set_ble_pairing_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    settings_set_ble_pairing_enable(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_keyboard_hid_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t is_enable = settings_get_keyboard_hid_enable();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &is_enable);
}

static data_frame_tx_t *cmd_processor_set_keyboard_hid_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Persisted by SAVE_SETTINGS; requires a reboot to (un)expose the USB HID
    // interface and register/tear down the BLE HID service at boot.
    settings_set_keyboard_hid_enable(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

#if defined(PROJECT_CHAMELEON_ULTRA)

static data_frame_tx_t *cmd_processor_hf14a_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    picc_14a_tag_t taginfo;
    status = pcd_14a_reader_scan_auto(&taginfo);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    // uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
    // dynamic length, so no struct
    uint8_t payload[1 + sizeof(taginfo.uid) + sizeof(taginfo.atqa) + sizeof(taginfo.sak) + 1 + sizeof(taginfo.ats)];
    uint16_t offset = 0;
    payload[offset++] = taginfo.uid_len;
    memcpy(&payload[offset], taginfo.uid, taginfo.uid_len);
    offset += taginfo.uid_len;
    memcpy(&payload[offset], taginfo.atqa, sizeof(taginfo.atqa));
    offset += sizeof(taginfo.atqa);
    payload[offset++] = taginfo.sak;
    payload[offset++] = taginfo.ats_len;
    memcpy(&payload[offset], taginfo.ats, taginfo.ats_len);
    offset += taginfo.ats_len;
    return data_frame_make(cmd, STATUS_HF_TAG_OK, offset, payload);
}

static data_frame_tx_t *cmd_processor_mf1_detect_support(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    status = check_std_mifare_nt_support();
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_detect_prng(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t type;
    status = check_prng_type((mf1_prng_type_t *)&type);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(type), &type);
}

// We have a reusable payload structure.
typedef struct {
    uint8_t type_known;
    uint8_t block_known;
    uint8_t key_known[6];
    uint8_t type_target;
    uint8_t block_target;
} PACKED nested_common_payload_t;

static data_frame_tx_t *cmd_processor_mf1_static_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    mf1_static_nested_core_t sncs;
    if (length != sizeof(nested_common_payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    nested_common_payload_t *payload = (nested_common_payload_t *)data;
    status = static_nested_recover_key(bytes_to_num(payload->key_known, 6), payload->block_known, payload->type_known, payload->block_target, payload->type_target, &sncs);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    // mf1_static_nested_core_t is PACKED and comprises only bytes so we can use it directly
    return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(sncs), (uint8_t *)(&sncs));
}

static data_frame_tx_t *cmd_processor_mf1_darkside_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 4) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    struct {
        uint8_t darkside_status;
        // DarksideCore_t is PACKED and comprises only bytes so we can use it directly
        DarksideCore_t dc;
    } PACKED payload;
    status = darkside_recover_key(data[1], data[0], data[2], data[3], &payload.dc, (mf1_darkside_status_t *)&payload.darkside_status);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    if (payload.darkside_status != DARKSIDE_OK) {
        return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(payload.darkside_status), &payload.darkside_status);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_mf1_detect_nt_dist(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t type_known;
        uint8_t block_known;
        uint8_t key_known[6];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    struct {
        uint8_t uid[4];
        uint32_t distance;
    } PACKED payload_resp;

    payload_t *payload = (payload_t *)data;
    uint32_t distance;
    status = nested_distance_detect(payload->block_known, payload->type_known, payload->key_known, payload_resp.uid, &distance);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    payload_resp.distance = U32HTONL(distance);
    return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(payload_resp), (uint8_t *)&payload_resp);
}

static data_frame_tx_t *cmd_processor_mf1_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    mf1_nested_core_t ncs[SETS_NR];
    if (length != sizeof(nested_common_payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    nested_common_payload_t *payload = (nested_common_payload_t *)data;
    status = nested_recover_key(bytes_to_num(payload->key_known, 6), payload->block_known, payload->type_known, payload->block_target, payload->type_target, ncs);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    // mf1_nested_core_t is PACKED and comprises only bytes so we can use it directly
    return data_frame_make(cmd, STATUS_HF_TAG_OK, sizeof(ncs), (uint8_t *)(&ncs));
}

static data_frame_tx_t *cmd_processor_mf1_enc_nested_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t key[6];
        uint8_t sector_count;
        uint8_t starting_sector;
    } PACKED payload_t;

    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    if (payload->sector_count == 0u || payload->sector_count > 40u ||
            payload->starting_sector >= payload->sector_count) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    uint64_t ui64Key = bytes_to_num(payload->key, 6);
    uint8_t sector_data[40][sizeof(mf1_static_nonce_sector_t)] = {0};
    uint8_t sectors_acquired = 0;
    uint32_t cuid = 0;

    status = mf1_static_encrypted_nonces_acquire(ui64Key, payload->sector_count, payload->starting_sector, sector_data, &sectors_acquired, &cuid);

    if (sectors_acquired > (uint8_t)(40u - payload->starting_sector)) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }

    uint8_t response_data[sizeof(uint32_t) + 40u * sizeof(mf1_static_nonce_sector_t)] = {0};
    num_to_bytes(cuid, 4, response_data);
    for (uint8_t i = 0; i < sectors_acquired; i++) {
        memcpy(response_data + sizeof(uint32_t) + i * sizeof(mf1_static_nonce_sector_t),
               sector_data[payload->starting_sector + i], sizeof(mf1_static_nonce_sector_t));
    }

    return data_frame_make(cmd, status, sectors_acquired * sizeof(mf1_static_nonce_sector_t) + sizeof(uint32_t), response_data);
}

static data_frame_tx_t *cmd_processor_mf1_auth_one_key_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t type;
        uint8_t block;
        uint8_t key[6];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    status = auth_key_use_522_hw(payload->block, payload->type, payload->key);
    pcd_14a_reader_mf1_unauth();
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_check_keys_of_sectors(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t key_count;
    if (!cmd_parse_count_u8(length, 10u, sizeof(mf1_key_t), &key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    // init
    mf1_toolbox_check_keys_of_sectors_in_t in = {
        .mask = *(mf1_toolbox_check_keys_of_sectors_mask_t *) &data[0],
        .keys_len = key_count,
        .keys = (mf1_key_t *) &data[10]
    };
    mf1_toolbox_check_keys_of_sectors_out_t out;
    status = mf1_toolbox_check_keys_of_sectors(&in, &out);

    return data_frame_make(cmd, status, sizeof(out), (uint8_t *)&out);
}

static data_frame_tx_t *cmd_processor_mf1_check_keys_on_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 9 || data[2] * 6 + 3 != length) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    mf1_toolbox_check_keys_on_block_in_t in = {
        .block = data[0],
        .key_type = data[1],
        .keys_len = data[2],
        .keys = (mf1_key_t *) &data[3]
    };

    mf1_toolbox_check_keys_on_block_out_t out;
    status = mf1_toolbox_check_keys_on_block(&in, &out);

    return data_frame_make(cmd, status, sizeof(out), (uint8_t *)&out);
}

static data_frame_tx_t *cmd_processor_mf1_hardnested_nonces_acquire(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t slow;
        uint8_t type_known;
        uint8_t block_known;
        uint8_t key_known[6];
        uint8_t type_target;
        uint8_t block_target;
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    payload_t *payload = (payload_t *)data;

    // Collect up to ~254 nonces per call. The count is returned in a single
    // leading byte (max 255), so the buffer is sized just under that limit
    // (254 * 4.5 = 1143 payload bytes, +1 count). Bigger batches than the old
    // 110 mean far fewer host round-trips and card re-selections while gathering
    // the ~1400 nonces hardnested needs. Still one frame (<= 4096), so a
    // large-packet BLE host completes each batch in a single exchange.
    uint8_t nonces[1148] = { 0x00 };
    if (length < 11) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    status = mf1_hardnested_nonces_acquire(
                 payload->slow,
                 payload->block_known,
                 payload->type_known,
                 bytes_to_num(payload->key_known, 6),
                 payload->block_target,
                 payload->type_target,
                 nonces + 1,
                 sizeof(nonces) - 1,         // The upper limit of the buffer size. Here we take out the first byte to mark the number of collections.
                 &nonces[0]                  // The number of random numbers collected above
             );
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, status, nonces[0] * 4.5, (uint8_t *)(nonces + 1));
}

static data_frame_tx_t *cmd_processor_mf1_read_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t type;
        uint8_t block;
        uint8_t key[6];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    uint8_t block[16] = { 0x00 };
    status = auth_key_use_522_hw(payload->block, payload->type, payload->key);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    status = pcd_14a_reader_mf1_read(payload->block, block);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, status, sizeof(block), block);
}

// Authenticate once to a sector, then read `count` consecutive blocks from it
// (MIFARE keeps the sector authenticated across reads) — ~4x fewer auths than
// read-one-block per block when dumping. All blocks must be in the one sector
// the start block belongs to. Returns the blocks actually read (16 bytes each);
// the host fills any tail it didn't get.
static data_frame_tx_t *cmd_processor_mf1_read_blocks(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t type;
        uint8_t block;   // start block
        uint8_t count;   // consecutive blocks to read (same sector)
        uint8_t key[6];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    payload_t *payload = (payload_t *)data;
    if (payload->count == 0 || payload->count > 16) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t end_block = (uint16_t)payload->block + payload->count - 1u;
    uint8_t start_sector = payload->block < 128u ? payload->block / 4u :
                           32u + (payload->block - 128u) / 16u;
    uint8_t end_sector = end_block < 128u ? end_block / 4u :
                         32u + (end_block - 128u) / 16u;
    if (end_block >= NFC_TAG_MF1_BLOCK_MAX || start_sector != end_sector) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    status = auth_key_use_522_hw(payload->block, payload->type, payload->key);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    static uint8_t out[16 * 16];
    uint16_t out_len = 0;
    for (uint8_t i = 0; i < payload->count; i++) {
        status = pcd_14a_reader_mf1_read(payload->block + i, &out[out_len]);
        if (status != STATUS_HF_TAG_OK) break; // return the prefix we did read
        out_len += 16;
    }
    if (out_len == 0) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_OK, out_len, out);
}

static data_frame_tx_t *cmd_processor_mf1_write_one_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t type;
        uint8_t block;
        uint8_t key[6];
        uint8_t block_data[16];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    status = auth_key_use_522_hw(payload->block, payload->type, payload->key);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    status = pcd_14a_reader_mf1_write(payload->block, payload->block_data);
    return data_frame_make(cmd, status, 0, NULL);
}

#if defined(PROJECT_CHAMELEON_ULTRA)

static data_frame_tx_t *cmd_processor_hf14a_set_field_on(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    device_mode_t mode = get_device_mode();
    if (mode != DEVICE_MODE_READER) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }

    iso_dep_session_abort();
    // Reset and turn on the antenna
    pcd_14a_reader_reset();
    pcd_14a_reader_antenna_on();

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_hf14a_set_field_off(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    device_mode_t mode = get_device_mode();
    if (mode != DEVICE_MODE_READER) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }

    // Turn off the antenna and invalidate any state tied to the powered card.
    if (iso_dep_session_is_active()) {
        iso_dep_session_abort();
    } else {
        pcd_14a_reader_antenna_off();
    }

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

#endif

static data_frame_tx_t *cmd_processor_hf14a_raw(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // Response Buffer
    uint8_t resp[DEF_FIFO_LENGTH] = { 0x00 };
    uint16_t resp_length = 0;

    enum { RAW_HEADER_LENGTH = 5 };
    if (length < RAW_HEADER_LENGTH || data == NULL || (data[0] & 0x03u) != 0u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    uint8_t options = data[0];
    bool check_response_crc = (options & 0x04u) != 0u;
    bool keep_rf_field = (options & 0x08u) != 0u;
    bool auto_select = (options & 0x10u) != 0u;
    bool append_crc = (options & 0x20u) != 0u;
    bool wait_response = (options & 0x40u) != 0u;
    bool activate_rf_field = (options & 0x80u) != 0u;
    uint16_t resp_timeout = cmd_read_u16be(&data[1]);
    uint16_t data_bitlength = cmd_read_u16be(&data[3]);
    uint16_t data_bytes = (data_bitlength + 7u) / 8u;
    uint16_t max_data_bytes = DEF_FIFO_LENGTH - (append_crc ? DEF_CRC_LENGTH : 0u);
    if (data_bytes > max_data_bytes || length != RAW_HEADER_LENGTH + data_bytes ||
            (append_crc && (data_bitlength == 0u || (data_bitlength & 7u) != 0u))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

#if defined(PROJECT_CHAMELEON_ULTRA)
    /* Raw RF operations can reset, reselect, or power down the target. */
    iso_dep_session_abort();
#endif

    uint8_t tx[DEF_FIFO_LENGTH] = {0};
    if (data_bytes > 0u) memcpy(tx, &data[RAW_HEADER_LENGTH], data_bytes);

    NRF_LOG_INFO("activate_rf_field  = %d", activate_rf_field);
    NRF_LOG_INFO("wait_response      = %d", wait_response);
    NRF_LOG_INFO("append_crc         = %d", append_crc);
    NRF_LOG_INFO("auto_select        = %d", auto_select);
    NRF_LOG_INFO("keep_rf_field      = %d", keep_rf_field);
    NRF_LOG_INFO("check_response_crc = %d", check_response_crc);

    status = pcd_14a_reader_raw_cmd(
                 activate_rf_field, wait_response, append_crc, auto_select,
                 keep_rf_field, check_response_crc, resp_timeout,
                 data_bitlength, tx,

                 resp,
                 &resp_length,
                 U8ARR_BIT_LEN(resp)
             );

    return data_frame_make(cmd, status, resp_length, resp);
}

static data_frame_tx_t *cmd_processor_hf14a_get_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    hf14a_config_t *hc = get_hf14a_config();
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(hf14a_config_t), (uint8_t *)hc);
}

static data_frame_tx_t *cmd_processor_hf14a_set_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != sizeof(hf14a_config_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    hf14a_config_t hc;
    memcpy(&hc, data, sizeof(hf14a_config_t));
    set_hf14a_config(&hc);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_manipulate_value_block(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t src_type;
        uint8_t src_block;
        uint8_t src_key[6];
        uint8_t operator;
        uint32_t operand;
        uint8_t dst_type;
        uint8_t dst_block;
        uint8_t dst_key[6];
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;

    // scan tag
    picc_14a_tag_t taginfo;
    if (pcd_14a_reader_scan_auto(&taginfo) != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
    }

    // auth src
    status = pcd_14a_reader_mf1_auth(&taginfo, payload->src_type, payload->src_block, payload->src_key);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }

    // value block operation
    int32_t operand = (int32_t)cmd_read_u32be(&data[offsetof(payload_t, operand)]);
    status = pcd_14a_reader_mf1_manipulate_value_block(payload->operator,
                                                       payload->src_block, operand);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }

    // auth dst if needed
    if (payload->src_block != payload->dst_block || payload->src_type != payload->dst_type) {
        status = pcd_14a_reader_mf1_auth(&taginfo, payload->dst_type, payload->dst_block, payload->dst_key);
        if (status != STATUS_HF_TAG_OK) {
            return data_frame_make(cmd, status, 0, NULL);
        }
    }

    // transfer value block
    status = pcd_14a_reader_mf1_transfer_value_block(payload->dst_block);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }

    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_em410x_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t card_buffer[2 + LF_EM410X_ELECTRA_TAG_ID_SIZE] = {0x00};
    status = scan_em410x(card_buffer);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }

    tag_specific_type_t tag_type = (card_buffer[0] << 8) | card_buffer[1];
    uint16_t id_size = (tag_type == TAG_TYPE_EM410X_ELECTRA) ? LF_EM410X_ELECTRA_TAG_ID_SIZE : LF_EM410X_TAG_ID_SIZE;

    return data_frame_make(cmd, STATUS_LF_TAG_OK, 2 + id_size, card_buffer);
}

static data_frame_tx_t *cmd_processor_em410x_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[5];
        uint8_t new_key[4];
        uint8_t old_keys[4]; // we can have more than one... struct just to compute offsets with min 1 key
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t old_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    status = write_em410x_to_t55xx(payload->id, payload->new_key,
                                   payload->old_keys, old_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_em410x_electra_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[13];
        uint8_t new_key[4];
        uint8_t old_keys[4]; // we can have more than one... struct just to compute offsets with min 1 key
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t old_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    status = write_em410x_electra_to_t55xx(payload->id, payload->new_key,
                                           payload->old_keys, old_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_hidprox_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[13];
        uint8_t old_key[4];
        uint8_t new_keys[4]; // we can have more than one... struct just to compute offsets with min 1 key
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t new_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, new_keys),
                            sizeof(payload->new_keys), &new_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    uint8_t format = payload->id[0];
    uint32_t fc = bytes_to_num(payload->id + 1, 4);
    uint64_t cn = payload->id[5];
    cn = (cn << 32) | (bytes_to_num(payload->id + 6, 4));
    uint32_t il = payload->id[10];
    uint32_t oem = bytes_to_num(payload->id + 11, 2);
    status = write_hidprox_to_t55xx(format, fc, cn, il, oem,
                                    payload->old_key, payload->new_keys, new_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_hidprox_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length > 1u || (length == 1u && data == NULL)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t card_data[16] = {0x00};
    uint8_t format_hint = length == 1u ? data[0] : 0u;
    status = scan_hidprox(card_data, format_hint);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(card_data), card_data);
}

static data_frame_tx_t *cmd_processor_ioprox_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t card_data[16] = {0};
    uint8_t hint = (data != NULL) ? data[0] : 0;
    status = scan_ioprox(card_data, hint);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }

    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(card_data), card_data);
}

static data_frame_tx_t *cmd_processor_ioprox_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t card_data[16];  // ioprox_codec_t->data layout: version, facility code, card number, raw8
        uint8_t new_key[4];
        uint8_t old_keys[4];  // we can have more than one... struct just to compute offsets with min 1 key
    } PACKED payload_t;

    payload_t *payload = (payload_t *)data;

    uint8_t old_cnt;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_cnt)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    // Pass card_data (including raw8 at index 4-11) directly to the T55xx writer.
    status = write_ioprox_to_t55xx(
                 payload->card_data,
                 payload->new_key,
                 payload->old_keys,
                 old_cnt
             );

    return data_frame_make(cmd, status, 0, NULL);
}



/**
 * @brief Decode raw8 data to structured ioProx format
 * @param raw8 Input 8 bytes
 * @param output 16 bytes ioprox_codec_t->data layout: version, facility code, card number, raw8
 * @return STATUS_SUCCESS on success
 */
static data_frame_tx_t *cmd_processor_ioprox_decode_raw(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 8) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t output[16];
    uint8_t result = decode_ioprox_raw(data, output);

    if (result != STATUS_SUCCESS) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }

    return data_frame_make(cmd, STATUS_SUCCESS, 16, output);
}

/**
 * @brief Encode ioProx parameters to structured ioProx format
 * @param ver Version byte
 * @param fc Facility code byte
 * @param cn Card number (16-bit)
 * @param out 16 bytes ioprox_codec_t->data layout: version, facility code, card number, raw8
 * @return STATUS_SUCCESS on success
 */
static data_frame_tx_t *cmd_processor_ioprox_compose_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 4) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t output[16];
    memset(output, 0, sizeof(output));
    uint8_t result = encode_ioprox_params(data[0], data[1], (data[2] << 8) | data[3], output);

    if (result != STATUS_SUCCESS) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 16, output);
}

static data_frame_tx_t *cmd_processor_viking_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t card_buffer[4] = {0x00};
    status = scan_viking(card_buffer);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(card_buffer), card_buffer);
}

static data_frame_tx_t *cmd_processor_pac_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t card_id[8] = {0x00};
    status = scan_pac(card_id);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(card_id), card_id);
}

static data_frame_tx_t *cmd_processor_viking_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[4];
        uint8_t new_key[4];
        uint8_t old_keys[4]; // we can have more than one... struct just to compute offsets with min 1 key
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t old_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    status = write_viking_to_t55xx(payload->id, payload->new_key,
                                   payload->old_keys, old_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}


static data_frame_tx_t *cmd_processor_pac_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[LF_PAC_TAG_ID_SIZE];
        uint8_t new_key[4];
        uint8_t old_keys[4];
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t old_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    status = write_pac_to_t55xx(payload->id, payload->new_key,
                               payload->old_keys, old_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_jablotron_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t card_buffer[5] = {0x00};
    status = scan_jablotron(card_buffer);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(card_buffer), card_buffer);
}

static data_frame_tx_t *cmd_processor_jablotron_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t id[5];
        uint8_t new_key[4];
        uint8_t old_keys[4];
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    uint8_t old_key_count;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_key_count)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    status = write_jablotron_to_t55xx(payload->id, payload->new_key,
                                      payload->old_keys, old_key_count);
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_lf_t55xx_write(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t block;    /* block number */
        uint8_t word[4];  /* 32-bit data word, big-endian */
        uint8_t use_pwd;  /* 1 = password write, 0 = open write */
        uint8_t pwd[4];   /* 32-bit password, big-endian (ignored when use_pwd == 0) */
        uint8_t page1;    /* 1 = target page 1, 0 = page 0 */
    } PACKED payload_t;

    if (!cmd_payload_exact(length, data, sizeof(payload_t))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *p = (payload_t *)data;

    bool page1;
    bool use_pwd;
    if (!cmd_parse_bool(p->page1, &page1) || !cmd_parse_bool(p->use_pwd, &use_pwd)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t max_block = page1 ? 3u : 7u;

    if (p->block > max_block) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    uint32_t word    = bytes_to_num(p->word, 4);
    uint32_t passwd  = bytes_to_num(p->pwd,  4);

    status = lf_t55xx_write_block(p->block, word, passwd, use_pwd, page1);
    return data_frame_make(cmd, status, 0, NULL);
}

#define GENERIC_READ_LEN 800
#define GENERIC_READ_TIMEOUT_MS 500
static data_frame_tx_t *cmd_processor_generic_read(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t *outdata = malloc(GENERIC_READ_LEN);

    if (outdata == NULL) {
        return data_frame_make(cmd, STATUS_MEM_ERR, 0, NULL);
    }

    size_t outlen = 0;
    if (!raw_read_to_buffer(outdata, GENERIC_READ_LEN, GENERIC_READ_TIMEOUT_MS, &outlen)) {
        free(outdata);
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    };
    data_frame_tx_t *frame = data_frame_make(cmd, STATUS_LF_TAG_OK, outlen, outdata);

    free(outdata);

    if (frame == NULL) {
        return data_frame_make(cmd, STATUS_CREATE_RESPONSE_ERR, 0, NULL);
    }

    return frame;
}

#endif


static data_frame_tx_t *cmd_processor_set_active_slot(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] >= TAG_MAX_SLOT_NUM) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    status = change_slot_auto(data[0]) ? STATUS_SUCCESS : STATUS_FLASH_WRITE_FAIL;
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_set_slot_tag_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t num_slot;
        uint16_t tag_type;
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    tag_specific_type_t tag_type = U16NTOHS(payload->tag_type);
    if (payload->num_slot >= TAG_MAX_SLOT_NUM || !is_tag_specific_type_valid(tag_type)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    tag_emulation_change_type(payload->num_slot, tag_type);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_delete_slot_sense_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t num_slot;
        uint8_t sense_type;
    } PACKED payload_t;
    payload_t *payload = (payload_t *)data;
    if ((length != sizeof(payload_t)) ||
            (payload->num_slot >= TAG_MAX_SLOT_NUM) ||
            (payload->sense_type != TAG_SENSE_HF && payload->sense_type != TAG_SENSE_LF)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    status = tag_emulation_delete_data(payload->num_slot, payload->sense_type)
             ? STATUS_SUCCESS : STATUS_FLASH_WRITE_FAIL;
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_set_slot_data_default(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t num_slot;
        uint16_t tag_type;
    } PACKED payload_t;
    if (length != sizeof(payload_t)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    payload_t *payload = (payload_t *)data;
    tag_specific_type_t tag_type = U16NTOHS(payload->tag_type);
    if (payload->num_slot >= TAG_MAX_SLOT_NUM || !is_tag_specific_type_valid(tag_type)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    tag_slot_specific_type_t configured;
    tag_emulation_get_specific_types_by_slot(payload->num_slot, &configured);
    tag_specific_type_t configured_type = get_sense_type_from_tag_type(tag_type) == TAG_SENSE_HF
                                          ? configured.tag_hf : configured.tag_lf;
    if (configured_type != tag_type) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    status = tag_emulation_factory_data(payload->num_slot, tag_type)
             ? STATUS_SUCCESS : STATUS_FLASH_WRITE_FAIL;
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_set_slot_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t slot_index;
        uint8_t sense_type;
        uint8_t enabled;
    } PACKED payload_t;

    payload_t *payload = (payload_t *)data;
    if (length != sizeof(payload_t) ||
            payload->slot_index >= TAG_MAX_SLOT_NUM ||
            (payload->sense_type != TAG_SENSE_HF && payload->sense_type != TAG_SENSE_LF) ||
            payload->enabled > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    uint8_t slot_now = payload->slot_index;
    bool was_enabled = is_slot_enabled(slot_now, payload->sense_type);
    tag_emulation_slot_set_enable(slot_now, payload->sense_type, payload->enabled);
    if (slot_now == tag_emulation_get_slot() && (!payload->enabled) &&
            (!is_slot_enabled(slot_now, payload->sense_type == TAG_SENSE_HF ? TAG_SENSE_LF : TAG_SENSE_HF))) {
        // HF and LF disabled, need to change slot
        uint8_t slot_prev = tag_emulation_slot_find_next(slot_now);
        NRF_LOG_INFO("slot_now = %d, slot_prev = %d", slot_now, slot_prev);
        if (slot_prev == slot_now) {
            set_slot_light_color(RGB_MAGENTA);
        } else {
            if (!change_slot_auto(slot_prev)) {
                tag_emulation_slot_set_enable(slot_now, payload->sense_type, was_enabled);
                if (get_device_mode() != DEVICE_MODE_READER) {
                    tag_emulation_sense_run();
                }
                return data_frame_make(cmd, STATUS_FLASH_WRITE_FAIL, 0, NULL);
            }
        }
    }
    if (slot_now == tag_emulation_get_slot() && get_device_mode() != DEVICE_MODE_READER) {
        tag_emulation_sense_switch(payload->sense_type, payload->enabled);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_slot_data_config_save(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    status = tag_emulation_save() ? STATUS_SUCCESS : STATUS_FLASH_WRITE_FAIL;
    return data_frame_make(cmd, status, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_active_slot(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t slot = tag_emulation_get_slot();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &slot);
}

static data_frame_tx_t *cmd_processor_get_slot_info(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    struct {
        uint16_t hf_tag_type;
        uint16_t lf_tag_type;
    } PACKED payload[8];

    tag_slot_specific_type_t tag_types;
    for (uint8_t slot = 0; slot < 8; slot++) {
        tag_emulation_get_specific_types_by_slot(slot, &tag_types);
        payload[slot].hf_tag_type = U16HTONS(tag_types.tag_hf);
        payload[slot].lf_tag_type = U16HTONS(tag_types.tag_lf);
    }

    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_wipe_fds(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    bool success = fds_wipe();
    status = success ? STATUS_SUCCESS : STATUS_FLASH_WRITE_FAIL;
    delayed_reset(50);
    return data_frame_make(cmd, status, 0, NULL);
}

static bool get_active_em410x_type(tag_specific_type_t *tag_type_out, uint16_t *id_size_out) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if ((tag_types.tag_lf == TAG_TYPE_EM410X || tag_types.tag_lf == TAG_TYPE_EM410X_ELECTRA) &&
            tag_emulation_is_active_type_loaded(tag_types.tag_lf)) {
        *tag_type_out = tag_types.tag_lf;
        *id_size_out = (tag_types.tag_lf == TAG_TYPE_EM410X_ELECTRA) ? LF_EM410X_ELECTRA_TAG_ID_SIZE : LF_EM410X_TAG_ID_SIZE;
        return true;
    }
    return false;
}

static data_frame_tx_t *set_lf_emulator_id(uint16_t cmd, tag_specific_type_t tag_type,
                                            uint16_t expected_length, uint16_t length,
                                            const uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != tag_type) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    if (!cmd_payload_exact(length, data, expected_length)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    tag_data_buffer_t *buffer = get_buffer_by_tag_type(tag_type);
    if (buffer == NULL || buffer->buffer == NULL || buffer->length > 20u ||
            expected_length > buffer->length) {
        return data_frame_make(cmd, STATUS_MEM_ERR, 0, NULL);
    }
    uint8_t previous[20];
    memcpy(previous, buffer->buffer, buffer->length);
    memcpy(buffer->buffer, data, expected_length);
    if (!tag_emulation_load_by_buffer(tag_type, false)) {
        memcpy(buffer->buffer, previous, buffer->length);
        (void)tag_emulation_load_by_buffer(tag_type, false);
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_em410x_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_specific_type_t tag_type;
    uint16_t id_size;
    if (!get_active_em410x_type(&tag_type, &id_size) || length != id_size) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return set_lf_emulator_id(cmd, tag_type, id_size, length, data);
}

static data_frame_tx_t *cmd_processor_em410x_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_specific_type_t tag_type;
    uint16_t id_size;
    if (!get_active_em410x_type(&tag_type, &id_size)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);  // no data in slot, don't send garbage
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(tag_type);
    uint8_t resp[2 + LF_EM410X_ELECTRA_TAG_ID_SIZE] = {0x00};
    resp[0] = tag_type >> 8;
    resp[1] = tag_type;
    memcpy(resp + 2, buffer->buffer, id_size);
    return data_frame_make(cmd, STATUS_SUCCESS, 2 + id_size, resp);
}

static data_frame_tx_t *cmd_processor_hidprox_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_HID_PROX, LF_HIDPROX_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_hidprox_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_HID_PROX ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_HID_PROX)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);  // no data in slot, don't send garbage
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_HID_PROX);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_HIDPROX_TAG_ID_SIZE, buffer->buffer);
}

static data_frame_tx_t *cmd_processor_ioprox_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_IOPROX, LF_IOPROX_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_idteck_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_IDTECK, LF_IDTECK_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_idteck_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_IDTECK ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_IDTECK)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_IDTECK);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_IDTECK_TAG_ID_SIZE, buffer->buffer);
}

#if defined(PROJECT_CHAMELEON_ULTRA)
// T55xx clone is only available on Chameleon Ultra; the Lite firmware
// has no LF reader hardware and does not compile the write_*_to_t55xx
// helpers in lf_reader_main.c.
static data_frame_tx_t *cmd_processor_idteck_write_to_t55xx(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    typedef struct {
        uint8_t card_data[LF_IDTECK_TAG_ID_SIZE];
        uint8_t new_key[4];
        uint8_t old_keys[4];
    } PACKED payload_t;

    payload_t *payload = (payload_t *)data;

    uint8_t old_cnt;
    if (!cmd_parse_count_u8(length, offsetof(payload_t, old_keys),
                            sizeof(payload->old_keys), &old_cnt)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    status = write_idteck_to_t55xx(payload->card_data, payload->new_key, payload->old_keys, old_cnt);
    return data_frame_make(cmd, status, 0, NULL);
}
#endif

static data_frame_tx_t *cmd_processor_ioprox_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_IOPROX ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_IOPROX)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);  // no data in slot, don't send garbage
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_IOPROX);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_IOPROX_TAG_ID_SIZE, buffer->buffer);
}

static data_frame_tx_t *cmd_processor_viking_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_VIKING, LF_VIKING_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_viking_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_VIKING ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_VIKING)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);  // no data in slot, don't send garbage
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_VIKING);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_VIKING_TAG_ID_SIZE, buffer->buffer);
}

static data_frame_tx_t *cmd_processor_pac_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_PAC, LF_PAC_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_pac_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_PAC ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_PAC)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_PAC);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_PAC_TAG_ID_SIZE, buffer->buffer);
}

static data_frame_tx_t *cmd_processor_jablotron_set_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return set_lf_emulator_id(cmd, TAG_TYPE_JABLOTRON, LF_JABLOTRON_TAG_ID_SIZE,
                              length, data);
}

static data_frame_tx_t *cmd_processor_jablotron_get_emu_id(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t tag_types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);
    if (tag_types.tag_lf != TAG_TYPE_JABLOTRON ||
            !tag_emulation_is_active_type_loaded(TAG_TYPE_JABLOTRON)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, data);
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_JABLOTRON);
    return data_frame_make(cmd, STATUS_SUCCESS, LF_JABLOTRON_TAG_ID_SIZE, buffer->buffer);
}

static bool is_mf1_emulator_type(tag_specific_type_t type) {
    return type == TAG_TYPE_MIFARE_Mini || type == TAG_TYPE_MIFARE_1024 ||
           type == TAG_TYPE_MIFARE_2048 || type == TAG_TYPE_MIFARE_4096;
}

static void active_slot_snapshot_encode_u32(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static data_frame_tx_t *cmd_processor_active_slot_snapshot(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (data == NULL || length < 2u || data[0] != ACTIVE_SLOT_SNAPSHOT_VERSION ||
            data[1] > ACTIVE_SLOT_SNAPSHOT_OP_ABORT) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    data_frame_transport_t transport = data_frame_get_transport();
    uint8_t operation = data[1];
    if (operation == ACTIVE_SLOT_SNAPSHOT_OP_BEGIN) {
        if (length != ACTIVE_SLOT_SNAPSHOT_BEGIN_REQUEST_SIZE) {
            return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
        }
        if (m_active_slot_snapshot.active || transport == DATA_FRAME_TRANSPORT_NONE ||
                get_device_mode() != DEVICE_MODE_TAG) {
            return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
        }

        uint8_t slot;
        tag_specific_type_t tag_type;
        uint32_t owner_generation;
        uint32_t revision = active_slot_snapshot_new_revision();
        if (!tag_emulation_snapshot_begin(&slot, &tag_type, &owner_generation) ||
                owner_generation == 0u) {
            return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
        }
        if (!active_slot_snapshot_transaction_begin(
                    &m_active_slot_snapshot, (uint8_t)transport, slot,
                    (uint16_t)tag_type, revision, app_timer_cnt_get())) {
            tag_emulation_snapshot_release();
            return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
        }

        uint8_t response[ACTIVE_SLOT_SNAPSHOT_BEGIN_RESPONSE_SIZE] = {
            ACTIVE_SLOT_SNAPSHOT_VERSION,
            ACTIVE_SLOT_SNAPSHOT_OP_BEGIN,
            slot,
            (uint8_t)((uint16_t)tag_type >> 8),
            (uint8_t)tag_type,
        };
        active_slot_snapshot_encode_u32(&response[5], owner_generation);
        active_slot_snapshot_encode_u32(&response[9], revision);
        return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
    }

    if (length != ACTIVE_SLOT_SNAPSHOT_END_REQUEST_SIZE) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t revision = cmd_read_u32be(&data[2]);
    if (!active_slot_snapshot_transaction_matches(
                &m_active_slot_snapshot, (uint8_t)transport, revision)) {
        return data_frame_make(cmd, m_active_slot_snapshot.active
                               ? STATUS_CMD_ERR : STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }

    if (operation == ACTIVE_SLOT_SNAPSHOT_OP_SAVE_RELEASE) {
        uint32_t now = app_timer_cnt_get();
        uint32_t absolute_elapsed = app_timer_cnt_diff_compute(
                                        now, m_active_slot_snapshot.started_at);
        if (!active_slot_snapshot_transaction_begin_commit(
                    &m_active_slot_snapshot, (uint8_t)transport, absolute_elapsed,
                    APP_TIMER_TICKS(ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS),
                    APP_TIMER_TICKS(ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS))) {
            return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
        }
        tag_snapshot_save_result_t result = tag_emulation_snapshot_save(
                                                m_active_slot_snapshot.slot,
                                                (tag_specific_type_t)m_active_slot_snapshot.tag_type);
        active_slot_snapshot_transaction_finish_commit(
            &m_active_slot_snapshot, (uint8_t)transport, app_timer_cnt_get());
        if (result == TAG_SNAPSHOT_SAVE_FLASH_FAIL) {
            return data_frame_make(cmd, STATUS_FLASH_WRITE_FAIL, 0, NULL);
        }
        if (result != TAG_SNAPSHOT_SAVE_OK) {
            return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
        }
    }

    active_slot_snapshot_transaction_clear(&m_active_slot_snapshot);
    tag_emulation_snapshot_release();
    uint8_t response[ACTIVE_SLOT_SNAPSHOT_END_RESPONSE_SIZE] = {
        ACTIVE_SLOT_SNAPSHOT_VERSION,
        operation,
    };
    active_slot_snapshot_encode_u32(&response[2], revision);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(response), response);
}

static bool is_mf0_ntag_emulator_type(tag_specific_type_t type) {
    switch (type) {
        case TAG_TYPE_MF0ICU1:
        case TAG_TYPE_MF0ICU2:
        case TAG_TYPE_MF0UL11:
        case TAG_TYPE_MF0UL21:
        case TAG_TYPE_NTAG_210:
        case TAG_TYPE_NTAG_212:
        case TAG_TYPE_NTAG_213:
        case TAG_TYPE_NTAG_215:
        case TAG_TYPE_NTAG_216:
            return true;
        default:
            return false;
    }
}

static data_frame_tx_t *before_hf_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &types);
    if (!tag_emulation_is_active_type_loaded(types.tag_hf)) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    return NULL;
}

static data_frame_tx_t *before_mf1_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &types);
    if (!is_mf1_emulator_type(types.tag_hf) ||
            !tag_emulation_is_active_type_loaded(types.tag_hf)) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    return NULL;
}

static data_frame_tx_t *before_mf0_ntag_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    tag_slot_specific_type_t types;
    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &types);
    if (!is_mf0_ntag_emulator_type(types.tag_hf) ||
            !tag_emulation_is_active_type_loaded(types.tag_hf)) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    return NULL;
}

#if defined(PROJECT_CHAMELEON_ULTRA)
static data_frame_tx_t *before_hf14a_4_emulator_loaded(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!tag_emulation_is_active_type_loaded(TAG_TYPE_HF14A_4)) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    return NULL;
}
#endif

static nfc_tag_14a_coll_res_reference_t *get_coll_res_data(bool write) {
    nfc_tag_14a_coll_res_reference_t *info;
    tag_slot_specific_type_t tag_types;

    tag_emulation_get_specific_types_by_slot(tag_emulation_get_slot(), &tag_types);

    switch (tag_types.tag_hf) {
        case TAG_TYPE_MIFARE_1024:
        case TAG_TYPE_MIFARE_2048:
        case TAG_TYPE_MIFARE_4096:
        case TAG_TYPE_MIFARE_Mini:
            info = write ? get_mifare_coll_res() : get_saved_mifare_coll_res();
            break;
        case TAG_TYPE_MF0ICU1:
        case TAG_TYPE_MF0ICU2:
        case TAG_TYPE_MF0UL11:
        case TAG_TYPE_MF0UL21:
        case TAG_TYPE_NTAG_210:
        case TAG_TYPE_NTAG_212:
        case TAG_TYPE_NTAG_213:
        case TAG_TYPE_NTAG_215:
        case TAG_TYPE_NTAG_216:
            info = nfc_tag_mf0_ntag_get_coll_res();
            break;
        case TAG_TYPE_HF14A_4:
            info = nfc_tag_14a_4_get_coll_res();
            break;
        default:
            // no collision resolution data for slot
            info = NULL;
            break;
    }

    return info;
}

static data_frame_tx_t *cmd_processor_hf14a_get_anti_coll_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    nfc_tag_14a_coll_res_reference_t *info = get_coll_res_data(false);

    if (info == NULL) return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
    if (info->size == NULL || info->uid == NULL || info->atqa == NULL ||
            info->sak == NULL || info->ats == NULL ||
            !is_valid_uid_size(*info->size)) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }

    // uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
    // dynamic length, so no struct
    uint8_t payload[1 + 10 + 2 + 1 + 1 + sizeof(info->ats->data)];
    uint16_t payload_length = 1u + (uint16_t)*info->size + 2u + 1u + 1u +
                              info->ats->length;
    if (payload_length > sizeof(payload)) {
        return data_frame_make(cmd, STATUS_CMD_ERR, 0, NULL);
    }
    uint16_t offset = 0;
    payload[offset++] = *info->size;
    memcpy(&payload[offset], info->uid, *info->size);
    offset += *info->size;
    memcpy(&payload[offset], info->atqa, 2);
    offset += 2;
    payload[offset++] = *info->sak;
    if (info->ats->length > 0) {
        payload[offset++] = info->ats->length;
        memcpy(&payload[offset], info->ats->data, info->ats->length);
        offset += info->ats->length;
    } else {
        payload[offset++] = 0;
    }
    return data_frame_make(cmd, STATUS_SUCCESS, offset, payload);
}

static data_frame_tx_t *cmd_processor_mf1_set_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Clear the on-device log only when *enabling* detection (i.e. starting a
    // fresh capture session). This previously ran on disable too, which wiped a
    // completed capture the instant the host stopped detection, forcing the host
    // to read results before disabling. Clearing only on enable makes Stop
    // non-destructive; a subsequent enable still starts fresh.
    if (data[0]) {
        nfc_tag_mf1_detection_log_clear();
    }
    nfc_tag_mf1_set_detection_enable(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t is_enable = nfc_tag_mf1_is_detection_enable();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, (uint8_t *)(&is_enable));
}

static data_frame_tx_t *cmd_processor_mf1_get_detection_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint32_t count = nfc_tag_mf1_detection_log_count();
    if (count == 0xFFFFFFFF) {
        count = 0;
    }
    uint32_t payload = U32HTONL(count);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(uint32_t), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_mf1_get_detection_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint32_t count;
    uint32_t index;
    uint8_t *resp = NULL;
    nfc_tag_mf1_auth_log_t *logs = mf1_get_auth_log(&count);
    if (length != 4 || count == 0xFFFFFFFF) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    index = cmd_read_u32be(data);
    // NRF_LOG_INFO("index = %d", index);
    if (index >= count) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    resp = (uint8_t *)(logs + index);
    length = MIN(count - index, NETDATA_MAX_DATA_LENGTH / sizeof(nfc_tag_mf1_auth_log_t)) * sizeof(nfc_tag_mf1_auth_log_t);
    return data_frame_make(cmd, STATUS_SUCCESS, length, resp);
}

static data_frame_tx_t *cmd_processor_mf1_write_emu_block_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 1u + NFC_TAG_MF1_DATA_SIZE ||
            (((length - 1u) % NFC_TAG_MF1_DATA_SIZE) != 0u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t block_index = data[0];
    uint8_t block_count = (length - 1) / NFC_TAG_MF1_DATA_SIZE;
    if (block_index + block_count > NFC_TAG_MF1_BLOCK_MAX) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_MIFARE_4096);
    nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)buffer->buffer;
    for (int i = 1, j = block_index; i < length; i += NFC_TAG_MF1_DATA_SIZE, j++) {
        uint8_t *p_block = &data[i];
        memcpy(info->memory[j], p_block, NFC_TAG_MF1_DATA_SIZE);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_read_emu_block_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if ((length != 2) || (data[1] < 1) || (data[1] > 32) || (data[0] + data[1] > NFC_TAG_MF1_BLOCK_MAX)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t block_index = data[0];
    uint8_t block_count = data[1];
    tag_data_buffer_t *buffer = get_buffer_by_tag_type(TAG_TYPE_MIFARE_4096);
    nfc_tag_mf1_information_t *info = (nfc_tag_mf1_information_t *)buffer->buffer;
    uint16_t result_length = block_count * NFC_TAG_MF1_DATA_SIZE;
    uint8_t result_buffer[result_length];
    for (int i = 0, j = block_index; i < result_length; i += NFC_TAG_MF1_DATA_SIZE, j++) {
        memcpy(&result_buffer[i], info->memory[j], NFC_TAG_MF1_DATA_SIZE);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, result_length, result_buffer);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_write_emu_page_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t byte;
    uint8_t active_slot = tag_emulation_get_slot();

    tag_slot_specific_type_t active_slot_tag_types;
    tag_emulation_get_specific_types_by_slot(active_slot, &active_slot_tag_types);

    int nr_pages = nfc_tag_mf0_ntag_get_nr_pages_by_tag_type(active_slot_tag_types.tag_hf);
    // This means wrong slot type.
    if (nr_pages <= 0) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, data);

    if (length < 2) {
        byte = nr_pages;
        return data_frame_make(cmd, STATUS_PAR_ERR, 1, &byte);
    }

    int page_index = data[0];
    int pages_count = data[1];
    int byte_length = (int)pages_count * NFC_TAG_MF0_NTAG_DATA_SIZE;

    if (pages_count == 0) {
        return data_frame_make(cmd, length == 2u ? STATUS_SUCCESS : STATUS_PAR_ERR, 0, NULL);
    }
    else if (
        (page_index >= ((int)nr_pages))
        || (pages_count > (((int)nr_pages) - page_index))
        || (((int)length - 2) != byte_length)
    ) {
        byte = nr_pages;
        return data_frame_make(cmd, STATUS_PAR_ERR, 1, &byte);
    }

    tag_data_buffer_t *buffer = get_buffer_by_tag_type(active_slot_tag_types.tag_hf);
    nfc_tag_mf0_ntag_information_t *info = (nfc_tag_mf0_ntag_information_t *)buffer->buffer;

    memcpy(&info->memory[page_index][0], &data[2], byte_length);

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_emu_page_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t byte;
    uint8_t active_slot = tag_emulation_get_slot();

    tag_slot_specific_type_t active_slot_tag_types;
    tag_emulation_get_specific_types_by_slot(active_slot, &active_slot_tag_types);

    int nr_pages = nfc_tag_mf0_ntag_get_nr_pages_by_tag_type(active_slot_tag_types.tag_hf);
    // This means wrong slot type.
    if (nr_pages <= 0) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, data);

    // Convert the int value to u8 value if it's valid.
    byte = nr_pages;
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &byte);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_read_emu_page_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t byte;
    uint8_t active_slot = tag_emulation_get_slot();

    tag_slot_specific_type_t active_slot_tag_types;
    tag_emulation_get_specific_types_by_slot(active_slot, &active_slot_tag_types);

    int nr_pages = nfc_tag_mf0_ntag_get_nr_pages_by_tag_type(active_slot_tag_types.tag_hf);
    // This means wrong slot type.
    if (nr_pages <= 0) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, data);

    if (length < 2) {
        byte = nr_pages;
        return data_frame_make(cmd, STATUS_PAR_ERR, 1, &byte);
    }

    int page_index = data[0];
    int pages_count = data[1];

    if (pages_count == 0) return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
    // Bounds-check page_index/pages_count against the slot's page count, just
    // like the write sibling — otherwise this reads (and returns) RAM past the
    // slot's memory[] buffer (remote info-disclosure over BLE/USB).
    else if (
        (page_index >= ((int)nr_pages))
        || (pages_count > (((int)nr_pages) - page_index))
    ) {
        byte = nr_pages;
        return data_frame_make(cmd, STATUS_PAR_ERR, 1, &byte);
    }

    tag_data_buffer_t *buffer = get_buffer_by_tag_type(active_slot_tag_types.tag_hf);
    nfc_tag_mf0_ntag_information_t *info = (nfc_tag_mf0_ntag_information_t *)buffer->buffer;

    return data_frame_make(cmd, STATUS_SUCCESS, pages_count * NFC_TAG_MF0_NTAG_DATA_SIZE, &info->memory[page_index][0]);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_version_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t *version_data = nfc_tag_mf0_ntag_get_version_data();
    if (version_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);

    return data_frame_make(cmd, STATUS_SUCCESS, NFC_TAG_MF0_NTAG_VER_SIZE, version_data);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_version_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != NFC_TAG_MF0_NTAG_VER_SIZE) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t *version_data = nfc_tag_mf0_ntag_get_version_data();
    if (version_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    memcpy(version_data, data, NFC_TAG_MF0_NTAG_VER_SIZE);

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_signature_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t *signature_data = nfc_tag_mf0_ntag_get_signature_data();
    if (signature_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);

    return data_frame_make(cmd, STATUS_SUCCESS, NFC_TAG_MF0_NTAG_SIG_SIZE, signature_data);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_signature_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != NFC_TAG_MF0_NTAG_SIG_SIZE) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t *signature_data = nfc_tag_mf0_ntag_get_signature_data();
    if (signature_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    memcpy(signature_data, data, NFC_TAG_MF0_NTAG_SIG_SIZE);

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_counter_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t index = data[0] & 0x7F;
    uint8_t *counter_data = nfc_tag_mf0_ntag_get_counter_data_by_index(index);
    if (counter_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);

    bool tearing = (counter_data[MF0_NTAG_AUTHLIM_OFF_IN_CTR] &
                    MF0_NTAG_TEARING_MASK_IN_AUTHLIM) != 0;

    uint8_t response[4];
    memcpy(response, counter_data, 3);
    response[3] = tearing ? 0x00 : 0xBD;

    return data_frame_make(cmd, STATUS_SUCCESS, 4, response);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_counter_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 4) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);

    uint8_t index = data[0] & 0x7F;
    uint8_t *counter_data = nfc_tag_mf0_ntag_get_counter_data_by_index(index);
    if (counter_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);

    // clear tearing event flag
    if ((data[0] & 0x80) == 0x80) {
        counter_data[MF0_NTAG_AUTHLIM_OFF_IN_CTR] &= ~MF0_NTAG_TEARING_MASK_IN_AUTHLIM;
    }

    // copy the actual counter value
    memcpy(counter_data, &data[1], 3);

    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_reset_auth_cnt(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    // all tags with counters support auth
    uint8_t *counter_data = nfc_tag_mf0_ntag_get_counter_data_by_index(0);
    if (counter_data == NULL) return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);

    uint8_t old_value = counter_data[MF0_NTAG_AUTHLIM_OFF_IN_CTR] & MF0_NTAG_AUTHLIM_MASK_IN_CTR;
    counter_data[MF0_NTAG_AUTHLIM_OFF_IN_CTR] &= ~MF0_NTAG_AUTHLIM_MASK_IN_CTR;

    return data_frame_make(cmd, STATUS_SUCCESS, 1, &old_value);
}

static data_frame_tx_t *cmd_processor_hf14a_set_anti_coll_data(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    // uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
    // dynamic length, so no struct
    cmd_hf14a_anticoll_t parsed;
    if (!cmd_parse_hf14a_anticoll(length, data, &parsed)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_14a_coll_res_reference_t *info = get_coll_res_data(true);
    if (info == NULL || info->size == NULL || info->uid == NULL ||
            info->atqa == NULL || info->sak == NULL || info->ats == NULL) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }

    *(info->size) = (nfc_tag_14a_uid_size)parsed.uid_len;
    memcpy(info->uid, &data[parsed.uid_offset], parsed.uid_len);
    memcpy(info->atqa, &data[parsed.atqa_offset], 2);
    info->sak[0] = data[parsed.sak_offset];
    info->ats->length = parsed.ats_len;
    memcpy(info->ats->data, &data[parsed.ats_offset], parsed.ats_len);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

#define TAG_NICK_MAX_LENGTH 32u

static data_frame_tx_t *cmd_processor_set_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 2u || length > 2u + TAG_NICK_MAX_LENGTH || data == NULL) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t slot = data[0];
    uint8_t sense_type = data[1];
    fds_slot_record_map_t map_info;
    if (slot >= TAG_MAX_SLOT_NUM || (sense_type != TAG_SENSE_HF && sense_type != TAG_SENSE_LF)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    get_fds_map_by_slot_sense_type_for_nick(slot, sense_type, &map_info);

    uint8_t buffer[36] = {0};
    buffer[0] = length - 2;
    memcpy(buffer + 1, data + 2, buffer[0]);

    bool ret = fds_write_sync(map_info.id, map_info.key, sizeof(buffer), buffer);
    if (!ret) {
        return data_frame_make(cmd, STATUS_FLASH_WRITE_FAIL, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t slot = data[0];
    uint8_t sense_type = data[1];
    uint8_t buffer[36] = {0};
    fds_slot_record_map_t map_info;

    if (slot >= TAG_MAX_SLOT_NUM || (sense_type != TAG_SENSE_HF && sense_type != TAG_SENSE_LF)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    get_fds_map_by_slot_sense_type_for_nick(slot, sense_type, &map_info);
    uint16_t buffer_length = sizeof(buffer);
    bool ret = fds_read_sync(map_info.id, map_info.key, &buffer_length, buffer);
    if (!ret || buffer_length < 1u || buffer[0] > TAG_NICK_MAX_LENGTH ||
            buffer[0] > buffer_length - 1u) {
        return data_frame_make(cmd, STATUS_FLASH_READ_FAIL, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, buffer[0], &buffer[1]);
}

static data_frame_tx_t *cmd_processor_get_all_slot_nicks(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t response_buffer[TAG_MAX_SLOT_NUM * 2 * (1 + TAG_NICK_MAX_LENGTH)];
    uint16_t response_length = 0;

    for (uint8_t slot = 0; slot < TAG_MAX_SLOT_NUM; slot++) {
        uint8_t hf_buffer[36] = {0};
        fds_slot_record_map_t hf_map_info;
        get_fds_map_by_slot_sense_type_for_nick(slot, TAG_SENSE_HF, &hf_map_info);
        uint16_t hf_buffer_length = sizeof(hf_buffer);
        bool hf_ret = fds_read_sync(hf_map_info.id, hf_map_info.key, &hf_buffer_length, hf_buffer);

        if (hf_ret && hf_buffer_length > 0u && hf_buffer[0] <= TAG_NICK_MAX_LENGTH &&
                hf_buffer[0] <= hf_buffer_length - 1u) {
            response_buffer[response_length++] = hf_buffer[0];
            for (uint8_t i = 1; i <= hf_buffer[0] && i < hf_buffer_length; i++) {
                response_buffer[response_length++] = hf_buffer[i];
            }
        } else {
            response_buffer[response_length++] = 0;
        }

        uint8_t lf_buffer[36] = {0};
        fds_slot_record_map_t lf_map_info;
        get_fds_map_by_slot_sense_type_for_nick(slot, TAG_SENSE_LF, &lf_map_info);
        uint16_t lf_buffer_length = sizeof(lf_buffer);
        bool lf_ret = fds_read_sync(lf_map_info.id, lf_map_info.key, &lf_buffer_length, lf_buffer);

        if (lf_ret && lf_buffer_length > 0u && lf_buffer[0] <= TAG_NICK_MAX_LENGTH &&
                lf_buffer[0] <= lf_buffer_length - 1u) {
            response_buffer[response_length++] = lf_buffer[0];
            for (uint8_t i = 1; i <= lf_buffer[0] && i < lf_buffer_length; i++) {
                response_buffer[response_length++] = lf_buffer[i];
            }
        } else {
            response_buffer[response_length++] = 0;
        }
    }

    return data_frame_make(cmd, STATUS_SUCCESS, response_length, response_buffer);
}

static data_frame_tx_t *cmd_processor_delete_slot_tag_nick(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t slot = data[0];
    uint8_t sense_type = data[1];
    fds_slot_record_map_t map_info;

    if (slot >= TAG_MAX_SLOT_NUM || (sense_type != TAG_SENSE_HF && sense_type != TAG_SENSE_LF)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    get_fds_map_by_slot_sense_type_for_nick(slot, sense_type, &map_info);
    (void)fds_delete_sync(map_info.id, map_info.key);
    if (fds_util_last_error() != NRF_SUCCESS) {
        return data_frame_make(cmd, STATUS_FLASH_WRITE_FAIL, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_emulator_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mf1_info[5] = {};
    mf1_info[0] = nfc_tag_mf1_is_detection_enable();
    mf1_info[1] = nfc_tag_mf1_is_gen1a_magic_mode();
    mf1_info[2] = nfc_tag_mf1_is_gen2_magic_mode();
    mf1_info[3] = nfc_tag_mf1_is_use_mf1_coll_res();
    mf1_info[4] = nfc_tag_mf1_get_write_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 5, mf1_info);
}

static data_frame_tx_t *cmd_processor_mf1_get_prng_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t type = nfc_tag_mf1_get_prng_type();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &type);
}

static data_frame_tx_t *cmd_processor_mf1_set_prng_type(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_prng_type(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &data[0]);  // echo back the set value
}

static data_frame_tx_t *cmd_processor_mf1_set_random_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_random_uid_mode(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_random_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t is_enable = nfc_tag_mf1_is_random_uid_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, (uint8_t *)(&is_enable));
}

static data_frame_tx_t *cmd_processor_mf1_set_reader_keys_anim(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    rgb_marquee_set_reader_keys_anim(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_gen1a_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mode = nfc_tag_mf1_is_gen1a_magic_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
}

static data_frame_tx_t *cmd_processor_mf1_set_gen1a_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_gen1a_magic_mode(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_gen2_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mode = nfc_tag_mf1_is_gen2_magic_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
}

static data_frame_tx_t *cmd_processor_mf1_set_gen2_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_gen2_magic_mode(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_block_anti_coll_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mode = nfc_tag_mf1_is_use_mf1_coll_res();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
}

static data_frame_tx_t *cmd_processor_mf1_set_block_anti_coll_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_use_mf1_coll_res(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mode = nfc_tag_mf1_get_write_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
}

static data_frame_tx_t *cmd_processor_mf1_set_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 3) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_write_mode(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf1_get_field_off_do_reset(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t enable = nfc_tag_mf1_is_field_off_do_reset();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &enable);
}

static data_frame_tx_t *cmd_processor_mf1_set_field_off_do_reset(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] >= 2) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf1_set_field_off_do_reset(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_get_enabled_slots(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    struct {
        uint8_t enabled_hf;
        uint8_t enabled_lf;
    } PACKED payload[8];
    for (uint8_t slot = 0; slot < 8; slot++) {
        payload[slot].enabled_hf = is_slot_enabled(slot, TAG_SENSE_HF);
        payload[slot].enabled_lf = is_slot_enabled(slot, TAG_SENSE_LF);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_get_ble_connect_key(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    return data_frame_make(cmd, STATUS_SUCCESS, BLE_PAIRING_KEY_LEN, settings_get_ble_connect_key());
}

static data_frame_tx_t *cmd_processor_set_ble_connect_key(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != BLE_PAIRING_KEY_LEN) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Must be 6 ASCII characters, can only be 0-9.
    bool is_valid_key = true;
    for (uint8_t i = 0; i < BLE_PAIRING_KEY_LEN; i++) {
        if (data[i] < '0' || data[i] > '9') {
            is_valid_key = false;
            break;
        }
    }
    if (!is_valid_key) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    // Key is valid, we can update to config
    settings_set_ble_connect_key(data);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_delete_all_ble_bonds(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    advertising_stop();
    delete_bonds_all();
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

#if defined(PROJECT_CHAMELEON_ULTRA)


/**
 * before reader run, reset reader and on antenna,
 * we must to wait some time, to init picc(power).
 */
static data_frame_tx_t *before_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    device_mode_t mode = get_device_mode();
    if (mode != DEVICE_MODE_READER) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    return NULL;
}


/**
 * before reader run, reset reader and on antenna,
 * we must to wait some time, to init picc(power).
 */
static data_frame_tx_t *before_hf_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    data_frame_tx_t *ret = before_reader_run(cmd, status, length, data);
    if (ret == NULL) {
        iso_dep_session_abort();
        pcd_14a_reader_reset();
        pcd_14a_reader_antenna_on();
        bsp_delay_ms(8);
    }
    return ret;
}

/**
 * after reader run, off antenna, to keep battery.
 */
static data_frame_tx_t *after_hf_reader_run(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    pcd_14a_reader_antenna_off();
    return NULL;
}

#endif

// fct will be defined after m_data_cmd_map because we need to know its size
data_frame_tx_t *cmd_processor_get_device_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data);

static data_frame_tx_t *cmd_processor_mf0_ntag_get_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    int rc = nfc_tag_mf0_ntag_get_uid_mode();
    if (rc < 0) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    else {
        uint8_t mode = rc;
        return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
    }
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_uid_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    bool enabled;
    if (!cmd_payload_exact(length, data, 1u) || !cmd_parse_bool(data[0], &enabled) ||
            !nfc_tag_mf0_ntag_set_uid_mode(enabled)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mode = nfc_tag_mf0_ntag_get_write_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, &mode);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_write_mode(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 4) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_mf0_ntag_set_write_mode(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_set_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 1 || data[0] > 1) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (data[0] != 0u) nfc_tag_mf0_ntag_detection_log_clear();
    nfc_tag_mf0_ntag_set_detection_enable(data[0]);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_enable(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t is_enable = nfc_tag_mf0_ntag_is_detection_enable();
    return data_frame_make(cmd, STATUS_SUCCESS, 1, (uint8_t *)(&is_enable));
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_count(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint32_t count = nfc_tag_mf0_ntag_detection_log_count();
    if (count == 0xFFFFFFFF) {
        count = 0;
    }
    uint32_t payload = U32HTONL(count);
    return data_frame_make(cmd, STATUS_SUCCESS, sizeof(uint32_t), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_mf0_ntag_get_detection_log(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint32_t count;
    uint32_t index;
    uint8_t *resp = NULL;
    nfc_tag_mf0_ntag_auth_log_t *logs = mf0_get_auth_log(&count);
    if (length != 4 || count == 0xFFFFFFFF) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    index = cmd_read_u32be(data);
    if (index >= count) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    resp = (uint8_t *)(logs + index);
    length = MIN(count - index, NETDATA_MAX_DATA_LENGTH / sizeof(nfc_tag_mf0_ntag_auth_log_t)) * sizeof(nfc_tag_mf0_ntag_auth_log_t);
    return data_frame_make(cmd, STATUS_SUCCESS, length, resp);
}

static data_frame_tx_t *cmd_processor_mf0_get_emulator_config(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t mf0_info[3] = {};
    mf0_info[0] = nfc_tag_mf0_ntag_is_detection_enable();
    mf0_info[1] = nfc_tag_mf0_ntag_get_uid_mode();
    mf0_info[2] = nfc_tag_mf0_ntag_get_write_mode();
    return data_frame_make(cmd, STATUS_SUCCESS, 3, mf0_info);
}

/**
 * (cmd -> processor) function map, the map struct is:
 *       cmd code                               before process               cmd processor                                after process
 */
#if defined(PROJECT_CHAMELEON_ULTRA)
static data_frame_tx_t *cmd_processor_em4x05_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    em4x05_data_t tag = {0};
    status = scan_em4x05(&tag);
    if (status != STATUS_LF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    struct {
        uint32_t config;
        uint32_t uid;
        uint32_t uid_hi;
        uint8_t  is_em4x69;
    } PACKED payload;
    payload.config    = U32HTONL(tag.config);
    payload.uid       = U32HTONL(tag.uid);
    payload.uid_hi    = U32HTONL(tag.uid_hi);
    payload.is_em4x69 = tag.is_em4x69 ? 1 : 0;
    return data_frame_make(cmd, STATUS_LF_TAG_OK, sizeof(payload), (uint8_t *)&payload);
}

static data_frame_tx_t *cmd_processor_lf_sniff(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    /* Optional 2-byte big-endian timeout in ms from host (default 2000ms) */
    uint32_t timeout_ms = 2000;
    if (length >= 2) {
        timeout_ms = ((uint32_t)data[0] << 8) | data[1];
        if (timeout_ms == 0 || timeout_ms > 10000) timeout_ms = 2000;
    }

    static uint8_t sniff_buf[LF_SNIFF_MAX_SAMPLES];
    size_t outlen = 0;
    if (!raw_read_to_buffer(sniff_buf, LF_SNIFF_MAX_SAMPLES, timeout_ms, &outlen)) {
        return data_frame_make(cmd, STATUS_MEM_ERR, 0, NULL);
    }

    if (outlen == 0) {
        return data_frame_make(cmd, STATUS_LF_TAG_NO_FOUND, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_LF_TAG_OK, (uint16_t)outlen, sniff_buf);
}

/* ========================================================================
 * HF14A AUTH TRACE — full anticoll + Crypto1 auth flow, every frame returned.
 *
 * Performs a complete reader-side auth sequence against a real MIFARE Classic
 * tag and packs every wire frame (synthesized anticoll + actual auth) into the
 * same trace-buffer format used by `hf 14a sniff`, so the existing host-side
 * decoder can render the full exchange.
 *
 * Buffer format (identical to hf 14a sniff): [bits_be16][data...] per frame.
 * Bit15 of the bit-count header: 0 = reader→card, 1 = card→reader.
 *
 * Request payload: [type:1][block:1][key:6]   — 8 bytes total
 *   type:  PICC_AUTHENT1A (0x60) or PICC_AUTHENT1B (0x61)
 *   block: target block number
 *   key:   6-byte sector key
 *
 * Response payload: packed frame buffer, decoded host-side.
 * Status: STATUS_HF_TAG_OK if the auth completed (NT, NR||AR, AT all captured),
 *         STATUS_HF_TAG_NO if no card was found,
 *         STATUS_MF_ERR_AUTH if AT was wrong or missing (partial trace returned),
 *         STATUS_PAR_ERR on bad payload length.
 * ======================================================================== */
#define HF_AUTH_TRACE_BUF_SIZE 256

static uint8_t  m_auth_trace_buf[HF_AUTH_TRACE_BUF_SIZE];
static uint16_t m_auth_trace_len = 0;

/* Append one frame to the trace buffer, mirroring hf14a_sniff_store(). */
static void auth_trace_store(const uint8_t *data, uint16_t szBits, bool is_tx) {
    uint16_t szBytes = (szBits + 7) / 8;
    if (m_auth_trace_len + 2 + szBytes > HF_AUTH_TRACE_BUF_SIZE) return;
    uint16_t hdr = szBits | (is_tx ? 0x8000u : 0x0000u);
    m_auth_trace_buf[m_auth_trace_len++] = (hdr >> 8) & 0xFF;
    m_auth_trace_buf[m_auth_trace_len++] =  hdr        & 0xFF;
    memcpy(&m_auth_trace_buf[m_auth_trace_len], data, szBytes);
    m_auth_trace_len += szBytes;
}

/* Synthesize the anticoll/SELECT/SAK/RATS/ATS frames from a populated tag
 * struct. The wire frames during scan_auto are perfectly determined by the
 * tag descriptor, so we reconstruct them rather than tap rc522.c.
 *
 * For 4-byte UID (cascade=1): CL1 with full UID.
 * For 7-byte UID (cascade=2): CL1 with CT||UID0..2, CL2 with UID3..6.
 * For 10-byte UID (cascade=3): CL1, CL2, CL3.
 *
 * The "first SAK" for an incomplete UID has bit 2 set (cascade flag); we
 * synthesize it as 0x04 (cascade required, no further info), which matches
 * what every real cascading tag returns. Final SAK is the captured tag->sak.
 */
static void auth_trace_emit_anticoll(const picc_14a_tag_t *tag) {
    /* 1. REQA — 7 bits, reader→card */
    uint8_t reqa = PICC_REQIDL;
    auth_trace_store(&reqa, 7, false);

    /* 2. ATQA — 16 bits, card→reader */
    auth_trace_store(tag->atqa, 16, true);

    /* 3. Anticoll/SELECT cycles — one per cascade level */
    const uint8_t cascade_anticoll[3] = { PICC_ANTICOLL1, PICC_ANTICOLL2, PICC_ANTICOLL3 };
    uint8_t uid_pos = 0;
    for (uint8_t cl = 0; cl < tag->cascade; cl++) {
        bool is_last = (cl == (uint8_t)(tag->cascade - 1));

        /* Anticoll request: <SEL> 0x20 — 16 bits, reader→card */
        uint8_t anticoll[2] = { cascade_anticoll[cl], 0x20 };
        auth_trace_store(anticoll, 16, false);

        /* Anticoll response: CT||UID3 + BCC for non-last, or UID4 + BCC for last
         * with full UID inline. 5 bytes / 40 bits, card→reader. */
        uint8_t resp[5];
        if (is_last) {
            /* Last level — 4 bytes of UID at the end of tag->uid */
            memcpy(resp, &tag->uid[uid_pos], 4);
        } else {
            /* Cascading — CT (0x88) + 3 bytes UID */
            resp[0] = 0x88;
            memcpy(&resp[1], &tag->uid[uid_pos], 3);
            uid_pos += 3;
        }
        resp[4] = resp[0] ^ resp[1] ^ resp[2] ^ resp[3];  /* BCC */
        auth_trace_store(resp, 40, true);

        /* SELECT: <SEL> 0x70 + 5-byte CT/UID/BCC + 2-byte CRC = 9 bytes / 72 bits */
        uint8_t sel[9] = { cascade_anticoll[cl], 0x70 };
        memcpy(&sel[2], resp, 5);
        crc_14a_calculate(sel, 7, &sel[7]);
        auth_trace_store(sel, 72, false);

        /* SAK: 1 byte SAK + 2-byte CRC = 24 bits, card→reader.
         * Intermediate cascades: synthesize SAK=0x04 (cascade bit set, generic).
         * Last cascade: real tag->sak. */
        uint8_t sak_frame[3];
        sak_frame[0] = is_last ? tag->sak : 0x04;
        crc_14a_calculate(sak_frame, 1, &sak_frame[1]);
        auth_trace_store(sak_frame, 24, true);
    }

    /* 4. RATS / ATS — only if scan_auto did RATS and the tag responded */
    if (tag->ats_len > 0) {
        /* RATS request: 0xE0 0x40 + 2-byte CRC = 4 bytes / 32 bits, reader→card.
         * 0x40 = FSDI=4 (FSD=48), CID=0 — same as pcd_14a_reader_ats_request(). */
        uint8_t rats[4] = { PICC_RATS, 0x40 };
        crc_14a_calculate(rats, 2, &rats[2]);
        auth_trace_store(rats, 32, false);

        /* ATS response: ats_len bytes (CRC was stripped by rc522 layer) +
         * recomputed 2-byte CRC, card→reader. */
        if ((uint16_t)tag->ats_len + 2 <= 64) {
            uint8_t ats_frame[64];
            memcpy(ats_frame, tag->ats, tag->ats_len);
            crc_14a_calculate(ats_frame, tag->ats_len, &ats_frame[tag->ats_len]);
            auth_trace_store(ats_frame, (tag->ats_len + 2) * 8, true);
        }
    }
}

/* Software-side MIFARE Classic auth tap. Mirrors mf1_toolbox.c::authex() but
 * stores every TX/RX frame into the trace buffer instead of just returning a
 * status. Fixed reader nonce 12345678 is used (same as authex). */
static uint8_t auth_trace_do_auth(picc_14a_tag_t *tag, uint8_t type, uint8_t blockNo, const uint8_t *key6) {
    struct Crypto1State pcs = { 0, 0 };
    static const uint8_t nr[4] = { 0x12, 0x34, 0x56, 0x78 };  /* fixed reader nonce */
    uint8_t  par[8]            = { 0 };
    uint8_t  mf_nr_ar[8]       = { 0 };
    uint8_t  answer[8]         = { 0 };
    uint8_t  parity_resp[8]    = { 0 };
    uint16_t len               = 0;
    uint8_t  status;

    /* AUTH command frame: [type, blockNo, CRC, CRC] — 32 bits, reader→card */
    uint8_t auth_cmd[4] = { type, blockNo };
    crc_14a_calculate(auth_cmd, 2, &auth_cmd[2]);
    auth_trace_store(auth_cmd, 32, false);

    /* Send AUTH, expect 4-byte NT (plaintext on first auth). We use the same
     * primitive as authex's send_cmd() under the non-encrypted path. */
    pcd_14a_reader_bytes_transfer(PCD_TRANSCEIVE, auth_cmd, 4, answer, &len, U8ARR_BIT_LEN(answer));
    if (len != 32) {
        return STATUS_HF_ERR_STAT;  /* no NT — partial trace already in buffer */
    }

    /* Capture NT — 32 bits, card→reader */
    auth_trace_store(answer, 32, true);
    uint32_t nt = ((uint32_t)answer[0] << 24) | ((uint32_t)answer[1] << 16)
                  | ((uint32_t)answer[2] <<  8) |  (uint32_t)answer[3];

    /* Initialise Crypto1 with key (LSB-first, MIFARE convention) */
    uint64_t ui64Key = 0;
    for (int i = 0; i < 6; i++) ui64Key |= (uint64_t)key6[i] << ((5 - i) * 8);
    crypto1_init(&pcs, ui64Key);

    /* First auth: feed (nt ^ uid) as plaintext into the cipher */
    uint32_t uid32 = get_u32_tag_uid(tag);
    crypto1_word(&pcs, nt ^ uid32, 0);

    /* Encrypt NR + parity */
    for (int pos = 0; pos < 4; pos++) {
        mf_nr_ar[pos] = crypto1_byte(&pcs, nr[pos], 0) ^ nr[pos];
        par[pos]      = filter(pcs.odd) ^ oddparity8(nr[pos]);
    }
    /* Compute AR = prng_successor(nt, 32) and encrypt byte-by-byte */
    uint32_t nt_succ = prng_successor(nt, 32);
    for (int pos = 4; pos < 8; pos++) {
        nt_succ = prng_successor(nt_succ, 8);
        mf_nr_ar[pos] = crypto1_byte(&pcs, 0x00, 0) ^ (nt_succ & 0xFF);
        par[pos]      = filter(pcs.odd) ^ oddparity8(nt_succ & 0xFF);
    }

    /* Capture NR||AR (encrypted) — 64 bits, reader→card. Stored before the
     * actual transfer so we still have the trace if the transfer fails. */
    auth_trace_store(mf_nr_ar, 64, false);

    /* Send NR||AR with manual parity, expect 4-byte AT */
    pcd_14a_reader_bits_transfer(mf_nr_ar, 64, par, answer, parity_resp, &len, U8ARR_BIT_LEN(answer));
    if (len != 32) {
        return STATUS_MF_ERR_AUTH;  /* no AT — auth was rejected, partial trace */
    }

    /* Capture AT — 32 bits, card→reader (still encrypted on the wire) */
    auth_trace_store(answer, 32, true);

    /* Verify AT == prng_successor(nt, 64) ^ ks3 */
    uint32_t at_recv = ((uint32_t)answer[0] << 24) | ((uint32_t)answer[1] << 16)
                       | ((uint32_t)answer[2] <<  8) |  (uint32_t)answer[3];
    uint32_t ntpp    = prng_successor(nt_succ, 32) ^ crypto1_word(&pcs, 0, 0);
    status = (ntpp == at_recv) ? STATUS_HF_TAG_OK : STATUS_MF_ERR_AUTH;

    /* Auth state may be left armed in the RC522 — clear it for safety so
     * subsequent reader operations start clean. */
    pcd_14a_reader_mf1_unauth();
    return status;
}

static data_frame_tx_t *cmd_processor_hf14a_auth_trace(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    /* Payload: [type:1][block:1][key:6][timeout_ms_be16:2] = 10 bytes.
     * Backward-compatible short form [type:1][block:1][key:6] = 8 bytes still
     * accepted; default 5000ms. */
    uint32_t timeout_ms = 5000;
    if (length == 10) {
        timeout_ms = ((uint32_t)data[8] << 8) | data[9];
        if (timeout_ms == 0 || timeout_ms > 30000) timeout_ms = 5000;
    } else if (length != 8) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint8_t type    = data[0];
    uint8_t block   = data[1];
    uint8_t *key    = &data[2];
    if (type != PICC_AUTHENT1A && type != PICC_AUTHENT1B) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    m_auth_trace_len = 0;

    /* Step 1: poll for a tag in the field until present or timeout.
     * scan_auto internally tries twice with REQA; if no card is present
     * each attempt fails fast (~3-5 ms) and we back off briefly between
     * iterations to keep the WDT happy and avoid hammering the RC522.
     * The antenna is already on (before_hf_reader_run hook) and stays on
     * for the whole polling window — same behaviour as a desktop reader. */
    picc_14a_tag_t tag;
    uint8_t scan_status = STATUS_HF_TAG_NO;
    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms)) {
        scan_status = pcd_14a_reader_scan_auto(&tag);
        if (scan_status == STATUS_HF_TAG_OK) {
            break;
        }
        /* 50 ms back-off between attempts — yields to USB/BLE main loop
         * and keeps the WDT fed (timeout is 5000 ms). */
        for (int i = 0; i < 50; i++) {
            bsp_delay_ms(1);
            bsp_wdt_feed();
        }
    }
    bsp_return_timer(p_at);

    if (scan_status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
    }

    /* Step 2: synthesize the wire frames that scan_auto just produced */
    auth_trace_emit_anticoll(&tag);

    /* Step 3: perform the auth, tapping every TX/RX into the same buffer */
    uint8_t auth_status = auth_trace_do_auth(&tag, type, block, key);

    /* Always return the buffer — even on auth failure, the partial trace is
     * useful for diagnosing why (wrong key vs. wrong block vs. no NT etc.). */
    if (m_auth_trace_len == 0) {
        return data_frame_make(cmd, auth_status, 0, NULL);
    }
    return data_frame_make(cmd, auth_status, m_auth_trace_len, m_auth_trace_buf);
}

#define HF_SNIFF_BUF_SIZE   3800   /* leave room for USB framing */
#define HF_SNIFF_MAX_FRAMES  200

static uint8_t  m_sniff_buf[HF_SNIFF_BUF_SIZE];
static uint16_t m_sniff_buf_len = 0;
static bool     m_sniff_active  = false;
static uint16_t m_sniff_cb_count = 0;   /* debug: total callback invocations */

/* Encode one frame into m_sniff_buf.
 * Format: [szBits_be16][data...]
 * Bit 15 of szBits: 0 = reader→card (RX), 1 = card→reader (TX).
 * Real szBits always < 512 so bit15 is always free in genuine frames.
 * Old parsers (bit15=0 for all frames) still work correctly. */
static void hf14a_sniff_store(const uint8_t *data, uint16_t szBits, bool is_tx) {
    uint16_t szBytes = (szBits + 7) / 8;
    if (m_sniff_buf_len + 2 + szBytes > HF_SNIFF_BUF_SIZE) return;
    uint16_t hdr = szBits | (is_tx ? 0x8000u : 0x0000u);
    m_sniff_buf[m_sniff_buf_len++] = (hdr >> 8) & 0xFF;
    m_sniff_buf[m_sniff_buf_len++] =  hdr        & 0xFF;
    memcpy(&m_sniff_buf[m_sniff_buf_len], data, szBytes);
    m_sniff_buf_len += szBytes;
}

static void hf14a_sniff_frame_cb(const uint8_t *data, uint16_t szBits,
                                 uint8_t flags) {
    (void)flags;
    m_sniff_cb_count++;   /* count even if buffer full or inactive */
    if (!m_sniff_active) return;
    hf14a_sniff_store(data, szBits, false);  /* reader→card */
}

static void hf14a_sniff_tx_frame_cb(const uint8_t *data, uint16_t szBits,
                                    uint8_t flags) {
    (void)flags;
    if (!m_sniff_active) return;
    hf14a_sniff_store(data, szBits, true);   /* card→reader */
}

static data_frame_tx_t *cmd_processor_hf14a_sniff(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (hf_capture_is_active()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    /* Optional 2-byte big-endian timeout in ms (default 5000ms) */
    uint32_t timeout_ms = 5000;
    if (length >= 2) {
        timeout_ms = ((uint32_t)data[0] << 8) | data[1];
        if (timeout_ms == 0 || timeout_ms > 30000) timeout_ms = 5000;
    }

    /* Reload active slot data before sniffing.
     * The NFCT anti-collision response is built from m_tag_information which
     * points into the shared tag data buffer. After a slot switch the buffer
     * may still contain the previous slot's UID if the FDS async load has not
     * completed. A forced reload here ensures the correct UID is presented
     * during the sniff session.
     * A short settle delay follows to allow the reload to complete before
     * the first field detection can trigger the anti-collision path. */
    tag_emulation_load_data();
    bsp_delay_ms(100);

    /* Install sniff callback into the already-running tag emulation stack.
     * Do NOT call tag_mode_enter() or sense_switch() here — those reinit
     * NFCT and wipe the anti-collision data, breaking the emulation.
     * The device must already be in emulator mode (hw mode --emulator)
     * with a slot active before running this command. */
    m_sniff_buf_len = 0;
    m_sniff_cb_count = 0;
    m_sniff_active  = true;
    /* passive mode intentionally disabled: CU acts as the card so it must
     * respond normally to the reader (ATQA/UID/SAK). TX sniff captures responses. */
    nfc_tag_14a_set_sniff_cb(hf14a_sniff_frame_cb);
    nfc_tag_14a_set_tx_sniff_cb(hf14a_sniff_tx_frame_cb);

    /* Wait for duration, yielding each ms so USB stack stays alive.
     * Feed watchdog every iteration — WDT timeout is 5000ms and the
     * main loop cannot feed it while we are blocking here. */
    autotimer *p_at = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_at, timeout_ms)) {
        bsp_delay_ms(1);
        bsp_wdt_feed();
    }
    bsp_return_timer(p_at);

    /* Remove callback and restore normal sense state */
    m_sniff_active = false;
    /* (passive mode was not enabled, nothing to restore) */
    nfc_tag_14a_clear_sniff_cb();
    nfc_tag_14a_clear_tx_sniff_cb();
    tag_emulation_sense_run();  /* restore slot-based sense state */

    if (m_sniff_buf_len == 0) {
        return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, m_sniff_buf_len, m_sniff_buf);
}

static uint8_t m_hf_capture_response[NETDATA_MAX_DATA_LENGTH];

static uint16_t hf_capture_result_status(hf_capture_result_t result) {
    switch (result) {
        case HF_CAPTURE_RESULT_OK: return STATUS_SUCCESS;
        case HF_CAPTURE_RESULT_BUSY: return STATUS_DEVICE_MODE_ERROR;
        case HF_CAPTURE_RESULT_SESSION: return STATUS_CMD_ERR;
        default: return STATUS_PAR_ERR;
    }
}

static data_frame_tx_t *cmd_processor_hf_capture_start(uint16_t cmd,
                                                        uint16_t status,
                                                        uint16_t length,
                                                        uint8_t *data) {
    (void)status;
    if (!cmd_payload_exact(length, data, 6u) ||
            data[0] != HF_CAPTURE_PROTOCOL_VERSION ||
            data[1] > HF_CAPTURE_MODE_READER || cmd_read_u32be(&data[2]) == 0u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (iso_dep_session_is_active()) {
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    uint32_t session_id = 0;
    hf_capture_result_t result = hf_capture_start((hf_capture_mode_t)data[1],
                                                   data_frame_get_transport(),
                                                   cmd_read_u32be(&data[2]),
                                                   &session_id);
    if (result != HF_CAPTURE_RESULT_OK) {
        return data_frame_make(cmd, hf_capture_result_status(result), 0, NULL);
    }
    uint16_t response_length = hf_capture_build_meta(
                                   session_id, m_hf_capture_response,
                                   sizeof(m_hf_capture_response));
    return data_frame_make(cmd, STATUS_SUCCESS, response_length,
                           m_hf_capture_response);
}

static data_frame_tx_t *cmd_processor_hf_capture_status(uint16_t cmd,
                                                         uint16_t status,
                                                         uint16_t length,
                                                         uint8_t *data) {
    (void)status;
    if (!cmd_payload_exact(length, data, 9u) ||
            data[0] != HF_CAPTURE_PROTOCOL_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t session_id = cmd_read_u32be(&data[1]);
    hf_capture_result_t result = hf_capture_resume(session_id,
                                                    cmd_read_u32be(&data[5]),
                                                    data_frame_get_transport());
    if (result != HF_CAPTURE_RESULT_OK) {
        return data_frame_make(cmd, hf_capture_result_status(result), 0, NULL);
    }
    session_id = hf_capture_session_id();
    uint16_t response_length = hf_capture_build_meta(
                                   session_id, m_hf_capture_response,
                                   sizeof(m_hf_capture_response));
    return data_frame_make(cmd, STATUS_SUCCESS, response_length,
                           m_hf_capture_response);
}

static data_frame_tx_t *cmd_processor_hf_capture_get(uint16_t cmd,
                                                      uint16_t status,
                                                      uint16_t length,
                                                      uint8_t *data) {
    (void)status;
    if (!cmd_payload_exact(length, data, 20u) ||
            data[0] != HF_CAPTURE_PROTOCOL_VERSION || data[5] > 1u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t session_id = cmd_read_u32be(&data[1]);
    bool acknowledge_present = data[5] == 1u;
    uint32_t ack_sequence = cmd_read_u32be(&data[6]);
    uint64_t ack_delivery_token = cmd_read_u64be(&data[10]);
    if (!acknowledge_present &&
            (ack_sequence != 0u || ack_delivery_token != 0u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint16_t requested_bytes = cmd_read_u16be(&data[18]);
    uint16_t response_length = 0;
    hf_capture_result_t result = hf_capture_get(
                                     session_id, acknowledge_present,
                                     ack_sequence, ack_delivery_token,
                                     data_frame_get_transport(), requested_bytes,
                                     m_hf_capture_response,
                                     sizeof(m_hf_capture_response),
                                     &response_length);
    return data_frame_make(cmd, hf_capture_result_status(result),
                           result == HF_CAPTURE_RESULT_OK ? response_length : 0u,
                           m_hf_capture_response);
}

static data_frame_tx_t *cmd_processor_hf_capture_stop(uint16_t cmd,
                                                       uint16_t status,
                                                       uint16_t length,
                                                       uint8_t *data) {
    (void)status;
    if (!cmd_payload_exact(length, data, 5u) ||
            data[0] != HF_CAPTURE_PROTOCOL_VERSION) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    uint32_t session_id = cmd_read_u32be(&data[1]);
    hf_capture_result_t result = hf_capture_stop(
                                     session_id, data_frame_get_transport());
    if (result != HF_CAPTURE_RESULT_OK) {
        return data_frame_make(cmd, hf_capture_result_status(result), 0, NULL);
    }
    uint16_t response_length = hf_capture_build_meta(
                                   session_id, m_hf_capture_response,
                                   sizeof(m_hf_capture_response));
    return data_frame_make(cmd, STATUS_SUCCESS, response_length,
                           m_hf_capture_response);
}

/* ========================================================================
 * HF14A-4 ISO14443-4 T=CL emulation commands (6000-range)
 * ======================================================================== */

/**
 * HF14A-4 APDU recv — non-blocking poll.
 * Returns STATUS_SUCCESS + APDU bytes if one is pending, STATUS_HF_TAG_NO otherwise.
 */
static data_frame_tx_t *cmd_processor_hf14a_4_apdu_recv(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_empty(length)) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    static uint8_t apdu_buf[NFC_14A_4_MAX_APDU];
    uint16_t apdu_len = 0;
    extern bool nfc_tag_14a_4_get_pending_apdu(uint8_t *buf, uint16_t *length);
    if (nfc_tag_14a_4_get_pending_apdu(apdu_buf, &apdu_len)) {
        return data_frame_make(cmd, STATUS_SUCCESS, apdu_len, apdu_buf);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
}

/**
 * HF14A-4 APDU send — push a response for the next WTX-waiting I-block.
 * payload: len_be16(2) + resp_bytes
 */
static data_frame_tx_t *cmd_processor_hf14a_4_apdu_send(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    extern void nfc_tag_14a_4_set_response(const uint8_t *data, uint16_t length);
    if (length < 2u || data == NULL) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint16_t resp_len = cmd_read_u16be(data);
    if (resp_len > NFC_14A_4_MAX_APDU || length != 2u + resp_len)
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    nfc_tag_14a_4_set_response(&data[2], resp_len);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

/**
 * HF14A-4 set anti-collision data (UID/ATQA/SAK/ATS).
 * payload: uid_len(1) uid(n) atqa(2) sak(1) ats_len(1) ats(m)
 */
static data_frame_tx_t *cmd_processor_hf14a_4_set_anti_coll(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    cmd_hf14a_anticoll_t parsed;
    if (!cmd_parse_hf14a_anticoll(length, data, &parsed)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_14a_coll_res_reference_t *info = get_coll_res_data(true);
    if (info == NULL || info->size == NULL || info->uid == NULL ||
            info->atqa == NULL || info->sak == NULL || info->ats == NULL) {
        return data_frame_make(cmd, STATUS_INVALID_SLOT_TYPE, 0, NULL);
    }
    *(info->size) = (nfc_tag_14a_uid_size)parsed.uid_len;
    memcpy(info->uid, &data[parsed.uid_offset], parsed.uid_len);
    memcpy(info->atqa, &data[parsed.atqa_offset], 2);
    info->sak[0] = data[parsed.sak_offset];
    info->ats->length = parsed.ats_len;
    memcpy(info->ats->data, &data[parsed.ats_offset], parsed.ats_len);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

/**
 * HF14A-4 add static APDU response pair (pre-load before hw mode -e).
 * payload: cmd_len(1) cmd(n) resp_len(2 BE) resp(m)
 * If cmd_len==0, clears all static responses.
 */
static data_frame_tx_t *cmd_processor_hf14a_4_static_resp(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length == 0u || data == NULL) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    uint8_t cmd_len = data[0];
    if (cmd_len == 0) {
        if (length != 1u) return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
        nfc_tag_14a_4_clear_static_responses();
        return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
    }
    if (cmd_len > NFC_14A_4_MAX_STATIC_CMD_LEN ||
            length < 1u + (uint16_t)cmd_len + 2u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    /* resp_len is 2 bytes big-endian to support responses > 255 bytes */
    uint16_t resp_len = cmd_read_u16be(&data[1u + cmd_len]);
    uint32_t expected = 1u + (uint32_t)cmd_len + 2u + resp_len;
    if (resp_len > NFC_14A_4_MAX_LARGE_RESP_LEN || expected != length) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    nfc_tag_14a_4_add_static_response(&data[1], cmd_len, &data[3 + cmd_len], resp_len);
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

/**
 * HF14A scan keeping field alive after completion — identical to hf14a_scan
 * but registered without after_hf_reader_run so the field stays on and the
 * card remains in T=CL state for subsequent hf14a_raw APDU calls.
 */
static data_frame_tx_t *cmd_processor_hf14a_scan_keep(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    picc_14a_tag_t taginfo;
    status = pcd_14a_reader_scan_auto(&taginfo);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, status, 0, NULL);
    }
    uint8_t payload[1 + sizeof(taginfo.uid) + sizeof(taginfo.atqa) + sizeof(taginfo.sak) + 1 + sizeof(taginfo.ats)];
    uint16_t offset = 0;
    payload[offset++] = taginfo.uid_len;
    memcpy(&payload[offset], taginfo.uid, taginfo.uid_len);
    offset += taginfo.uid_len;
    memcpy(&payload[offset], taginfo.atqa, sizeof(taginfo.atqa));
    offset += sizeof(taginfo.atqa);
    payload[offset++] = taginfo.sak;
    payload[offset++] = taginfo.ats_len;
    memcpy(&payload[offset], taginfo.ats, taginfo.ats_len);
    offset += taginfo.ats_len;
    return data_frame_make(cmd, STATUS_HF_TAG_OK, offset, payload);
}


/**
 * HF14A-4 reader APDU — activate field, select card (with RATS), send one
 * ISO14443-4 T=CL APDU, return the response, keep field alive.
 *
 * This performs the full select+RATS+APDU sequence in a single firmware call,
 * avoiding the USB round-trip gap that would cause the card to lose power.
 *
 * payload: apdu_bytes (raw APDU, no PCB wrapping needed — added here)
 * returns: raw APDU response bytes (PCB stripped)
 */
static data_frame_tx_t *cmd_processor_hf14a_4_reader_apdu(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length == 0 || length > 512) {
        pcd_14a_reader_antenna_off();
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    /* Step 1: cycle field briefly to return card to IDLE state, then
     * do full select + RATS via scan_auto. This is needed because the card
     * may be in T=CL active state from a previous APDU exchange and won't
     * respond to REQA/WUPA until powered off. */
    pcd_14a_reader_antenna_off();
    bsp_delay_ms(5);
    pcd_14a_reader_reset();
    pcd_14a_reader_antenna_on();
    bsp_delay_ms(8);

    pcd_14a_reader_timeout_set(200);
    picc_14a_tag_t taginfo;
    status = pcd_14a_reader_scan_auto(&taginfo);
    if (status != STATUS_HF_TAG_OK) {
        pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
        pcd_14a_reader_antenna_off();
        uint8_t dbg[2] = {0x01, status};
        return data_frame_make(cmd, STATUS_HF_TAG_NO, 2, dbg);
    }
    if ((taginfo.sak & 0x20u) == 0u || taginfo.ats_len < 2u) {
        pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
        pcd_14a_reader_antenna_off();
        return data_frame_make(cmd, STATUS_HF_ERR_ATS, 0, NULL);
    }
    NRF_LOG_INFO("14A4_READER_APDU: scan_auto OK sak=%02x ats_len=%d",
                 taginfo.sak, taginfo.ats_len);

    static uint8_t resp_chain[ISO_DEP_READER_MAX_APDU_RESPONSE];
    iso_dep_reader_t reader;
    iso_dep_result_t exchange;
    iso_dep_reader_init(&reader, &taginfo);
    bool ok = iso_dep_reader_transceive(&reader, data, length,
                                        resp_chain, sizeof(resp_chain), &exchange);
    pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
    if (!ok) {
        pcd_14a_reader_antenna_off();
        uint16_t out_status = exchange.error == ISO_DEP_ERR_CRC ?
                              STATUS_HF_ERR_CRC : STATUS_HF_ERR_STAT;
        uint8_t dbg[3] = {(uint8_t)exchange.error, exchange.rf_status,
                          exchange.wtx_count};
        return data_frame_make(cmd, out_status, sizeof(dbg), dbg);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_OK,
                           exchange.response_len, resp_chain);
}

static uint16_t iso_dep_session_failure_status(const iso_dep_result_t *result) {
    if (result->error == ISO_DEP_ERR_CRC) return STATUS_HF_ERR_CRC;
    if (result->error == ISO_DEP_ERR_TIMEOUT || result->rf_status == STATUS_HF_TAG_NO) {
        return STATUS_HF_TAG_NO;
    }
    switch (result->rf_status) {
        case STATUS_HF_ERR_STAT:
        case STATUS_HF_ERR_CRC:
        case STATUS_HF_COLLISION:
        case STATUS_HF_ERR_BCC:
        case STATUS_HF_ERR_PARITY:
        case STATUS_HF_ERR_ATS:
            return result->rf_status;
        default:
            return STATUS_HF_ERR_STAT;
    }
}

static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_start(
    uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    iso_dep_session_abort();
    if (!cmd_payload_empty(length)) {
        pcd_14a_reader_polling_annotation_clear();
        pcd_14a_reader_antenna_off();
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }

    picc_14a_tag_t tag;
    uint32_t session_id;
    status = cmd == DATA_CMD_HF14A_4_READER_SESSION_START_APPLE_TRANSIT ?
             iso_dep_session_start_apple_transit(&tag, &session_id) :
             iso_dep_session_start(&tag, &session_id);
    if (status != STATUS_HF_TAG_OK) {
        uint8_t diagnostics[2] = {
            status == STATUS_HF_ERR_ATS ? 0x02u : 0x01u,
            (uint8_t)status,
        };
        return data_frame_make(cmd, status, sizeof(diagnostics), diagnostics);
    }

    static uint8_t payload[4u + 1u + sizeof(tag.uid) + sizeof(tag.atqa) +
                           sizeof(tag.sak) + 1u + sizeof(tag.ats)];
    uint16_t offset = 0u;
    payload[offset++] = (uint8_t)(session_id >> 24);
    payload[offset++] = (uint8_t)(session_id >> 16);
    payload[offset++] = (uint8_t)(session_id >> 8);
    payload[offset++] = (uint8_t)session_id;
    payload[offset++] = tag.uid_len;
    memcpy(&payload[offset], tag.uid, tag.uid_len);
    offset += tag.uid_len;
    memcpy(&payload[offset], tag.atqa, sizeof(tag.atqa));
    offset += sizeof(tag.atqa);
    payload[offset++] = tag.sak;
    payload[offset++] = tag.ats_len;
    memcpy(&payload[offset], tag.ats, tag.ats_len);
    offset += tag.ats_len;
    return data_frame_make(cmd, STATUS_HF_TAG_OK, offset, payload);
}

static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_exchange(
    uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length < 5u || length > 4u + 512u || data == NULL) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (get_device_mode() != DEVICE_MODE_READER) {
        iso_dep_session_abort();
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }

    static uint8_t response[ISO_DEP_READER_MAX_APDU_RESPONSE];
    iso_dep_result_t exchange;
    iso_dep_session_exchange_status_t exchange_status = iso_dep_session_exchange(
        cmd_read_u32be(data), &data[4], length - 4u,
        response, sizeof(response), &exchange);
    if (exchange_status == ISO_DEP_SESSION_EXCHANGE_INVALID) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (exchange_status == ISO_DEP_SESSION_EXCHANGE_RF_ERROR) {
        uint8_t diagnostics[3] = {
            (uint8_t)exchange.error,
            exchange.rf_status,
            exchange.wtx_count,
        };
        return data_frame_make(cmd, iso_dep_session_failure_status(&exchange),
                               sizeof(diagnostics), diagnostics);
    }
    return data_frame_make(cmd, STATUS_HF_TAG_OK,
                           exchange.response_len, response);
}

static data_frame_tx_t *cmd_processor_hf14a_4_reader_session_stop(
    uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (!cmd_payload_exact(length, data, 4u)) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    if (get_device_mode() != DEVICE_MODE_READER) {
        iso_dep_session_abort();
        return data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
    }
    if (!iso_dep_session_stop(cmd_read_u32be(data))) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    return data_frame_make(cmd, STATUS_SUCCESS, 0, NULL);
}

/* -----------------------------------------------------------------------
 * tcl_apdu_: ISO 14443-4 APDU helper used by cmd_processor_hf14a_4_emv_scan.
 * Sends one I-block, receives full response handling card-side chaining.
 * ----------------------------------------------------------------------- */
static iso_dep_reader_t m_tcl_reader;

static bool tcl_apdu_(
    const uint8_t *apdu, uint8_t apdu_sz,
    uint8_t **rdata_ptr, uint16_t *rlen_ptr,
    uint8_t *abuf, uint8_t *rbuf, uint8_t *chain_buf,
    uint16_t *rbits_p, uint8_t *blk_p) {
    (void)abuf;
    (void)rbuf;
    (void)rbits_p;
    iso_dep_result_t result;
    bool ok = iso_dep_reader_transceive(&m_tcl_reader, apdu, apdu_sz,
                                        chain_buf, ISO_DEP_READER_MAX_APDU_RESPONSE,
                                        &result);
    *blk_p = m_tcl_reader.block_num;
    *rdata_ptr = chain_buf;
    *rlen_ptr = result.response_len;
    return ok;
}

/* Fill an EMV DOL (Data Object List: repeated tag+length) with simulated
 * offline-terminal data. amount is a 6-byte n12 BCD value. Unknown fields are
 * zero-filled. Returns the number of bytes written to out. Used to build the
 * GPO PDOL and the GENERATE AC CDOL1 for an offline purchase simulation. */
static void emv_dol_put(uint8_t *dst, uint8_t len,
                        const uint8_t *src, uint8_t src_len) {
    if (len == 0 || src_len == 0) return;
    uint8_t n = src_len < len ? src_len : len;
    memcpy(&dst[len - n], &src[src_len - n], n);
}

static uint8_t emv_fill_dol(const uint8_t *dol, uint8_t dol_len, uint8_t *out,
                            uint8_t out_cap, const uint8_t amount[6]) {
    uint8_t o = 0, i = 0;
    while (i < dol_len) {
        uint32_t tag = dol[i];
        uint8_t tl = 1;
        if ((dol[i] & 0x1F) == 0x1F && i + 1 < dol_len) {
            tag = (tag << 8) | dol[i + 1];
            tl = 2;
            if ((dol[i + 1] & 0x80) && i + 2 < dol_len) {
                tag = (tag << 8) | dol[i + 2];
                tl = 3;
            }
        }
        i += tl;
        if (i >= dol_len) break;
        uint8_t len = dol[i++];
        /* Bound against the REAL destination capacity: a DOL's declared field
         * lengths can sum far past the buffer (stack overflow) otherwise. */
        if (o + len > out_cap) break;
        for (uint8_t k = 0; k < len; k++) out[o + k] = 0x00;
        static const uint8_t amount_other[6] = {0, 0, 0, 0, 0, 0};
        static const uint8_t terminal_country[2] = {0x07, 0x24}; /* Spain */
        static const uint8_t currency[2] = {0x09, 0x78};         /* EUR */
        static const uint8_t txn_date[3] = {0x26, 0x07, 0x07};   /* YYMMDD */
        static const uint8_t tvr[5] = {0, 0, 0, 0, 0};
        static const uint8_t terminal_caps[3] = {0xE0, 0xA0, 0x00};
        static const uint8_t add_terminal_caps[5] = {0x8E, 0x00, 0xB0, 0x50, 0x05};
        static const uint8_t ttq[4] = {0xD6, 0x20, 0xC0, 0x00};
        static const uint8_t cvm_results[3] = {0x1F, 0x03, 0x00};
        static const uint8_t txn_seq_counter[4] = {0, 0, 0, 1};
        static const uint8_t merchant_category[2] = {0x59, 0x99};
        static const uint8_t terminal_txn_info[3] = {0xC8, 0x80, 0x00};
        static const uint8_t terminal_txn_type[1] = {0x00};
        static const uint8_t merchant_type_indicator[1] = {0x01};
        static const uint8_t txn_cert[20] = {0};
        switch (tag) {
            case 0x9F02: emv_dol_put(&out[o], len, amount, 6); break; /* Amount Authorised */
            case 0x9F03: emv_dol_put(&out[o], len, amount_other, sizeof(amount_other)); break;
            case 0x9F1A: emv_dol_put(&out[o], len, terminal_country, sizeof(terminal_country)); break;
            case 0x5F2A: emv_dol_put(&out[o], len, currency, sizeof(currency)); break;
            case 0x9A:   emv_dol_put(&out[o], len, txn_date, sizeof(txn_date)); break;
            case 0x95:   emv_dol_put(&out[o], len, tvr, sizeof(tvr)); break;
            case 0x9C:   if (len >= 1) out[o + len - 1] = 0x00; break; /* Purchase */
            case 0x9F37: for (uint8_t k = 0; k < len; k++) out[o + k] = (uint8_t)(0x11 * (k + 1)); break; /* Unpredictable Number */
            case 0x9F35: if (len >= 1) out[o] = 0x22; break; /* Terminal Type */
            case 0x9F33: emv_dol_put(&out[o], len, terminal_caps, sizeof(terminal_caps)); break;
            case 0x9F34: emv_dol_put(&out[o], len, cvm_results, sizeof(cvm_results)); break;
            case 0x9F40: emv_dol_put(&out[o], len, add_terminal_caps, sizeof(add_terminal_caps)); break;
            case 0x9F41: emv_dol_put(&out[o], len, txn_seq_counter, sizeof(txn_seq_counter)); break;
            case 0x9F53: if (len >= 1) out[o + len - 1] = 0x52; break; /* Transaction category */
            case 0x9F58: emv_dol_put(&out[o], len, merchant_type_indicator, sizeof(merchant_type_indicator)); break;
            case 0x9F59: emv_dol_put(&out[o], len, terminal_txn_info, sizeof(terminal_txn_info)); break;
            case 0x9F5A: emv_dol_put(&out[o], len, terminal_txn_type, sizeof(terminal_txn_type)); break;
            case 0x9F66: emv_dol_put(&out[o], len, ttq, sizeof(ttq)); break;
            case 0x9F15: emv_dol_put(&out[o], len, merchant_category, sizeof(merchant_category)); break;
            case 0x98:   emv_dol_put(&out[o], len, txn_cert, sizeof(txn_cert)); break;
            default: break; /* 9C type=00, 95 TVR=0, 9F03 other amount=0, ... */
        }
        o += len;
    }
    return o;
}

static uint8_t emv_dol_value_len(const uint8_t *dol, uint8_t dol_len,
                                 uint8_t out_cap) {
    uint16_t total = 0;
    uint8_t i = 0;
    while (i < dol_len) {
        uint8_t tl = 1;
        if ((dol[i] & 0x1F) == 0x1F && i + 1 < dol_len) {
            tl = 2;
            if ((dol[i + 1] & 0x80) && i + 2 < dol_len) tl = 3;
        }
        i += tl;
        if (i >= dol_len) return 0;
        total += dol[i++];
        if (total > out_cap) return 0;
    }
    return (uint8_t)total;
}

static bool emv_find_tlv_value(const uint8_t *d, uint16_t dl, uint32_t wanted,
                               uint8_t *out, uint8_t *out_len,
                               uint8_t out_cap, uint8_t depth) {
    if (depth > 6) return false;
    uint16_t i = 0;
    while (i < dl) {
        if (d[i] == 0x00 || d[i] == 0xFF) { i++; continue; }
        uint8_t first = d[i++];
        uint32_t tag = first;
        bool constructed = (first & 0x20) != 0;
        if ((first & 0x1F) == 0x1F) {
            tag = first;
            while (i < dl) {
                tag = (tag << 8) | d[i];
                if ((d[i++] & 0x80) == 0) break;
            }
        }
        if (i >= dl) break;
        uint16_t len = d[i++];
        if (len & 0x80) {
            uint8_t nb = len & 0x7F;
            len = 0;
            for (uint8_t k = 0; k < nb && i < dl; k++) len = (len << 8) | d[i++];
        }
        if (i + len > dl) break;
        if (tag == wanted) {
            uint8_t n = (uint8_t)(len > out_cap ? out_cap : len);
            memcpy(out, &d[i], n);
            *out_len = n;
            return true;
        }
        if (constructed && emv_find_tlv_value(&d[i], len, wanted, out, out_len,
                                             out_cap, depth + 1)) {
            return true;
        }
        i += len;
    }
    return false;
}

/* Find CDOL1 (tag 8C) inside an EMV record body, recursing into constructed
 * templates (70/77/...). Returns true and fills out/out_len on success. */
static bool emv_find_cdol1(const uint8_t *d, uint16_t dl, uint8_t *out,
                           uint8_t *out_len, uint8_t depth) {
    if (depth > 6) return false; /* bound recursion (crafted-TLV stack overflow) */
    uint16_t i = 0;
    while (i < dl) {
        if (d[i] == 0x00 || d[i] == 0xFF) { i++; continue; }
        uint8_t first = d[i++];
        bool constructed = (first & 0x20) != 0;
        if ((first & 0x1F) == 0x1F) {
            while (i < dl && (d[i] & 0x80)) i++;
            if (i < dl) i++;
        }
        if (i >= dl) break;
        uint16_t len = d[i++];
        if (len & 0x80) {
            uint8_t nb = len & 0x7F;
            len = 0;
            for (uint8_t k = 0; k < nb && i < dl; k++) len = (len << 8) | d[i++];
        }
        if (i + len > dl) break;
        if (!constructed && first == 0x8C) {
            uint8_t cl = (uint8_t)(len > 64 ? 64 : len);
            memcpy(out, &d[i], cl);
            *out_len = cl;
            return true;
        }
        if (constructed && emv_find_cdol1(&d[i], len, out, out_len, depth + 1)) {
            return true;
        }
        i += len;
    }
    return false;
}

static bool emv_has_card_identity_tlv(const uint8_t *d, uint16_t dl,
                                      uint8_t depth) {
    if (depth > 6) return false;
    uint16_t i = 0;
    while (i < dl) {
        if (d[i] == 0x00 || d[i] == 0xFF) { i++; continue; }
        uint8_t first = d[i++];
        uint16_t tag = first;
        bool constructed = (first & 0x20) != 0;
        if ((first & 0x1F) == 0x1F) {
            tag = first;
            while (i < dl) {
                tag = (tag << 8) | d[i];
                if ((d[i++] & 0x80) == 0) break;
            }
        }
        if (i >= dl) break;
        uint16_t len = d[i++];
        if (len & 0x80) {
            uint8_t nb = len & 0x7F;
            len = 0;
            for (uint8_t k = 0; k < nb && i < dl; k++) len = (len << 8) | d[i++];
        }
        if (i + len > dl) break;
        if (tag == 0x57 || tag == 0x5A || tag == 0x5F24) return true;
        if (constructed && emv_has_card_identity_tlv(&d[i], len, depth + 1)) {
            return true;
        }
        i += len;
    }
    return false;
}

/**
 * HF14A-4 EMV scan — complete EMV card read in a single firmware call.
 *
 * Performs: field cycle → scan_auto (select+RATS) → PPSE → SELECT AID →
 * GPO → READ RECORDs, all without returning to the host between APDUs.
 *
 * Response format (packed, little-endian lengths):
 *   tag_info:    uid_len(1) uid(n) atqa(2) sak(1) ats_len(1) ats(m)
 *   num_apdus(1)
 *   for each APDU pair:
 *     cmd_len(1) cmd(n) resp_len(2 LE) resp(m)
 *
 * Returns STATUS_HF_TAG_NO if card not found.
 * Returns STATUS_HF_TAG_OK with packed data on success (partial data if
 * some APDUs fail — num_apdus reflects how many completed).
 */
static data_frame_tx_t *cmd_processor_hf14a_4_emv_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 0u && length != 6u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    static uint8_t out[NETDATA_MAX_DATA_LENGTH];
    uint16_t out_len = 0;

    /* ---- helpers -------------------------------------------------- */
    static uint8_t  abuf[64];   /* TX frame: PCB + APDU + CRC */
    static uint8_t  rbuf[270];  /* single-frame receive buffer: up to 256 bytes data + PCB + CRC + slack (FSDI=8 → FSD=256) */
    static uint8_t  chain_buf[512]; /* reassembled chained response */
    uint16_t rbits;
    uint8_t  blk = 0;  /* alternating block number */

    /* SEND_APDU: thin wrapper that calls the static tcl_apdu_ helper. */
#define SEND_APDU(ap, asz, rd, rl)         tcl_apdu_((ap),(asz),(rd),(rl),abuf,rbuf,chain_buf,&rbits,&blk)



    /* Append a cmd+resp pair to out buffer */
#define APPEND_PAIR(cmd_ptr, cmd_sz, resp_ptr, resp_sz) do { \
        uint32_t _pair_len = 1u + (uint32_t)(cmd_sz) + 2u + (uint32_t)(resp_sz); \
        if (num_apdus != UINT8_MAX && out_len + _pair_len <= NETDATA_MAX_DATA_LENGTH) { \
            out[out_len++] = (uint8_t)(cmd_sz); \
            memcpy(&out[out_len], (cmd_ptr), (cmd_sz)); \
            out_len += (cmd_sz); \
            out[out_len++] = (uint8_t)((resp_sz) & 0xFF); \
            out[out_len++] = (uint8_t)((resp_sz) >> 8); \
            memcpy(&out[out_len], (resp_ptr), (resp_sz)); \
            out_len += (resp_sz); \
            num_apdus++; \
        } \
    } while (0)

    /* Offline transaction simulation: if a 6-byte amount (n12 BCD) is supplied,
     * inject it into the GPO PDOL and issue GENERATE AC after the records.
     * With no payload this behaves exactly like the read-only EMV scan. */
    uint8_t txn_amount[6] = {0, 0, 0, 0, 0, 0};
    bool do_txn = (length >= 6);
    if (do_txn) memcpy(txn_amount, data, 6);
    static uint8_t cdol1[64];
    uint8_t cdol1_len = 0;

    /* ---- Step 1: scan_auto ----------------------------------------- */
    bsp_delay_ms(10);
    static picc_14a_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    status = STATUS_HF_TAG_NO;
    /* Phone wallets/HCE payment applets may take longer to wake and expose an
     * ISO-DEP target than plastic cards. Poll briefly instead of doing only two
     * fast attempts so the GUI can start the scan before the phone is tapped. */
    autotimer *p_scan_timer = bsp_obtain_timer(0);
    while (NO_TIMEOUT_1MS(p_scan_timer, 3000)) {
        memset(&tag, 0, sizeof(tag));
        status = pcd_14a_reader_scan_auto(&tag);
        if (status == STATUS_HF_TAG_OK) {
            break;
        }
        for (uint8_t i = 0; i < 30; i++) {
            bsp_delay_ms(1);
            bsp_wdt_feed();
        }
    }
    bsp_return_timer(p_scan_timer);
    if (status != STATUS_HF_TAG_OK) {
        return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
    }
    if ((tag.sak & 0x20u) == 0u || tag.ats_len < 2u) {
        return data_frame_make(cmd, STATUS_HF_ERR_ATS, 0, NULL);
    }
    iso_dep_reader_init(&m_tcl_reader, &tag);

    /* After scan_auto completes RATS, give the RC522 time to settle.
     * The hf14a_scan_keep + hf14a_raw path works because the USB round-trip
     * (~2ms) gives the RC522 time to exit its post-receive state before
     * the next transceive. Replicate that delay here. */
    bsp_delay_ms(5);

    /* ---- Clear RC522 stale state after RATS ----------------------- */
    /* After scan_auto+RATS, CommandReg=0x0C (PCD_TRANSCEIVE) and
     * ComIrqReg=0x64 (RxIRq+TxIRq+b6 set). bytes_transfer's wait loop
     * reads ComIrqReg immediately after StartSend — if RxIRq is already
     * set it exits before the PPSE frame is even transmitted.
     *
     * Fix sequence:
     * 1. Idle the RC522 — stops active TRANSCEIVE state
     * 2. Wait for CommandReg to confirm idle (RC522 state machine settles)
     * 3. Clear all ComIrqReg interrupt flags
     * 4. Flush FIFO and clear StartSend bit */
    write_register_single(CommandReg, PCD_IDLE);
    /* Spin until CommandReg confirms idle (usually immediate) */
    {
        uint16_t _w = 0;
        while ((read_register_single(CommandReg) & 0x0F) != PCD_IDLE && _w++ < 1000);
    }
    write_register_single(ComIrqReg,  0x7F);          /* clear ALL IRQ flags */
    set_register_mask(FIFOLevelReg,   0x80);          /* flush FIFO */
    clear_register_mask(BitFramingReg, 0x80);         /* clear StartSend */

    /* ---- Pack tag info ------------------------------------------- */
    out[out_len++] = tag.uid_len;
    memcpy(&out[out_len], tag.uid, tag.uid_len);
    out_len += tag.uid_len;
    memcpy(&out[out_len], tag.atqa, 2);
    out_len += 2;
    out[out_len++] = tag.sak;
    out[out_len++] = tag.ats_len;
    memcpy(&out[out_len], tag.ats, tag.ats_len);
    out_len += tag.ats_len;

    /* Placeholder for num_apdus — fill in at end */
    uint16_t num_apdus_offset = out_len;
    out[out_len++] = 0;
    uint8_t num_apdus = 0;

    pcd_14a_reader_timeout_set(600);

    /* ---- Step 2: SELECT PPSE ------------------------------------- */
    static const uint8_t ppse_cmd[] = {
        0x00, 0xA4, 0x04, 0x00, 0x0E,
        0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E,
        0x44, 0x44, 0x46, 0x30, 0x31, 0x00
    };
    uint8_t *ppse_resp = NULL;
    uint16_t ppse_rlen = 0;
    {
        bool _ppse_ok = SEND_APDU(ppse_cmd, sizeof(ppse_cmd), &ppse_resp, &ppse_rlen);
        if (!_ppse_ok) {
            goto done;
        }
    }
    APPEND_PAIR(ppse_cmd, sizeof(ppse_cmd), ppse_resp, ppse_rlen);

    /* Keep the T=CL session alive after PPSE. Phone wallets commonly tear down
     * their transient payment applet when the RF field drops, so a field cycle
     * here loses SELECT AID/GPO. tcl_apdu_ already clears stale RxIrq before
     * each exchange; just idle/flush the RC522 without resetting ISO-DEP block
     * numbering. */
    write_register_single(CommandReg, PCD_IDLE);
    write_register_single(ComIrqReg, 0x7F);
    set_register_mask(FIFOLevelReg, 0x80);
    clear_register_mask(BitFramingReg, 0x80);

    /* ---- Extract first AID from PPSE ----------------------------- */
    uint8_t aid[16];
    uint8_t aid_len = 0;
    for (uint16_t i = 0; i + 1 < ppse_rlen; i++) {
        if (ppse_resp[i] == 0x4F && ppse_resp[i + 1] > 0 && ppse_resp[i + 1] <= 16 &&
                (uint16_t)(i + 2 + ppse_resp[i + 1]) <= ppse_rlen) {
            aid_len = ppse_resp[i + 1];
            memcpy(aid, &ppse_resp[i + 2], aid_len);
            break;
        }
    }
    if (aid_len == 0) goto done;

    /* ---- Step 3: SELECT AID -------------------------------------- */
    uint8_t sel_cmd[32];
    uint8_t sel_len = 0;
    sel_cmd[sel_len++] = 0x00;
    sel_cmd[sel_len++] = 0xA4;
    sel_cmd[sel_len++] = 0x04;
    sel_cmd[sel_len++] = 0x00;
    sel_cmd[sel_len++] = aid_len;
    memcpy(&sel_cmd[sel_len], aid, aid_len);
    sel_len += aid_len;
    sel_cmd[sel_len++] = 0x00;

    uint8_t *sel_resp;
    uint16_t sel_rlen;
    if (!SEND_APDU(sel_cmd, sel_len, &sel_resp, &sel_rlen)) goto done;
    APPEND_PAIR(sel_cmd, sel_len, sel_resp, sel_rlen);
    if (do_txn && cdol1_len == 0) {
        emv_find_cdol1(sel_resp, sel_rlen >= 2 ? sel_rlen - 2 : sel_rlen,
                       cdol1, &cdol1_len, 0);
    }

    /* ---- Step 4: GPO — parse PDOL from SELECT AID FCI ------------------ */
    static uint8_t pdol_def[64];
    static uint8_t pdol_data[44];
    uint8_t pdol_def_len = 0;
    uint8_t pdol_len = 0;
    if (emv_find_tlv_value(sel_resp, sel_rlen >= 2 ? sel_rlen - 2 : sel_rlen,
                           0x9F38, pdol_def, &pdol_def_len,
                           sizeof(pdol_def), 0)) {
        pdol_len = emv_dol_value_len(pdol_def, pdol_def_len, sizeof(pdol_data));
        if (pdol_len > 0) {
            emv_fill_dol(pdol_def, pdol_def_len, pdol_data,
                         sizeof(pdol_data), txn_amount);
        }
    }
    /* ---- Step 5: GPO -----------------------------------------------
     * Build GPO from parsed PDOL. pdol_len from FCI may be 0 if truncated.
     * BUILD_GPO fills the PDOL data: TTQ = A0 00 00 00 for first 4 bytes
     * (MSD+EMV contactless, offline, no DDA), rest zeros.
     * TTQ=A0000000 is the lowest-security POS profile; Mastercard and Visa
     * contactless cards respond to it even without a full terminal setup. */
    static uint8_t gpo_buf[8 + 44]; /* static: keep off stack */
    /* PDOL template: TTQ first 4 bytes, zeros after.
     * TTQ A0000000: MSD+EMV contactless capable, offline, no CDA/DDA. */
    static uint8_t gpo_pdol_template[44] = {
        0xA0, 0x00, 0x00, 0x00,  /* TTQ (9F66): MSD+cEMV, offline, no DDA */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Amount Authorised (9F02) */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* Amount Other (9F03) */
        0x02, 0x08,                           /* Country Code (9F1A): Norway */
        0x00, 0x00, 0x00, 0x00, 0x00,         /* TVR (95) */
        0x09, 0x78,                           /* Currency (5F2A): EUR */
        0x25, 0x01, 0x01,                     /* Date (9A) */
        0x00,                                 /* Transaction Type (9C) */
        0x00, 0x00, 0x00, 0x00,               /* Unpredictable Number (9F37) */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* extra zeros */
        0x00, 0x00, 0x00
    };
    /* Inject the simulated amount (or reset to zero for a read-only scan) into
     * the PDOL Amount Authorised field so the card's cryptogram covers it. */
    for (uint8_t _a = 0; _a < 6; _a++) {
        gpo_pdol_template[4 + _a] = do_txn ? txn_amount[_a] : 0x00;
    }
    uint8_t gpo_len = 0;
    uint8_t *gpo_resp = NULL;
    uint16_t gpo_rlen = 0;
#define BUILD_GPO(pl, src) do { \
        gpo_len = 0; \
        gpo_buf[gpo_len++]=0x80; gpo_buf[gpo_len++]=0xA8; \
        gpo_buf[gpo_len++]=0x00; gpo_buf[gpo_len++]=0x00; \
        gpo_buf[gpo_len++]=(uint8_t)((pl)+2); \
        gpo_buf[gpo_len++]=0x83; gpo_buf[gpo_len++]=(pl); \
        memcpy(&gpo_buf[gpo_len], (src), (pl)); \
        gpo_len += (pl); \
        gpo_buf[gpo_len++]=0x00; \
    } while(0)
#define GPO_OK(rp,rl) ((rl)>=2 && \
        (rp)[(rl)-2]==0x90 && (rp)[(rl)-1]==0x00 && \
        ((rp)[0]==0x77||(rp)[0]==0x80))
    /* Attempt 1: use the actual PDOL tag order from FCI, as EMV terminals do. */
    BUILD_GPO(pdol_len, pdol_len > 0 ? pdol_data : gpo_pdol_template);
    if (!SEND_APDU(gpo_buf, gpo_len, &gpo_resp, &gpo_rlen) ||
            !GPO_OK(gpo_resp, gpo_rlen)) {
        /* Attempt 2: try 4 bytes (TTQ only — some Visa/MC accept this) */
        if (pdol_len != 4) {
            BUILD_GPO(4, gpo_pdol_template);
            if (SEND_APDU(gpo_buf, gpo_len, &gpo_resp, &gpo_rlen) &&
                    GPO_OK(gpo_resp, gpo_rlen)) goto gpo_done;
        }
        /* Attempt 3: try 29 bytes (common Mastercard/Visa PDOL size) */
        if (pdol_len != 29) {
            BUILD_GPO(29, gpo_pdol_template);
            if (SEND_APDU(gpo_buf, gpo_len, &gpo_resp, &gpo_rlen) &&
                    GPO_OK(gpo_resp, gpo_rlen)) goto gpo_done;
        }
        /* Attempt 4: try 33 bytes (MC with Amount+Country+Currency fields) */
        if (pdol_len != 33) {
            BUILD_GPO(33, gpo_pdol_template);
            if (SEND_APDU(gpo_buf, gpo_len, &gpo_resp, &gpo_rlen) &&
                    GPO_OK(gpo_resp, gpo_rlen)) goto gpo_done;
        }
        /* Attempts 5-7: additional MC sizes (+TermType, +DataAuth, +IssuerAppData) */
        {
            static const uint8_t extra_pl[] = {34, 36, 38};
            for (uint8_t ei = 0; ei < sizeof(extra_pl); ei++) {
                uint8_t pl = extra_pl[ei];
                if (pl == pdol_len || pl == 4 || pl == 29 || pl == 33) continue;
                BUILD_GPO(pl, gpo_pdol_template);
                if (SEND_APDU(gpo_buf, gpo_len, &gpo_resp, &gpo_rlen) &&
                        GPO_OK(gpo_resp, gpo_rlen)) goto gpo_done;
            }
        }
        goto done;  /* all attempts failed */
    }
gpo_done:
#undef BUILD_GPO
#undef GPO_OK
    APPEND_PAIR(gpo_buf, gpo_len, gpo_resp, gpo_rlen);
    static uint8_t afl_buf[32];
    uint8_t *afl = afl_buf;
    uint8_t afl_len = 0;
    if (gpo_rlen > 0 && gpo_resp[0] == 0x77) {
        uint16_t body_len = gpo_rlen >= 2u ? gpo_rlen - 2u : 0u;
        (void)emv_find_tlv_value(gpo_resp, body_len, 0x94,
                                 afl_buf, &afl_len, sizeof(afl_buf), 0);
    } else if (gpo_rlen > 6 && gpo_resp[0] == 0x80) {
        /* Format 1: tag 80, len, AIP(2), AFL(...), SW(2) => AFL starts at [4]. */
        uint16_t available = gpo_rlen - 6u;
        afl_len = (uint8_t)(available > sizeof(afl_buf) ? sizeof(afl_buf) : available);
        memcpy(afl_buf, &gpo_resp[4], afl_len);
    }
    if ((afl_len & 3u) != 0u) afl_len = 0;
    uint8_t records_read = 0;
    bool found_identity_record = false;

    /* READ each record listed in the AFL (when the GPO provided one) */
    if (afl_len > 0) {
        for (uint8_t a = 0; a + 3 < afl_len; a += 4) {
            uint8_t sfi    = (afl[a] >> 3) & 0x1F;
            uint8_t rec_s  = afl[a + 1];
            uint8_t rec_e  = afl[a + 2];
            uint8_t oda_count = afl[a + 3];
            if ((afl[a] & 0x07u) != 0u || sfi == 0 || sfi > 30u ||
                    rec_s == 0u || rec_s > rec_e ||
                    oda_count > (uint8_t)(rec_e - rec_s + 1u)) continue;
            /* `unsigned` r: a uint8_t would wrap forever when rec_e == 0xFF. */
            for (unsigned r = rec_s; r <= rec_e; r++) {
                bsp_wdt_feed();
                if (out_len >= NETDATA_MAX_DATA_LENGTH - 300) break;
                uint8_t rr_cmd[5] = {0x00, 0xB2, (uint8_t)r, (uint8_t)((sfi << 3) | 4), 0x00};
                uint8_t *rr_resp;
                uint16_t rr_rlen;
                if (!SEND_APDU(rr_cmd, 5, &rr_resp, &rr_rlen)) {
                    /* RC522 FIFO is 64 bytes — records > 61 bytes fail. */
                    NRF_LOG_INFO("14A4_EMV_SCAN: READ RECORD SFI=%d rec=%d failed (response too large?)", sfi, r);
                    continue;
                }
                APPEND_PAIR(rr_cmd, 5, rr_resp, rr_rlen);
                records_read++;
                if (!found_identity_record) {
                    found_identity_record = emv_has_card_identity_tlv(
                        rr_resp, rr_rlen >= 2 ? rr_rlen - 2 : rr_rlen, 0);
                }
                if (do_txn && cdol1_len == 0) {
                    emv_find_cdol1(rr_resp, rr_rlen >= 2 ? rr_rlen - 2 : rr_rlen,
                                   cdol1, &cdol1_len, 0);
                }
            }
        }
    }

    /* Fallback (nfc-frog style): if the AFL yielded no identity data — some
     * cards omit or mis-report it in the GPO — brute-force READ RECORD across
     * all valid EMV SFIs with a fast 16-record cap. Stop each SFI on the first
     * error SW (6A83/6A82/...), and bound the total to keep the response sane. */
    if (records_read == 0 || !found_identity_record) {
        for (uint8_t sfi = 1; sfi <= 30; sfi++) {
            bsp_wdt_feed();
            if (out_len >= NETDATA_MAX_DATA_LENGTH - 300) break;
            for (uint8_t r = 1; r <= 16; r++) {
                if (out_len >= NETDATA_MAX_DATA_LENGTH - 300) break;
                uint8_t rr_cmd[5] = {0x00, 0xB2, r, (uint8_t)((sfi << 3) | 4), 0x00};
                uint8_t *rr_resp;
                uint16_t rr_rlen;
                if (!SEND_APDU(rr_cmd, 5, &rr_resp, &rr_rlen)) break;
                /* Success only on SW 90 00; any error (6A83/6A82/...) => next SFI */
                if (rr_rlen >= 2 && rr_resp[rr_rlen - 2] == 0x90 &&
                        rr_resp[rr_rlen - 1] == 0x00) {
                    APPEND_PAIR(rr_cmd, 5, rr_resp, rr_rlen);
                    records_read++;
                    if (!found_identity_record) {
                        found_identity_record = emv_has_card_identity_tlv(
                            rr_resp, rr_rlen >= 2 ? rr_rlen - 2 : rr_rlen, 0);
                    }
                    if (do_txn && cdol1_len == 0) {
                        emv_find_cdol1(rr_resp,
                                       rr_rlen >= 2 ? rr_rlen - 2 : rr_rlen,
                                       cdol1, &cdol1_len, 0);
                    }
                } else {
                    break;
                }
            }
        }
    }

    /* ---- Optional: GENERATE AC — offline purchase simulation -------------
     * Requests an ARQC cryptogram for the simulated amount. This is what a POS
     * asks the card during a purchase; the cryptogram is NOT sent to any bank,
     * so nothing is authorised and no funds move. Best-effort: cards may return
     * an error SW without a full terminal profile — that's fine (still no
     * charge). Visa qVSDC cards already produce the cryptogram in the GPO. */
    if (do_txn && cdol1_len > 0u) {
        static uint8_t gac_data[64];
        uint8_t expected_dlen = emv_dol_value_len(cdol1, cdol1_len, sizeof(gac_data));
        uint8_t gac_dlen = emv_fill_dol(cdol1, cdol1_len, gac_data,
                                        sizeof(gac_data), txn_amount);
        if (gac_dlen > 0u && gac_dlen == expected_dlen && gac_dlen <= 55u) {
            static uint8_t gac_cmd[80];
            uint8_t gc = 0;
            gac_cmd[gc++] = 0x80;      /* CLA */
            gac_cmd[gc++] = 0xAE;      /* INS GENERATE AC */
            gac_cmd[gc++] = 0x80;      /* P1 = ARQC (online cryptogram) */
            gac_cmd[gc++] = 0x00;
            gac_cmd[gc++] = gac_dlen;
            memcpy(&gac_cmd[gc], gac_data, gac_dlen);
            gc += gac_dlen;
            gac_cmd[gc++] = 0x00;
            uint8_t *gac_resp;
            uint16_t gac_rlen;
            if (SEND_APDU(gac_cmd, gc, &gac_resp, &gac_rlen)) {
                APPEND_PAIR(gac_cmd, gc, gac_resp, gac_rlen);
            }
        }
    }

done:
    iso_dep_reader_deselect(&m_tcl_reader);
    pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
    pcd_14a_reader_antenna_off();
    out[num_apdus_offset] = num_apdus;
    /* Return HF_TAG_OK even with 0 APDUs so Python can see tag info */
    return data_frame_make(cmd, STATUS_HF_TAG_OK, out_len, out);
}

/**
 * HF14A-4 DESFire scan — enumerate a MIFARE DESFire card in a single call.
 *
 * Performs: field cycle -> scan_auto (select+RATS) -> GetVersion (+ 0xAF
 * additional frames) -> GetApplicationIDs -> per-AID SelectApplication +
 * GetFileIDs, all in one T=CL session. Same packed response format as the EMV
 * scan (tag info + cmd/resp APDU pairs); the host parses the DESFire structures.
 * Reuses the static tcl_apdu_ helper and the SEND_APDU/APPEND_PAIR macros.
 */
static data_frame_tx_t *cmd_processor_hf14a_4_desfire_scan(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    if (length != 0u) {
        return data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
    }
    static uint8_t out[NETDATA_MAX_DATA_LENGTH];
    uint16_t out_len = 0;
    static uint8_t  abuf[64];
    static uint8_t  rbuf[270];
    static uint8_t  chain_buf[512];
    uint16_t rbits;
    uint8_t  blk = 0;

    bsp_delay_ms(10);
    static picc_14a_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    status = pcd_14a_reader_scan_auto(&tag);
    if (status != STATUS_HF_TAG_OK) {
        bsp_delay_ms(20);
        memset(&tag, 0, sizeof(tag));
        status = pcd_14a_reader_scan_auto(&tag);
        if (status != STATUS_HF_TAG_OK) {
            return data_frame_make(cmd, STATUS_HF_TAG_NO, 0, NULL);
        }
    }
    if ((tag.sak & 0x20u) == 0u || tag.ats_len < 2u) {
        return data_frame_make(cmd, STATUS_HF_ERR_ATS, 0, NULL);
    }
    iso_dep_reader_init(&m_tcl_reader, &tag);
    bsp_delay_ms(5);

    /* Clear RC522 stale state after RATS (see emv_scan for rationale) */
    write_register_single(CommandReg, PCD_IDLE);
    {
        uint16_t _w = 0;
        while ((read_register_single(CommandReg) & 0x0F) != PCD_IDLE && _w++ < 1000);
    }
    write_register_single(ComIrqReg,  0x7F);
    set_register_mask(FIFOLevelReg,   0x80);
    clear_register_mask(BitFramingReg, 0x80);

    /* Pack tag info (same layout as emv_scan) */
    out[out_len++] = tag.uid_len;
    memcpy(&out[out_len], tag.uid, tag.uid_len);
    out_len += tag.uid_len;
    memcpy(&out[out_len], tag.atqa, 2);
    out_len += 2;
    out[out_len++] = tag.sak;
    out[out_len++] = tag.ats_len;
    memcpy(&out[out_len], tag.ats, tag.ats_len);
    out_len += tag.ats_len;

    uint16_t num_apdus_offset = out_len;
    out[out_len++] = 0;
    uint8_t num_apdus = 0;
    pcd_14a_reader_timeout_set(600);

    uint8_t *r = NULL;
    uint16_t rl = 0;

    /* GetVersion (0x60) + additional frames (0xAF) while SW == 91 AF */
    static const uint8_t getver[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    if (SEND_APDU(getver, sizeof(getver), &r, &rl)) {
        APPEND_PAIR(getver, sizeof(getver), r, rl);
        static const uint8_t getmore[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
        uint8_t guard = 0;
        while (rl >= 2 && r[rl - 2] == 0x91 && r[rl - 1] == 0xAF && guard++ < 4) {
            if (!SEND_APDU(getmore, sizeof(getmore), &r, &rl)) break;
            APPEND_PAIR(getmore, sizeof(getmore), r, rl);
        }
    }

    /* GetFreeMemory (0x6E) — 3-byte free EEPROM (EV1+; harmless if unsupported) */
    static const uint8_t getfree[] = {0x90, 0x6E, 0x00, 0x00, 0x00};
    if (SEND_APDU(getfree, sizeof(getfree), &r, &rl)) {
        APPEND_PAIR(getfree, sizeof(getfree), r, rl);
    }

    /* GetApplicationIDs (0x6A) — response is N*3 bytes of AIDs + SW */
    static const uint8_t getaids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    uint8_t aids[24];
    uint8_t aids_count = 0;
    if (SEND_APDU(getaids, sizeof(getaids), &r, &rl)) {
        APPEND_PAIR(getaids, sizeof(getaids), r, rl);
        if (rl >= 2) {
            uint16_t adata = rl - 2;
            for (uint16_t i = 0; i + 3 <= adata && aids_count < 8; i += 3) {
                memcpy(&aids[aids_count * 3], &r[i], 3);
                aids_count++;
            }
        }
    }

    /* Per application: SelectApplication (0x5A), GetKeySettings (0x45),
     * GetFileIDs (0x6F), and GetFileSettings (0xF5) per file. */
    static const uint8_t getks[]    = {0x90, 0x45, 0x00, 0x00, 0x00};
    static const uint8_t getfiles[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
    for (uint8_t a = 0; a < aids_count; a++) {
        if (out_len >= NETDATA_MAX_DATA_LENGTH - 400) break;
        uint8_t selapp[9] = {0x90, 0x5A, 0x00, 0x00, 0x03,
                             aids[a * 3], aids[a * 3 + 1], aids[a * 3 + 2], 0x00};
        if (!SEND_APDU(selapp, sizeof(selapp), &r, &rl)) continue;
        APPEND_PAIR(selapp, sizeof(selapp), r, rl);
        /* GetKeySettings (0x45): settings byte + key count */
        if (SEND_APDU(getks, sizeof(getks), &r, &rl)) {
            APPEND_PAIR(getks, sizeof(getks), r, rl);
        }
        if (SEND_APDU(getfiles, sizeof(getfiles), &r, &rl)) {
            APPEND_PAIR(getfiles, sizeof(getfiles), r, rl);
            /* Copy file IDs before further SEND_APDU overwrites the chain buf. */
            uint8_t fids[32];
            uint8_t fcount = (rl >= 2) ? (uint8_t)(rl - 2) : 0;
            if (fcount > sizeof(fids)) fcount = sizeof(fids);
            memcpy(fids, r, fcount);
            for (uint8_t f = 0; f < fcount; f++) {
                if (out_len >= NETDATA_MAX_DATA_LENGTH - 300) break;
                /* GetFileSettings (0xF5) for this file id */
                uint8_t gfs[7] = {0x90, 0xF5, 0x00, 0x00, 0x01, fids[f], 0x00};
                if (SEND_APDU(gfs, sizeof(gfs), &r, &rl)) {
                    APPEND_PAIR(gfs, sizeof(gfs), r, rl);
                }
            }
        }
    }

    iso_dep_reader_deselect(&m_tcl_reader);
    pcd_14a_reader_timeout_set(DEF_COM_TIMEOUT);
    pcd_14a_reader_antenna_off();
    out[num_apdus_offset] = num_apdus;
    return data_frame_make(cmd, STATUS_HF_TAG_OK, out_len, out);
}

static data_frame_tx_t *cmd_processor_hf14a_4_debug_counters(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint8_t buf[4];
    nfc_tag_14a_4_get_debug_counters(&buf[0], &buf[1], &buf[2], &buf[3]);
    return data_frame_make(cmd, STATUS_SUCCESS, 4, buf);
}

/* Shared because command dispatch is serialized and data_frame_make copies it. */
static uint8_t m_emv_trace_response[NETDATA_MAX_DATA_LENGTH];

static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_start(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint16_t response_status = STATUS_SUCCESS;
    uint16_t response_len = emv_trace_start(data, length, m_emv_trace_response,
                                            sizeof(m_emv_trace_response),
                                            &response_status);
    return data_frame_make(cmd, response_status, response_len, m_emv_trace_response);
}

static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_meta(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint16_t response_status = STATUS_SUCCESS;
    uint16_t response_len = emv_trace_meta(data, length, m_emv_trace_response,
                                           sizeof(m_emv_trace_response),
                                           &response_status);
    return data_frame_make(cmd, response_status, response_len, m_emv_trace_response);
}

static data_frame_tx_t *cmd_processor_hf14a_4_emv_trace_get(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    uint16_t response_status = STATUS_SUCCESS;
    uint16_t response_len = emv_trace_get(data, length, m_emv_trace_response,
                                          sizeof(m_emv_trace_response),
                                          &response_status);
    return data_frame_make(cmd, response_status, response_len, m_emv_trace_response);
}
#endif

static cmd_data_map_t m_data_cmd_map[] = {
    {    DATA_CMD_GET_APP_VERSION,              NULL,                        cmd_processor_get_app_version,               NULL                   },
    {    DATA_CMD_CHANGE_DEVICE_MODE,           NULL,                        cmd_processor_change_device_mode,            NULL                   },
    {    DATA_CMD_GET_DEVICE_MODE,              NULL,                        cmd_processor_get_device_mode,               NULL                   },
    {    DATA_CMD_SET_ACTIVE_SLOT,              NULL,                        cmd_processor_set_active_slot,               NULL                   },
    {    DATA_CMD_SET_SLOT_TAG_TYPE,            NULL,                        cmd_processor_set_slot_tag_type,             NULL                   },
    {    DATA_CMD_SET_SLOT_DATA_DEFAULT,        NULL,                        cmd_processor_set_slot_data_default,         NULL                   },
    {    DATA_CMD_SET_SLOT_ENABLE,              NULL,                        cmd_processor_set_slot_enable,               NULL                   },
    {    DATA_CMD_SET_SLOT_TAG_NICK,            NULL,                        cmd_processor_set_slot_tag_nick,             NULL                   },
    {    DATA_CMD_GET_SLOT_TAG_NICK,            NULL,                        cmd_processor_get_slot_tag_nick,             NULL                   },
    {    DATA_CMD_SLOT_DATA_CONFIG_SAVE,        NULL,                        cmd_processor_slot_data_config_save,         NULL                   },
    {    DATA_CMD_ENTER_BOOTLOADER,             NULL,                        cmd_processor_enter_bootloader,              NULL                   },
    {    DATA_CMD_GET_DEVICE_CHIP_ID,           NULL,                        cmd_processor_get_device_chip_id,            NULL                   },
    {    DATA_CMD_GET_DEVICE_ADDRESS,           NULL,                        cmd_processor_get_device_address,            NULL                   },
    {    DATA_CMD_SAVE_SETTINGS,                NULL,                        cmd_processor_save_settings,                 NULL                   },
    {    DATA_CMD_RESET_SETTINGS,               NULL,                        cmd_processor_reset_settings,                NULL                   },
    {    DATA_CMD_SET_ANIMATION_MODE,           NULL,                        cmd_processor_set_animation_mode,            NULL                   },
    {    DATA_CMD_GET_ANIMATION_MODE,           NULL,                        cmd_processor_get_animation_mode,            NULL                   },
    {    DATA_CMD_GET_GIT_VERSION,              NULL,                        cmd_processor_get_git_version,               NULL                   },
    {    DATA_CMD_GET_ACTIVE_SLOT,              NULL,                        cmd_processor_get_active_slot,               NULL                   },
    {    DATA_CMD_GET_SLOT_INFO,                NULL,                        cmd_processor_get_slot_info,                 NULL                   },
    {    DATA_CMD_WIPE_FDS,                     NULL,                        cmd_processor_wipe_fds,                      NULL                   },
    {    DATA_CMD_DELETE_SLOT_TAG_NICK,         NULL,                        cmd_processor_delete_slot_tag_nick,          NULL                   },
    {    DATA_CMD_GET_ENABLED_SLOTS,            NULL,                        cmd_processor_get_enabled_slots,             NULL                   },
    {    DATA_CMD_DELETE_SLOT_SENSE_TYPE,       NULL,                        cmd_processor_delete_slot_sense_type,        NULL                   },
    {    DATA_CMD_GET_BATTERY_INFO,             NULL,                        cmd_processor_get_battery_info,              NULL                   },
    {    DATA_CMD_GET_BUTTON_PRESS_CONFIG,      NULL,                        cmd_processor_get_button_press_config,       NULL                   },
    {    DATA_CMD_SET_BUTTON_PRESS_CONFIG,      NULL,                        cmd_processor_set_button_press_config,       NULL                   },
    {    DATA_CMD_GET_LONG_BUTTON_PRESS_CONFIG, NULL,                        cmd_processor_get_long_button_press_config,  NULL                   },
    {    DATA_CMD_SET_LONG_BUTTON_PRESS_CONFIG, NULL,                        cmd_processor_set_long_button_press_config,  NULL                   },
    {    DATA_CMD_GET_BLE_PAIRING_KEY,          NULL,                        cmd_processor_get_ble_connect_key,           NULL                   },
    {    DATA_CMD_SET_BLE_PAIRING_KEY,          NULL,                        cmd_processor_set_ble_connect_key,           NULL                   },
    {    DATA_CMD_DELETE_ALL_BLE_BONDS,         NULL,                        cmd_processor_delete_all_ble_bonds,          NULL                   },
    {    DATA_CMD_GET_DEVICE_MODEL,             NULL,                        cmd_processor_get_device_model,              NULL                   },
    {    DATA_CMD_GET_DEVICE_SETTINGS,          NULL,                        cmd_processor_get_device_settings,           NULL                   },
    {    DATA_CMD_GET_DEVICE_CAPABILITIES,      NULL,                        cmd_processor_get_device_capabilities,       NULL                   },
    {    DATA_CMD_GET_BLE_PAIRING_ENABLE,       NULL,                        cmd_processor_get_ble_pairing_enable,        NULL                   },
    {    DATA_CMD_SET_BLE_PAIRING_ENABLE,       NULL,                        cmd_processor_set_ble_pairing_enable,        NULL                   },
    {    DATA_CMD_GET_KEYBOARD_HID_ENABLE,      NULL,                        cmd_processor_get_keyboard_hid_enable,       NULL                   },
    {    DATA_CMD_SET_KEYBOARD_HID_ENABLE,      NULL,                        cmd_processor_set_keyboard_hid_enable,       NULL                   },
    {    DATA_CMD_GET_SLEEP_TIMEOUT,            NULL,                        cmd_processor_get_sleep_timeout,             NULL                   },
    {    DATA_CMD_SET_SLEEP_TIMEOUT,            NULL,                        cmd_processor_set_sleep_timeout,             NULL                   },
    {    DATA_CMD_GET_ALL_SLOT_NICKS,           NULL,                        cmd_processor_get_all_slot_nicks,            NULL                   },
    {    DATA_CMD_KEYBOARD_UPLOAD_BEGIN,        cmd_before_keyboard,         cmd_processor_keyboard_upload_begin,         NULL                   },
    {    DATA_CMD_KEYBOARD_UPLOAD_CHUNK,        cmd_before_keyboard,         cmd_processor_keyboard_upload_chunk,         NULL                   },
    {    DATA_CMD_KEYBOARD_UPLOAD_COMMIT,       cmd_before_keyboard,         cmd_processor_keyboard_upload_commit,        NULL                   },
    {    DATA_CMD_KEYBOARD_RUN,                 cmd_before_keyboard,         cmd_processor_keyboard_run,                 NULL                   },
    {    DATA_CMD_KEYBOARD_CANCEL,              cmd_before_keyboard,         cmd_processor_keyboard_cancel,              NULL                   },
    {    DATA_CMD_KEYBOARD_GET_STATUS,          cmd_before_keyboard,         cmd_processor_keyboard_get_status,          NULL                   },
    {    DATA_CMD_KEYBOARD_CLEAR,               cmd_before_keyboard,         cmd_processor_keyboard_clear,               NULL                   },
    {    DATA_CMD_KEYBOARD_SET_TEMP_BLE_NAME,   cmd_before_keyboard,         cmd_processor_keyboard_set_temp_ble_name,   NULL                   },
    {    DATA_CMD_KEYBOARD_ARM_BLE,             cmd_before_keyboard,         cmd_processor_keyboard_arm_ble,             NULL                   },
    {    DATA_CMD_ACTIVE_SLOT_SNAPSHOT,          NULL,                        cmd_processor_active_slot_snapshot,          NULL                   },

    {    DATA_CMD_BLE_SCAN_START,               NULL,                        cmd_processor_ble_scan_start,                NULL                   },
    {    DATA_CMD_BLE_SCAN_STOP,                NULL,                        cmd_processor_ble_scan_stop,                 NULL                   },
    {    DATA_CMD_BLE_SCAN_GET_COUNT,           NULL,                        cmd_processor_ble_scan_get_count,            NULL                   },
    {    DATA_CMD_BLE_SCAN_GET_RESULTS,         NULL,                        cmd_processor_ble_scan_get_results,          NULL                   },
    {    DATA_CMD_BLE_ADVERTISING_SET,          NULL,                        cmd_processor_ble_advertising_set,           NULL                   },
    {    DATA_CMD_BLE_ADVERTISING_GET,          NULL,                        cmd_processor_ble_advertising_get,           NULL                   },
    {    DATA_CMD_BLE_LINK_PROBE,               NULL,                        cmd_processor_ble_link_probe,                NULL                   },

    {    DATA_CMD_BLE_CONNECT,                  NULL,                        cmd_processor_ble_connect,                   NULL                   },
    {    DATA_CMD_BLE_DISCONNECT,               NULL,                        cmd_processor_ble_disconnect,                NULL                   },
    {    DATA_CMD_BLE_CENTRAL_STATE,            NULL,                        cmd_processor_ble_central_state,             NULL                   },
    {    DATA_CMD_BLE_GATT_DISCOVER,            NULL,                        cmd_processor_ble_gatt_discover,             NULL                   },
    {    DATA_CMD_BLE_GATT_GET_CHARS,           NULL,                        cmd_processor_ble_gatt_get_chars,            NULL                   },
    {    DATA_CMD_BLE_FUZZ_START,               NULL,                        cmd_processor_ble_fuzz_start,                NULL                   },
    {    DATA_CMD_BLE_FUZZ_STOP,                NULL,                        cmd_processor_ble_fuzz_stop,                 NULL                   },
    {    DATA_CMD_BLE_FUZZ_GET_LOG,             NULL,                        cmd_processor_ble_fuzz_get_log,              NULL                   },
    {    DATA_CMD_BLE_GATT_READ,                NULL,                        cmd_processor_ble_gatt_read,                 NULL                   },
    {    DATA_CMD_BLE_GATT_GET_READ,            NULL,                        cmd_processor_ble_gatt_get_read,             NULL                   },
    {    DATA_CMD_BLE_SUBSCRIBE,                NULL,                        cmd_processor_ble_subscribe,                 NULL                   },
    {    DATA_CMD_BLE_GET_NOTIFICATIONS,        NULL,                        cmd_processor_ble_get_notifications,         NULL                   },
    {    DATA_CMD_BLE_FIND_CCCD,                NULL,                        cmd_processor_ble_find_cccd,                 NULL                   },
    {    DATA_CMD_BLE_GET_CCCD,                 NULL,                        cmd_processor_ble_get_cccd,                  NULL                   },
    {    DATA_CMD_BLE_GATT_WRITE,               NULL,                        cmd_processor_ble_gatt_write,                NULL                   },
    {    DATA_CMD_BLE_GET_WRITE,                NULL,                        cmd_processor_ble_get_write,                 NULL                   },
    {    DATA_CMD_BLE_GET_MTU,                  NULL,                        cmd_processor_ble_get_mtu,                   NULL                   },
    {    DATA_CMD_BLE_DESC_DISCOVER,            NULL,                        cmd_processor_ble_desc_discover,             NULL                   },
    {    DATA_CMD_BLE_DESC_GET,                 NULL,                        cmd_processor_ble_desc_get,                  NULL                   },
    {    DATA_CMD_BLE_SVC_DISCOVER,             NULL,                        cmd_processor_ble_svc_discover,              NULL                   },
    {    DATA_CMD_BLE_SVC_GET,                  NULL,                        cmd_processor_ble_svc_get,                   NULL                   },
    {    DATA_CMD_BLE_DEVICE_INFO,              NULL,                        cmd_processor_ble_device_info,               NULL                   },
    {    DATA_CMD_BLE_GET_DEVICE_INFO,          NULL,                        cmd_processor_ble_get_device_info,           NULL                   },

    {    DATA_CMD_BLE_SET_ADDR,                 NULL,                        cmd_processor_ble_set_addr,                  NULL                   },
    {    DATA_CMD_BLE_GET_ADDR,                 NULL,                        cmd_processor_ble_get_addr,                  NULL                   },
    {    DATA_CMD_BLE_RADIO_SET,                NULL,                        cmd_processor_ble_radio_set,                 NULL                   },
    {    DATA_CMD_BLE_RADIO_GET,                NULL,                        cmd_processor_ble_radio_get,                 NULL                   },
    {    DATA_CMD_BLE_FLOOD_START,              NULL,                        cmd_processor_ble_flood_start,              NULL                   },
    {    DATA_CMD_BLE_FLOOD_STOP,               NULL,                        cmd_processor_ble_flood_stop,               NULL                   },
    {    DATA_CMD_BLE_FLOOD_COUNT,              NULL,                        cmd_processor_ble_flood_count,              NULL                   },
    {    DATA_CMD_BLE_KICK,                     NULL,                        cmd_processor_ble_kick,                     NULL                   },
    {    DATA_CMD_BLE_ADV_FLOOD_START,          NULL,                        cmd_processor_ble_adv_flood_start,          NULL                   },
    {    DATA_CMD_BLE_ADV_FLOOD_STOP,           NULL,                        cmd_processor_ble_adv_flood_stop,           NULL                   },
    {    DATA_CMD_BLE_ADV_LAB_START,            NULL,                        cmd_processor_ble_adv_lab_start,            NULL                   },
    {    DATA_CMD_BLE_ADV_LAB_STATUS,           NULL,                        cmd_processor_ble_adv_lab_status,           NULL                   },
    {    DATA_CMD_BLE_ADV_LAB_STOP,             NULL,                        cmd_processor_ble_adv_lab_stop,             NULL                   },

#if defined(PROJECT_CHAMELEON_ULTRA)

    {    DATA_CMD_HF14A_SCAN,                   before_hf_reader_run,        cmd_processor_hf14a_scan,                    after_hf_reader_run    },
    {    DATA_CMD_MF1_DETECT_SUPPORT,           before_hf_reader_run,        cmd_processor_mf1_detect_support,            after_hf_reader_run    },
    {    DATA_CMD_MF1_DETECT_PRNG,              before_hf_reader_run,        cmd_processor_mf1_detect_prng,               after_hf_reader_run    },
    {    DATA_CMD_MF1_STATIC_NESTED_ACQUIRE,    before_hf_reader_run,        cmd_processor_mf1_static_nested_acquire,     after_hf_reader_run    },
    {    DATA_CMD_MF1_DARKSIDE_ACQUIRE,         before_hf_reader_run,        cmd_processor_mf1_darkside_acquire,          after_hf_reader_run    },
    {    DATA_CMD_MF1_DETECT_NT_DIST,           before_hf_reader_run,        cmd_processor_mf1_detect_nt_dist,            after_hf_reader_run    },
    {    DATA_CMD_MF1_NESTED_ACQUIRE,           before_hf_reader_run,        cmd_processor_mf1_nested_acquire,            after_hf_reader_run    },
    {    DATA_CMD_MF1_ENC_NESTED_ACQUIRE,       before_hf_reader_run,        cmd_processor_mf1_enc_nested_acquire,        after_hf_reader_run    },

    {    DATA_CMD_MF1_AUTH_ONE_KEY_BLOCK,       before_hf_reader_run,        cmd_processor_mf1_auth_one_key_block,        after_hf_reader_run    },
    {    DATA_CMD_MF1_READ_ONE_BLOCK,           before_hf_reader_run,        cmd_processor_mf1_read_one_block,            after_hf_reader_run    },
    {    DATA_CMD_MF1_READ_BLOCKS,              before_hf_reader_run,        cmd_processor_mf1_read_blocks,               after_hf_reader_run    },
    {    DATA_CMD_MF1_WRITE_ONE_BLOCK,          before_hf_reader_run,        cmd_processor_mf1_write_one_block,           after_hf_reader_run    },
    {    DATA_CMD_HF14A_RAW,                    before_reader_run,           cmd_processor_hf14a_raw,                     NULL                   },
    {    DATA_CMD_MF1_MANIPULATE_VALUE_BLOCK,   before_hf_reader_run,        cmd_processor_mf1_manipulate_value_block,    after_hf_reader_run    },
    {    DATA_CMD_MF1_CHECK_KEYS_OF_SECTORS,    before_hf_reader_run,        cmd_processor_mf1_check_keys_of_sectors,     after_hf_reader_run    },
    {    DATA_CMD_MF1_HARDNESTED_ACQUIRE,       before_hf_reader_run,        cmd_processor_mf1_hardnested_nonces_acquire, after_hf_reader_run    },
    {    DATA_CMD_MF1_CHECK_KEYS_ON_BLOCK,      before_hf_reader_run,        cmd_processor_mf1_check_keys_on_block,       after_hf_reader_run    },

    {    DATA_CMD_EM410X_SCAN,                  before_reader_run,           cmd_processor_em410x_scan,                   NULL                   },
    {    DATA_CMD_EM410X_WRITE_TO_T55XX,        before_reader_run,           cmd_processor_em410x_write_to_t55xx,         NULL                   },
    {    DATA_CMD_EM410X_ELECTRA_WRITE_TO_T55XX, before_reader_run,           cmd_processor_em410x_electra_write_to_t55xx, NULL                   },
    {    DATA_CMD_HIDPROX_SCAN,                 before_reader_run,           cmd_processor_hidprox_scan,                  NULL                   },
    {    DATA_CMD_HIDPROX_WRITE_TO_T55XX,       before_reader_run,           cmd_processor_hidprox_write_to_t55xx,        NULL                   },
    {    DATA_CMD_VIKING_SCAN,                  before_reader_run,           cmd_processor_viking_scan,                   NULL                   },
    {    DATA_CMD_VIKING_WRITE_TO_T55XX,        before_reader_run,           cmd_processor_viking_write_to_t55xx,         NULL                   },
    {    DATA_CMD_IOPROX_SCAN,                  before_reader_run,           cmd_processor_ioprox_scan,                   NULL                   },
    {    DATA_CMD_IOPROX_WRITE_TO_T55XX,        before_reader_run,           cmd_processor_ioprox_write_to_t55xx,         NULL                   },
    {    DATA_CMD_PAC_SCAN,                     before_reader_run,           cmd_processor_pac_scan,                      NULL                   },
    {    DATA_CMD_PAC_WRITE_TO_T55XX,           before_reader_run,           cmd_processor_pac_write_to_t55xx,            NULL                   },
    {    DATA_CMD_JABLOTRON_SCAN,               before_reader_run,           cmd_processor_jablotron_scan,                NULL                   },
    {    DATA_CMD_JABLOTRON_WRITE_TO_T55XX,     before_reader_run,           cmd_processor_jablotron_write_to_t55xx,      NULL                   },
    {    DATA_CMD_IDTECK_WRITE_TO_T55XX,        before_reader_run,           cmd_processor_idteck_write_to_t55xx,         NULL                   },
    {    DATA_CMD_LF_T55XX_WRITE,               before_reader_run,           cmd_processor_lf_t55xx_write,                NULL                   },
    {    DATA_CMD_ADC_GENERIC_READ,             before_reader_run,           cmd_processor_generic_read,                  NULL                   },

    {    DATA_CMD_HF14A_SET_FIELD_ON,           before_reader_run,           cmd_processor_hf14a_set_field_on,            NULL                   },
    {    DATA_CMD_HF14A_SET_FIELD_OFF,          before_reader_run,           cmd_processor_hf14a_set_field_off,           NULL                   },

    {    DATA_CMD_HF14A_GET_CONFIG,             NULL,                        cmd_processor_hf14a_get_config,              NULL                   },
    {    DATA_CMD_HF14A_SET_CONFIG,             NULL,                        cmd_processor_hf14a_set_config,              NULL                   },

    {    DATA_CMD_IOPROX_DECODE_RAW,            NULL,                        cmd_processor_ioprox_decode_raw,             NULL                   },
    {    DATA_CMD_IOPROX_COMPOSE_ID,            NULL,                        cmd_processor_ioprox_compose_id,             NULL                   },
    {    DATA_CMD_EM4X05_SCAN,                  before_reader_run,           cmd_processor_em4x05_scan,                   NULL                   },
    {    DATA_CMD_LF_SNIFF,                     before_reader_run,           cmd_processor_lf_sniff,                      NULL                   },
    {    DATA_CMD_HF14A_SNIFF,                  NULL,                        cmd_processor_hf14a_sniff,                   NULL                   },
    {    DATA_CMD_HF_CAPTURE_START,             NULL,                        cmd_processor_hf_capture_start,              NULL                   },
    {    DATA_CMD_HF_CAPTURE_STATUS,            NULL,                        cmd_processor_hf_capture_status,             NULL                   },
    {    DATA_CMD_HF_CAPTURE_GET,               NULL,                        cmd_processor_hf_capture_get,                NULL                   },
    {    DATA_CMD_HF_CAPTURE_STOP,              NULL,                        cmd_processor_hf_capture_stop,               NULL                   },
    {    DATA_CMD_HF14A_AUTH_TRACE,             before_hf_reader_run,        cmd_processor_hf14a_auth_trace,              after_hf_reader_run    },

#endif

    {    DATA_CMD_HF14A_GET_ANTI_COLL_DATA,     before_hf_emulator_loaded,   cmd_processor_hf14a_get_anti_coll_data,      NULL                   },
    {    DATA_CMD_HF14A_SET_ANTI_COLL_DATA,     before_hf_emulator_loaded,   cmd_processor_hf14a_set_anti_coll_data,      NULL                   },

    {    DATA_CMD_MF1_WRITE_EMU_BLOCK_DATA,     before_mf1_emulator_loaded,  cmd_processor_mf1_write_emu_block_data,      NULL                   },
    {    DATA_CMD_MF1_SET_DETECTION_ENABLE,     before_mf1_emulator_loaded,  cmd_processor_mf1_set_detection_enable,      NULL                   },
    {    DATA_CMD_MF1_GET_DETECTION_COUNT,      NULL,                        cmd_processor_mf1_get_detection_count,       NULL                   },
    {    DATA_CMD_MF1_GET_DETECTION_LOG,        NULL,                        cmd_processor_mf1_get_detection_log,         NULL                   },
    {    DATA_CMD_MF1_GET_DETECTION_ENABLE,     before_mf1_emulator_loaded,  cmd_processor_mf1_get_detection_enable,      NULL                   },
    {    DATA_CMD_MF1_READ_EMU_BLOCK_DATA,      before_mf1_emulator_loaded,  cmd_processor_mf1_read_emu_block_data,       NULL                   },
    {    DATA_CMD_MF1_GET_EMULATOR_CONFIG,      before_mf1_emulator_loaded,  cmd_processor_mf1_get_emulator_config,       NULL                   },
    {    DATA_CMD_MF1_GET_PRNG_TYPE,            before_mf1_emulator_loaded,  cmd_processor_mf1_get_prng_type,             NULL                   },
    {    DATA_CMD_MF1_SET_PRNG_TYPE,            before_mf1_emulator_loaded,  cmd_processor_mf1_set_prng_type,             NULL                   },
    {    DATA_CMD_MF1_SET_RANDOM_UID_MODE,      before_mf1_emulator_loaded,  cmd_processor_mf1_set_random_uid_mode,       NULL                   },
    {    DATA_CMD_MF1_GET_RANDOM_UID_MODE,      before_mf1_emulator_loaded,  cmd_processor_mf1_get_random_uid_mode,       NULL                   },
    {    DATA_CMD_MF1_SET_READER_KEYS_ANIM,     NULL,                        cmd_processor_mf1_set_reader_keys_anim,      NULL                   },
    {    DATA_CMD_MF1_GET_GEN1A_MODE,           before_mf1_emulator_loaded,  cmd_processor_mf1_get_gen1a_mode,            NULL                   },
    {    DATA_CMD_MF1_SET_GEN1A_MODE,           before_mf1_emulator_loaded,  cmd_processor_mf1_set_gen1a_mode,            NULL                   },
    {    DATA_CMD_MF1_GET_GEN2_MODE,            before_mf1_emulator_loaded,  cmd_processor_mf1_get_gen2_mode,             NULL                   },
    {    DATA_CMD_MF1_SET_GEN2_MODE,            before_mf1_emulator_loaded,  cmd_processor_mf1_set_gen2_mode,             NULL                   },
    {    DATA_CMD_MF1_GET_BLOCK_ANTI_COLL_MODE, before_mf1_emulator_loaded,  cmd_processor_mf1_get_block_anti_coll_mode,  NULL                   },
    {    DATA_CMD_MF1_SET_BLOCK_ANTI_COLL_MODE, before_mf1_emulator_loaded,  cmd_processor_mf1_set_block_anti_coll_mode,  NULL                   },
    {    DATA_CMD_MF1_GET_WRITE_MODE,           before_mf1_emulator_loaded,  cmd_processor_mf1_get_write_mode,            NULL                   },
    {    DATA_CMD_MF1_SET_WRITE_MODE,           before_mf1_emulator_loaded,  cmd_processor_mf1_set_write_mode,            NULL                   },
    {    DATA_CMD_MF1_GET_FIELD_OFF_DO_RESET,   before_mf1_emulator_loaded,  cmd_processor_mf1_get_field_off_do_reset,    NULL                   },
    {    DATA_CMD_MF1_SET_FIELD_OFF_DO_RESET,   before_mf1_emulator_loaded,  cmd_processor_mf1_set_field_off_do_reset,    NULL                   },

    {    DATA_CMD_MF0_NTAG_GET_UID_MAGIC_MODE,    before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_uid_mode,         NULL              },
    {    DATA_CMD_MF0_NTAG_SET_UID_MAGIC_MODE,    before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_uid_mode,         NULL              },
    {    DATA_CMD_MF0_NTAG_READ_EMU_PAGE_DATA,    before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_read_emu_page_data,   NULL              },
    {    DATA_CMD_MF0_NTAG_WRITE_EMU_PAGE_DATA,   before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_write_emu_page_data,  NULL              },
    {    DATA_CMD_MF0_NTAG_GET_VERSION_DATA,      before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_version_data,     NULL              },
    {    DATA_CMD_MF0_NTAG_SET_VERSION_DATA,      before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_version_data,     NULL              },
    {    DATA_CMD_MF0_NTAG_GET_SIGNATURE_DATA,    before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_signature_data,   NULL              },
    {    DATA_CMD_MF0_NTAG_SET_SIGNATURE_DATA,    before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_signature_data,   NULL              },
    {    DATA_CMD_MF0_NTAG_GET_COUNTER_DATA,      before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_counter_data,     NULL              },
    {    DATA_CMD_MF0_NTAG_SET_COUNTER_DATA,      before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_counter_data,     NULL              },
    {    DATA_CMD_MF0_NTAG_RESET_AUTH_CNT,        before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_reset_auth_cnt,       NULL              },
    {    DATA_CMD_MF0_NTAG_GET_PAGE_COUNT,        before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_emu_page_count,   NULL              },
    {    DATA_CMD_MF0_NTAG_GET_WRITE_MODE,        before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_write_mode,       NULL              },
    {    DATA_CMD_MF0_NTAG_SET_WRITE_MODE,        before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_write_mode,       NULL              },
    {    DATA_CMD_MF0_NTAG_SET_DETECTION_ENABLE,  before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_set_detection_enable, NULL              },
    {    DATA_CMD_MF0_NTAG_GET_DETECTION_COUNT,   NULL,                            cmd_processor_mf0_ntag_get_detection_count,  NULL              },
    {    DATA_CMD_MF0_NTAG_GET_DETECTION_LOG,     NULL,                            cmd_processor_mf0_ntag_get_detection_log,    NULL              },
    {    DATA_CMD_MF0_NTAG_GET_DETECTION_ENABLE,  before_mf0_ntag_emulator_loaded, cmd_processor_mf0_ntag_get_detection_enable, NULL              },
    {    DATA_CMD_MF0_NTAG_GET_EMULATOR_CONFIG,   before_mf0_ntag_emulator_loaded, cmd_processor_mf0_get_emulator_config,       NULL              },

    {    DATA_CMD_EM410X_SET_EMU_ID,              NULL,                      cmd_processor_em410x_set_emu_id,             NULL                   },
    {    DATA_CMD_EM410X_GET_EMU_ID,              NULL,                      cmd_processor_em410x_get_emu_id,             NULL                   },
    {    DATA_CMD_HIDPROX_SET_EMU_ID,             NULL,                      cmd_processor_hidprox_set_emu_id,            NULL                   },
    {    DATA_CMD_HIDPROX_GET_EMU_ID,             NULL,                      cmd_processor_hidprox_get_emu_id,            NULL                   },
    {    DATA_CMD_IOPROX_SET_EMU_ID,              NULL,                      cmd_processor_ioprox_set_emu_id,             NULL                   },
    {    DATA_CMD_IOPROX_GET_EMU_ID,              NULL,                      cmd_processor_ioprox_get_emu_id,             NULL                   },
    {    DATA_CMD_VIKING_SET_EMU_ID,              NULL,                      cmd_processor_viking_set_emu_id,             NULL                   },
    {    DATA_CMD_VIKING_GET_EMU_ID,              NULL,                      cmd_processor_viking_get_emu_id,             NULL                   },
    {    DATA_CMD_PAC_SET_EMU_ID,                 NULL,                      cmd_processor_pac_set_emu_id,                NULL                   },
    {    DATA_CMD_PAC_GET_EMU_ID,                 NULL,                      cmd_processor_pac_get_emu_id,                NULL                   },
    {    DATA_CMD_JABLOTRON_SET_EMU_ID,           NULL,                      cmd_processor_jablotron_set_emu_id,          NULL                   },
    {    DATA_CMD_JABLOTRON_GET_EMU_ID,           NULL,                      cmd_processor_jablotron_get_emu_id,          NULL                   },
    {    DATA_CMD_IDTECK_SET_EMU_ID,              NULL,                      cmd_processor_idteck_set_emu_id,             NULL                   },
    {    DATA_CMD_IDTECK_GET_EMU_ID,              NULL,                      cmd_processor_idteck_get_emu_id,             NULL                   },
#if defined(PROJECT_CHAMELEON_ULTRA)
    /* ISO14443-4 T=CL emulation */
    {    DATA_CMD_HF14A_4_APDU_RECV,              before_hf14a_4_emulator_loaded, cmd_processor_hf14a_4_apdu_recv,          NULL                   },
    {    DATA_CMD_HF14A_4_APDU_SEND,              before_hf14a_4_emulator_loaded, cmd_processor_hf14a_4_apdu_send,          NULL                   },
    {    DATA_CMD_HF14A_4_SET_ANTI_COLL,          before_hf14a_4_emulator_loaded, cmd_processor_hf14a_4_set_anti_coll,      NULL                   },
    {    DATA_CMD_HF14A_4_STATIC_RESP,            before_hf14a_4_emulator_loaded, cmd_processor_hf14a_4_static_resp,        NULL                   },
    {    DATA_CMD_HF14A_4_READER_APDU,            before_hf_reader_run,        cmd_processor_hf14a_4_reader_apdu,           NULL                   },
    {    DATA_CMD_HF14A_4_EMV_SCAN,               before_hf_reader_run,        cmd_processor_hf14a_4_emv_scan,              after_hf_reader_run    },
    {    DATA_CMD_HF14A_4_DESFIRE_SCAN,           before_hf_reader_run,        cmd_processor_hf14a_4_desfire_scan,          after_hf_reader_run    },
    {    DATA_CMD_HF14A_4_EMV_TRACE_START,        before_hf_reader_run,        cmd_processor_hf14a_4_emv_trace_start,       after_hf_reader_run    },
    {    DATA_CMD_HF14A_4_EMV_TRACE_META,         NULL,                        cmd_processor_hf14a_4_emv_trace_meta,        NULL                   },
    {    DATA_CMD_HF14A_4_EMV_TRACE_GET,          NULL,                        cmd_processor_hf14a_4_emv_trace_get,         NULL                   },
    {    DATA_CMD_HF14A_4_DEBUG_COUNTERS,         NULL,                        cmd_processor_hf14a_4_debug_counters,         NULL                   },
    {    DATA_CMD_HF14A_4_READER_SESSION_START,   before_reader_run,           cmd_processor_hf14a_4_reader_session_start,  NULL                   },
    {    DATA_CMD_HF14A_4_READER_SESSION_START_APPLE_TRANSIT, before_reader_run, cmd_processor_hf14a_4_reader_session_start, NULL                  },
    {    DATA_CMD_HF14A_4_READER_SESSION_EXCHANGE, NULL,                       cmd_processor_hf14a_4_reader_session_exchange, NULL                  },
    {    DATA_CMD_HF14A_4_READER_SESSION_STOP,    NULL,                        cmd_processor_hf14a_4_reader_session_stop,   NULL                   },
    /* HF14A scan keeping field alive */
    {    DATA_CMD_HF14A_SCAN_KEEP,                before_hf_reader_run,        cmd_processor_hf14a_scan_keep,               NULL                   },
#endif
};
data_frame_tx_t *cmd_processor_get_device_capabilities(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    size_t count = ARRAYLEN(m_data_cmd_map);
    uint16_t commands[count];
    memset(commands, 0, count * sizeof(uint16_t));

    for (size_t i = 0; i < count; i++) {
        commands[i] = U16HTONS(m_data_cmd_map[i].cmd);
    }

    return data_frame_make(cmd, STATUS_SUCCESS, count * sizeof(uint16_t), (uint8_t *)commands);
}

/**
 * @brief Auto select source to response
 *
 * @param resp data
 */
static void auto_response_data(data_frame_tx_t *resp) {
    data_frame_transport_t transport = data_frame_get_transport();
    if (transport == DATA_FRAME_TRANSPORT_USB && is_usb_working()) {
        usb_cdc_write(resp->buffer, resp->length);
    } else if (transport == DATA_FRAME_TRANSPORT_BLE && is_nus_working()) {
        nus_data_response(resp->buffer, resp->length);
    } else if (transport == DATA_FRAME_TRANSPORT_NONE && is_usb_working()) {
        usb_cdc_write(resp->buffer, resp->length);
    } else if (transport == DATA_FRAME_TRANSPORT_NONE && is_nus_working()) {
        nus_data_response(resp->buffer, resp->length);
    } else {
        NRF_LOG_ERROR("Request transport is no longer available.");
    }
}

#if defined(PROJECT_CHAMELEON_ULTRA)
static bool hf_capture_blocks_command(uint16_t cmd,
                                      data_frame_transport_t transport) {
    if (!hf_capture_is_active()) return false;
    if (transport != hf_capture_owner()) {
        return cmd != DATA_CMD_HF_CAPTURE_STATUS;
    }
    switch (cmd) {
        case DATA_CMD_CHANGE_DEVICE_MODE:
        case DATA_CMD_SET_ACTIVE_SLOT:
        case DATA_CMD_SET_SLOT_TAG_TYPE:
        case DATA_CMD_SET_SLOT_DATA_DEFAULT:
        case DATA_CMD_SET_SLOT_ENABLE:
        case DATA_CMD_SLOT_DATA_CONFIG_SAVE:
        case DATA_CMD_WIPE_FDS:
        case DATA_CMD_DELETE_SLOT_SENSE_TYPE:
        case DATA_CMD_ACTIVE_SLOT_SNAPSHOT:
            return true;
        default:
            return false;
    }
}
#endif

void app_cmd_hf_capture_process(void) {
#if defined(PROJECT_CHAMELEON_ULTRA)
    hf_capture_clock_process();
    if (!hf_capture_notification_due()) return;

    data_frame_transport_t owner = hf_capture_owner();
    bool ready = (owner == DATA_FRAME_TRANSPORT_USB && is_usb_working() &&
                  is_usb_tx_idle()) ||
                 (owner == DATA_FRAME_TRANSPORT_BLE && is_nus_working() &&
                  is_nus_tx_idle());
    if (!ready) return;

    uint16_t payload_length = hf_capture_build_meta(
                                  hf_capture_session_id(),
                                  m_hf_capture_response,
                                  sizeof(m_hf_capture_response));
    if (payload_length == 0u) return;
    data_frame_tx_t *event = data_frame_make(DATA_CMD_HF_CAPTURE_EVENT,
                                             STATUS_SUCCESS,
                                             payload_length,
                                             m_hf_capture_response);
    if (event == NULL) return;
    uint32_t result = owner == DATA_FRAME_TRANSPORT_USB ?
                      usb_cdc_write_try(event->buffer, event->length) :
                      nus_data_response_try(event->buffer, event->length);
    if (result == NRF_SUCCESS) hf_capture_notification_sent();
#endif
}


/**@brief Function to process data frame(cmd)
 */
void on_data_frame_received(uint16_t cmd, uint16_t status, uint16_t length, uint8_t *data) {
    data_frame_tx_t *response = NULL;
    bool is_cmd_support = false;
    data_frame_transport_t transport = data_frame_get_transport();
    if (transport == DATA_FRAME_TRANSPORT_BLE &&
            settings_get_ble_pairing_enable() &&
            !ble_command_link_authorized()) {
        response = data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
        auto_response_data(response);
        return;
    }
    app_cmd_active_slot_snapshot_process();
#if defined(PROJECT_CHAMELEON_ULTRA)
    if (hf_capture_blocks_command(cmd, transport)) {
        response = data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
        auto_response_data(response);
        return;
    }
    if (iso_dep_session_is_active() &&
            !iso_dep_session_is_current_transport_owner()) {
        response = data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
        auto_response_data(response);
        return;
    }
#endif
    if (m_active_slot_snapshot.active) {
        if ((uint8_t)transport != m_active_slot_snapshot.owner) {
            response = data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
            auto_response_data(response);
            return;
        }
        if (cmd != DATA_CMD_ACTIVE_SLOT_SNAPSHOT) {
            uint16_t block_count = active_slot_snapshot_block_count(
                                       (tag_specific_type_t)m_active_slot_snapshot.tag_type);
            if (!active_slot_snapshot_is_read_command(cmd)) {
                response = data_frame_make(cmd, STATUS_DEVICE_MODE_ERROR, 0, NULL);
                auto_response_data(response);
                return;
            }
            if (!active_slot_snapshot_read_request_valid(
                        cmd, length, data, block_count)) {
                response = data_frame_make(cmd, STATUS_PAR_ERR, 0, NULL);
                auto_response_data(response);
                return;
            }
            active_slot_snapshot_transaction_refresh(
                &m_active_slot_snapshot, (uint8_t)transport, app_timer_cnt_get());
        }
    }
    for (int i = 0; i < ARRAY_SIZE(m_data_cmd_map); i++) {
        if (m_data_cmd_map[i].cmd == cmd) {
            is_cmd_support = true;
            if (m_data_cmd_map[i].cmd_before != NULL) {
                data_frame_tx_t *before_resp = m_data_cmd_map[i].cmd_before(cmd, status, length, data);
                if (before_resp != NULL) {
                    // some problem found before run cmd.
                    response = before_resp;
                    break;
                }
            }
            if (m_data_cmd_map[i].cmd_processor != NULL) response = m_data_cmd_map[i].cmd_processor(cmd, status, length, data);
            if (m_data_cmd_map[i].cmd_after != NULL) {
                data_frame_tx_t *after_resp = m_data_cmd_map[i].cmd_after(cmd, status, length, data);
                if (after_resp != NULL) {
                    // some problem found after run cmd.
                    response = after_resp;
                    break;
                }
            }
            break;
        }
    }
    if (is_cmd_support) {
        // check and response
        if (response != NULL) {
            auto_response_data(response);
        }
    } else {
        // response cmd unsupported.
        response = data_frame_make(cmd, STATUS_INVALID_CMD, 0, NULL);
        auto_response_data(response);
        NRF_LOG_INFO("Data frame cmd invalid: %d,", cmd);
    }
}
