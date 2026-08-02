#include "usb_main.h"
#include "syssleep.h"
#include "dataframe.h"
#include "netdata.h"
#include "keyboard_hid.h"
#include "settings.h"
#if defined(PROJECT_CHAMELEON_ULTRA)
#include "iso_dep_session.h"
#endif
#include "app_cmd.h"

#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"
#include "app_usbd_core.h"
#include "app_usbd_serial_num.h"
#include "app_usbd_string_desc.h"

#define NRF_LOG_MODULE_NAME usb_cdc
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

// USB DEFINES START
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst, app_usbd_cdc_acm_user_event_t event);

#define CDC_ACM_COMM_INTERFACE 0
#define CDC_ACM_COMM_EPIN NRF_DRV_USBD_EPIN2

#define CDC_ACM_DATA_INTERFACE 1
#define CDC_ACM_DATA_EPIN NRF_DRV_USBD_EPIN1
#define CDC_ACM_DATA_EPOUT NRF_DRV_USBD_EPOUT1

/** @brief CDC_ACM class instance */
APP_USBD_CDC_ACM_GLOBAL_DEF(m_app_cdc_acm,
                            cdc_acm_user_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_AT_V250);

// USB DEFINES END

// USB CODE START
volatile bool g_usb_connected = false;
volatile bool g_usb_port_opened = false;
volatile bool g_usb_led_marquee_enable = true;
static uint8_t cdc_data_buffer[NRF_DRV_USBD_EPSIZE];

#define USB_TX_QUEUE_DEPTH 2
typedef struct {
    volatile bool valid;
    uint16_t length;
    uint8_t data[NETDATA_MAX_FRAME_LENGTH];
} usb_tx_entry_t;

static usb_tx_entry_t m_usb_tx_queue[USB_TX_QUEUE_DEPTH];
static uint8_t m_usb_tx_head;
static uint8_t m_usb_tx_count;
static bool m_usb_tx_active;
static volatile uint32_t m_usb_tx_generation;
static uint16_t m_usb_rx_length;
static uint16_t m_usb_rx_offset;

// Do not dispatch another command until its complete response can be retained.
static bool usb_response_ready(void) {
    return g_usb_connected && g_usb_port_opened && m_usb_tx_count < USB_TX_QUEUE_DEPTH;
}

static void usb_tx_clear(void) {
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    m_usb_tx_generation++;
    for (uint8_t i = 0; i < USB_TX_QUEUE_DEPTH; i++) {
        m_usb_tx_queue[i].valid = false;
    }
    m_usb_tx_head = 0;
    m_usb_tx_count = 0;
    m_usb_tx_active = false;
    app_util_critical_region_exit(nested);
}

static void usb_tx_start(void) {
    if (m_usb_tx_active || m_usb_tx_count == 0 ||
            !g_usb_connected || !g_usb_port_opened) {
        return;
    }

    usb_tx_entry_t *entry = &m_usb_tx_queue[m_usb_tx_head];
    if (!entry->valid) {
        return;
    }
    ret_code_t err = app_usbd_cdc_acm_write(&m_app_cdc_acm, entry->data, entry->length);
    if (err == NRF_SUCCESS) {
        m_usb_tx_active = true;
    } else if (err != NRF_ERROR_BUSY && err != NRF_ERROR_INVALID_STATE) {
        NRF_LOG_WARNING("CDC ACM write deferred: 0x%x", err);
    }
}

static void usb_rx_arm(void) {
    if (!g_usb_port_opened || m_usb_rx_length != 0) {
        return;
    }

    while (g_usb_port_opened && m_usb_rx_length == 0) {
        ret_code_t err = app_usbd_cdc_acm_read_any(&m_app_cdc_acm, cdc_data_buffer,
                                                    sizeof(cdc_data_buffer));
        if (err == NRF_SUCCESS) {
            m_usb_rx_length = app_usbd_cdc_acm_rx_size(&m_app_cdc_acm);
            m_usb_rx_offset = data_frame_receive_from(cdc_data_buffer, m_usb_rx_length,
                                                       DATA_FRAME_TRANSPORT_USB);
            if (m_usb_rx_offset != m_usb_rx_length) {
                return;
            }
            m_usb_rx_length = 0;
            m_usb_rx_offset = 0;
            continue;
        }
        if (err != NRF_ERROR_IO_PENDING && err != NRF_ERROR_BUSY &&
                err != NRF_ERROR_INVALID_STATE) {
            NRF_LOG_WARNING("CDC ACM read deferred: 0x%x", err);
        }
        return;
    }
}

