#include "lf_em4x05_data.h"

#include <string.h>

#include "app_status.h"

/* EM4x05 command/response framing is intentionally quarantined. The previous
 * implementation could report false UIDs and has not been verified against
 * hardware captures or golden vectors. */
bool em4x05_read(em4x05_data_t *out, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return false;
}

uint8_t scan_em4x05(em4x05_data_t *out) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
    }
    return STATUS_NOT_IMPLEMENTED;
}
