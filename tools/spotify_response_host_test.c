#include "spotify_response_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static spotify_response_workspace_t workspace;
static bool decode(spotify_http_kind_t kind,const char *s)
{ return spotify_response_decode(&workspace,kind,s,strlen(s)); }
static void rejected(spotify_http_kind_t kind,const char *s)
{
    memset(&workspace,0xa5,sizeof(workspace));
    assert(!decode(kind,s));assert(workspace.expires_s==0);
    for(unsigned i=0;i<sizeof(workspace.text);++i)assert(!workspace.text[i]);
}
int main(void)
{
    assert(decode(SPOTIFY_HTTP_RESOLVE,"{\"ap_list\":[\"ap-test.spotify.com:4070\"]}"));
    assert(!strcmp(workspace.text,"ap-test.spotify.com:4070"));
    const char *bad_ap[]={"{}","null","{\"ap_list\":[]}","{\"ap_list\":[null]}",
        "{\"ap_list\":[\"127.0.0.1:4070\"]}","{\"ap_list\":[\"a.spotify.com.evil:4070\"]}",
        "{\"ap_list\":[\"a.spotify.com:22\"]}","{\"ap_list\":[\"a.spotify.com:4070\"],\"ap_list\":[]}",
        "{\"ap_list\":[\"a.spotify.com:4070\"],\"error\":null}"};
    for(unsigned i=0;i<sizeof(bad_ap)/sizeof(*bad_ap);++i)rejected(SPOTIFY_HTTP_RESOLVE,bad_ap[i]);
    assert(decode(SPOTIFY_HTTP_TOKEN,"{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":3600}"));
    assert(!strcmp(workspace.text,"private") && workspace.expires_s==3600);
    const char *bad_token[]={"{}","{\"access_token\":null,\"expires_in\":3600}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":-1}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":0}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":86401}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":1.2}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":1e3}",
        "{\"access_token\":\"private\\r\\nX\",\"token_type\":\"Bearer\",\"expires_in\":3600}",
        "{\"access_token\":\"private\",\"token_type\":\"other\",\"expires_in\":3600}",
        "{\"access_token\":\"private\",\"token_type\":\"Bearer\",\"expires_in\":3600,\"error\":{}}"};
    for(unsigned i=0;i<sizeof(bad_token)/sizeof(*bad_token);++i)rejected(SPOTIFY_HTTP_TOKEN,bad_token[i]);
    assert(decode(SPOTIFY_HTTP_STORAGE,"{\"cdnurl\":[\"https://audio.scdn.co/a?sig=private\\u0026x=y\"]}"));
    assert(!strcmp(workspace.text,"https://audio.scdn.co/a?sig=private&x=y"));
    const char *bad_cdn[]={"{}","{\"cdnurl\":[]}","{\"cdnurl\":[null]}",
        "{\"cdnurl\":[\"http://audio.scdn.co/a\"]}","{\"cdnurl\":[\"https://evil.invalid/a\"]}",
        "{\"cdnurl\":[\"https://audio.scdn.co/a\\u0000evil\"]}"};
    for(unsigned i=0;i<sizeof(bad_cdn)/sizeof(*bad_cdn);++i)rejected(SPOTIFY_HTTP_STORAGE,bad_cdn[i]);
    char oversized[8194];memset(oversized,' ',sizeof(oversized));
    assert(!spotify_response_decode(&workspace,SPOTIFY_HTTP_TOKEN,oversized,sizeof(oversized)));
    rejected(SPOTIFY_HTTP_TOKEN,"{\"a\":[[[[[[[[[[0]]]]]]]]]]}");
    puts("Spotify response codec: malformed/duplicate/missing fields, bounds/depth, token TTL, AP/CDN allowlists and failure scrubbing PASS");
}
