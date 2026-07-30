#include "keyboard_hid.h"

#include <string.h>

#include "app_error.h"
#include "app_usbd_core.h"
#include "app_usbd_hid_kbd.h"
#include "ble_hids.h"
#include "ble_conn_state.h"
#include "ble_link_ctx_manager.h"
#include "ble_srv_common.h"
#include "nrf_error.h"

#define KEYBOARD_REPORT_SIZE 8
#define KEYBOARD_INPUT_REPORT_INDEX 0
#define KEYBOARD_USB_INTERFACE 2

static void usb_hid_event_handler(app_usbd_class_inst_t const *instance,
                                  app_usbd_hid_user_event_t event);
static void ble_hid_event_handler(ble_hids_t *hids, ble_hids_evt_t *event);

APP_USBD_HID_KBD_GLOBAL_DEF(m_usb_keyboard,
                            KEYBOARD_USB_INTERFACE,
                            NRF_DRV_USBD_EPIN3,
                            usb_hid_event_handler,
                            APP_USBD_HID_SUBCLASS_BOOT);

BLE_LINK_CTX_MANAGER_DEF(m_hids_link_ctx_storage,
                          NRF_SDH_BLE_TOTAL_LINK_COUNT,
                          BLE_HIDS_LINK_CTX_SIZE_CALC(KEYBOARD_REPORT_SIZE));
static ble_hids_t m_hids = {
    .p_link_ctx_storage = &m_hids_link_ctx_storage,
};
static bool m_hids_registered;

static uint16_t m_ble_conn_handle = BLE_CONN_HANDLE_INVALID;
static volatile bool m_usb_tx_active;
static volatile bool m_usb_tx_complete_pending;
static bool m_usb_sync_required;
static uint8_t m_usb_desired[KEYBOARD_REPORT_SIZE];
static uint8_t m_usb_inflight[KEYBOARD_REPORT_SIZE];
static uint8_t m_usb_confirmed[KEYBOARD_REPORT_SIZE];
static volatile bool m_ble_tx_active;
static volatile bool m_ble_tx_complete_pending;
static volatile bool m_ble_report_notifications;
static volatile bool m_ble_boot_notifications;
static volatile bool m_ble_boot_mode;
static volatile bool m_ble_suspended;
static volatile bool m_ble_sync_required;
static uint8_t m_ble_desired[KEYBOARD_REPORT_SIZE];
static uint8_t m_ble_inflight[KEYBOARD_REPORT_SIZE];
static uint8_t m_ble_confirmed[KEYBOARD_REPORT_SIZE];

static uint8_t const m_keyboard_report_map[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x06,       /* Usage (Keyboard) */
    0xA1, 0x01,       /* Collection (Application) */
    0x05, 0x07,       /* Usage Page (Keyboard) */
    0x19, 0xE0,       /* Usage Minimum (Left Control) */
    0x29, 0xE7,       /* Usage Maximum (Right GUI) */
    0x15, 0x00,       /* Logical Minimum (0) */
    0x25, 0x01,       /* Logical Maximum (1) */
    0x75, 0x01,       /* Report Size (1) */
    0x95, 0x08,       /* Report Count (8) */
    0x81, 0x02,       /* Input (Data, Variable, Absolute) */
    0x95, 0x01,       /* Report Count (1) */
    0x75, 0x08,       /* Report Size (8) */
    0x81, 0x01,       /* Input (Constant) */
    0x95, 0x06,       /* Report Count (6) */
    0x75, 0x08,       /* Report Size (8) */
    0x15, 0x00,       /* Logical Minimum (0) */
    0x25, 0x65,       /* Logical Maximum (101) */
    0x05, 0x07,       /* Usage Page (Keyboard) */
    0x19, 0x00,       /* Usage Minimum (Reserved) */
    0x29, 0x65,       /* Usage Maximum (Keyboard Application) */
    0x81, 0x00,       /* Input (Data, Array) */
    0xC0,             /* End Collection */
};

static app_usbd_hid_kbd_ctx_t *usb_context(void) {
    return &m_usb_keyboard.specific.p_data->ctx;
}

static bool reports_equal(uint8_t const *left, uint8_t const *right) {
    return memcmp(left, right, KEYBOARD_REPORT_SIZE) == 0;
}

