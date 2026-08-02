#include "fds_util.h"

#include <limits.h>
#include <string.h>

#include "app_util_platform.h"
#include "bsp_wdt.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "sdk_config.h"

#define NRF_LOG_MODULE_NAME fds_sync
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
NRF_LOG_MODULE_REGISTER();

#define FDS_SYNC_WDT_FEED_MS      100U
#define FDS_RECORD_OVERHEAD_WORDS 5U /* Three header words and two page-tag words. */
#define FDS_MAX_RECORD_WORDS      (FDS_VIRTUAL_PAGE_SIZE - FDS_RECORD_OVERHEAD_WORDS)

typedef enum {
    FDS_SYNC_OP_NONE,
    FDS_SYNC_OP_INIT,
    FDS_SYNC_OP_WRITE,
    FDS_SYNC_OP_DELETE,
    FDS_SYNC_OP_GC,
} fds_sync_op_t;

static struct {
    volatile bool busy;
    volatile bool complete;
    volatile bool timed_out;
    volatile bool initialized;
    volatile bool registered;
    volatile bool ignore_pm;
    volatile fds_sync_op_t pending;
    volatile ret_code_t result;
    volatile ret_code_t last_error;
    volatile uint32_t record_id;
    volatile uint16_t file_id;
    volatile uint16_t record_key;
} m_fds_state = {
    .ignore_pm = true,
    .last_error = FDS_ERR_NOT_INITIALIZED,
};

/* FDS retains the data pointer until its event arrives, including after our timeout. */
static uint32_t m_write_buffer[FDS_MAX_RECORD_WORDS];

static void fds_set_last_error(ret_code_t result) {
    CRITICAL_REGION_ENTER();
    m_fds_state.last_error = result;
    CRITICAL_REGION_EXIT();
}

ret_code_t fds_util_last_error(void) {
    ret_code_t result;
    CRITICAL_REGION_ENTER();
    result = m_fds_state.last_error;
    CRITICAL_REGION_EXIT();
    return result;
}

bool fds_util_is_ready(void) {
    bool initialized;
    CRITICAL_REGION_ENTER();
    initialized = m_fds_state.initialized;
    CRITICAL_REGION_EXIT();
    return initialized;
}

static bool fds_lock(bool require_initialized) {
    bool acquired = false;
    ret_code_t result = NRF_SUCCESS;

    /* A synchronous wait in an ISR can prevent the FDS completion interrupt. */
    if (__get_IPSR() != 0U) {
        fds_set_last_error(NRF_ERROR_INVALID_STATE);
        return false;
    }

    CRITICAL_REGION_ENTER();
    if (m_fds_state.busy) {
        result = FDS_ERR_BUSY;
    } else if (require_initialized && !m_fds_state.initialized) {
        result = FDS_ERR_NOT_INITIALIZED;
    } else {
        m_fds_state.busy = true;
        acquired = true;
    }
    m_fds_state.last_error = result;
    CRITICAL_REGION_EXIT();
    return acquired;
}

static void fds_finish(ret_code_t result) {
    CRITICAL_REGION_ENTER();
    m_fds_state.last_error = result;
    if (m_fds_state.timed_out && m_fds_state.pending != FDS_SYNC_OP_NONE) {
        /* Keep the lock and write buffer until the outstanding event arrives. */
        m_fds_state.timed_out = true;
    } else {
        m_fds_state.pending = FDS_SYNC_OP_NONE;
        m_fds_state.complete = false;
        m_fds_state.timed_out = false;
        m_fds_state.busy = false;
    }
    CRITICAL_REGION_EXIT();
}

static void fds_prepare_wait(fds_sync_op_t operation, uint16_t file_id,
                             uint16_t record_key, uint32_t record_id) {
    CRITICAL_REGION_ENTER();
    m_fds_state.file_id = file_id;
    m_fds_state.record_key = record_key;
    m_fds_state.record_id = record_id;
    m_fds_state.result = FDS_ERR_OPERATION_TIMEOUT;
    m_fds_state.complete = false;
    m_fds_state.timed_out = false;
    __DMB();
    m_fds_state.pending = operation;
    CRITICAL_REGION_EXIT();
}

static void fds_cancel_wait(void) {
    CRITICAL_REGION_ENTER();
    m_fds_state.pending = FDS_SYNC_OP_NONE;
    m_fds_state.complete = false;
    CRITICAL_REGION_EXIT();
}

