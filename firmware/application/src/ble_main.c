#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"

#include "nrf.h"
#include "nordic_common.h"
#include "nrf_sdh.h"
#include "nrf_ble_qwr.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_lesc.h"
#include "ble_conn_state.h"

#include "peer_manager.h"
#include "peer_manager_handler.h"

#include "app_timer.h"
#include "app_util_platform.h"

#include "syssleep.h"
#include "ble_main.h"
#include "ble_scan.h"
#include "ble_central.h"
#include "dataframe.h"
#include "netdata.h"
#include "hw_connect.h"
#include "settings.h"
#include "rgb_marquee.h"
#include "keyboard_hid.h"
#include "keyboard_payload.h"
#include "app_cmd.h"
#if defined(PROJECT_CHAMELEON_ULTRA)
#include "iso_dep_session.h"
#endif

#define NRF_LOG_MODULE_NAME ble_main
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();


#define LESC_DEBUG_MODE                 0                                           /**< Set to 1 to use the LESC debug keys. The debug mode allows you to use a sniffer to inspect traffic. */
#define LESC_MITM_NC                    1                                           /**< Use MITM (Numeric Comparison). */

#define SEC_PARAMS_BOND                 1                                           /**< Perform bonding. */
#if LESC_MITM_NC
#define SEC_PARAMS_MITM                 1                                           /**< Man In The Middle protection required. */
#define SEC_PARAMS_IO_CAPABILITIES      BLE_GAP_IO_CAPS_DISPLAY_ONLY
#else
#define SEC_PARAMS_MITM                 0                                           /**< Man In The Middle protection required. */
#define SEC_PARAMS_IO_CAPABILITIES      BLE_GAP_IO_CAPS_DISPLAY_ONLY
#endif
#define SEC_PARAMS_LESC                 1                                           /**< LE Secure Connections pairing required. */
#define SEC_PARAMS_KEYPRESS             0                                           /**< Keypress notifications not required. */
#define SEC_PARAMS_OOB                  0                                           /**< Out Of Band data not available. */
#define SEC_PARAMS_MIN_KEY_SIZE         16                                          /**< Require full-length encryption keys. */
#define SEC_PARAMS_MAX_KEY_SIZE         16                                          /**< Maximum encryption key size in octets. */

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(7.5, UNIT_1_25_MS)            /**< Minimum acceptable connection interval (7.5 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(15, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (15 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define BATTERY_LEVEL_MEAS_INTERVAL     APP_TIMER_TICKS(5000)                       /**< Battery level measurement interval (ticks). This value corresponds to N seconds. */

#define ADC_REF_VOLTAGE_IN_MILLIVOLTS  600  //!< Reference voltage (in milli volts) used by ADC while doing conversion.
#define ADC_RES_12BIT                  16383 //!< Maximum digital value for 14-bit ADC conversion.
#define ADC_PRE_SCALING_COMPENSATION   12    //!< The ADC is configured to use VDD with 1/3 prescaling as input. And hence the result of conversion is to be multiplied by 3 to get the actual value of the battery voltage.
#define ADC_CHANNEL                    0
#define ADC_BUF_SIZE                   2048
#define ADC_BUF_COUNT                  2


/**@brief Macro to convert the result of ADC conversion in millivolts.
 *
 * @param[in]  ADC_VALUE   ADC result.
 *
 * @retval     Result converted to millivolts.
 */
#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE)\
        ((((ADC_VALUE) * ADC_REF_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_12BIT) * ADC_PRE_SCALING_COMPENSATION)

APP_TIMER_DEF(m_battery_timer_id);                                                  /**< Battery measurement timer. */
BLE_BAS_DEF(m_bas);                                                                 /**< Battery service instance. */
BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

uint16_t          batt_lvl_in_milli_volts = 0;
uint8_t           percentage_batt_lvl = 0;
static nrf_saadc_value_t adc_buf[ADC_BUF_COUNT][ADC_BUF_SIZE];
static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
lf_adc_callback_t m_lf_adc_callback      = NULL;

static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE},
    {BLE_UUID_BATTERY_SERVICE, BLE_UUID_TYPE_BLE},
    // Keep the HID UUID LAST: advertised only when the keyboard feature is on
    // (see adv_uuids_count()). Reordering breaks that gating.
    {BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE},
};

// Number of service UUIDs to advertise. The trailing HID UUID is only exposed
// when the opt-in keyboard feature is enabled, so an idle device advertises the
// exact same set it did before the HID service existed. Uses the boot-time
// value so advertising always matches the HID service actually registered at
// boot (toggling the setting requires a reboot to take effect).
static uint8_t adv_uuids_count(void) {
    uint8_t count = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    if (!settings_get_keyboard_hid_enable_first_load()) {
        count--;
    }
    return count;
}
volatile bool g_is_ble_connected = false;
volatile bool g_is_low_battery_shutdown = false;
volatile bool g_is_ble_advertising = false;
static volatile bool g_ble_radio_on = true;
static ble_opt_t m_static_pin_option;
static uint8_t m_peripheral_name[BLE_TEMPORARY_NAME_MAX_LENGTH] = DEVICE_NAME_STR;
static uint8_t m_peripheral_name_length = sizeof(DEVICE_NAME_STR) - 1u;

#define BLE_ADV_FLOOD_MAX_INTERVAL_MS 10240u
static volatile uint8_t m_adv_flood_state = 0;
static uint8_t m_adv_flood_payload[31];
static uint16_t m_adv_flood_interval = 0;

static volatile uint8_t m_adv_lab_state;
static uint8_t m_adv_lab_profile;
static uint8_t m_adv_lab_mode;
static uint8_t m_adv_lab_reason;
static uint8_t m_adv_lab_name_target;
static uint8_t m_adv_lab_name_index = 0xffu;
static uint8_t m_adv_lab_name_count;
static uint8_t m_adv_lab_name_lengths[BLE_ADV_LAB_MAX_NAMES];
static uint8_t m_adv_lab_names[BLE_ADV_LAB_MAX_NAMES][BLE_ADV_LAB_MAX_NAME_LENGTH];
static uint8_t m_adv_lab_base_adv[31];
static uint8_t m_adv_lab_base_scan[31];
static uint8_t m_adv_lab_base_adv_length;
static uint8_t m_adv_lab_base_scan_length;
static uint8_t m_adv_lab_adv[2][31];
static uint8_t m_adv_lab_scan[2][31];
static uint8_t m_adv_lab_active_buffer;
static uint16_t m_adv_lab_interval_units;
static uint16_t m_adv_lab_rotation_ms;
static uint16_t m_adv_lab_duration_units;
static uint8_t m_adv_lab_max_events;
static uint32_t m_adv_lab_rotation_count;
static uint32_t m_adv_lab_rotation_period_ticks;
static uint32_t m_adv_lab_rotation_elapsed_ticks;
static uint32_t m_adv_lab_rotation_last_tick;
static bool m_adv_lab_restore_normal;
static bool m_adv_lab_restart_after_disconnect;
static ble_adv_modes_config_t m_adv_lab_saved_modes;
static bool m_peer_delete_pending;

static void adv_lab_restore_modes(bool allow_disconnect_restart);
static uint32_t adv_lab_finish(uint8_t reason, bool restore_normal);

#define NUS_TX_QUEUE_DEPTH 2
typedef struct {
    volatile bool valid;
    uint16_t length;
    uint8_t data[NETDATA_MAX_FRAME_LENGTH];
} nus_tx_entry_t;

static nus_tx_entry_t m_nus_tx_queue[NUS_TX_QUEUE_DEPTH];
static uint8_t m_nus_tx_head;
static uint8_t m_nus_tx_count;
static uint16_t m_nus_tx_offset;
static volatile bool m_nus_tx_sending;
static volatile bool m_nus_tx_pending;
static volatile bool m_nus_hvn_inflight;
static volatile uint32_t m_nus_tx_generation;
static bool m_nus_comm_started;
static uint8_t m_nus_rx_pending[BLE_NUS_MAX_DATA_LEN];
static uint16_t m_nus_rx_length;
static uint16_t m_nus_rx_offset;
static volatile uint32_t m_nus_rx_generation;
static uint32_t m_nus_rx_decoder_generation;

static bool nus_response_ready(void) {
    return g_is_ble_connected && m_nus_comm_started && m_nus_tx_count < NUS_TX_QUEUE_DEPTH;
}

// Simple function to provide an index to the next input buffer
// Will simply alernate between 0 and 1 when SAADC_BUF_COUNT is 2
static uint32_t next_free_buf_index(void) {
    static uint32_t buffer_index = -1;
    buffer_index = (buffer_index + 1) % ADC_BUF_COUNT;
    return buffer_index;
}


/**@brief Function for the ble connect key setup.
 *
 * @details This function will set up the ble connect passkey.
 */