static void usb_rx_resume(void) {
    if (m_usb_rx_length != 0) {
        uint16_t consumed = data_frame_receive_from(cdc_data_buffer + m_usb_rx_offset,
                                                     m_usb_rx_length - m_usb_rx_offset,
                                                     DATA_FRAME_TRANSPORT_USB);
        m_usb_rx_offset += consumed;
        if (m_usb_rx_offset != m_usb_rx_length) {
            return;
        }
        m_usb_rx_length = 0;
        m_usb_rx_offset = 0;
    }
    usb_rx_arm();
}

/** @brief User event handler @ref app_usbd_cdc_acm_user_ev_handler_t */
static void cdc_acm_user_ev_handler(app_usbd_class_inst_t const *p_inst, app_usbd_cdc_acm_user_event_t event) {

    // app_usbd_cdc_acm_t const *p_cdc_acm = app_usbd_cdc_acm_class_get(p_inst);

    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
            NRF_LOG_INFO("CDC ACM port opened");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_USB);
            g_usb_port_opened = true;
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_USB);
            m_usb_rx_length = 0;
            m_usb_rx_offset = 0;
            usb_rx_arm();
            usb_tx_start();
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            NRF_LOG_INFO("CDC ACM port closed");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_USB);
            g_usb_port_opened = false;
            g_usb_led_marquee_enable = true;
            m_usb_rx_length = 0;
            m_usb_rx_offset = 0;
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_USB);
            usb_tx_clear();
            break;

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            if (m_usb_tx_active && m_usb_tx_count != 0) {
                m_usb_tx_queue[m_usb_tx_head].valid = false;
                m_usb_tx_head = (m_usb_tx_head + 1) % USB_TX_QUEUE_DEPTH;
                m_usb_tx_count--;
            }
            m_usb_tx_active = false;
            usb_tx_start();
            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            // Get amount of data transfered to process data
            m_usb_rx_length = app_usbd_cdc_acm_rx_size(&m_app_cdc_acm);
            m_usb_rx_offset = 0;
            usb_rx_resume();
            break;
        }
        default:
            break;
    }
}

static void usbd_user_ev_handler(app_usbd_event_type_t event) {
    switch (event) {
        case APP_USBD_EVT_DRV_SUSPEND:
            NRF_LOG_INFO("USB SUSPEND");
            break;

        case APP_USBD_EVT_DRV_RESUME:
            NRF_LOG_INFO("USB RESUME");
            usb_tx_start();
            usb_rx_arm();
            break;

        case APP_USBD_EVT_DRV_RESET:
            NRF_LOG_INFO("USB RESET");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_USB);
            keyboard_hid_usb_reset();
            g_usb_port_opened = false;
            m_usb_rx_length = 0;
            m_usb_rx_offset = 0;
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_USB);
            usb_tx_clear();
            break;

        case APP_USBD_EVT_STARTED:
            NRF_LOG_INFO("USB STARTED");
            usb_tx_start();
            usb_rx_arm();
            break;

        case APP_USBD_EVT_STOPPED:
            NRF_LOG_INFO("USB STOPPED");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_USB);
            g_usb_port_opened = false;
            app_usbd_disable();
            break;

        case APP_USBD_EVT_POWER_DETECTED:
            sleep_timer_stop();
            NRF_LOG_INFO("USB power detected");
            if (!nrf_drv_usbd_is_enabled()) {
                app_usbd_enable();
            }
            g_usb_led_marquee_enable = true;
            break;

        case APP_USBD_EVT_POWER_REMOVED:
            sleep_timer_start(SLEEP_DELAY_MS_USB_POWER_DISCONNECTED);
            NRF_LOG_INFO("USB power removed");
#if defined(PROJECT_CHAMELEON_ULTRA)
            iso_dep_session_owner_disconnected(DATA_FRAME_TRANSPORT_USB);
#endif
            app_cmd_transport_disconnected(DATA_FRAME_TRANSPORT_USB);
            keyboard_hid_usb_reset();
            g_usb_connected = false;
            g_usb_port_opened = false;
            g_usb_led_marquee_enable = false;
            m_usb_rx_length = 0;
            m_usb_rx_offset = 0;
            data_frame_reset_transport(DATA_FRAME_TRANSPORT_USB);
            usb_tx_clear();
            app_usbd_stop();
            break;

        case APP_USBD_EVT_POWER_READY:
            NRF_LOG_INFO("USB ready");
            g_usb_connected = true;
            app_usbd_start();
            break;

        default:
            // NRF_LOG_INFO("Other usb event: %d", event);
            break;
    }
}

