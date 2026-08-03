#ifndef MF1_AUTH_LOG_INTERNAL_H
#define MF1_AUTH_LOG_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#define MF1_AUTH_LOG_FLAG_KEY_B  0x01u
#define MF1_AUTH_LOG_FLAG_NESTED 0x02u
#define MF1_AUTH_LOG_FLAG_SUCCESS 0x04u
#define MF1_AUTH_LOG_RECORD_SIZE 18u

typedef struct {
    uint8_t block;
    uint8_t flags;
    uint8_t uid[4];
    uint8_t nt[4];
    uint8_t nr[4];
    uint8_t ar[4];
} mf1_auth_log_record_t;

typedef struct {
    bool pending;
    uint32_t generation;
} mf1_auth_log_latch_t;

static inline uint8_t mf1_auth_log_flags(bool key_b, bool nested) {
    return (key_b ? MF1_AUTH_LOG_FLAG_KEY_B : 0u) |
           (nested ? MF1_AUTH_LOG_FLAG_NESTED : 0u);
}

static inline void mf1_auth_log_latch_reset(mf1_auth_log_latch_t *latch) {
    latch->pending = false;
    latch->generation = 0u;
}

static inline bool mf1_auth_log_latch_begin(mf1_auth_log_latch_t *latch,
                                             bool enabled,
                                             uint32_t generation) {
    latch->pending = enabled;
    latch->generation = enabled ? generation : 0u;
    return enabled;
}

static inline bool mf1_auth_log_latch_active(const mf1_auth_log_latch_t *latch,
                                              bool enabled,
                                              uint32_t generation) {
    return enabled && latch->pending && latch->generation == generation;
}

static inline bool mf1_auth_log_latch_finish(mf1_auth_log_latch_t *latch,
                                              bool enabled,
                                              uint32_t generation) {
    bool commit = mf1_auth_log_latch_active(latch, enabled, generation);
    mf1_auth_log_latch_reset(latch);
    return commit;
}

#endif
