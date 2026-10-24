#include "google_agenda_codec.h"
#include "agenda_time.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { JSON_OBJECT = 1, JSON_ARRAY, JSON_STRING, JSON_PRIMITIVE };
typedef struct { google_json_t *doc; size_t pos, size; } parser_t;
static void space(parser_t *p) { while (p->pos < p->size && strchr(" \r\n\t", p->doc->text[p->pos]) != NULL) ++p->pos; }
static bool value(parser_t *p, unsigned depth)
{
    space(p);
    if (depth > 8U || p->pos >= p->size || p->doc->count >= GOOGLE_JSON_TOKENS) return false;
    const uint16_t index = p->doc->count++;
    google_json_token_t *t = &p->doc->tokens[index];
    const char *s = p->doc->text;
    t->begin = (uint16_t)p->pos;
    const char c = s[p->pos++];
    if (c == '{' || c == '[') {
        t->kind = c == '{' ? JSON_OBJECT : JSON_ARRAY;
        const char close = c == '{' ? '}' : ']';
        space(p);
        if (p->pos < p->size && s[p->pos] == close) ++p->pos;
        else for (;;) {
            if (c == '{') {
                space(p);
                if (p->pos >= p->size || s[p->pos] != '"' || !value(p, depth + 1U)) return false;
                space(p);
                if (p->pos >= p->size || s[p->pos++] != ':') return false;
            }
            if (!value(p, depth + 1U)) return false;
            space(p);
            if (p->pos >= p->size) return false;
            if (s[p->pos] == close) { ++p->pos; break; }
            if (s[p->pos++] != ',') return false;
        }
    } else if (c == '"') {
        t->kind = JSON_STRING;
        ++t->begin;
        bool closed = false;
        while (p->pos < p->size) {
            const unsigned char ch = (unsigned char)s[p->pos++];
            if (ch == '"') { closed = true; break; }
            if (ch < 32U) return false;
            if (ch == '\\') {
                if (p->pos >= p->size) return false;
                const char esc = s[p->pos++];
                if (esc == 'u') {
                    for (unsigned i = 0U; i < 4U; ++i)
                        if (p->pos >= p->size || strchr("0123456789abcdefABCDEF", s[p->pos++]) == NULL) return false;
                } else if (strchr("\"\\/bfnrt", esc) == NULL) return false;
            }
        }
        if (!closed) return false;
    } else {
        t->kind = JSON_PRIMITIVE;
        while (p->pos < p->size && strchr(" \r\n\t,]}", s[p->pos]) == NULL) ++p->pos;
        size_t n = p->pos - t->begin;
        const char *v = s + t->begin;
        if (!((n == 4U && (memcmp(v, "true", 4U) == 0 || memcmp(v, "null", 4U) == 0)) ||
              (n == 5U && memcmp(v, "false", 5U) == 0))) {
            size_t i = 0U;
            if (i < n && v[i] == '-') ++i;
            if (i >= n || v[i] < '0' || v[i] > '9') return false;
            if (v[i++] != '0') while (i < n && v[i] >= '0' && v[i] <= '9') ++i;
            if (i < n && v[i] == '.') {
                size_t first = ++i;
                while (i < n && v[i] >= '0' && v[i] <= '9') ++i;
                if (i == first) return false;
            }
            if (i < n && (v[i] == 'e' || v[i] == 'E')) {
                ++i;
                if (i < n && (v[i] == '+' || v[i] == '-')) ++i;
                size_t first = i;
                while (i < n && v[i] >= '0' && v[i] <= '9') ++i;
                if (i == first) return false;
            }
            if (i != n) return false;
        }
    }
    t->end = (uint16_t)(t->kind == JSON_STRING ? p->pos - 1U : p->pos);
    t->next = p->doc->count;
    return true;
}