static ret_code_t fds_wait(void) {
    for (uint32_t elapsed = 0; elapsed < FDS_SYNC_OPERATION_TIMEOUT_MS; elapsed++) {
        bool complete;
        ret_code_t result;

        CRITICAL_REGION_ENTER();
        complete = m_fds_state.complete;
        result = m_fds_state.result;
        CRITICAL_REGION_EXIT();
        if (complete) {
            return result;
        }

        /* The watchdog is initialized after the startup FDS reads. */
        if ((elapsed % FDS_SYNC_WDT_FEED_MS) == 0U && NRF_WDT->RUNSTATUS != 0U) {
            bsp_wdt_feed();
        }
        nrf_delay_ms(1);
    }

    /* Catch an event delivered at the timeout boundary. */
    bool complete;
    ret_code_t result;
    CRITICAL_REGION_ENTER();
    complete = m_fds_state.complete;
    result = m_fds_state.result;
    if (!complete) {
        /* Resolve the timeout/event race before releasing the critical section. */
        m_fds_state.timed_out = true;
    }
    CRITICAL_REGION_EXIT();
    return complete ? result : FDS_ERR_OPERATION_TIMEOUT;
}

static ret_code_t fds_find_record(uint16_t id, uint16_t key, fds_record_desc_t *desc) {
    fds_find_token_t token = {0};
    return fds_record_find(id, key, desc, &token);
}

static ret_code_t fds_gc_locked(void) {
    fds_prepare_wait(FDS_SYNC_OP_GC, 0, 0, 0);
    ret_code_t result = fds_gc();
    if (result != NRF_SUCCESS) {
        fds_cancel_wait();
        return result;
    }
    return fds_wait();
}

static ret_code_t fds_write_record_locked(uint16_t id, uint16_t key,
                                           uint16_t data_length_words) {
    fds_record_desc_t record_desc;
    ret_code_t find_result = fds_find_record(id, key, &record_desc);
    bool update = find_result == NRF_SUCCESS;
    if (!update && find_result != FDS_ERR_NOT_FOUND) {
        return find_result;
    }

    fds_record_t record = {
        .file_id = id,
        .key = key,
        .data = {
            .p_data = m_write_buffer,
            .length_words = data_length_words,
        },
    };

    fds_prepare_wait(FDS_SYNC_OP_WRITE, id, key, 0);
    ret_code_t result = update ? fds_record_update(&record_desc, &record)
                               : fds_record_write(&record_desc, &record);
    if (result != NRF_SUCCESS) {
        fds_cancel_wait();
        return result;
    }
    return fds_wait();
}

static ret_code_t fds_write_with_gc_locked(uint16_t id, uint16_t key,
                                            uint16_t data_length_words) {
    ret_code_t result = fds_write_record_locked(id, key, data_length_words);
    if (result != FDS_ERR_NO_SPACE_IN_FLASH) {
        return result;
    }

    NRF_LOG_INFO("FDS is full; running garbage collection before retry.");
    result = fds_gc_locked();
    if (result != NRF_SUCCESS) {
        return result;
    }
    return fds_write_record_locked(id, key, data_length_words);
}

bool fds_is_exists(uint16_t id, uint16_t key) {
    if (!fds_lock(true)) {
        return false;
    }

    fds_record_desc_t record_desc;
    ret_code_t result = fds_find_record(id, key, &record_desc);
    bool exists = result == NRF_SUCCESS;
    fds_finish(exists || result == FDS_ERR_NOT_FOUND ? NRF_SUCCESS : result);
    return exists;
}