static uint32_t normalize_busy_error(uint32_t error) {
    return error == NRF_ERROR_BUSY ? NRF_ERROR_RESOURCES : error;
}

static bool usb_transport_available(void) {
    app_usbd_hid_kbd_ctx_t *context = usb_context();
    return app_usbd_core_state_get() == APP_USBD_STATE_Configured &&
           app_usbd_hid_state_valid(&context->hid_ctx) &&
           !app_usbd_hid_state_flag_test(&context->hid_ctx,
                                         APP_USBD_HID_STATE_FLAG_SUSPENDED);
}

static bool usb_transfer_pending(void) {
    return app_usbd_hid_state_flag_test(&usb_context()->hid_ctx,
                                        APP_USBD_HID_STATE_FLAG_TRANS_IN_PROGRESS);
}

bool keyboard_hid_usb_ready(void) {
    return usb_transport_available() && !m_usb_tx_active &&
           !m_usb_tx_complete_pending && !m_usb_sync_required &&
           !usb_transfer_pending() &&
           reports_equal(m_usb_desired, m_usb_confirmed);
}

static bool ble_transport_available(void) {
    if (m_ble_conn_handle == BLE_CONN_HANDLE_INVALID || m_ble_suspended ||
            !ble_conn_state_encrypted(m_ble_conn_handle) ||
            !ble_conn_state_mitm_protected(m_ble_conn_handle) ||
            !ble_conn_state_lesc(m_ble_conn_handle)) {
        return false;
    }

    bool notifications = m_ble_boot_mode ? m_ble_boot_notifications
                                         : m_ble_report_notifications;
    if (!notifications) {
        uint8_t cccd[2] = {0};
        ble_gatts_value_t value = {
            .len = sizeof(cccd),
            .offset = 0,
            .p_value = cccd,
        };
        uint16_t handle = m_ble_boot_mode
                          ? m_hids.boot_kb_inp_rep_handles.cccd_handle
                          : m_hids.inp_rep_array[KEYBOARD_INPUT_REPORT_INDEX]
                                .char_handles.cccd_handle;
        notifications = sd_ble_gatts_value_get(m_ble_conn_handle, handle, &value) ==
                            NRF_SUCCESS &&
                        ble_srv_is_notification_enabled(cccd);
    }
    return notifications;
}

bool keyboard_hid_ble_ready(void) {
    return ble_transport_available() && !m_ble_tx_active &&
           !m_ble_tx_complete_pending && !m_ble_sync_required &&
           reports_equal(m_ble_desired, m_ble_confirmed);
}

bool keyboard_hid_outputs_ready(uint8_t outputs) {
    if (outputs == 0 ||
            (outputs & ~(KEYBOARD_HID_OUTPUT_USB | KEYBOARD_HID_OUTPUT_BLE)) != 0) {
        return false;
    }
    if ((outputs & KEYBOARD_HID_OUTPUT_USB) && !keyboard_hid_usb_ready()) {
        return false;
    }
    if ((outputs & KEYBOARD_HID_OUTPUT_BLE) && !keyboard_hid_ble_ready()) {
        return false;
    }
    return true;
}

static uint32_t usb_submit_desired(void) {
    app_usbd_hid_kbd_ctx_t *context = usb_context();
    if (!usb_transport_available()) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_usb_tx_active || m_usb_tx_complete_pending || usb_transfer_pending()) {
        return NRF_ERROR_RESOURCES;
    }
    if (!m_usb_sync_required && reports_equal(m_usb_desired, m_usb_confirmed)) {
        return NRF_SUCCESS;
    }

    app_usbd_hid_access_lock(&context->hid_ctx);
    memcpy(&context->rep, m_usb_desired, KEYBOARD_REPORT_SIZE);
    if (m_usb_sync_required &&
            reports_equal(context->report_buff, m_usb_desired)) {
        // Force the Nordic class to resubmit a report whose previous transfer was lost.
        context->report_buff[KEYBOARD_REPORT_SIZE - 1] ^= 0xFFu;
    }
    app_usbd_hid_access_unlock(&context->hid_ctx);
    memcpy(m_usb_inflight, m_usb_desired, KEYBOARD_REPORT_SIZE);
    m_usb_tx_active = true;
    uint32_t error = m_usb_keyboard.specific.inst.hid_inst.p_hid_methods->ep_transfer_in(
        keyboard_hid_usb_class_instance());
    if (error != NRF_SUCCESS) {
        m_usb_tx_active = false;
    } else {
        m_usb_sync_required = false;
    }
    return normalize_busy_error(error);
}