bool google_json_parse(google_json_t *doc, const char *text, size_t size)
{
    if (doc == NULL || text == NULL || size == 0U || size > 16384U || memchr(text, 0, size) != NULL) return false;
    memset(doc, 0, sizeof(*doc));
    doc->text = text;
    parser_t p = {.doc = doc, .size = size};
    if (!value(&p, 0U)) return false;
    space(&p);
    if (p.pos != size || doc->tokens[0].kind != JSON_OBJECT) return false;
    /* Duplicate member names are rejected, including escaped spellings. */
    for (uint16_t object = 0U; object < doc->count; ++object) {
        if (doc->tokens[object].kind != JSON_OBJECT) continue;
        for (uint16_t key = object + 1U; key < doc->tokens[object].next;) {
            char name[80];
            if (!google_json_string(doc, key, name, sizeof(name))) return false;
            uint16_t next = doc->tokens[key + 1U].next;
            for (uint16_t other = next; other < doc->tokens[object].next; other = doc->tokens[other + 1U].next) {
                char check[80];
                if (!google_json_string(doc, other, check, sizeof(check)) || strcmp(name, check) == 0) return false;
            }
            key = next;
        }
    }
    return true;
}

static unsigned hex(char c) { return c <= '9' ? (unsigned)(c - '0') : (unsigned)((c | 32) - 'a' + 10); }
bool google_json_string(const google_json_t *doc, int token, char *out, size_t size)
{
    if (doc == NULL || token < 0 || token >= doc->count || out == NULL || size == 0U || doc->tokens[token].kind != JSON_STRING) return false;
    size_t used = 0U;
    const google_json_token_t *t = &doc->tokens[token];
    for (uint16_t p = t->begin; p < t->end; ++p) {
        unsigned char c = (unsigned char)doc->text[p];
        if (c == '\\') {
            c = (unsigned char)doc->text[++p];
            if (c == 'u') {
                uint32_t code = 0U;
                for (unsigned j = 0U; j < 4U; ++j) code = code * 16U + hex(doc->text[++p]);
                if (code >= 0xd800U && code <= 0xdbffU) {
                    if (p + 6U >= t->end || doc->text[p + 1U] != '\\' || doc->text[p + 2U] != 'u') return false;
                    p += 2U;
                    uint32_t low = 0U;
                    for (unsigned j = 0U; j < 4U; ++j) low = low * 16U + hex(doc->text[++p]);
                    if (low < 0xdc00U || low > 0xdfffU) return false;
                    code = 0x10000U + ((code - 0xd800U) << 10U) + low - 0xdc00U;
                } else if (code >= 0xdc00U && code <= 0xdfffU) return false;
                char encoded[4]; size_t n;
                if (code < 0x80U) { encoded[0] = (char)code; n = 1U; }
                else if (code < 0x800U) { encoded[0] = (char)(0xc0U | (code >> 6U)); encoded[1] = (char)(0x80U | (code & 63U)); n = 2U; }
                else if (code < 0x10000U) { encoded[0] = (char)(0xe0U | (code >> 12U)); encoded[1] = (char)(0x80U | ((code >> 6U) & 63U)); encoded[2] = (char)(0x80U | (code & 63U)); n = 3U; }
                else { encoded[0] = (char)(0xf0U | (code >> 18U)); encoded[1] = (char)(0x80U | ((code >> 12U) & 63U)); encoded[2] = (char)(0x80U | ((code >> 6U) & 63U)); encoded[3] = (char)(0x80U | (code & 63U)); n = 4U; }
                if (code == 0U || n >= size - used) return false;
                memcpy(out + used, encoded, n); used += n; continue;
            }
            if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f';
        }
        if (used + 1U >= size) return false;
        out[used++] = (char)c;
    }
    out[used] = '\0';
    return true;
}

int google_json_field(const google_json_t *doc, int object, const char *name)
{
    if (doc == NULL || object < 0 || object >= doc->count || doc->tokens[object].kind != JSON_OBJECT) return -1;
    for (int key = object + 1; key < doc->tokens[object].next; key = doc->tokens[key + 1].next) {
        char actual[80];
        if (google_json_string(doc, key, actual, sizeof(actual)) && strcmp(actual, name) == 0) return key + 1;
    }
    return -1;
}
bool google_json_true(const google_json_t *doc, int token)
{
    return doc != NULL && token >= 0 && token < doc->count && doc->tokens[token].kind == JSON_PRIMITIVE &&
        doc->tokens[token].end - doc->tokens[token].begin == 4 && memcmp(doc->text + doc->tokens[token].begin, "true", 4U) == 0;
}

