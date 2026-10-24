#pragma once
#include <time.h>
#if defined(_WIN32) && !defined(ESP_PLATFORM)
static inline struct tm *agenda_localtime_r(const time_t *t, struct tm *out)
{ return localtime_s(out, t) == 0 ? out : NULL; }
static inline struct tm *agenda_gmtime_r(const time_t *t, struct tm *out)
{ return gmtime_s(out, t) == 0 ? out : NULL; }
#define localtime_r agenda_localtime_r
#define gmtime_r agenda_gmtime_r
#endif
