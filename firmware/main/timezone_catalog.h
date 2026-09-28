/* IANA zone selection mapped to the POSIX TZ values accepted by newlib. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Keep these persisted values stable: firmware before the complete catalog
 * stored exactly these five choices in its onboarding profile. */
#define TIMEZONE_CATALOG_SAO_PAULO     0U
#define TIMEZONE_CATALOG_BRASILIA      1U
#define TIMEZONE_CATALOG_BUENOS_AIRES  2U
#define TIMEZONE_CATALOG_NEW_YORK      3U
#define TIMEZONE_CATALOG_LONDON        4U
#define TIMEZONE_CATALOG_COUNT         462U

typedef struct {
    const char *iana;
    size_t iana_length;
    const char *posix;
    size_t posix_length;
} timezone_catalog_entry_t;

/* The returned slices point at immutable firmware data and remain valid for
 * the process lifetime. */
bool timezone_catalog_get(uint16_t index, timezone_catalog_entry_t *out_entry);
bool timezone_catalog_is_valid(uint16_t index);
size_t timezone_catalog_count(void);

/* Copy helpers make it safe for callers to create a terminated value for APIs
 * such as setenv() without modifying the embedded catalog. */
bool timezone_catalog_copy_iana(uint16_t index, char *out, size_t out_size);
bool timezone_catalog_copy_posix(uint16_t index, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
