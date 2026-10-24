#include "spotify_pairing_policy.h"
#include "google_agenda_codec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static google_json_t workspace;
static bool parse(const char *source) {
    char text[4098]; assert(strlen(source)<sizeof(text));strcpy(text,source);
    spotify_pairing_form_t result;
    const bool valid=spotify_pairing_form(text,strlen(text),&result);
    if(valid) assert(result.username && result.blob && result.client_key);
    return valid;
}
int main(void) {
    char text[4098];
    snprintf(text,sizeof(text),"action=addUser&userName=test%%40mail&blob=%052d&clientKey=%0128d&deviceName=Phone",0,0);
    assert(parse(text));
    assert(!parse("action=addUser&userName=bad%00name&blob=AAAA&clientKey=AAAA"));
    assert(!parse("action=addUser&userName=x&userName=y"));
    assert(!parse("action=getInfo"));assert(!parse("action=addUser&userName=%"));
    assert(!parse("action=addUser&userName=%0"));assert(!parse("action=addUser&userName=%0Z"));
    assert(!parse("action=addUser&userName=x&blob=AAAA&clientKey=AAAA"));
    const size_t n=strlen(text);spotify_pairing_form_t form;
    for(size_t i=0;i<n;++i) {
        char mutated[4098];memcpy(mutated,text,n+1);mutated[i]=0;
        assert(!spotify_pairing_form(mutated,n,&form));
    }
    spotify_app_credentials_t output;
    const char *good="{\"client_id\":\"0123456789abcdef0123456789abcdef\",\"client_secret\":\"fedcba9876543210fedcba9876543210\"}";
    assert(spotify_app_credentials(good,strlen(good),&output,&workspace));
    const char *bad="{\"client_id\":\"0123456789abcdef0123456789abcdef\",\"client_id\":\"0123456789abcdef0123456789abcdef\"}";
    assert(!spotify_app_credentials(bad,strlen(bad),&output,&workspace));
    assert(!output.client_id[0] && !output.client_secret[0]);
    for(size_t i=0;i<strlen(good);++i) assert(!spotify_app_credentials(good,i,&output,&workspace));
    puts("Spotify private pairing policy: bounded JSON/form, duplicate/missing fields, percent escapes, embedded NUL, truncation PASS");
}