static uint32_t ble_submit_desired(void) {
    if (!ble_transport_available()) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (m_ble_tx_active || m_ble_tx_complete_pending) {
        return NRF_ERROR_RESOURCES;
    }
    if (!m_ble_sync_required && reports_equal(m_ble_desired, m_ble_confirmed)) {
        return NRF_SUCCESS;
    }

    uint32_t error;
    memcpy(m_ble_inflight, m_ble_desired, KEYBOARD_REPORT_SIZE);
    if (m_ble_boot_mode) {
        error = ble_hids_boot_kb_inp_rep_send(&m_hids,
                                              KEYBOARD_REPORT_SIZE,
                                              m_ble_inflight,
                                              m_ble_conn_handle);
    } else {
        error = ble_hids_inp_rep_send(&m_hids,
                                      KEYBOARD_INPUT_REPORT_INDEX,
                                      KEYBOARD_REPORT_SIZE,
                                      m_ble_inflight,
                                      m_ble_conn_handle);
    }
    if (error == NRF_SUCCESS) {
        m_ble_tx_active = true;
        m_ble_sync_required = false;
    }
    return normalize_busy_error(error);
}

uint32_t keyboard_hid_send_report(uint8_t outputs, uint8_t modifiers, uint8_t usage) {
    if (outputs == 0 ||
            (outputs & ~(KEYBOARD_HID_OUTPUT_USB | KEYBOARD_HID_OUTPUT_BLE)) != 0) {
        return NRF_ERROR_INVALID_PARAM;
    }

    if ((outputs & KEYBOARD_HID_OUTPUT_USB) && !usb_transport_available()) {
        return NRF_ERROR_INVALID_STATE;
    }
    if ((outputs & KEYBOARD_HID_OUTPUT_BLE) && !ble_transport_available()) {
        return NRF_ERROR_INVALID_STATE;
    }
    if (((outputs & KEYBOARD_HID_OUTPUT_USB) && !keyboard_hid_usb_ready()) ||
            ((outputs & KEYBOARD_HID_OUTPUT_BLE) && !keyboard_hid_ble_ready())) {
        return NRF_ERROR_RESOURCES;
    }

    uint8_t report[KEYBOARD_REPORT_SIZE] = {modifiers, 0, usage, 0, 0, 0, 0, 0};
    uint32_t error;

    if (outputs & KEYBOARD_HID_OUTPUT_USB) {
        memcpy(m_usb_desired, report, KEYBOARD_REPORT_SIZE);
    }
    if (outputs & KEYBOARD_HID_OUTPUT_BLE) {
        memcpy(m_ble_desired, report, KEYBOARD_REPORT_SIZE);
    }

    if (outputs & KEYBOARD_HID_OUTPUT_BLE) {
        error = ble_submit_desired();
        if (error != NRF_SUCCESS) {
            return error;
        }
    }
    if (outputs & KEYBOARD_HID_OUTPUT_USB) {
        error = usb_submit_desired();
        if (error != NRF_SUCCESS) {
            return error;
        }
    }
    return NRF_SUCCESS;
}

void keyboard_hid_abort(void) {
    uint8_t released[KEYBOARD_REPORT_SIZE] = {0};
    memcpy(m_usb_desired, released, sizeof(released));
    memcpy(m_ble_desired, released, sizeof(released));
    keyboard_hid_process();
}

