#include "update_admission.h"

update_admission_result_t update_admission_begin(
    const uint8_t *manifest_wire, size_t manifest_wire_bytes,
    const uint8_t *signature_wire, size_t signature_wire_bytes,
    const update_keyring_t *keyring, const update_environment_t *environment,
    const update_replay_record_t *accepted_record,
    update_image_hash_session_t *hash_session, update_manifest_t *out_manifest)
{
    if (manifest_wire == NULL || signature_wire == NULL || keyring == NULL ||
        environment == NULL || accepted_record == NULL || hash_session == NULL || out_manifest == NULL) {
        return UPDATE_ADMISSION_INVALID_ARGUMENT;
    }
    update_manifest_t manifest = {0};
    if (update_manifest_parse(manifest_wire, manifest_wire_bytes, &manifest) != UPDATE_MANIFEST_OK) {
        return UPDATE_ADMISSION_MANIFEST;
    }
    update_signature_t signature = {0};
    if (update_signature_parse(signature_wire, signature_wire_bytes, &signature) != UPDATE_SIGNATURE_OK) {
        return UPDATE_ADMISSION_SIGNATURE_WIRE;
    }
    const update_trusted_key_t *key = NULL;
    if (update_keyring_lookup(keyring, signature.key_id, &key) != UPDATE_KEYRING_OK) {
        return UPDATE_ADMISSION_KEYRING;
    }
    if (update_signature_verify_manifest(manifest_wire, manifest_wire_bytes, &signature, key) != UPDATE_SIGNATURE_OK) {
        return UPDATE_ADMISSION_SIGNATURE;
    }
    if (update_metadata_check(&manifest.metadata, environment) != UPDATE_METADATA_MATCH) {
        return UPDATE_ADMISSION_METADATA;
    }
    if (update_replay_check(&manifest, accepted_record) != UPDATE_REPLAY_CANDIDATE) {
        return UPDATE_ADMISSION_REPLAY;
    }
    if (update_image_hash_begin(hash_session, &manifest) != UPDATE_IMAGE_HASH_OK) {
        return UPDATE_ADMISSION_HASH;
    }
    *out_manifest = manifest;
    return UPDATE_ADMISSION_OK;
}