void set_ble_connect_key(uint8_t *key) {
    static uint8_t passkey[BLE_PAIRING_KEY_LEN];
    memcpy(passkey, key, BLE_PAIRING_KEY_LEN);
    m_static_pin_option.gap_opt.passkey.p_passkey = passkey;
    // NRF_LOG_RAW_HEXDUMP_INFO(passkey, 6);
    APP_ERROR_CHECK(sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &m_static_pin_option));
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void) {
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode, m_peripheral_name,
                                          m_peripheral_name_length);
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Battery Service events.
 *
 * @details This function will be called for all Battery Service events which are passed to the
 |          application.
 *
 * @param[in] p_bas  Battery Service structure.
 * @param[in] p_evt  Event received from the Battery Service.
 */
static void on_bas_evt(ble_bas_t *p_bas, ble_bas_evt_t *p_evt) {
    switch (p_evt->evt_type) {
        case BLE_BAS_EVT_NOTIFICATION_ENABLED:
            break; // BLE_BAS_EVT_NOTIFICATION_ENABLED

        case BLE_BAS_EVT_NOTIFICATION_DISABLED:
            break; // BLE_BAS_EVT_NOTIFICATION_DISABLED

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_tx_clear(void) {
    CRITICAL_REGION_ENTER();
    m_nus_tx_generation++;
    for (uint8_t i = 0; i < NUS_TX_QUEUE_DEPTH; i++) {
        m_nus_tx_queue[i].valid = false;
    }
    m_nus_tx_head = 0;
    m_nus_tx_count = 0;
    m_nus_tx_offset = 0;
    m_nus_hvn_inflight = false;
    CRITICAL_REGION_EXIT();
}

static void nus_tx_send(void) {
    uint32_t generation;
    bool retry;
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    if (m_nus_tx_sending) {
        m_nus_tx_pending = true;
        app_util_critical_region_exit(nested);
        return;
    }
    m_nus_tx_sending = true;
    generation = m_nus_tx_generation;
    app_util_critical_region_exit(nested);

    while (g_is_ble_connected && m_nus_comm_started) {
        nus_tx_entry_t *entry;
        uint16_t chunk_len;
        uint8_t *chunk;
        app_util_critical_region_enter(&nested);
        if (generation != m_nus_tx_generation || m_nus_tx_count == 0) {
            app_util_critical_region_exit(nested);
            break;
        }
        entry = &m_nus_tx_queue[m_nus_tx_head];
        if (!entry->valid) {
            app_util_critical_region_exit(nested);
            break;
        }
        chunk_len = MIN(m_ble_nus_max_data_len, entry->length - m_nus_tx_offset);
        chunk = entry->data + m_nus_tx_offset;
        app_util_critical_region_exit(nested);
        ret_code_t err = ble_nus_data_send(&m_nus, chunk, &chunk_len, m_conn_handle);
        app_util_critical_region_enter(&nested);
        if (generation != m_nus_tx_generation || !entry->valid) {
            app_util_critical_region_exit(nested);
            break;
        }
        if (err == NRF_SUCCESS) {
            m_nus_hvn_inflight = true;
            m_nus_tx_offset += chunk_len;
            if (m_nus_tx_offset == entry->length) {
                entry->valid = false;
                m_nus_tx_head = (m_nus_tx_head + 1) % NUS_TX_QUEUE_DEPTH;
                m_nus_tx_count--;
                m_nus_tx_offset = 0;
            }
            app_util_critical_region_exit(nested);
            continue;
        }
        if (err == NRF_ERROR_BUSY || err == NRF_ERROR_RESOURCES) {
            app_util_critical_region_exit(nested);
            break;
        }
        app_util_critical_region_exit(nested);

        NRF_LOG_WARNING("BLE NUS response cancelled at %u/%u: 0x%x",
                        m_nus_tx_offset, entry->length, err);
        if (err == NRF_ERROR_INVALID_STATE || err == NRF_ERROR_NOT_FOUND) {
            nus_tx_clear();
            break;
        }
        app_util_critical_region_enter(&nested);
        if (generation != m_nus_tx_generation || !entry->valid) {
            app_util_critical_region_exit(nested);
            break;
        }
        entry->valid = false;
        m_nus_tx_head = (m_nus_tx_head + 1) % NUS_TX_QUEUE_DEPTH;
        m_nus_tx_count--;
        m_nus_tx_offset = 0;
        app_util_critical_region_exit(nested);
    }
    app_util_critical_region_enter(&nested);
    m_nus_tx_sending = false;
    retry = m_nus_tx_pending;
    m_nus_tx_pending = false;
    app_util_critical_region_exit(nested);
    if (retry) {
        nus_tx_send();
    }
}

static void nus_rx_clear(void) {
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    m_nus_rx_generation++;
    m_nus_rx_length = 0;
    m_nus_rx_offset = 0;
    app_util_critical_region_exit(nested);
}

static void nus_rx_resume(void) {
    uint8_t nested = 0;
    uint16_t length;
    uint16_t offset;
    uint32_t generation;
    uint32_t decoder_generation;
    app_util_critical_region_enter(&nested);
    length = m_nus_rx_length;
    offset = m_nus_rx_offset;
    generation = m_nus_rx_generation;
    decoder_generation = m_nus_rx_decoder_generation;
    app_util_critical_region_exit(nested);
    if (length == 0) {
        return;
    }
    uint16_t consumed = data_frame_receive_from_generation(m_nus_rx_pending + offset,
                                                           length - offset,
                                                           DATA_FRAME_TRANSPORT_BLE,
                                                           decoder_generation);
    app_util_critical_region_enter(&nested);
    if (generation != m_nus_rx_generation) {
        app_util_critical_region_exit(nested);
        return;
    }
    m_nus_rx_offset = offset + consumed;
    if (m_nus_rx_offset == length) {
        m_nus_rx_length = 0;
        m_nus_rx_offset = 0;
    }
    app_util_critical_region_exit(nested);
}

static void nus_rx_overrun(void) {
    NRF_LOG_WARNING("BLE NUS ingress queue overrun; disconnecting peer");
#if defined(PROJECT_CHAMELEON_ULTRA)
    iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_BLE);
#endif
    app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_BLE);
    m_nus_comm_started = false;
    nus_rx_clear();
    nus_tx_clear();
    data_frame_reset_transport(DATA_FRAME_TRANSPORT_BLE);
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
        ret_code_t err = sd_ble_gap_disconnect(m_conn_handle,
                                               BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
            NRF_LOG_WARNING("BLE NUS overrun disconnect failed: 0x%x", err);
        }
    }
}

static void nus_data_handler(ble_nus_evt_t *p_evt) {
    if (p_evt->conn_handle != m_conn_handle) {
        return;
    }
    if (p_evt->type == BLE_NUS_EVT_COMM_STARTED) {
        m_nus_comm_started = true;
        nus_tx_send();
        return;
    }
    if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED) {
#if defined(PROJECT_CHAMELEON_ULTRA)
        iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_BLE);
#endif
        app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_BLE);
        m_nus_comm_started = false;
        nus_rx_clear();
        nus_tx_clear();
        data_frame_reset_transport(DATA_FRAME_TRANSPORT_BLE);
        return;
    }
    if (p_evt->type == BLE_NUS_EVT_RX_DATA) {
        if (!m_nus_comm_started) {
            return;
        }
        NRF_LOG_DEBUG("Received data from BLE NUS.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        if (m_nus_rx_length != 0 || p_evt->params.rx_data.length > sizeof(m_nus_rx_pending)) {
            nus_rx_overrun();
            return;
        }
        memcpy(m_nus_rx_pending, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        m_nus_rx_decoder_generation =
            data_frame_get_transport_generation(DATA_FRAME_TRANSPORT_BLE);
        m_nus_rx_length = p_evt->params.rx_data.length;
        m_nus_rx_offset = 0;
        nus_rx_resume();
    } else if (p_evt->type == BLE_NUS_EVT_TX_RDY) {
        uint8_t nested = 0;
        app_util_critical_region_enter(&nested);
        m_nus_hvn_inflight = false;
        app_util_critical_region_exit(nested);
        nus_tx_send();
    }
}
/**@snippet [Handling the data received over BLE] */

uint32_t nus_data_response_try(const uint8_t *p_data, uint16_t length) {
    if (p_data == NULL || length == 0 || length > NETDATA_MAX_FRAME_LENGTH) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (!g_is_ble_connected || !m_nus_comm_started) {
        return NRF_ERROR_INVALID_STATE;
    }
    uint8_t tail;
    uint32_t generation;
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    if (m_nus_tx_count >= NUS_TX_QUEUE_DEPTH ||
            !data_frame_current_transport_generation_valid()) {
        app_util_critical_region_exit(nested);
        return m_nus_tx_count >= NUS_TX_QUEUE_DEPTH
               ? NRF_ERROR_RESOURCES : NRF_ERROR_INVALID_STATE;
    }

    tail = (m_nus_tx_head + m_nus_tx_count) % NUS_TX_QUEUE_DEPTH;
    generation = m_nus_tx_generation;
    m_nus_tx_queue[tail].valid = false;
    m_nus_tx_count++;
    app_util_critical_region_exit(nested);

    memcpy(m_nus_tx_queue[tail].data, p_data, length);
    app_util_critical_region_enter(&nested);
    if (generation != m_nus_tx_generation ||
            !data_frame_current_transport_generation_valid()) {
        if (generation == m_nus_tx_generation && m_nus_tx_count != 0u) {
            uint8_t reserved_tail = (m_nus_tx_head + m_nus_tx_count - 1u) % NUS_TX_QUEUE_DEPTH;
            if (reserved_tail == tail) m_nus_tx_count--;
        }
        app_util_critical_region_exit(nested);
        return NRF_ERROR_INVALID_STATE;
    }
    m_nus_tx_queue[tail].length = length;
    __DMB();
    m_nus_tx_queue[tail].valid = true;
    app_util_critical_region_exit(nested);
    nus_tx_send();
    return NRF_SUCCESS;
}

