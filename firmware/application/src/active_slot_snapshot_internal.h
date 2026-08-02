#ifndef ACTIVE_SLOT_SNAPSHOT_INTERNAL_H
#define ACTIVE_SLOT_SNAPSHOT_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "data_cmd.h"
#include "fds_util.h"

#define ACTIVE_SLOT_SNAPSHOT_VERSION             2u
#define ACTIVE_SLOT_SNAPSHOT_OP_BEGIN            0u
#define ACTIVE_SLOT_SNAPSHOT_OP_SAVE_RELEASE     1u
#define ACTIVE_SLOT_SNAPSHOT_OP_ABORT            2u

#define ACTIVE_SLOT_SNAPSHOT_BEGIN_REQUEST_SIZE  2u
#define ACTIVE_SLOT_SNAPSHOT_END_REQUEST_SIZE    6u
#define ACTIVE_SLOT_SNAPSHOT_BEGIN_RESPONSE_SIZE 13u
#define ACTIVE_SLOT_SNAPSHOT_END_RESPONSE_SIZE   6u

#define ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS       5000u
#define ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS   120000u
#define ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS       (FDS_WRITE_WITH_GC_MAX_MS + 1000u)

typedef struct {
    bool active;
    uint8_t owner;
    uint8_t slot;
    uint16_t tag_type;
    uint32_t revision;
    uint32_t started_at;
    uint32_t last_activity;
    bool committing;
} active_slot_snapshot_transaction_t;

static inline bool active_slot_snapshot_transaction_begin(
    active_slot_snapshot_transaction_t *transaction,
    uint8_t owner,
    uint8_t slot,
    uint16_t tag_type,
    uint32_t revision,
    uint32_t now
) {
    if (transaction == NULL || transaction->active || owner == 0u || revision == 0u) {
        return false;
    }
    transaction->active = true;
    transaction->owner = owner;
    transaction->slot = slot;
    transaction->tag_type = tag_type;
    transaction->revision = revision;
    transaction->started_at = now;
    transaction->last_activity = now;
    return true;
}

static inline bool active_slot_snapshot_transaction_matches(
    const active_slot_snapshot_transaction_t *transaction,
    uint8_t owner,
    uint32_t revision
) {
    return transaction != NULL && transaction->active &&
           transaction->owner == owner && revision != 0u &&
           transaction->revision == revision;
}

static inline void active_slot_snapshot_transaction_refresh(
    active_slot_snapshot_transaction_t *transaction,
    uint8_t owner,
    uint32_t now
) {
    if (transaction != NULL && transaction->active && transaction->owner == owner) {
        transaction->last_activity = now;
    }
}

static inline bool active_slot_snapshot_transaction_expired(
    const active_slot_snapshot_transaction_t *transaction,
    uint32_t idle_elapsed,
    uint32_t absolute_elapsed,
    uint32_t idle_limit,
    uint32_t absolute_limit
) {
    return transaction != NULL && transaction->active && !transaction->committing &&
           (idle_elapsed >= idle_limit || absolute_elapsed >= absolute_limit);
}

static inline bool active_slot_snapshot_commit_budget_available(
    uint32_t absolute_elapsed,
    uint32_t absolute_limit,
    uint32_t commit_budget
) {
    return absolute_elapsed <= absolute_limit &&
           commit_budget <= absolute_limit - absolute_elapsed;
}

static inline bool active_slot_snapshot_transaction_begin_commit(
    active_slot_snapshot_transaction_t *transaction,
    uint8_t owner,
    uint32_t absolute_elapsed,
    uint32_t absolute_limit,
    uint32_t commit_budget
) {
    if (transaction == NULL || !transaction->active || transaction->committing ||
            transaction->owner != owner ||
            !active_slot_snapshot_commit_budget_available(
                absolute_elapsed, absolute_limit, commit_budget)) {
        return false;
    }
    transaction->committing = true;
    return true;
}

static inline void active_slot_snapshot_transaction_finish_commit(
    active_slot_snapshot_transaction_t *transaction,
    uint8_t owner,
    uint32_t now
) {
    if (transaction != NULL && transaction->active && transaction->committing &&
        transaction->owner == owner) {
        transaction->committing = false;
        transaction->last_activity = now;
    }
}

static inline void active_slot_snapshot_transaction_clear(
    active_slot_snapshot_transaction_t *transaction
) {
    if (transaction != NULL) memset(transaction, 0, sizeof(*transaction));
}

static inline bool active_slot_snapshot_is_read_command(uint16_t cmd) {
    return cmd == DATA_CMD_MF1_READ_EMU_BLOCK_DATA ||
           cmd == DATA_CMD_MF1_GET_EMULATOR_CONFIG ||
           cmd == DATA_CMD_MF1_GET_WRITE_MODE ||
           cmd == DATA_CMD_HF14A_GET_ANTI_COLL_DATA;
}

static inline bool active_slot_snapshot_read_request_valid(
    uint16_t cmd,
    uint16_t length,
    const uint8_t *data,
    uint16_t block_count
) {
    if (cmd == DATA_CMD_MF1_READ_EMU_BLOCK_DATA) {
        if (length != 2u || data == NULL || data[1] == 0u || data[1] > 32u) {
            return false;
        }
        return (uint16_t)data[0] + data[1] <= block_count;
    }
    return active_slot_snapshot_is_read_command(cmd) && length == 0u;
}

#endif
