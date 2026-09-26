#include "update_recovery_policy.h"

update_recovery_action_t update_recovery_reconcile(const update_recovery_input_t *input)
{
    if (input == NULL) {
        return UPDATE_RECOVERY_REQUIRED;
    }
    if (input->running_slot == UPDATE_SLOT_PENDING_VERIFY) {
        return input->fallback_slot == UPDATE_SLOT_VALID
                   ? UPDATE_RECOVERY_RESUME_PENDING_HEALTH
                   : UPDATE_RECOVERY_REQUIRED;
    }
    if (input->running_slot != UPDATE_SLOT_VALID) {
        return UPDATE_RECOVERY_REQUIRED;
    }
    if (!input->journal_valid) {
        /* A valid running application is safer than making a decision from damaged NVS. */
        return UPDATE_RECOVERY_PRESERVE_ACTIVE;
    }
    switch (input->journal_state) {
    case UPDATE_JOURNAL_IDLE:
    case UPDATE_JOURNAL_ACCEPTED:
        return UPDATE_RECOVERY_NO_ACTION;
    case UPDATE_JOURNAL_P4_STAGED:
    case UPDATE_JOURNAL_P4_PENDING:
        /* No pending running slot means activation did not complete; retain the active app. */
        return UPDATE_RECOVERY_PRESERVE_ACTIVE;
    default:
        return UPDATE_RECOVERY_REQUIRED;
    }
}
