#include "spotify_response_codec.h"
#include <string.h>

static bool first_string(spotify_response_workspace_t *w, const char *name)
{
    const int array = google_json_field(&w->json, 0, name);
    return array >= 0 && w->json.tokens[array].kind == 2 &&
        array + 1 < w->json.tokens[array].next &&
        google_json_string(&w->json, array + 1, w->text, sizeof(w->text));
}
static bool access_point(const char *s)
{
    const char *colon = strchr(s, ':');
    if (!colon || strlen(s) > 253U) return false;
    const char suffix[] = ".spotify.com";
    if ((size_t)(colon-s) <= sizeof(suffix)-1 ||
        memcmp(colon-sizeof(suffix)+1,suffix,sizeof(suffix)-1)) return false;
    for (const char *p=s;p<colon;++p)
        if (!((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='-' || *p=='.')) return false;
    return !strcmp(colon,":4070") || !strcmp(colon,":443") || !strcmp(colon,":80");
}
bool spotify_response_decode(spotify_response_workspace_t *w, spotify_http_kind_t kind,
    const char *body, size_t size)
{
    if (!w) return false;
    w->text[0]=0;w->expires_s=0;
    bool valid=false;
    if (!body || size>SPOTIFY_HTTP_JSON_MAX || !google_json_parse(&w->json,body,size) ||
        google_json_field(&w->json,0,"error")>=0) goto done;
    if (kind==SPOTIFY_HTTP_RESOLVE) {
        valid=first_string(w,"ap_list") && access_point(w->text);
    } else if (kind==SPOTIFY_HTTP_STORAGE) {
        spotify_http_request_t r={.range="bytes=0-8191"};spotify_http_policy_t policy;
        r.url=w->text;
        valid=first_string(w,"cdnurl") && spotify_http_validate_request(&r,&policy) && policy.kind==SPOTIFY_HTTP_AUDIO;
    } else if (kind==SPOTIFY_HTTP_TOKEN) {
        const int token=google_json_field(&w->json,0,"access_token");
        const int expires=google_json_field(&w->json,0,"expires_in");
        const int type=google_json_field(&w->json,0,"token_type");
        char type_text[16];
        if (!google_json_string(&w->json,token,w->text,sizeof(w->text)) || !w->text[0] ||
            !google_json_string(&w->json,type,type_text,sizeof(type_text)) || strcmp(type_text,"Bearer") ||
            expires<0 || w->json.tokens[expires].kind!=4) goto done;
        for (const unsigned char *p=(const unsigned char *)w->text;*p;++p)
            if (*p<=32 || *p>=127) goto done;
        const google_json_token_t *n=&w->json.tokens[expires];
        if (n->end-n->begin>5) goto done;
        for (unsigned i=n->begin;i<n->end;++i) {
            if (body[i]<'0' || body[i]>'9') goto done;
            w->expires_s=w->expires_s*10U+(unsigned)(body[i]-'0');
        }
        valid=w->expires_s>0 && w->expires_s<=86400U;
    }
done:
    if (!valid) {
        volatile char *p=w->text;for(size_t i=0;i<sizeof(w->text);++i)p[i]=0;
        w->expires_s=0;
    }
    return valid;
}