bool google_agenda_parse_time(const char *s, uint32_t *out)
{
    if (s == NULL || out == NULL) return false;
    const size_t size = strlen(s);
    if (size < 20U || size > 35U) return false;
    const unsigned positions[] = {0,1,2,3,5,6,8,9,11,12,14,15,17,18};
    for (size_t i = 0U; i < sizeof(positions)/sizeof(positions[0]); ++i)
        if (s[positions[i]] < '0' || s[positions[i]] > '9') return false;
    if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':') return false;
    unsigned y,m,d,h,min,sec;
    if (sscanf(s, "%4u-%2u-%2uT%2u:%2u:%2u", &y,&m,&d,&h,&min,&sec) != 6 ||
        y < 2025U || y > 2105U || m < 1U || m > 12U || h > 23U || min > 59U || sec > 59U) return false;
    static const unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = y % 4U == 0U && (y % 100U != 0U || y % 400U == 0U);
    if (d < 1U || d > days[m - 1U] + (m == 2U && leap ? 1U : 0U)) return false;
    int64_t total_days = 0;
    for (unsigned year = 1970U; year < y; ++year)
        total_days += year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U) ? 366 : 365;
    for (unsigned month = 1U; month < m; ++month) total_days += days[month - 1U] + (month == 2U && leap ? 1U : 0U);
    total_days += d - 1U;
    size_t p = 19U;
    if (s[p] == '.') { size_t first = ++p; while (p < size && s[p] >= '0' && s[p] <= '9') ++p; if (p == first) return false; }
    int offset = 0;
    if (s[p] == 'Z' && p + 1U == size) { /* UTC */ }
    else if ((s[p] == '+' || s[p] == '-') && p + 6U == size && s[p + 3U] == ':' &&
        s[p + 1U] >= '0' && s[p + 1U] <= '9' && s[p + 2U] >= '0' && s[p + 2U] <= '9' &&
        s[p + 4U] >= '0' && s[p + 4U] <= '9' && s[p + 5U] >= '0' && s[p + 5U] <= '9') {
        int oh = (s[p+1U]-'0')*10 + s[p+2U]-'0', om = (s[p+4U]-'0')*10 + s[p+5U]-'0';
        if (oh > 23 || om > 59) return false;
        offset = (oh*60+om)*60 * (s[p] == '+' ? 1 : -1);
    } else return false;
    int64_t epoch = total_days*86400 + h*3600U + min*60U + sec - offset;
    if (epoch < AGENDA_MIN_EPOCH || epoch > UINT32_MAX) return false;
    *out = (uint32_t)epoch;
    return true;
}

bool google_agenda_format_time(uint32_t epoch, char out[21])
{
    time_t t = (time_t)epoch;
    struct tm utc = {0};
    return out != NULL && gmtime_r(&t, &utc) != NULL && strftime(out, 21U, "%Y-%m-%dT%H:%M:%SZ", &utc) == 20U;
}

bool google_agenda_month_window(uint32_t now, uint32_t *from, uint32_t *until)
{
    if (now < AGENDA_MIN_EPOCH || from == NULL || until == NULL) return false;
    time_t current = (time_t)now;
    struct tm local = {0};
    if (localtime_r(&current, &local) == NULL) return false;
    local.tm_mday = 1; local.tm_hour = 0; local.tm_min = 0; local.tm_sec = 0;
    local.tm_isdst = -1;
    const time_t first = mktime(&local);
    ++local.tm_mon; local.tm_isdst = -1;
    const time_t next = mktime(&local);
    if (first < 0 || next <= first || (uint64_t)next > UINT32_MAX) return false;
    *from = (uint32_t)first; *until = (uint32_t)next;
    return true;
}

bool google_agenda_json_escape(const char *text, char *out, size_t size)
{
    if (text == NULL || out == NULL || size == 0U) return false;
    size_t used = 0U;
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        if (*p < 32U || *p == 127U) return false;
        if (*p == '"' || *p == '\\') { if (used + 2U >= size) return false; out[used++] = '\\'; }
        if (used + 1U >= size) return false;
        out[used++] = (char)*p;
    }
    out[used] = 0;
    return true;
}