bool fds_read_sync(uint16_t id, uint16_t key, uint16_t *length, uint8_t *buffer) {
    if (length == NULL || buffer == NULL) {
        fds_set_last_error(NRF_ERROR_NULL);
        return false;
    }

    uint16_t capacity = *length;
    *length = 0;
    if (!fds_lock(true)) {
        return false;
    }

    fds_record_desc_t record_desc;
    ret_code_t result = fds_find_record(id, key, &record_desc);
    if (result != NRF_SUCCESS) {
        fds_finish(result);
        return false;
    }

    fds_flash_record_t flash_record;
    result = fds_record_open(&record_desc, &flash_record);
    if (result == FDS_ERR_CRC_CHECK_FAILED) {
        /* Records made before CRC support have a zero CRC. Migrate them once. */
        fds_header_t const *header = (fds_header_t const *)record_desc.p_record;
        uint32_t record_bytes = (uint32_t)header->length_words * sizeof(uint32_t);
        if (header->crc16 != 0U || header->file_id != id || header->record_key != key ||
                header->length_words > FDS_MAX_RECORD_WORDS || record_bytes > capacity ||
                record_bytes > UINT16_MAX) {
            fds_finish(result);
            return false;
        }

        memcpy(m_write_buffer, header + 1, record_bytes);
        memcpy(buffer, m_write_buffer, record_bytes);
        result = fds_write_with_gc_locked(id, key, header->length_words);
        if (result == NRF_SUCCESS) {
            *length = (uint16_t)record_bytes;
            NRF_LOG_INFO("Migrated legacy CRC-less FDS record 0x%04x/0x%04x.", id, key);
        }
        fds_finish(result);
        return result == NRF_SUCCESS;
    }
    if (result != NRF_SUCCESS) {
        fds_finish(result);
        return false;
    }

    uint32_t record_bytes = (uint32_t)flash_record.p_header->length_words * sizeof(uint32_t);
    if (flash_record.p_header->length_words > FDS_MAX_RECORD_WORDS ||
            record_bytes > capacity || record_bytes > UINT16_MAX) {
        result = NRF_ERROR_DATA_SIZE;
    } else {
        memcpy(buffer, flash_record.p_data, record_bytes);
    }

    ret_code_t close_result = fds_record_close(&record_desc);
    if (result == NRF_SUCCESS && close_result != NRF_SUCCESS) {
        result = close_result;
    }
    if (result == NRF_SUCCESS) {
        *length = (uint16_t)record_bytes;
    }
    fds_finish(result);
    return result == NRF_SUCCESS;
}

bool fds_write_sync(uint16_t id, uint16_t key, uint16_t length, void *buffer) {
    uint32_t data_length_words = ((uint32_t)length + sizeof(uint32_t) - 1U) / sizeof(uint32_t);
    if (length > 0U && buffer == NULL) {
        fds_set_last_error(NRF_ERROR_NULL);
        return false;
    }
    if (data_length_words > FDS_MAX_RECORD_WORDS) {
        fds_set_last_error(FDS_ERR_RECORD_TOO_LARGE);
        return false;
    }
    if (!fds_lock(true)) {
        return false;
    }
    if (length == 0U) {
        fds_finish(NRF_SUCCESS);
        return true;
    }

    uint32_t padded_length = data_length_words * sizeof(uint32_t);
    memset(m_write_buffer, 0, padded_length);
    memcpy(m_write_buffer, buffer, length);

    ret_code_t result = fds_write_with_gc_locked(id, key, (uint16_t)data_length_words);
    fds_finish(result);
    return result == NRF_SUCCESS;
}

static ret_code_t fds_delete_record_locked(fds_record_desc_t *record_desc) {
    uint32_t record_id;
    ret_code_t result = fds_record_id_from_desc(record_desc, &record_id);
    if (result != NRF_SUCCESS) {
        return result;
    }
    fds_prepare_wait(FDS_SYNC_OP_DELETE, 0, 0, record_id);
    result = fds_record_delete(record_desc);
    if (result != NRF_SUCCESS) {
        fds_cancel_wait();
        return result;
    }
    return fds_wait();
}

int fds_delete_sync(uint16_t id, uint16_t key) {
    if (!fds_lock(true)) {
        return 0;
    }

    int delete_count = 0;
    while (true) {
        fds_record_desc_t record_desc;
        ret_code_t result = fds_find_record(id, key, &record_desc);
        if (result == FDS_ERR_NOT_FOUND) {
            fds_finish(NRF_SUCCESS);
            return delete_count;
        }
        if (result != NRF_SUCCESS) {
            fds_finish(result);
            return 0;
        }

        result = fds_delete_record_locked(&record_desc);
        if (result != NRF_SUCCESS) {
            fds_finish(result);
            return 0;
        }
        delete_count++;
    }
}

static bool is_peer_manager_record(uint16_t id_or_key) {
    return id_or_key > 0xBFFFU;
}

