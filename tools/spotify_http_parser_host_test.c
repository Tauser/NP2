#include "http_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void feed(http_parser *p, const char *text, size_t n)
{
    http_parser_settings callbacks = {0};
    /* Fragment a single large header as a network stream would. */
    for (size_t pos=0;pos<n;) {
        size_t part=n-pos;if(part>31)part=31;
        size_t read=http_parser_execute(p,&callbacks,text+pos,part);
        pos+=read;
        if(read!=part)break;
    }
}
int main(void)
{
    http_parser parser;
    http_parser_init(&parser,HTTP_RESPONSE);
    const char ordinary[]="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: application/json\r\n\r\n{}";
    feed(&parser,ordinary,sizeof(ordinary)-1);
    assert(HTTP_PARSER_ERRNO(&parser)==HPE_OK);
    char excessive[10000];
    const char prefix[]="HTTP/1.1 200 OK\r\nX-Long: ";
    memcpy(excessive,prefix,sizeof(prefix)-1);
    memset(excessive+sizeof(prefix)-1,'a',sizeof(excessive)-sizeof(prefix)+1);
    http_parser_init(&parser,HTTP_RESPONSE);
    feed(&parser,excessive,sizeof(excessive));
    assert(HTTP_PARSER_ERRNO(&parser)==HPE_HEADER_OVERFLOW);
    assert(parser.nread>8192 && parser.nread<8300);
    puts("IDF HTTP parser: ordinary response accepted; fragmented header rejected at 8 KiB PASS");
}