void nus_data_response(uint8_t *p_data, uint16_t length) {
    NRF_LOG_INFO("BLE nus service response data length: %d", length);
    NRF_LOG_HEXDUMP_DEBUG(p_data, length);
    ret_code_t err = nus_data_response_try(p_data, length);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("BLE NUS response not queued: 0x%x", err);
    }
}

bool is_nus_working(void) {
    return g_is_ble_connected && m_nus_comm_started;
}

bool is_nus_tx_idle(void) {
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    bool idle = m_nus_tx_count == 0 && !m_nus_tx_sending &&
                !m_nus_tx_pending && !m_nus_hvn_inflight;
    app_util_critical_region_exit(nested);
    return idle;
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}

__INLINE uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
    return (uint32_t)(MIN((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min, out_max));
}

//Battery voltage to percentage calculation
uint32_t BATVOL2PERCENT(uint16_t VOL) {
    // Based on https://github.com/RfidResearchGroup/ChameleonUltra/issues/167#issuecomment-1766908799

#if defined(PROJECT_CHAMELEON_ULTRA)
// Ultra
#define P100VOL 4200
#define P80VOL  4034
#define P60VOL  3904
#define P40VOL  3824
#define P20VOL  3754
#define P5VOL   3644
#else
// Lite
#define P100VOL 4200
#define P80VOL  3934
#define P60VOL  3844
#define P40VOL  3784
#define P20VOL  3744
#define P5VOL   3644
#endif

    if (VOL > P80VOL) {
        //80-100
        return map(VOL, P80VOL, P100VOL, 80, 100);
    } else if (VOL > P60VOL) {
        //60-80
        return map(VOL, P60VOL, P80VOL, 60, 80);
    } else if (VOL > P40VOL) {
        //40-60
        return map(VOL, P40VOL, P60VOL, 40, 60);
    } else if (VOL > P20VOL) {
        //20-60
        return map(VOL, P20VOL, P40VOL, 20, 40);
    } else if (VOL > P5VOL) {
        //5-20
        return map(VOL, P5VOL, P20VOL, 5, 20);
    } else {
        //<5
        return 0;
    }
}

/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void) {
    uint32_t           err_code;

    // -------------------------------------------------------------
    // Initialize Queued Write Module.
    nrf_ble_qwr_init_t qwr_init = {0};
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);


    // -------------------------------------------------------------
    // Initialize NUS.
    ble_nus_init_t     nus_init;
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
    data_frame_set_flow_callback(DATA_FRAME_TRANSPORT_BLE, nus_rx_resume);
    data_frame_set_ready_callback(DATA_FRAME_TRANSPORT_BLE, nus_response_ready);

    // -------------------------------------------------------------
    // battery service

    ble_bas_init_t bas_init_obj;

    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler          = on_bas_evt;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref         = NULL;
    bas_init_obj.initial_batt_level   = 100;

    if (settings_get_ble_pairing_enable_first_load()) {
        bas_init_obj.bl_rd_sec        = SEC_MITM;
        bas_init_obj.bl_cccd_wr_sec   = SEC_MITM;
        bas_init_obj.bl_report_rd_sec = SEC_MITM;
    } else {
        bas_init_obj.bl_rd_sec        = SEC_OPEN;
        bas_init_obj.bl_cccd_wr_sec   = SEC_OPEN;
        bas_init_obj.bl_report_rd_sec = SEC_OPEN;
    }

    err_code = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err_code);

    // Keyboard HID is opt-in: only register the GATT service at boot when the
    // feature is enabled, and never assert on failure. A full attribute table
    // (NRF_ERROR_NO_MEM) must not turn into a boot loop that breaks USB and BLE.
    if (settings_get_keyboard_hid_enable_first_load()) {
        uint32_t hid_err = keyboard_hid_ble_ensure_registered();
        if (hid_err != NRF_SUCCESS) {
            NRF_LOG_ERROR("Keyboard HID service registration failed: 0x%08x", hid_err);
        }
    }
}

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt) {
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error) {
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void) {
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt) {
    switch (ble_adv_evt) {
        case BLE_ADV_EVT_FAST:
            g_is_ble_advertising = true;
            NRF_LOG_INFO("BLE_ADV_EVT_FAST");
            break;
        case BLE_ADV_EVT_IDLE:
            g_is_ble_advertising = false;
            NRF_LOG_INFO("BLE_ADV_EVT_IDLE");
            break;
        default:
            break;
    }
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context) {
    ret_code_t err_code;

    keyboard_hid_on_ble_evt(p_ble_evt);

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            // Only the peripheral (app/NUS) link is handled here. Central links
            // to a fuzzing target are handled in ble_central.c — ignore them so
            // we don't clobber the app connection state.
            if (p_ble_evt->evt.gap_evt.params.connected.role != BLE_GAP_ROLE_PERIPH) {
                ble_scan_mark_inactive();
                break;
            }
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_BLE);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_BLE);
            sleep_timer_stop();

            NRF_LOG_INFO("Connected");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            g_is_ble_connected = true;
            m_nus_comm_started = false;
            nus_rx_clear();
            nus_tx_clear();
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_BLE);
            g_is_ble_advertising = false;
            if (m_adv_lab_state == 2u) {
                m_adv_lab_state = 3u;
                m_adv_lab_reason = 4u;
                rgb_marquee_set_ble_active_anim(false);
                adv_lab_restore_modes(m_adv_lab_restore_normal);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            // Ignore disconnects of the central (target) link; only react to the
            // app/NUS peripheral link going away.
            if (p_ble_evt->evt.gap_evt.conn_handle != m_conn_handle) {
                break;
            }
            NRF_LOG_INFO("Disconnected");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_BLE);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_BLE);
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            g_is_ble_connected = false;
            m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;
            m_nus_comm_started = false;
            nus_rx_clear();
            nus_tx_clear();
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_BLE);
            if (m_adv_lab_state == 3u) {
                m_advertising.adv_modes_config = m_adv_lab_saved_modes;
                m_adv_lab_state = 0u;
                if (m_adv_lab_restart_after_disconnect && g_ble_radio_on) {
                    m_adv_lab_restart_after_disconnect = false;
                    advertising_start(false);
                }
            }
            if (keyboard_payload_is_armed() && g_ble_radio_on) {
                uint32_t advertise_error = ble_keyboard_advertising_start();
                if (advertise_error != NRF_SUCCESS) {
                    NRF_LOG_WARNING("Failed to restart armed keyboard advertising: 0x%x",
                                    advertise_error);
                }
            }
            if (!g_ble_radio_on) {
                advertising_stop();
            }
            // The higher-priority advertising observer records a successful
            // automatic restart through on_adv_evt().
            // call sleep_timer_start *after* unsetting g_is_ble_connected
            sleep_timer_start(SLEEP_DELAY_MS_BLE_DISCONNECTED);
            break;

        case BLE_GAP_EVT_ADV_SET_TERMINATED:
            if (m_adv_lab_state == 2u) {
                uint8_t reason =
                    p_ble_evt->evt.gap_evt.params.adv_set_terminated.reason ==
                    BLE_GAP_EVT_ADV_SET_TERMINATED_REASON_LIMIT_REACHED ? 3u : 2u;
                (void)adv_lab_finish(reason, true);
            }
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys = {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        }
        break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            if (p_ble_evt->evt.gap_evt.conn_handle != m_conn_handle ||
                    m_conn_handle == BLE_CONN_HANDLE_INVALID) {
                break;
            }
            // Pairing not supported? No, is supported now, hahahaha...
            // But... the pairing is enable?
            if (settings_get_ble_pairing_enable()) {
                NRF_LOG_DEBUG("Pairing is enable, The BLE_GAP_EVT_SEC_PARAMS_REQUEST event is handled by the pairing manager.");
            } else {
                err_code = sd_ble_gap_sec_params_reply(p_ble_evt->evt.gap_evt.conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BLE_GAP_EVT_PASSKEY_DISPLAY: {
            NRF_LOG_INFO("BLE passkey display requested");
        }
        break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            if (p_ble_evt->evt.gatts_evt.conn_handle != m_conn_handle ||
                    m_conn_handle == BLE_CONN_HANDLE_INVALID) {
                break;
            }
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE &&
                    err_code != BLE_ERROR_INVALID_CONN_HANDLE) {
                NRF_LOG_WARNING("GATTC timeout disconnect failed: 0x%x", err_code);
            }
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_SUCCESS && err_code != NRF_ERROR_INVALID_STATE &&
                    err_code != BLE_ERROR_INVALID_CONN_HANDLE) {
                NRF_LOG_WARNING("GATTS timeout disconnect failed: 0x%x", err_code);
            }
            break;

        case BLE_GAP_EVT_ADV_REPORT:
            ble_scan_on_adv_report(&p_ble_evt->evt.gap_evt.params.adv_report);
            break;

        case BLE_GAP_EVT_TIMEOUT:
            if (p_ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_CONN) {
                ble_scan_mark_inactive();
            }
            break;

        default:
            // No implementation needed.
            UNUSED_VARIABLE(err_code);
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void) {
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t *p_gatt, nrf_ble_gatt_evt_t const *p_evt) {
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED)) {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void) {
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);

    // Also negotiate a larger MTU on the central (BLE-audit target) link, so GATT
    // read/write/fuzz aren't limited to the 23-byte default. nrf_ble_gatt is a
    // global observer, so it auto-exchanges MTU on the central connection too.
    err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

