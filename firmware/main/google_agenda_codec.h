#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "agenda_service.h"

#define GOOGLE_JSON_TOKENS 512U
typedef struct { uint16_t begin, end, next; uint8_t kind; } google_json_token_t;
typedef struct { const char *text; uint16_t count; google_json_token_t tokens[GOOGLE_JSON_TOKENS]; } google_json_t;
bool google_json_parse(google_json_t *doc, const char *text, size_t size);
int google_json_field(const google_json_t *doc, int object, const char *name);
bool google_json_string(const google_json_t *doc, int token, char *out, size_t size);
bool google_json_true(const google_json_t *doc, int token);
bool google_agenda_parse_time(const char *text, uint32_t *out);
bool google_agenda_format_time(uint32_t epoch, char out[21]);
/* Current calendar month in the panel's local timezone; upper bound exclusive. */
bool google_agenda_month_window(uint32_t now, uint32_t *from, uint32_t *until);
bool google_agenda_json_escape(const char *text, char *out, size_t size);
