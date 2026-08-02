#include <assert.h>
#include <stdint.h>

#include "active_slot_snapshot_internal.h"

static void test_transaction_owner_revision_and_leases(void) {
    active_slot_snapshot_transaction_t transaction = {0};
    assert(!active_slot_snapshot_transaction_begin(
               &transaction, 0u, 2u, 1001u, 7u, 100u));
    assert(active_slot_snapshot_transaction_begin(
               &transaction, 1u, 2u, 1001u, 7u, 100u));
    assert(!active_slot_snapshot_transaction_begin(
               &transaction, 2u, 2u, 1001u, 8u, 100u));
    assert(active_slot_snapshot_transaction_matches(&transaction, 1u, 7u));
    assert(!active_slot_snapshot_transaction_matches(&transaction, 2u, 7u));
    assert(!active_slot_snapshot_transaction_matches(&transaction, 1u, 0u));
    assert(!active_slot_snapshot_transaction_matches(&transaction, 1u, 8u));

    active_slot_snapshot_transaction_refresh(&transaction, 2u, 200u);
    assert(transaction.last_activity == 100u);
    active_slot_snapshot_transaction_refresh(&transaction, 1u, 200u);
    assert(transaction.last_activity == 200u);

    assert(!active_slot_snapshot_transaction_expired(
               &transaction, ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS - 1u,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS - 1u,
               ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS));
    assert(active_slot_snapshot_transaction_expired(
               &transaction, ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS - 1u,
               ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS));
    assert(active_slot_snapshot_transaction_expired(
               &transaction, 0u, ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS));

    assert(FDS_WRITE_WITH_GC_MAX_MS ==
           3u * FDS_SYNC_OPERATION_TIMEOUT_MS);
    assert(ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS > FDS_WRITE_WITH_GC_MAX_MS);
    assert(active_slot_snapshot_commit_budget_available(
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS -
                   ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS));
    assert(!active_slot_snapshot_commit_budget_available(
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS -
                   ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS + 1u,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS));

    assert(active_slot_snapshot_transaction_begin_commit(
               &transaction, 1u, 1000u,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_COMMIT_MAX_MS));
    assert(transaction.committing);
    // A delayed FDS write/GC/retry owns its bounded commit window and cannot be
    // reported as an ordinary lease expiry while the handler is blocked.
    assert(!active_slot_snapshot_transaction_expired(
               &transaction, ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_IDLE_LEASE_MS,
               ACTIVE_SLOT_SNAPSHOT_ABSOLUTE_LEASE_MS));
    active_slot_snapshot_transaction_finish_commit(&transaction, 2u, 2000u);
    assert(transaction.committing);
    active_slot_snapshot_transaction_finish_commit(&transaction, 1u, 3000u);
    assert(!transaction.committing);
    assert(transaction.last_activity == 3000u);

    active_slot_snapshot_transaction_clear(&transaction);
    assert(!transaction.active);
    assert(transaction.revision == 0u);
}

static void test_exact_frozen_read_allowlist(void) {
    const uint8_t valid_read[] = {32u, 16u};
    const uint8_t zero_count[] = {0u, 0u};
    const uint8_t too_many[] = {63u, 2u};

    assert(active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_READ_EMU_BLOCK_DATA, 2u, valid_read, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_READ_EMU_BLOCK_DATA, 1u, valid_read, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_READ_EMU_BLOCK_DATA, 2u, zero_count, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_READ_EMU_BLOCK_DATA, 2u, too_many, 64u));

    assert(active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_GET_EMULATOR_CONFIG, 0u, NULL, 64u));
    assert(active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_GET_WRITE_MODE, 0u, NULL, 64u));
    assert(active_slot_snapshot_read_request_valid(
               DATA_CMD_HF14A_GET_ANTI_COLL_DATA, 0u, NULL, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_GET_WRITE_MODE, 1u, valid_read, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_GET_ACTIVE_SLOT, 0u, NULL, 64u));
    assert(!active_slot_snapshot_read_request_valid(
               DATA_CMD_MF1_WRITE_EMU_BLOCK_DATA, 2u, valid_read, 64u));
}

int main(void) {
    test_transaction_owner_revision_and_leases();
    test_exact_frozen_read_allowlist();
    return 0;
}