// Effective ATT MTU for a connection (used by the central harness to size
// reads/writes). Returns the SoftDevice default (23) until MTU exchange completes.
uint16_t ble_link_mtu(uint16_t conn_handle) {
    return nrf_ble_gatt_eff_mtu_get(&m_gatt, conn_handle);
}


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void) {
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = adv_uuids_count();
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = 0;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static bool valid_utf8_name(const uint8_t *name, uint8_t length) {
    uint8_t index = 0u;
    while (index < length) {
        uint8_t first = name[index++];
        if (first < 0x20u || first == 0x7fu) return false;
        if (first < 0x80u) continue;

        uint8_t continuation;
        uint32_t codepoint;
        if ((first & 0xe0u) == 0xc0u) {
            continuation = 1u;
            codepoint = first & 0x1fu;
            if (codepoint < 2u) return false;
        } else if ((first & 0xf0u) == 0xe0u) {
            continuation = 2u;
            codepoint = first & 0x0fu;
        } else if ((first & 0xf8u) == 0xf0u) {
            continuation = 3u;
            codepoint = first & 0x07u;
        } else {
            return false;
        }
        if ((uint16_t)index + continuation > length) return false;
        while (continuation-- > 0u) {
            uint8_t next = name[index++];
            if ((next & 0xc0u) != 0x80u) return false;
            codepoint = (codepoint << 6u) | (next & 0x3fu);
        }
        if ((codepoint < 0x800u && first >= 0xe0u) ||
                (codepoint < 0x10000u && first >= 0xf0u) ||
                (codepoint >= 0xd800u && codepoint <= 0xdfffu) ||
                codepoint > 0x10ffffu) {
            return false;
        }
    }
    return true;
}

static uint32_t normal_advertising_data_update(void) {
    if (m_adv_flood_state != 0u || ble_adv_lab_is_active()) return NRF_ERROR_BUSY;

    ble_advdata_t advdata;
    ble_advdata_t srdata;
    memset(&advdata, 0, sizeof(advdata));
    memset(&srdata, 0, sizeof(srdata));
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = false;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    srdata.uuids_complete.uuid_cnt = adv_uuids_count();
    srdata.uuids_complete.p_uuids = m_adv_uuids;

    uint8_t buffer = m_advertising.adv_data.adv_data.p_data ==
                     m_advertising.enc_advdata[0] ? 1u : 0u;
    ble_gap_adv_data_t next = {
        .adv_data = {
            .p_data = m_advertising.enc_advdata[buffer],
            .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX,
        },
        .scan_rsp_data = {
            .p_data = m_advertising.enc_scan_rsp_data[buffer],
            .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX,
        },
    };
    uint32_t error = ble_advdata_encode(&advdata, next.adv_data.p_data,
                                        &next.adv_data.len);
    if (error != NRF_SUCCESS) return error;
    error = ble_advdata_encode(&srdata, next.scan_rsp_data.p_data,
                               &next.scan_rsp_data.len);
    if (error != NRF_SUCCESS) return error;

    if (g_is_ble_advertising) {
        error = sd_ble_gap_adv_set_configure(&m_advertising.adv_handle, &next, NULL);
        if (error != NRF_SUCCESS) return error;
    }
    m_advertising.adv_data = next;
    m_advertising.p_adv_data = &m_advertising.adv_data;
    return NRF_SUCCESS;
}

uint32_t ble_peripheral_name_set_temporary(const uint8_t *name, uint8_t length) {
    const uint8_t *effective = name;
    uint8_t effective_length = length;
    if (length == 0u) {
        effective = (const uint8_t *)DEVICE_NAME_STR;
        effective_length = sizeof(DEVICE_NAME_STR) - 1u;
    } else if (name == NULL || length > BLE_TEMPORARY_NAME_MAX_LENGTH ||
               !valid_utf8_name(name, length)) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_adv_flood_state != 0u || ble_adv_lab_is_active()) return NRF_ERROR_BUSY;

    uint8_t previous[BLE_TEMPORARY_NAME_MAX_LENGTH];
    uint8_t previous_length = m_peripheral_name_length;
    memcpy(previous, m_peripheral_name, previous_length);

    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    uint32_t error = sd_ble_gap_device_name_set(&sec_mode, effective,
                                                effective_length);
    if (error != NRF_SUCCESS) return error;
    memcpy(m_peripheral_name, effective, effective_length);
    m_peripheral_name_length = effective_length;
    error = normal_advertising_data_update();
    if (error != NRF_SUCCESS) {
        (void)sd_ble_gap_device_name_set(&sec_mode, previous, previous_length);
        memcpy(m_peripheral_name, previous, previous_length);
        m_peripheral_name_length = previous_length;
        (void)normal_advertising_data_update();
    }
    return error;
}

uint8_t ble_peripheral_name_get(uint8_t *name) {
    if (name != NULL) memcpy(name, m_peripheral_name, m_peripheral_name_length);
    return m_peripheral_name_length;
}

uint32_t ble_keyboard_advertising_start(void) {
    if (!g_ble_radio_on || m_adv_flood_state != 0u || ble_adv_lab_is_active()) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
        return NRF_SUCCESS;
    }
    if (g_is_ble_advertising) {
        g_is_ble_advertising = false;
        uint32_t error = ble_advertising_restart_without_whitelist(&m_advertising);
        return error == NRF_SUCCESS && g_is_ble_advertising
               ? NRF_SUCCESS : NRF_ERROR_INVALID_STATE;
    }
    m_advertising.whitelist_temporarily_disabled = true;
    advertising_start(false);
    return g_is_ble_advertising ? NRF_SUCCESS : NRF_ERROR_INVALID_STATE;
}

/**@brief Clear bond information from persistent storage.
 */
