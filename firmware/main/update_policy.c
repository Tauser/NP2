#include "update_policy.h"

#include <stddef.h>

static bool link_matches(update_link_version_t a, update_link_version_t b)
{
    return a.major == b.major && a.minor == b.minor && a.patch == b.patch &&
           a.rpc == b.rpc && a.sdio_sw_aggr == b.sdio_sw_aggr;
}

update_metadata_result_t update_metadata_check(const update_metadata_t *m,
                                               const update_environment_t *e)
{
    if (m == NULL || e == NULL || m->product_id == 0 || m->board_id == 0 ||
        e->product_id == 0 || e->board_id == 0 ||
        m->revision_min > m->revision_max || m->schema_min == 0 ||
        m->schema_min > m->schema_max || e->persisted_schema == 0) {
        return UPDATE_METADATA_INVALID;
    }
    if (!e->current_app_confirmed || !e->transaction_idle) {
        return UPDATE_METADATA_BUSY;
    }
    /* C6 updates remain closed until its reproducibility/recovery gate and
     * the intermediate-pair matrix are implemented and physically proven. */
    if (m->replaces_c6 || m->replaces_bootloader || m->replaces_partition_table) {
        return UPDATE_METADATA_UNSUPPORTED_OPERATION;
    }
    if (m->target != UPDATE_TARGET_ESP32P4 || m->product_id != e->product_id ||
        m->board_id != e->board_id || e->revision < m->revision_min ||
        e->revision > m->revision_max) {
        return UPDATE_METADATA_WRONG_DEVICE;
    }
    if (m->image_bytes == 0 || m->image_bytes > e->inactive_slot_bytes) {
        return UPDATE_METADATA_IMAGE_SIZE;
    }
    if (m->security_version != e->security_version) {
        return UPDATE_METADATA_SECURITY_VERSION;
    }
    if (e->persisted_schema < m->schema_min || e->persisted_schema > m->schema_max) {
        return UPDATE_METADATA_SCHEMA;
    }
    const update_link_version_t baseline = {3, 0, 6, 2, true};
    if (!link_matches(e->current_c6, baseline) ||
        !link_matches(m->required_c6, e->current_c6)) {
        return UPDATE_METADATA_C6_MISMATCH;
    }
    return UPDATE_METADATA_MATCH;
}

void update_boot_policy_init(update_boot_policy_t *p, uint64_t boot_ms,
                             bool pending_verify)
{
    if (p == NULL) {
        return;
    }
    *p = (update_boot_policy_t){
        .boot_ms = boot_ms,
        .last_sample_ms = boot_ms,
        .pending_verify = pending_verify,
        .initialized = true,
    };
}

static bool progress_fresh(bool seen, uint64_t progress_ms, uint64_t boot_ms,
                           uint64_t now_ms, uint64_t max_age_ms)
{
    return seen && progress_ms >= boot_ms && progress_ms <= now_ms &&
           now_ms - progress_ms <= max_age_ms;
}

update_boot_action_t update_boot_policy_poll(update_boot_policy_t *p,
                                             uint64_t now_ms,
                                             const update_health_sample_t *s)
{
    if (p == NULL || !p->initialized) {
        return UPDATE_BOOT_RECOVERY;
    }
    if (!p->pending_verify) {
        return UPDATE_BOOT_NOT_PENDING;
    }
    if (now_ms < p->last_sample_ms || now_ms < p->boot_ms) {
        p->clock_fault = true;
    }
    if (p->clock_fault) {
        return UPDATE_BOOT_RECOVERY;
    }
    if (now_ms - p->boot_ms >= UPDATE_HEALTH_DEADLINE_MS) {
        return s != NULL && s->fallback_bootable ? UPDATE_BOOT_ROLLBACK :
                                                 UPDATE_BOOT_RECOVERY;
    }
    if (now_ms - p->last_sample_ms > UPDATE_HEALTH_SAMPLE_GAP_MS) {
        p->tracking_health = false;
    }
    p->last_sample_ms = now_ms;
    const bool healthy = s != NULL && s->display_ready && s->first_frame_presented &&
        s->local_services_ready &&
        progress_fresh(s->app_progress_seen, s->app_progress_ms, p->boot_ms,
                       now_ms, UPDATE_HEALTH_APP_AGE_MS) &&
        progress_fresh(s->ui_progress_seen, s->ui_progress_ms, p->boot_ms,
                       now_ms, UPDATE_HEALTH_UI_AGE_MS);
    if (!healthy) {
        p->tracking_health = false;
        return UPDATE_BOOT_WAIT;
    }
    if (!p->tracking_health) {
        p->tracking_health = true;
        p->healthy_since_ms = now_ms;
    }
    return now_ms - p->healthy_since_ms >= UPDATE_HEALTH_STABLE_MS ?
           UPDATE_BOOT_CONFIRM : UPDATE_BOOT_WAIT;
}
