#pragma once
#include <cstdint>
#include <memory>
#include <istream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>
#include "spotify_exchange.h"
namespace bell {
class HTTPClient {
public:
    using ValueHeader=std::pair<std::string,std::string>;
    using Headers=std::vector<ValueHeader>;
    struct RangeHeader {
        static ValueHeader range(int32_t from,int32_t to);
        static ValueHeader last(int32_t count);
    };
    class Response {
        struct Buffer:std::streambuf {
            void reset(uint8_t *p,size_t n) { char *s=reinterpret_cast<char*>(p);setg(s,s,s ? s+n : nullptr); }
        } buffer;
        np_spotify_http_reply_t reply{};
        std::istream input{&buffer};
        uint64_t total=0;
        void clear();
        void perform(const std::string&,bool,const Headers&,const std::vector<uint8_t>&);
    public:
        Response()=default;
        ~Response() { clear(); }
        Response(const Response&)=delete;
        Response& operator=(const Response&)=delete;
        void connect(const std::string&) {} // Network is owned by the exchange callback.
        void get(const std::string& url,Headers headers={}) { perform(url,false,headers,{}); }
        void post(const std::string& url,Headers headers={},const std::vector<uint8_t>& body={}) { perform(url,true,headers,body); }
        std::string_view body() { return {reinterpret_cast<const char*>(reply.body),reply.size}; }
        std::vector<uint8_t> bytes() { return reply.size ? std::vector<uint8_t>(reply.body,reply.body+reply.size) : std::vector<uint8_t>{}; }
        std::istream& stream() { return input; }
        size_t contentLength() { return reply.size; }
        size_t totalLength() { return static_cast<size_t>(total); }
    };
    static std::unique_ptr<Response> get(const std::string& url,Headers headers={}) {
        auto p=std::make_unique<Response>();p->get(url,std::move(headers));return p;
    }
    static std::unique_ptr<Response> post(const std::string& url,Headers headers={},const std::vector<uint8_t>& body={}) {
        auto p=std::make_unique<Response>();p->post(url,std::move(headers),body);return p;
    }
};
}