void delete_bonds_all(void) {
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for setting filtered whitelist.
 *
 * @param[in] skip  Filter passed to @ref pm_peer_id_list.
 */
static void whitelist_set(pm_peer_id_list_skip_t skip) {
    pm_peer_id_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
    uint32_t     peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    ret_code_t err_code = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("Whitelist peer cnt %d, MAX_PEERS_WLIST %d", peer_id_count, BLE_GAP_WHITELIST_ADDR_MAX_COUNT);

    err_code = pm_whitelist_set(peer_ids, peer_id_count);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
void advertising_start(bool erase_bonds) {
    if (!g_ble_radio_on) {
        return;
    }
    if (ble_adv_lab_is_active()) {
        return;
    }
    if (g_is_ble_advertising && !erase_bonds) {
        return;
    }

    if (g_is_ble_advertising) {
        advertising_stop();
    }

    if (erase_bonds == true && settings_get_ble_pairing_enable()) {
        // Advertising is started by PM_EVT_PEERS_DELETE_SUCCEEDED event.
        // So we don't call `ble_advertising_start()` after `delete_bonds_all()`.
        m_peer_delete_pending = true;
        delete_bonds_all();
    } else {
        if (settings_get_ble_pairing_enable()) {
            whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
        }
        ret_code_t ret = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
        if (ret == NRF_SUCCESS) {
            g_is_ble_advertising = true;
        } else if (ret != NRF_ERROR_INVALID_STATE) {
            NRF_LOG_WARNING("Failed to start BLE advertising: 0x%x", ret);
        }
    }
}

/**
 * @brief Function for stop advertising.
 */
void advertising_stop(void) {
    if (!g_is_ble_advertising) {
        return;
    }
    uint32_t err_code = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if (err_code == NRF_SUCCESS || err_code == NRF_ERROR_INVALID_STATE) {
        g_is_ble_advertising = false;
    } else {
        NRF_LOG_WARNING("Failed to stop BLE advertising: 0x%x", err_code);
    }
}

bool is_ble_advertising(void) {
    return g_is_ble_advertising || m_adv_flood_state != 0 ||
           m_adv_lab_state == 2u;
}

bool ble_command_link_authorized(void) {
    return m_conn_handle != BLE_CONN_HANDLE_INVALID && m_nus_comm_started &&
           ble_conn_state_encrypted(m_conn_handle);
}

bool ble_keyboard_link_authorized(void) {
    return ble_command_link_authorized() &&
           ble_conn_state_mitm_protected(m_conn_handle) &&
           ble_conn_state_lesc(m_conn_handle);
}

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const *p_evt) {
    pm_handler_on_pm_evt(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id) {
        case PM_EVT_CONN_SEC_SUCCEEDED:
            // p_evt->peer_id;
            break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            m_peer_delete_pending = false;
            advertising_start(false);
            break;

        case PM_EVT_PEERS_DELETE_FAILED:
            m_peer_delete_pending = false;
            NRF_LOG_WARNING("Failed to delete BLE peers; advertising with existing bonds");
            advertising_start(false);
            break;

        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
            if (p_evt->params.peer_data_update_succeeded.flash_changed
                    && (p_evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING)) {
                NRF_LOG_INFO("New Bond, add the peer to the whitelist if possible");
                // Note: You should check on what kind of white list policy your application should use.

                whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
            }
            break;
        case PM_EVT_CONN_SEC_CONFIG_REQ: {
            pm_conn_sec_config_t cfg;
            cfg.allow_repairing = true;
            pm_conn_sec_config_reply(p_evt->conn_handle, &cfg);
        }
        break;
        default:
            break;
    }
}

/**@brief Function for the Peer Manager initialization.
 */
static void peer_manager_init(void) {
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAMS_BOND;
    sec_param.mitm           = SEC_PARAMS_MITM;
    sec_param.lesc           = SEC_PARAMS_LESC;
    sec_param.keypress       = SEC_PARAMS_KEYPRESS;
    sec_param.io_caps        = SEC_PARAMS_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAMS_OOB;
    sec_param.min_key_size   = SEC_PARAMS_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAMS_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the ADC interrupt.
 *
 * @details  This function fetchs conversion result from the ADC, then callback lf with samples.
 */
void saadc_event_handler(nrfx_saadc_evt_t const *p_event) {
    if (p_event->type == NRFX_SAADC_EVT_DONE && m_lf_adc_callback != NULL) {
        ret_code_t err_code;
        err_code = nrfx_saadc_buffer_convert(&adc_buf[next_free_buf_index()][0], ADC_BUF_SIZE);
        APP_ERROR_CHECK(err_code);

        m_lf_adc_callback(p_event->data.done.p_buffer, p_event->data.done.size);
    }
}

/**@brief Function for configuring ADC to do battery level conversion.
 */
static void adc_configure(void) {
    ret_code_t err_code;

    nrfx_saadc_config_t cfg = NRFX_SAADC_DEFAULT_CONFIG;
    err_code = nrfx_saadc_init(&cfg, saadc_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_channel_config_t ch = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(BAT_SENSE);
    err_code = nrfx_saadc_channel_init(ADC_CHANNEL, &ch);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling the Battery measurement timer timeout.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *          This function will start the ADC.
 *
 * @param[in] p_context   Pointer used for passing some arbitrary information (context) from the
 *                        app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void *p_context) {
    UNUSED_PARAMETER(p_context);

    // When LF sample is enabled, PWM triggers ADC samples through PPI, at 125khz sample rate.
    // To avoid corrupt LF samples, here we ignore battery level measure timer for a while.
    if (m_lf_adc_callback != NULL) {
        return;
    }

    // Here we fetch the conversion result from the ADC, convert the value into
    // percentage and send it to peer.
    ret_code_t err_code;
    nrf_saadc_value_t adc_result;
    err_code = nrfx_saadc_sample_convert(ADC_CHANNEL, &adc_result);
    APP_ERROR_CHECK(err_code);

    batt_lvl_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(adc_result) + 100;
    percentage_batt_lvl = BATVOL2PERCENT(batt_lvl_in_milli_volts);

    // if battery service is notification enable, we can send msg to device.
    err_code = ble_bas_battery_level_update(&m_bas, percentage_batt_lvl, BLE_CONN_HANDLE_ALL);
    if ((err_code != NRF_SUCCESS) &&
            (err_code != NRF_ERROR_INVALID_STATE) &&
            (err_code != NRF_ERROR_RESOURCES) &&
            (err_code != NRF_ERROR_BUSY) &&
            (err_code != NRF_ERROR_FORBIDDEN) &&
            (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)) {
        APP_ERROR_HANDLER(err_code);
    }

    // check low battery level, if level == 0, we can try to shutdown.
    if (percentage_batt_lvl == 0) {
        NRF_LOG_INFO("battery too low, try to shutdown...");
        g_is_low_battery_shutdown = true;
        sleep_timer_start(SLEEP_NO_BATTERY_SHUTDOWN);
    } else {
        g_is_low_battery_shutdown = false;
    }
}

void create_battery_timer(void) {
    ret_code_t err_code;
    // Create battery timer.
    err_code = app_timer_create(&m_battery_timer_id, APP_TIMER_MODE_REPEATED, battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
    // Start battery timer
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

// ---------------------------------------------------------------------------
// BLE identity & radio toggle (cybersecurity fork additions).
//
// Identity + radio power are settings on OUR radio (no scope selector —
// they're inherently local). The environment-wide broadcast tools live in
// the 7050-block + ble_central scan-buffer-wide kick/flood (CLAUDE.md
// fork-specific exemption — operator-authorised).
// ---------------------------------------------------------------------------
// Original (factory) address cache, captured at first call. Format: the BLE
// spec mandates bit14 of the high byte be 1 for a static-random address, so
// we mask in 0xC000 over the FICR bytes (matches the pattern the firmware
// already applies in cmd_processor_get_device_address).
static bool     m_orig_addr_known = false;
static uint8_t  m_orig_addr_type;                      // BLE_GAP_ADDR_TYPE_PUBLIC
static uint8_t  m_orig_addr[BLE_GAP_ADDR_LEN];

static void restore_address_gap_procedures(bool advertising, bool flooding,
                                           bool scanning, uint8_t scan_mode,
                                           uint8_t flood_fill, uint16_t flood_interval) {
    if (flooding) {
        uint16_t interval_ms = (uint16_t)(((uint32_t)flood_interval * 625u + 999u) / 1000u);
        if (ble_adv_flood_start(flood_fill, interval_ms) != NRF_SUCCESS) {
            NRF_LOG_WARNING("Failed to restore advertising flood");
        }
    } else if (advertising) {
        advertising_start(false);
    }

    if (scanning && ble_scan_start(scan_mode) != NRF_SUCCESS) {
        NRF_LOG_WARNING("Failed to restore BLE scan");
    }
}

static void cache_original_address(void) {
    if (m_orig_addr_known) {
        return;
    }
    // S140 v7.2.0: sd_ble_gap_addr_get() takes a single ble_gap_addr_t* and
    // returns both the address type and the 6 bytes in one struct.
    ble_gap_addr_t addr = {0};
    uint32_t err = sd_ble_gap_addr_get(&addr);
    if (err == NRF_SUCCESS) {
        m_orig_addr_type = addr.addr_type;
        memcpy(m_orig_addr, addr.addr, BLE_GAP_ADDR_LEN);
    } else {
        // Fall back to the FICR value with the 0xC000 static-random bit pattern.
        m_orig_addr[0] = (uint8_t)(NRF_FICR->DEVICEADDR[0] & 0xFF);
        m_orig_addr[1] = (uint8_t)((NRF_FICR->DEVICEADDR[0] >> 8) & 0xFF);
        m_orig_addr[2] = (uint8_t)((NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF);
        m_orig_addr[3] = (uint8_t)((NRF_FICR->DEVICEADDR[0] >> 24) & 0xFF);
        m_orig_addr[4] = (uint8_t)(NRF_FICR->DEVICEADDR[1] & 0xFF);
        m_orig_addr[5] = (uint8_t)(((NRF_FICR->DEVICEADDR[1] >> 8) & 0x3F) | 0xC0);
        m_orig_addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
    }
    m_orig_addr_known = true;
}

uint32_t ble_addr_set(uint8_t mode, const uint8_t *addr_le) {
    if (ble_adv_lab_is_active()) return NRF_ERROR_BUSY;
    cache_original_address();

    if (g_is_ble_connected || ble_central_is_connected() || ble_central_is_connecting()) {
        return NRF_ERROR_BUSY;
    }

    if (mode > BLE_ADDR_MODE_RANDOM_NONRESOLV ||
            (mode == BLE_ADDR_MODE_RANDOM_STATIC && addr_le == NULL)) {
        return NRF_ERROR_INVALID_PARAM;
    }

    ble_gap_addr_t addr;
    memset(&addr, 0, sizeof(addr));

    switch (mode) {
    case BLE_ADDR_MODE_RESTORE_ORIGINAL:
        memcpy(addr.addr, m_orig_addr, BLE_GAP_ADDR_LEN);
        addr.addr_type = m_orig_addr_type;
        break;

    case BLE_ADDR_MODE_RANDOM_STATIC:
        memcpy(addr.addr, addr_le, BLE_GAP_ADDR_LEN);
        addr.addr[5] = (uint8_t)((addr.addr[5] & 0x3F) | 0xC0);
        addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
        break;

    case BLE_ADDR_MODE_RANDOM_PRIVATE:
        break;

    case BLE_ADDR_MODE_RANDOM_NONRESOLV:
        break;

    default:
        return NRF_ERROR_INVALID_PARAM;
    }

    bool was_advertising = g_is_ble_advertising;
    bool was_flooding = m_adv_flood_state != 0;
    bool was_scanning = is_ble_scanning();
    uint8_t scan_mode = ble_scan_get_mode();
    uint8_t flood_fill = m_adv_flood_payload[5];
    uint16_t flood_interval = m_adv_flood_interval;

    if (was_flooding) {
        uint32_t err = ble_adv_flood_stop();
        if (err != NRF_SUCCESS) {
            restore_address_gap_procedures(false, true, false, scan_mode,
                                           flood_fill, flood_interval);
            return err;
        }
    }
    if (was_advertising) {
        advertising_stop();
        if (g_is_ble_advertising) {
            return NRF_ERROR_BUSY;
        }
    }
    if (was_scanning) {
        uint32_t err = ble_scan_stop();
        if (err != NRF_SUCCESS) {
            restore_address_gap_procedures(was_advertising, was_flooding, false,
                                           scan_mode, flood_fill, flood_interval);
            return err;
        }
    }

    ble_gap_privacy_params_t old_privacy = {0};
    uint32_t err = sd_ble_gap_privacy_get(&old_privacy);
    if (err == NRF_SUCCESS) {
        ble_gap_privacy_params_t privacy = old_privacy;
        privacy.p_device_irk = NULL;
        if (mode == BLE_ADDR_MODE_RANDOM_PRIVATE || mode == BLE_ADDR_MODE_RANDOM_NONRESOLV) {
            privacy.privacy_mode = BLE_GAP_PRIVACY_MODE_DEVICE_PRIVACY;
            privacy.private_addr_type = (mode == BLE_ADDR_MODE_RANDOM_PRIVATE) ?
                                        BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE :
                                        BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE;
            err = sd_ble_gap_privacy_set(&privacy);
        } else {
            privacy.privacy_mode = BLE_GAP_PRIVACY_MODE_OFF;
            if (privacy.private_addr_type != BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE &&
                    privacy.private_addr_type != BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_NON_RESOLVABLE) {
                privacy.private_addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
            }
            err = sd_ble_gap_privacy_set(&privacy);
            if (err == NRF_SUCCESS) {
                err = sd_ble_gap_addr_set(&addr);
                if (err != NRF_SUCCESS) {
                    old_privacy.p_device_irk = NULL;
                    (void)sd_ble_gap_privacy_set(&old_privacy);
                }
            }
        }
    }

    restore_address_gap_procedures(was_advertising, was_flooding, was_scanning,
                                   scan_mode, flood_fill, flood_interval);
    return err;
}

uint32_t ble_addr_get(uint8_t *addr_type, uint8_t *addr_out) {
    if (addr_type == NULL || addr_out == NULL) {
        return NRF_ERROR_INVALID_PARAM;
    }
    ble_gap_addr_t addr = {0};
    uint32_t err = NRF_ERROR_INVALID_STATE;
    if (is_ble_advertising()) {
        err = sd_ble_gap_adv_addr_get(m_advertising.adv_handle, &addr);
    }
    if (err != NRF_SUCCESS) {
        err = sd_ble_gap_addr_get(&addr);
    }
    if (err != NRF_SUCCESS) {
        return err;
    }
    *addr_type = addr.addr_type;
    memcpy(addr_out, addr.addr, BLE_GAP_ADDR_LEN);
    return NRF_SUCCESS;
}

uint32_t ble_radio_set(uint8_t on) {
    if (on) {
        if (g_ble_radio_on) {
            return NRF_SUCCESS;
        }
        g_ble_radio_on = true;
        if (m_adv_lab_state == 3u) {
            m_adv_lab_restart_after_disconnect = true;
            return NRF_SUCCESS;
        }
        // Restore peripheral advertising if it was running before. We don't
        // know the operator's pre-toggle intent (erase_bonds or not), so we
        // pick the non-destructive restart.
        if (!g_is_ble_advertising) {
            advertising_start(false);
        }
        return NRF_SUCCESS;
    }

    // Mark the policy off before stopping GAP procedures so asynchronous
    // disconnect/bond events cannot restart advertising during shutdown.
    g_ble_radio_on = false;
    uint32_t err = ble_adv_lab_stop();
    if (err != NRF_SUCCESS) {
        g_ble_radio_on = true;
        return err;
    }
    err = ble_adv_flood_stop();
    if (err != NRF_SUCCESS) {
        g_ble_radio_on = true;
        return err;
    }
    advertising_stop();
    if (is_ble_advertising()) {
        g_ble_radio_on = true;
        return NRF_ERROR_BUSY;
    }
    if (is_ble_scanning()) {
        err = ble_scan_stop();
        if (err != NRF_SUCCESS) {
            g_ble_radio_on = true;
            return err;
        }
    }
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID && m_adv_lab_state == 3u) {
        err = sd_ble_gap_disconnect(m_conn_handle,
                                    BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
            g_ble_radio_on = true;
            return err;
        }
    }
    if (ble_central_is_connected() || ble_central_is_connecting()) {
        err = ble_central_disconnect();
        if (err != NRF_SUCCESS) {
            g_ble_radio_on = true;
            return err;
        }
    }
    return NRF_SUCCESS;
}

uint32_t ble_radio_get(uint8_t *out) {
    if (out == NULL) {
        return NRF_ERROR_INVALID_PARAM;
    }
    out[0] = g_ble_radio_on ? 1 : 0;
    out[1] = is_ble_advertising() ? 1 : 0;
    out[2] = is_ble_scanning() ? 1 : 0;
    out[3] = ble_central_is_connected() ? 1 : 0;
    return NRF_SUCCESS;
}

// ---- environment-wide broadcast (full 2.4 GHz BLE spectrum spam) --------
uint32_t ble_adv_flood_start(uint8_t fill_byte, uint16_t interval_ms) {
    if (!g_ble_radio_on) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (ble_adv_lab_is_active()) return NRF_ERROR_BUSY;
    // Clamp interval to regulatory minimum and reject values above the
    // SoftDevice max before stopping normal advertising.
    if (interval_ms < 100) interval_ms = 100;
    if (interval_ms > BLE_ADV_FLOOD_MAX_INTERVAL_MS) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_adv_flood_state == 1) {
        uint32_t err = ble_adv_flood_stop();
        if (err != NRF_SUCCESS) {
            return err;
        }
    }
    bool restore_normal_advertising = g_is_ble_advertising;
    // Stop normal peripheral advertising first so it doesn't fight the flood.
    advertising_stop();

    // Fill 31-byte legacy adv payload. AD structure:
    // [len=2][type=0x01 (flags)][flags=0x06 (LE General Discoverable, BR/EDR off)]
    // [len=0x1B][type=0xFF (manufacturer data)][26 bytes of fill]
    m_adv_flood_payload[0] = 0x02;
    m_adv_flood_payload[1] = 0x01;
    m_adv_flood_payload[2] = 0x06;
    m_adv_flood_payload[3] = 27;                // type byte + 26 manufacturer bytes
    m_adv_flood_payload[4] = 0xFF;               // manufacturer specific
    for (uint8_t i = 5; i < 31; i++) {
        m_adv_flood_payload[i] = fill_byte;
    }

    m_adv_flood_interval = MSEC_TO_UNITS(interval_ms, UNIT_0_625_MS);

    // S140 v7.2.0 ble_gap_adv_params_t layout: properties (with type),
    // p_peer_addr, interval, duration, max_adv_evts, channel_mask[5],
    // filter_policy, primary_phy, secondary_phy, set_id, scan_req_notification.
    ble_gap_adv_params_t params = {0};
    params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
    params.p_peer_addr     = NULL;
    params.interval        = m_adv_flood_interval;
    params.duration        = 0;        // forever
    params.max_adv_evts    = 0;        // no event limit
    params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    params.primary_phy     = BLE_GAP_PHY_1MBPS;
    // channel_mask = 0 means "all primary channels enabled".

    ble_gap_adv_data_t data = {
        .adv_data = { .p_data = m_adv_flood_payload, .len = 31 },
        .scan_rsp_data = { .p_data = NULL, .len = 0 },
    };
    // S140 in this SDK supports one advertising set. Reconfigure the normal
    // advertising module's handle instead of allocating a second one.
    uint32_t err = sd_ble_gap_adv_set_configure(&m_advertising.adv_handle, &data, &params);
    if (err != NRF_SUCCESS) {
        if (restore_normal_advertising) advertising_start(false);
        return err;
    }
    err = sd_ble_gap_adv_start(m_advertising.adv_handle, BLE_CONN_CFG_TAG_DEFAULT);
    if (err != NRF_SUCCESS) {
        (void)sd_ble_gap_adv_set_configure(&m_advertising.adv_handle, NULL, NULL);
        if (restore_normal_advertising) advertising_start(false);
        return err;
    }
    g_is_ble_advertising = false;
    m_adv_flood_state = 1;
    rgb_marquee_set_ble_active_anim(true);
    return NRF_SUCCESS;
}

uint32_t ble_adv_flood_stop(void) {
    if (m_adv_flood_state == 0) {
        return NRF_SUCCESS;
    }
    uint32_t err = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    if (err != NRF_SUCCESS && err != NRF_ERROR_INVALID_STATE) {
        return err;
    }
    err = sd_ble_gap_adv_set_configure(&m_advertising.adv_handle, NULL, NULL);
    m_adv_flood_state = 0;
    rgb_marquee_set_ble_active_anim(false);
    return err;
}

static bool adv_lab_data_valid(const uint8_t *data, uint8_t length,
                               bool scan_response, bool *has_name) {
    uint8_t offset = 0u;
    *has_name = false;
    while (offset < length) {
        uint8_t field_length = data[offset];
        if (field_length == 0u ||
                (uint16_t)offset + 1u + field_length > length) {
            return false;
        }
        uint8_t type = data[offset + 1u];
        if (scan_response && type == BLE_GAP_AD_TYPE_FLAGS) return false;
        if (type == BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME ||
                type == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME) {
            if (*has_name) return false;
            *has_name = true;
        }
        offset += 1u + field_length;
    }
    return offset == length;
}

static void adv_lab_build_buffer(uint8_t buffer, uint8_t name_index) {
    if (m_adv_lab_base_adv_length != 0u) {
        memcpy(m_adv_lab_adv[buffer], m_adv_lab_base_adv,
               m_adv_lab_base_adv_length);
    }
    if (m_adv_lab_base_scan_length != 0u) {
        memcpy(m_adv_lab_scan[buffer], m_adv_lab_base_scan,
               m_adv_lab_base_scan_length);
    }
    uint8_t adv_length = m_adv_lab_base_adv_length;
    uint8_t scan_length = m_adv_lab_base_scan_length;
    if (m_adv_lab_name_count != 0u) {
        uint8_t name_length = m_adv_lab_name_lengths[name_index];
        uint8_t *target = m_adv_lab_name_target == 1u
                          ? m_adv_lab_adv[buffer] : m_adv_lab_scan[buffer];
        uint8_t *target_length = m_adv_lab_name_target == 1u
                                 ? &adv_length : &scan_length;
        target[(*target_length)++] = name_length + 1u;
        target[(*target_length)++] = BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME;
        memcpy(&target[*target_length], m_adv_lab_names[name_index], name_length);
        *target_length += name_length;
    }
}

static void adv_lab_lengths(uint8_t *adv_length, uint8_t *scan_length) {
    *adv_length = m_adv_lab_base_adv_length;
    *scan_length = m_adv_lab_base_scan_length;
    if (m_adv_lab_name_count != 0u) {
        uint8_t addition = m_adv_lab_name_lengths[m_adv_lab_name_index] + 2u;
        if (m_adv_lab_name_target == 1u) *adv_length += addition;
        else *scan_length += addition;
    }
}

static void adv_lab_disable_normal_modes(void) {
    m_adv_lab_saved_modes = m_advertising.adv_modes_config;
    memset(&m_advertising.adv_modes_config, 0,
           sizeof(m_advertising.adv_modes_config));
    m_advertising.adv_modes_config.ble_adv_on_disconnect_disabled = true;
}

static void adv_lab_restore_modes(bool allow_disconnect_restart) {
    m_advertising.adv_modes_config = m_adv_lab_saved_modes;
    if (!allow_disconnect_restart) {
        m_advertising.adv_modes_config.ble_adv_on_disconnect_disabled = true;
    }
}

static uint32_t adv_lab_finish(uint8_t reason, bool restore_normal) {
    uint32_t error = NRF_SUCCESS;
    if (m_adv_lab_state == 2u) {
        error = sd_ble_gap_adv_stop(m_advertising.adv_handle);
        if (error == NRF_ERROR_INVALID_STATE) error = NRF_SUCCESS;
    }
    uint32_t configure_error = sd_ble_gap_adv_set_configure(
        &m_advertising.adv_handle, NULL, NULL);
    if (configure_error != NRF_SUCCESS && configure_error != NRF_ERROR_INVALID_STATE &&
            error == NRF_SUCCESS) {
        error = configure_error;
    }
    m_advertising.adv_modes_config = m_adv_lab_saved_modes;
    m_adv_lab_state = error == NRF_SUCCESS ? 0u : 4u;
    m_adv_lab_reason = error == NRF_SUCCESS ? reason : 7u;
    rgb_marquee_set_ble_active_anim(false);
    if (restore_normal && m_adv_lab_restore_normal && g_ble_radio_on &&
            m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        advertising_start(false);
    }
    return error;
}

bool ble_adv_lab_is_active(void) {
    return m_adv_lab_state == 2u || m_adv_lab_state == 3u;
}

uint32_t ble_adv_lab_start(uint8_t profile, uint8_t mode, uint8_t name_target,
                           uint16_t interval_units, uint16_t rotation_ms,
                           uint16_t duration_units, uint8_t max_adv_events,
                           const uint8_t *adv_data, uint8_t adv_length,
                           const uint8_t *scan_data, uint8_t scan_length,
                           const uint8_t *names, uint16_t names_length,
                           uint8_t name_count) {
    if (!g_ble_radio_on) return NRF_ERROR_INVALID_STATE;
    if (profile < 1u || profile > 3u || mode > 2u ||
            name_target > 2u || adv_length > 31u || scan_length > 31u ||
            name_count > BLE_ADV_LAB_MAX_NAMES ||
            (adv_length != 0u && adv_data == NULL) ||
            (scan_length != 0u && scan_data == NULL) ||
            (names_length != 0u && names == NULL)) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID || m_adv_flood_state != 0u ||
            ble_adv_lab_is_active() || is_ble_scanning() ||
            ble_central_is_connected() || ble_central_is_connecting() ||
            keyboard_payload_is_running()) {
        return NRF_ERROR_BUSY;
    }
    uint16_t minimum_interval = mode == 0u ? 32u : 160u;
    if (interval_units < minimum_interval || interval_units > 0x4000u ||
            (mode == 2u && (scan_length != 0u || name_target == 2u))) {
        return NRF_ERROR_INVALID_PARAM;
    }
    if ((name_count == 0u && (name_target != 0u || rotation_ms != 0u)) ||
            (name_count != 0u && name_target == 0u) ||
            (profile == 1u && name_count > 1u) ||
            (profile == 2u && name_count == 0u) ||
            (profile == 3u && name_count != 0u) ||
            (name_count <= 1u && rotation_ms != 0u)) {
        return NRF_ERROR_INVALID_PARAM;
    }
    uint16_t interval_ms = (uint16_t)(((uint32_t)interval_units * 625u + 999u) / 1000u);
    if (name_count > 1u && (rotation_ms < 100u || rotation_ms < interval_ms)) {
        return NRF_ERROR_INVALID_PARAM;
    }

    bool adv_has_name;
    bool scan_has_name;
    if (!adv_lab_data_valid(adv_data, adv_length, false, &adv_has_name) ||
            !adv_lab_data_valid(scan_data, scan_length, true, &scan_has_name) ||
            (adv_has_name && scan_has_name) ||
            (name_count != 0u && (adv_has_name || scan_has_name))) {
        return NRF_ERROR_INVALID_PARAM;
    }

    uint16_t offset = 0u;
    for (uint8_t index = 0u; index < name_count; index++) {
        if (offset >= names_length) return NRF_ERROR_INVALID_PARAM;
        uint8_t length = names[offset++];
        if (length == 0u || length > BLE_ADV_LAB_MAX_NAME_LENGTH ||
                (uint32_t)offset + length > names_length ||
                !valid_utf8_name(&names[offset], length)) {
            return NRF_ERROR_INVALID_PARAM;
        }
        uint8_t target_length = name_target == 1u ? adv_length : scan_length;
        if ((uint16_t)target_length + length + 2u > 31u) {
            return NRF_ERROR_DATA_SIZE;
        }
        m_adv_lab_name_lengths[index] = length;
        memcpy(m_adv_lab_names[index], &names[offset], length);
        offset += length;
    }
    if (offset != names_length) return NRF_ERROR_INVALID_PARAM;

    if (adv_length != 0u) memcpy(m_adv_lab_base_adv, adv_data, adv_length);
    if (scan_length != 0u) memcpy(m_adv_lab_base_scan, scan_data, scan_length);
    m_adv_lab_base_adv_length = adv_length;
    m_adv_lab_base_scan_length = scan_length;
    m_adv_lab_profile = profile;
    m_adv_lab_mode = mode;
    m_adv_lab_name_target = name_target;
    m_adv_lab_name_count = name_count;
    m_adv_lab_name_index = name_count == 0u ? 0xffu : 0u;
    m_adv_lab_interval_units = interval_units;
    m_adv_lab_rotation_ms = rotation_ms;
    m_adv_lab_duration_units = duration_units;
    m_adv_lab_max_events = max_adv_events;
    m_adv_lab_rotation_count = 0u;
    m_adv_lab_reason = 0u;
    m_adv_lab_rotation_period_ticks = name_count > 1u
                                      ? APP_TIMER_TICKS(rotation_ms) : 0u;
    m_adv_lab_rotation_elapsed_ticks = 0u;
    m_adv_lab_rotation_last_tick = app_timer_cnt_get();
    m_adv_lab_restart_after_disconnect = false;
    m_adv_lab_active_buffer = 0u;
    adv_lab_build_buffer(0u, 0u);
    uint8_t active_adv_length;
    uint8_t active_scan_length;
    adv_lab_lengths(&active_adv_length, &active_scan_length);

    ble_gap_adv_params_t params = {0};
    params.properties.type = mode == 0u
                             ? BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED
                             : mode == 1u
                             ? BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED
                             : BLE_GAP_ADV_TYPE_NONCONNECTABLE_NONSCANNABLE_UNDIRECTED;
    params.interval = interval_units;
    params.duration = duration_units;
    params.max_adv_evts = max_adv_events;
    params.filter_policy = BLE_GAP_ADV_FP_ANY;
    params.primary_phy = BLE_GAP_PHY_1MBPS;
    ble_gap_adv_data_t data = {
        .adv_data = {.p_data = m_adv_lab_adv[0], .len = active_adv_length},
        .scan_rsp_data = {.p_data = active_scan_length == 0u ? NULL : m_adv_lab_scan[0],
                          .len = active_scan_length},
    };

    m_adv_lab_restore_normal = g_is_ble_advertising || m_peer_delete_pending;
    advertising_stop();
    if (g_is_ble_advertising) return NRF_ERROR_BUSY;
    adv_lab_disable_normal_modes();
    uint32_t error = sd_ble_gap_adv_set_configure(&m_advertising.adv_handle,
                                                  &data, &params);
    if (error == NRF_SUCCESS) {
        error = sd_ble_gap_adv_start(m_advertising.adv_handle,
                                     mode == 0u ? APP_BLE_CONN_CFG_TAG
                                                : BLE_CONN_CFG_TAG_DEFAULT);
    }
    if (error != NRF_SUCCESS) {
        (void)sd_ble_gap_adv_set_configure(&m_advertising.adv_handle, NULL, NULL);
        m_advertising.adv_modes_config = m_adv_lab_saved_modes;
        m_adv_lab_state = 4u;
        m_adv_lab_reason = 7u;
        if (m_adv_lab_restore_normal) advertising_start(false);
        return error;
    }

    g_is_ble_advertising = false;
    m_adv_lab_state = 2u;
    rgb_marquee_set_ble_active_anim(true);
    return NRF_SUCCESS;
}

void ble_adv_lab_process(void) {
    if (m_adv_lab_state != 2u || m_adv_lab_name_count < 2u) {
        return;
    }
    uint32_t now = app_timer_cnt_get();
    m_adv_lab_rotation_elapsed_ticks += app_timer_cnt_diff_compute(
                                           now, m_adv_lab_rotation_last_tick);
    m_adv_lab_rotation_last_tick = now;
    if (m_adv_lab_rotation_elapsed_ticks < m_adv_lab_rotation_period_ticks) {
        return;
    }
    uint32_t steps = m_adv_lab_rotation_elapsed_ticks /
                     m_adv_lab_rotation_period_ticks;
    m_adv_lab_rotation_elapsed_ticks %= m_adv_lab_rotation_period_ticks;
    uint8_t next_name = (uint8_t)((m_adv_lab_name_index +
                                  steps % m_adv_lab_name_count) %
                                 m_adv_lab_name_count);
    uint8_t next_buffer = m_adv_lab_active_buffer ^ 1u;
    adv_lab_build_buffer(next_buffer, next_name);
    uint8_t adv_length;
    uint8_t scan_length;
    uint8_t old_name = m_adv_lab_name_index;
    m_adv_lab_name_index = next_name;
    adv_lab_lengths(&adv_length, &scan_length);
    m_adv_lab_name_index = old_name;
    ble_gap_adv_data_t data = {
        .adv_data = {.p_data = m_adv_lab_adv[next_buffer], .len = adv_length},
        .scan_rsp_data = {.p_data = scan_length == 0u ? NULL : m_adv_lab_scan[next_buffer],
                          .len = scan_length},
    };
    uint32_t error = sd_ble_gap_adv_set_configure(&m_advertising.adv_handle,
                                                  &data, NULL);
    if (error != NRF_SUCCESS) {
        (void)adv_lab_finish(7u, true);
        return;
    }
    m_adv_lab_active_buffer = next_buffer;
    m_adv_lab_name_index = next_name;
    m_adv_lab_rotation_count += steps;
}

uint32_t ble_adv_lab_stop(void) {
    if (m_adv_lab_state == 0u) return NRF_SUCCESS;
    if (m_adv_lab_state == 3u) {
        adv_lab_restore_modes(m_adv_lab_restore_normal);
        m_adv_lab_reason = 1u;
        rgb_marquee_set_ble_active_anim(false);
        return NRF_SUCCESS;
    }
    return adv_lab_finish(1u, true);
}

uint16_t ble_adv_lab_get_status(uint8_t *out, uint16_t max_length) {
    if (out == NULL || max_length < BLE_ADV_LAB_STATUS_LENGTH) return 0u;
    uint8_t adv_length = m_adv_lab_base_adv_length;
    uint8_t scan_length = m_adv_lab_base_scan_length;
    if (m_adv_lab_name_count != 0u && m_adv_lab_name_index != 0xffu) {
        uint8_t addition = m_adv_lab_name_lengths[m_adv_lab_name_index] + 2u;
        if (m_adv_lab_name_target == 1u) adv_length += addition;
        else scan_length += addition;
    }
    out[0] = BLE_ADV_LAB_VERSION;
    out[1] = m_adv_lab_state;
    out[2] = m_adv_lab_profile;
    out[3] = m_adv_lab_mode;
    out[4] = m_adv_lab_reason;
    out[5] = m_adv_lab_name_index;
    out[6] = m_adv_lab_name_count;
    out[7] = adv_length;
    out[8] = scan_length;
    out[9] = (uint8_t)(m_adv_lab_interval_units >> 8u);
    out[10] = (uint8_t)m_adv_lab_interval_units;
    out[11] = (uint8_t)(m_adv_lab_rotation_ms >> 8u);
    out[12] = (uint8_t)m_adv_lab_rotation_ms;
    out[13] = (uint8_t)(m_adv_lab_duration_units >> 8u);
    out[14] = (uint8_t)m_adv_lab_duration_units;
    out[15] = m_adv_lab_max_events;
    out[16] = (uint8_t)(m_adv_lab_rotation_count >> 24u);
    out[17] = (uint8_t)(m_adv_lab_rotation_count >> 16u);
    out[18] = (uint8_t)(m_adv_lab_rotation_count >> 8u);
    out[19] = (uint8_t)m_adv_lab_rotation_count;
    return BLE_ADV_LAB_STATUS_LENGTH;
}

/**
 * @brief Function for init ble slave.
 */
void ble_slave_init(void) {
    adc_configure();                    // ADC initialization
    create_battery_timer();             // Create a battery power update timer
    ble_stack_init();                   // BLE protocol stack initialization
    gap_params_init();                  // GAP parameter initialization
    gatt_init();                        // Gatt protocol initialization
    services_init();                    // Initialization of service characteristics
    advertising_init();                 // Broadcast parameter initialization
    conn_params_init();                 // Connection parameter initialization
    peer_manager_init();                // Peer manager Initialization
    ble_central_init();                 // Directed BLE fuzzing harness (central role) timer
}

void register_lf_adc_callback(lf_adc_callback_t cb) {
    m_lf_adc_callback = cb;

    nrfx_saadc_uninit();

    ret_code_t err_code;

    nrfx_saadc_config_t cfg = NRFX_SAADC_DEFAULT_CONFIG;
    err_code = nrfx_saadc_init(&cfg, saadc_event_handler);
    APP_ERROR_CHECK(err_code);

    nrf_saadc_channel_config_t ch = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN5);
    ch.acq_time = NRF_SAADC_ACQTIME_5US;
    err_code = nrfx_saadc_channel_init(ADC_CHANNEL, &ch);
    APP_ERROR_CHECK(err_code);

    err_code = nrfx_saadc_buffer_convert(&adc_buf[next_free_buf_index()][0], ADC_BUF_SIZE);
    APP_ERROR_CHECK(err_code);

    err_code = nrfx_saadc_buffer_convert(&adc_buf[next_free_buf_index()][0], ADC_BUF_SIZE);
    APP_ERROR_CHECK(err_code);
}

void unregister_lf_adc_callback(void) {
    nrfx_saadc_uninit();
    adc_configure();
    m_lf_adc_callback = NULL;
}