// USB CODE END

void usb_cdc_init(void) {
    ret_code_t ret;
    static const app_usbd_config_t usbd_config = {
        .ev_state_proc = usbd_user_ev_handler
    };

    app_usbd_serial_num_generate();

    ret = app_usbd_init(&usbd_config);
    APP_ERROR_CHECK(ret);

    app_usbd_class_inst_t const *class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&m_app_cdc_acm);
    ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    // Keyboard HID is opt-in: only expose the composite USB HID keyboard
    // interface when the feature is enabled. A USB interface set is fixed at
    // enumeration, so toggling the setting requires a device reboot to apply.
    if (settings_get_keyboard_hid_enable_first_load()) {
        ret = app_usbd_class_append(keyboard_hid_usb_class_instance());
        APP_ERROR_CHECK(ret);
    }

    data_frame_set_flow_callback(DATA_FRAME_TRANSPORT_USB, usb_rx_resume);
    data_frame_set_ready_callback(DATA_FRAME_TRANSPORT_USB, usb_response_ready);
}

uint32_t usb_cdc_write_try(const void *p_buf, uint16_t length) {
    if (p_buf == NULL || length == 0 || length > NETDATA_MAX_FRAME_LENGTH) {
        return NRF_ERROR_INVALID_PARAM;
    }
    while (app_usbd_event_queue_process());
    if (!g_usb_connected || !g_usb_port_opened) {
        return NRF_ERROR_INVALID_STATE;
    }
    uint8_t tail;
    uint32_t generation;
    uint8_t nested = 0;
    app_util_critical_region_enter(&nested);
    if (m_usb_tx_count >= USB_TX_QUEUE_DEPTH ||
            !data_frame_current_transport_generation_valid()) {
        ret_code_t error = m_usb_tx_count >= USB_TX_QUEUE_DEPTH
                           ? NRF_ERROR_RESOURCES : NRF_ERROR_INVALID_STATE;
        app_util_critical_region_exit(nested);
        return error;
    }
    tail = (m_usb_tx_head + m_usb_tx_count) % USB_TX_QUEUE_DEPTH;
    generation = m_usb_tx_generation;
    m_usb_tx_queue[tail].valid = false;
    m_usb_tx_count++;
    app_util_critical_region_exit(nested);

    memcpy(m_usb_tx_queue[tail].data, p_buf, length);
    while (app_usbd_event_queue_process());
    app_util_critical_region_enter(&nested);
    uint8_t reserved_tail = m_usb_tx_count == 0 ? USB_TX_QUEUE_DEPTH :
                            (m_usb_tx_head + m_usb_tx_count - 1u) % USB_TX_QUEUE_DEPTH;
    if (generation != m_usb_tx_generation || reserved_tail != tail ||
            !data_frame_current_transport_generation_valid()) {
        if (generation == m_usb_tx_generation && reserved_tail == tail) {
            m_usb_tx_count--;
        }
        app_util_critical_region_exit(nested);
        return NRF_ERROR_INVALID_STATE;
    }
    m_usb_tx_queue[tail].length = length;
    __DMB();
    m_usb_tx_queue[tail].valid = true;
    app_util_critical_region_exit(nested);
    usb_tx_start();
    return NRF_SUCCESS;
}

void usb_cdc_write(const void *p_buf, uint16_t length) {
    ret_code_t err = usb_cdc_write_try(p_buf, length);
    if (err != NRF_SUCCESS) {
        NRF_LOG_WARNING("CDC ACM response not queued: 0x%x", err);
    }
}

// override fputc to printf to cdc serial
/* dont't enable
int fputc(int ch, FILE *f){
    static int ch_static;
    ch_static = ch;

    // must cdc is available
    if (g_usb_port_opened && g_usb_connected) {
        // send and wait done.
        ret_code_t ret;
        do {
            ret = app_usbd_cdc_acm_write(&m_app_cdc_acm, &ch_static, 1);
        } while(ret == NRF_ERROR_BUSY);

        // error log
        if (ret != NRF_SUCCESS) {
            NRF_LOG_ERROR("CDC ACM Unavailable, fputc: %c, return code: %d", ch_static, ret);
        }
    }

    return ch;
}
*/

bool is_usb_working(void) {
    return g_usb_port_opened;
}

bool is_usb_tx_idle(void) {
    return m_usb_tx_count == 0 && !m_usb_tx_active;
}
