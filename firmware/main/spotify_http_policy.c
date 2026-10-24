#include "spotify_http_policy.h"
#include <string.h>
#include <limits.h>

static bool bounded_ascii(const char *s, size_t max, bool required)
{
    if (!s) return !required;
    size_t n = 0;
    for (; n <= max && s[n]; ++n)
        if ((unsigned char)s[n] < 0x20U || (unsigned char)s[n] > 0x7eU) return false;
    return n <= max && (!required || n != 0);
}
static bool number(const char **p, uint64_t *out)
{
    uint64_t n = 0; unsigned digits = 0;
    while (**p >= '0' && **p <= '9') {
        if (++digits > 10) return false;
        n = n*10U + (unsigned)(*(*p)++ - '0');
        if (n > INT32_MAX) return false;
    }
    if (!digits) return false;
    *out = n; return true;
}
static bool range_request(const char *range, spotify_http_policy_t *p)
{
    if (!range || strncmp(range, "bytes=", 6)) return false;
    range += 6;
    p->suffix = *range == '-';
    if (p->suffix) {
        ++range;
        return number(&range, &p->last) && p->last > 0 &&
            p->last <= SPOTIFY_HTTP_AUDIO_MAX && !*range;
    }
    return number(&range, &p->first) && *range++ == '-' && number(&range, &p->last) &&
        !*range && p->last >= p->first && p->last-p->first < SPOTIFY_HTTP_AUDIO_MAX;
}
static bool audio_host(const char *url)
{
    static const char scheme[] = "https://";
    if (strncmp(url, scheme, sizeof(scheme)-1)) return false;
    const char *host = url+sizeof(scheme)-1, *end = strchr(host, '/');
    if (!end || end == host) return false;
    for (const char *c=host;c<end;++c)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-' || *c == '.')) return false;
    const char *suffixes[] = {".scdn.co", ".spotifycdn.com", ".spotifycdn.net", ".spotify-com.akamaized.net"};
    for (unsigned i=0;i<sizeof(suffixes)/sizeof(suffixes[0]);++i) {
        size_t n=strlen(suffixes[i]);
        if ((size_t)(end-host)>n && !memcmp(end-n,suffixes[i],n)) return true;
    }
    return false;
}
static bool token_body(const spotify_http_request_t *r, bool basic)
{
    static const char grant[]="grant_type=client_credentials";
    if (basic) return r->body_size==sizeof(grant)-1 && !memcmp(r->body,grant,sizeof(grant)-1);
    static const char prefix[]="grant_type=client_credentials&client_id=";
    static const char separator[]="&client_secret=";
    if (r->body_size<=sizeof(prefix)-1 || memcmp(r->body,prefix,sizeof(prefix)-1)) return false;
    size_t i=sizeof(prefix)-1, start=i;
    while (i<r->body_size && r->body[i]!='&') {
        uint8_t c=r->body[i++];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_')) return false;
    }
    if (i-start<16 || r->body_size-i<=sizeof(separator)-1 || memcmp(r->body+i,separator,sizeof(separator)-1)) return false;
    i+=sizeof(separator)-1;start=i;
    for (;i<r->body_size;++i) {
        uint8_t c=r->body[i];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_')) return false;
    }
    return i-start>=16;
}
bool spotify_http_validate_request(const spotify_http_request_t *r, spotify_http_policy_t *out)
{
    if (!r || !out || !bounded_ascii(r->url,SPOTIFY_HTTP_URL_MAX,true) ||
        !bounded_ascii(r->authorization,SPOTIFY_HTTP_AUTH_MAX,false) ||
        !bounded_ascii(r->content_type,96,false) || !bounded_ascii(r->range,SPOTIFY_HTTP_RANGE_MAX,false) ||
        r->body_size>SPOTIFY_HTTP_BODY_MAX || (r->body_size && !r->body) || strchr(r->url,'#') || strchr(r->url,' ')) return false;
    spotify_http_policy_t p={.body_limit=SPOTIFY_HTTP_JSON_MAX};
    const bool auth=r->authorization && *r->authorization, range=r->range && *r->range;
    if (!strcmp(r->url,"https://apresolve.spotify.com/")) {
        if (r->post || auth || range || r->body_size) return false;
        p.kind=SPOTIFY_HTTP_RESOLVE;
        p.body_limit=4096U;
    } else if (!strcmp(r->url,"https://accounts.spotify.com/api/token")) {
        if (!r->post || (auth && (strncmp(r->authorization,"Basic ",6) || !r->authorization[6])) || range || !r->body_size ||
            !token_body(r,auth) ||
            !r->content_type || strcmp(r->content_type,"application/x-www-form-urlencoded")) return false;
        p.kind=SPOTIFY_HTTP_TOKEN;
        p.body_limit=4096U;
    } else {
        static const char prefix[]="https://api.spotify.com/v1/storage-resolve/files/audio/interactive/";
        if (!strncmp(r->url,prefix,sizeof(prefix)-1)) {
            if (r->post || !auth || strncmp(r->authorization,"Bearer ",7) || !r->authorization[7] || range || r->body_size) return false;
            const char *id=r->url+sizeof(prefix)-1;
            if (strlen(id)!=40U+strlen("?alt=json&product=9")) return false;
            for (unsigned i=0;i<40;++i) if (!((id[i]>='0' && id[i]<='9') || (id[i]>='a' && id[i]<='f'))) return false;
            if (strcmp(id+40,"?alt=json&product=9")) return false;
            p.kind=SPOTIFY_HTTP_STORAGE;
        } else if (audio_host(r->url)) {
            if (r->post || auth || r->body_size || !range_request(r->range,&p)) return false;
            p.kind=SPOTIFY_HTTP_AUDIO;
            p.body_limit=(size_t)(p.suffix ? p.last : p.last-p.first+1U);
        } else return false;
    }
    *out=p;return true;
}
bool spotify_http_validate_response(const spotify_http_policy_t *p, int status,
    int64_t length, const char *range, size_t actual, bool complete, uint64_t *file_size)
{
    if (!p || actual>p->body_limit || length < -1 || length>(int64_t)p->body_limit) return false;
    if (p->kind!=SPOTIFY_HTTP_AUDIO)
        return status==200 && (!complete || (actual>0 && (length<0 || actual==(size_t)length)));
    if (status!=206 || !bounded_ascii(range,96,true) || strncmp(range,"bytes ",6)) return false;
    const char *s=range+6;uint64_t first,last,total;
    if (!number(&s,&first) || *s++!='-' || !number(&s,&last) || *s++!='/' || !number(&s,&total) ||
        *s || !total || first>last || last>=total || last-first>=p->body_limit) return false;
    uint64_t expected_first=p->suffix ? (p->last<total ? total-p->last : 0) : p->first;
    uint64_t expected_last=p->suffix || p->last>=total ? total-1 : p->last;
    if (first!=expected_first || last!=expected_last || length!=(int64_t)(last-first+1) ||
        (complete && actual!=(size_t)length)) return false;
    if (file_size) *file_size=total;
    return true;
}
