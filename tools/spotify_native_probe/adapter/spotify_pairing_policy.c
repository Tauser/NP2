#include "spotify_pairing_policy.h"
#include "google_agenda_codec.h"
#include <string.h>
static int hex(char c) { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : c >= 'A' && c <= 'F' ? c-'A'+10 : -1; }
static bool decode(char *value)
{
    char *out = value;
    for (char *p = value; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '+') c = ' ';
        else if (c == '%') {
            if (!p[1] || !p[2] || hex(p[1]) < 0 || hex(p[2]) < 0) return false;
            c = (unsigned char)(hex(p[1])*16 + hex(p[2])); p += 2;
        }
        if (c < 32 || c == 127) return false;
        *out++ = (char)c;
    }
    *out = 0; return true;
}
static bool base64(const char *value, size_t low, size_t high)
{
    const size_t size = strlen(value);
    if (size < low || size > high || size % 4U) return false;
    bool padding = false;
    for (size_t i = 0; i < size; ++i) {
        const char c = value[i];
        if (c == '=') { if (i + 2U < size) return false; padding = true; }
        else if (padding || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/')) return false;
    }
    return true;
}
bool spotify_pairing_form(char *data, size_t size, spotify_pairing_form_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!data || !size || size > SPOTIFY_PAIRING_BODY_MAX || memchr(data, 0, size) || data[size]) return false;
    spotify_pairing_form_t result = {0};
    char *names[16] = {0}; unsigned count = 0; bool action = false;
    for (char *part = data; part;) {
        char *next = strchr(part, '&'); if (next) *next++ = 0;
        char *value = strchr(part, '=');
        if (!value || count == 16) return false;
        *value++ = 0;
        if (!decode(part) || !decode(value) || !*part || strlen(part) > 40) return false;
        for (unsigned i = 0; i < count; ++i) if (!strcmp(names[i], part)) return false;
        names[count++] = part;
        if (!strcmp(part,"action")) { if (strcmp(value,"addUser")) return false; action = true; }
        else if (!strcmp(part,"userName")) { if (!*value || strlen(value) > 128) return false; result.username=value; }
        else if (!strcmp(part,"blob")) { if (!base64(value,52,2732)) return false; result.blob=value; }
        else if (!strcmp(part,"clientKey")) { if (!base64(value,128,128) || strchr(value,'=')) return false; result.client_key=value; }
        else if (!strcmp(part,"deviceName")) { if (strlen(value) > 128) return false; result.device_name=value; }
        else if (strlen(value) > 128) return false;
        part = next;
    }
    if (!action || !result.username || !result.blob || !result.client_key) return false;
    *out = result; return true;
}
bool spotify_app_credentials(const char *json, size_t size, spotify_app_credentials_t *out, void *workspace)
{
    if (!out) return false;
    memset(out,0,sizeof(*out));
    google_json_t *doc = workspace;
    if (!json || size > 512 || !doc || !google_json_parse(doc,json,size) || doc->count != 5) return false;
    spotify_app_credentials_t result = {0};
    if (!google_json_string(doc,google_json_field(doc,0,"client_id"),result.client_id,sizeof(result.client_id)) ||
        !google_json_string(doc,google_json_field(doc,0,"client_secret"),result.client_secret,sizeof(result.client_secret)) ||
        strlen(result.client_id) != 32 || strlen(result.client_secret) != 32) return false;
    for (unsigned i=0;i<32;++i) if (hex(result.client_id[i]) < 0 || hex(result.client_secret[i]) < 0) return false;
    *out=result; return true;
}
