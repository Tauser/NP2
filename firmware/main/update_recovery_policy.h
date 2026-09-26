#pragma once

#include <stdbool.h>

#include "update_journal.h"

typedef enum {
    UPDATE_SLOT_UNKNOWN = 0,
    UPDATE_SLOT_EMPTY,
    UPDATE_SLOT_VALID,
    UPDATE_SLOT_PENDING_VERIFY,
    UPDATE_SLOT_INVALID,
} update_slot_state_t;

typedef struct {
    bool journal_valid;
    update_journal_state_t journal_state;
    update_slot_state_t running_slot;
    update_slot_state_t fallback_slot;
} update_recovery_input_t;

typedef enum {
    UPDATE_RECOVERY_NO_ACTION = 0,
    UPDATE_RECOVERY_RESUME_PENDING_HEALTH,
    UPDATE_RECOVERY_PRESERVE_ACTIVE,
    UPDATE_RECOVERY_REQUIRED,
} update_recovery_action_t;

/* Reconciles journal intent with real P4 slot states. It never selects a slot. */
update_recovery_action_t update_recovery_reconcile(const update_recovery_input_t *input);
