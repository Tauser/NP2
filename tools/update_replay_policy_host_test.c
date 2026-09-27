#include <assert.h>
#include <stdio.h>
#include "update_replay_policy.h"
int main(void) {
    update_manifest_t candidate = {.manifest_id = {1}, .app_version = 2};
    update_replay_record_t none = {0};
    assert(update_replay_check(&candidate, &none) == UPDATE_REPLAY_CANDIDATE);
    update_replay_record_t prior = {.valid = true, .manifest_id = {2}, .highest_app_version = 1};
    assert(update_replay_check(&candidate, &prior) == UPDATE_REPLAY_CANDIDATE);
    prior.manifest_id[0] = 1; assert(update_replay_check(&candidate, &prior) == UPDATE_REPLAY_DUPLICATE);
    prior.manifest_id[0] = 2; prior.highest_app_version = 2;
    assert(update_replay_check(&candidate, &prior) == UPDATE_REPLAY_NOT_NEWER);
    candidate.manifest_id[0] = 0; assert(update_replay_check(&candidate, &none) == UPDATE_REPLAY_INVALID);
    puts("update_replay_policy_host_test: all checks passed"); return 0;
}
