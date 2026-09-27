#include <assert.h>
#include <stdio.h>

#include "update_recovery_policy.h"

int main(void)
{
    update_recovery_input_t input = {.journal_valid = true,
                                     .journal_state = UPDATE_JOURNAL_IDLE,
                                     .running_slot = UPDATE_SLOT_VALID,
                                     .fallback_slot = UPDATE_SLOT_VALID};
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_NO_ACTION);
    input.journal_state = UPDATE_JOURNAL_P4_STAGED;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_PRESERVE_ACTIVE);
    input.journal_state = UPDATE_JOURNAL_P4_PENDING;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_PRESERVE_ACTIVE);
    input.journal_valid = false;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_PRESERVE_ACTIVE);
    input.running_slot = UPDATE_SLOT_PENDING_VERIFY;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_RESUME_PENDING_HEALTH);
    input.fallback_slot = UPDATE_SLOT_INVALID;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_REQUIRED);
    input.running_slot = UPDATE_SLOT_INVALID;
    assert(update_recovery_reconcile(&input) == UPDATE_RECOVERY_REQUIRED);
    assert(update_recovery_reconcile(NULL) == UPDATE_RECOVERY_REQUIRED);
    puts("update_recovery_policy_host_test: all checks passed");
    return 0;
}