static void usb_hid_event_handler(app_usbd_class_inst_t const *instance,
                                  app_usbd_hid_user_event_t event) {
    app_usbd_hid_kbd_t const *keyboard = app_usbd_hid_kbd_class_get(instance);

    switch (event) {
        case APP_USBD_HID_USER_EVT_SET_BOOT_PROTO:
        case APP_USBD_HID_USER_EVT_SET_REPORT_PROTO:
            APP_ERROR_CHECK(hid_kbd_on_set_protocol(keyboard, event));
            m_usb_sync_required = true;
            break;

        case APP_USBD_HID_USER_EVT_IN_REPORT_DONE:
            if (m_usb_tx_active) {
                m_usb_tx_complete_pending = true;
            }
            break;

        default:
            break;
    }
}

app_usbd_class_inst_t const *keyboard_hid_usb_class_instance(void) {
    return app_usbd_hid_kbd_class_inst_get(&m_usb_keyboard);
}

void keyboard_hid_usb_reset(void) {
    (void)hid_kbd_clear_buffer(keyboard_hid_usb_class_instance());
    memset(m_usb_desired, 0, sizeof(m_usb_desired));
    memset(m_usb_inflight, 0, sizeof(m_usb_inflight));
    memset(m_usb_confirmed, 0, sizeof(m_usb_confirmed));
    m_usb_tx_active = false;
    m_usb_tx_complete_pending = false;
    m_usb_sync_required = false;
}

static void ble_hid_error_handler(uint32_t error) {
    APP_ERROR_HANDLER(error);
}

static void ble_hid_event_handler(ble_hids_t *hids, ble_hids_evt_t *event) {
    (void)hids;
    switch (event->evt_type) {
        case BLE_HIDS_EVT_NOTIF_ENABLED:
        case BLE_HIDS_EVT_NOTIF_DISABLED: {
            bool enabled = event->evt_type == BLE_HIDS_EVT_NOTIF_ENABLED;
            ble_hids_char_id_t const *id = &event->params.notification.char_id;
            if (id->uuid == BLE_UUID_BOOT_KEYBOARD_INPUT_REPORT_CHAR) {
                m_ble_boot_notifications = enabled;
                if (enabled) {
                    m_ble_sync_required = true;
                }
            } else if (id->uuid == BLE_UUID_REPORT_CHAR &&
                       id->rep_type == BLE_HIDS_REP_TYPE_INPUT &&
                       id->rep_index == KEYBOARD_INPUT_REPORT_INDEX) {
                m_ble_report_notifications = enabled;
                if (enabled) {
                    m_ble_sync_required = true;
                }
            }
            break;
        }

        case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
            m_ble_boot_mode = true;
            m_ble_sync_required = true;
            break;

        case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
            m_ble_boot_mode = false;
            m_ble_sync_required = true;
            break;

        case BLE_HIDS_EVT_HOST_SUSP:
            m_ble_suspended = true;
            break;

        case BLE_HIDS_EVT_HOST_EXIT_SUSP:
            m_ble_suspended = false;
            m_ble_sync_required = true;
            break;

        default:
            break;
    }
}

uint32_t keyboard_hid_ble_ensure_registered(void) {
    if (m_hids_registered) {
        return NRF_SUCCESS;
    }

    ble_hids_init_t init;
    static ble_hids_inp_rep_init_t input_report;
    security_req_t security = SEC_MITM;

    memset(&init, 0, sizeof(init));
    memset(&input_report, 0, sizeof(input_report));

    input_report.max_len = KEYBOARD_REPORT_SIZE;
    input_report.rep_ref.report_id = 0;
    input_report.rep_ref.report_type = BLE_HIDS_REP_TYPE_INPUT;
    input_report.sec.rd = security;
    input_report.sec.wr = SEC_NO_ACCESS;
    input_report.sec.cccd_wr = security;

    init.evt_handler = ble_hid_event_handler;
    init.error_handler = ble_hid_error_handler;
    init.is_kb = true;
    init.inp_rep_count = 1;
    init.p_inp_rep_array = &input_report;
    init.rep_map.p_data = (uint8_t *)m_keyboard_report_map;
    init.rep_map.data_len = sizeof(m_keyboard_report_map);
    init.rep_map.rd_sec = security;
    init.hid_information.bcd_hid = 0x0111;
    init.hid_information.b_country_code = 0;
    init.hid_information.flags = HID_INFO_FLAG_REMOTE_WAKE_MSK |
                                 HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;
    init.hid_information.rd_sec = security;
    init.protocol_mode_rd_sec = security;
    init.protocol_mode_wr_sec = security;
    init.ctrl_point_wr_sec = security;
    init.boot_kb_inp_rep_sec.rd = security;
    init.boot_kb_inp_rep_sec.wr = SEC_NO_ACCESS;
    init.boot_kb_inp_rep_sec.cccd_wr = security;
    init.boot_kb_outp_rep_sec.rd = security;
    init.boot_kb_outp_rep_sec.wr = security;
    init.boot_kb_outp_rep_sec.cccd_wr = SEC_NO_ACCESS;

    // Do NOT APP_ERROR_CHECK here: a full GATT attribute table returns
    // NRF_ERROR_NO_MEM, and asserting would reset the device in a boot loop.
    uint32_t err_code = ble_hids_init(&m_hids, &init);
    if (err_code == NRF_SUCCESS) {
        m_hids_registered = true;
    }
    return err_code;
}

