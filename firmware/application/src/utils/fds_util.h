#ifndef FDS_UTIL_H__
#define FDS_UTIL_H__

#include "fds.h"

#define FDS_SYNC_OPERATION_TIMEOUT_MS 15000U
#define FDS_WRITE_WITH_GC_MAX_MS      (3U * FDS_SYNC_OPERATION_TIMEOUT_MS)

bool fds_read_sync(uint16_t id, uint16_t key, uint16_t *length, uint8_t *buffer);
bool fds_write_sync(uint16_t id, uint16_t key, uint16_t length, void *buffer);
/* Returns the number deleted; inspect fds_util_last_error() when zero is ambiguous. */
int fds_delete_sync(uint16_t id, uint16_t key);
bool fds_is_exists(uint16_t id, uint16_t key);
void fds_util_init(void);
/* Detailed status for bool/count APIs, including timeout and busy failures. */
bool fds_util_is_ready(void);
ret_code_t fds_util_last_error(void);
bool fds_gc_sync(void);
bool fds_wipe(void);

#endif
