#include "update_replay_policy.h"

#include <string.h>

static bool all_zero(const uint8_t *bytes, size_t bytes_count)
{
    uint8_t value = 0;
    for (size_t index = 0; index < bytes_count; ++index) value |= bytes[index];
    return value == 0;
}

update_replay_result_t update_replay_check(const update_manifest_t *candidate,
                                           const update_replay_record_t *accepted)
{
    if (candidate == NULL || accepted == NULL || candidate->app_version == 0U ||
        all_zero(candidate->manifest_id, sizeof(candidate->manifest_id))) {
        return UPDATE_REPLAY_INVALID;
    }
    if (!accepted->valid) return UPDATE_REPLAY_CANDIDATE;
    if (all_zero(accepted->manifest_id, sizeof(accepted->manifest_id))) return UPDATE_REPLAY_INVALID;
    if (memcmp(candidate->manifest_id, accepted->manifest_id, sizeof(candidate->manifest_id)) == 0)
        return UPDATE_REPLAY_DUPLICATE;
    return candidate->app_version > accepted->highest_app_version ? UPDATE_REPLAY_CANDIDATE :
                                                                     UPDATE_REPLAY_NOT_NEWER;
}