bool keyboard_hid_ble_is_registered(void) {
    return m_hids_registered;
}

static void ble_state_clear(void) {
    m_ble_conn_handle = BLE_CONN_HANDLE_INVALID;
    m_ble_tx_active = false;
    m_ble_tx_complete_pending = false;
    m_ble_report_notifications = false;
    m_ble_boot_notifications = false;
    m_ble_boot_mode = false;
    m_ble_suspended = false;
    m_ble_sync_required = false;
    memset(m_ble_desired, 0, sizeof(m_ble_desired));
    memset(m_ble_inflight, 0, sizeof(m_ble_inflight));
    memset(m_ble_confirmed, 0, sizeof(m_ble_confirmed));
}

void keyboard_hid_process(void) {
    if (m_usb_tx_active && !m_usb_tx_complete_pending && !usb_transfer_pending()) {
        // A bus reset or suspend/resume can discard an IN transfer without IN_REPORT_DONE.
        m_usb_tx_active = false;
        m_usb_sync_required = true;
    }
    if (m_usb_tx_complete_pending) {
        m_usb_tx_complete_pending = false;
        memcpy(m_usb_confirmed, m_usb_inflight, KEYBOARD_REPORT_SIZE);
        m_usb_tx_active = false;
    }
    if (!m_usb_tx_active && !m_usb_tx_complete_pending &&
            (m_usb_sync_required ||
             !reports_equal(m_usb_desired, m_usb_confirmed))) {
        (void)usb_submit_desired();
    }

    if (m_ble_tx_complete_pending) {
        m_ble_tx_complete_pending = false;
        memcpy(m_ble_confirmed, m_ble_inflight, KEYBOARD_REPORT_SIZE);
        m_ble_tx_active = false;
    }
    if (!m_ble_tx_active && !m_ble_tx_complete_pending &&
            (m_ble_sync_required ||
             !reports_equal(m_ble_desired, m_ble_confirmed))) {
        (void)ble_submit_desired();
    }
}

void keyboard_hid_on_ble_evt(ble_evt_t const *event) {
    if (event == NULL || !m_hids_registered) {
        // When the HID service is not registered (feature disabled or not yet
        // armed) m_hids is uninitialized: never dispatch events into it, and
        // leave m_ble_conn_handle INVALID so keyboard_hid_process() stays inert.
        return;
    }

    if (event->header.evt_id == BLE_GAP_EVT_CONNECTED) {
        if (event->evt.gap_evt.params.connected.role == BLE_GAP_ROLE_PERIPH) {
            ble_state_clear();
            m_ble_conn_handle = event->evt.gap_evt.conn_handle;
            ble_hids_on_ble_evt(event, &m_hids);
        }
        return;
    }

    uint16_t conn_handle = event->evt.common_evt.conn_handle;
    if (conn_handle != m_ble_conn_handle ||
            m_ble_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return;
    }

    ble_hids_on_ble_evt(event, &m_hids);

    // With the default one-entry HVN queue, a successful HIDS submit owns the
    // next completion even though NUS and BAS share the same connection.
    if (event->header.evt_id == BLE_GATTS_EVT_HVN_TX_COMPLETE && m_ble_tx_active) {
        m_ble_tx_complete_pending = true;
    } else if (event->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
        ble_state_clear();
    }
}