static bool event_matches_locked(fds_evt_t const *event) {
    switch (m_fds_state.pending) {
        case FDS_SYNC_OP_INIT:
            return event->id == FDS_EVT_INIT;
        case FDS_SYNC_OP_WRITE:
            return (event->id == FDS_EVT_WRITE || event->id == FDS_EVT_UPDATE) &&
                   event->write.file_id == m_fds_state.file_id &&
                   event->write.record_key == m_fds_state.record_key;
        case FDS_SYNC_OP_DELETE:
            return event->id == FDS_EVT_DEL_RECORD &&
                   event->del.record_id == m_fds_state.record_id;
        case FDS_SYNC_OP_GC:
            return event->id == FDS_EVT_GC;
        default:
            return false;
    }
}

static bool event_is_peer_manager_record(fds_evt_t const *event) {
    if (event->id == FDS_EVT_WRITE || event->id == FDS_EVT_UPDATE) {
        return is_peer_manager_record(event->write.file_id) ||
               is_peer_manager_record(event->write.record_key);
    }
    if (event->id == FDS_EVT_DEL_RECORD || event->id == FDS_EVT_DEL_FILE) {
        return is_peer_manager_record(event->del.file_id) ||
               is_peer_manager_record(event->del.record_key);
    }
    return false;
}

static void fds_evt_handler(fds_evt_t const *event) {
    bool matched;
    bool ignored;

    CRITICAL_REGION_ENTER();
    matched = event_matches_locked(event);
    ignored = m_fds_state.ignore_pm && event_is_peer_manager_record(event) && !matched;
    if (!ignored) {
        if (event->id == FDS_EVT_INIT) {
            m_fds_state.initialized = event->result == NRF_SUCCESS;
        }
        if (matched) {
            if (m_fds_state.timed_out) {
                /* The caller already failed; only release retained resources now. */
                m_fds_state.pending = FDS_SYNC_OP_NONE;
                m_fds_state.complete = false;
                m_fds_state.timed_out = false;
                m_fds_state.busy = false;
            } else {
                m_fds_state.result = event->result;
                __DMB();
                m_fds_state.complete = true;
            }
        }
    }
    CRITICAL_REGION_EXIT();
}

void fds_util_init(void) {
    if (!fds_lock(false)) {
        return;
    }
    if (m_fds_state.initialized) {
        fds_finish(NRF_SUCCESS);
        return;
    }

    ret_code_t result = NRF_SUCCESS;
    if (!m_fds_state.registered) {
        result = fds_register(fds_evt_handler);
        if (result == NRF_SUCCESS) {
            m_fds_state.registered = true;
        }
    }
    if (result == NRF_SUCCESS) {
        fds_prepare_wait(FDS_SYNC_OP_INIT, 0, 0, 0);
        result = fds_init();
        if (result == NRF_SUCCESS) {
            result = fds_wait();
        } else {
            fds_cancel_wait();
        }
    }
    fds_finish(result);
    if (result != NRF_SUCCESS) {
        NRF_LOG_ERROR("FDS initialization failed: 0x%08x", result);
    }
}

bool fds_gc_sync(void) {
    if (!fds_lock(true)) {
        return false;
    }
    ret_code_t result = fds_gc_locked();
    fds_finish(result);
    return result == NRF_SUCCESS;
}

bool fds_wipe(void) {
    if (!fds_lock(true)) {
        return false;
    }

    bool previous_ignore_pm;
    CRITICAL_REGION_ENTER();
    previous_ignore_pm = m_fds_state.ignore_pm;
    m_fds_state.ignore_pm = false;
    CRITICAL_REGION_EXIT();

    ret_code_t result = NRF_SUCCESS;
    while (result == NRF_SUCCESS) {
        fds_find_token_t token = {0};
        fds_record_desc_t record_desc = {0};
        result = fds_record_iterate(&record_desc, &token);
        if (result == FDS_ERR_NOT_FOUND) {
            result = NRF_SUCCESS;
            break;
        }
        if (result == NRF_SUCCESS) {
            result = fds_delete_record_locked(&record_desc);
        }
    }
    if (result == NRF_SUCCESS) {
        result = fds_gc_locked();
    }

    CRITICAL_REGION_ENTER();
    m_fds_state.ignore_pm = previous_ignore_pm;
    CRITICAL_REGION_EXIT();
    fds_finish(result);
    return result == NRF_SUCCESS;
}
