#include "NetatmoHttp.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace dashy {
HttpResponse NetatmoHttps::request(const std::string& method, const std::string& path,
                                 const std::string& bearer, const std::string& body) {
    // Credentials only go to this fixed HTTPS host; redirects are never followed.
    if ((method!="GET" && method!="POST") || path.empty() || path[0]!='/'
        || path.find_first_of("\r\n")!=std::string::npos || bearer.find_first_of("\r\n")!=std::string::npos)
        return {-1,"",""};
    WiFiClientSecure tls;
    if (!tls.setCACert(certificate_.c_str())) return {-1,"",""};
    tls.setTimeout(8000);
    tls.setHandshakeTimeout(8000);
    HTTPClient http;
    http.setTimeout(8000);
    const auto url=std::string("https://api.netatmo.com")+path;
    if (!http.begin(tls,url.c_str())) return {-1,"",""};
    http.addHeader("Accept","application/json");
    if (!bearer.empty()) http.addHeader("Authorization",("Bearer "+bearer).c_str());
    if (method=="POST") http.addHeader("Content-Type","application/x-www-form-urlencoded");
    const int code=method=="POST" ? http.POST(reinterpret_cast<const uint8_t*>(body.data()),body.size()) : http.GET();
    HttpResponse result{code,"",http.header("Retry-After").c_str()};
    if (code==200) {
        if (http.getSize()>256*1024) result.code=-1;
        else {
            result.body=http.body();
            if (result.body.size()>256*1024) { result.code=-1; result.body.clear(); }
        }
    }
    http.end();
    return result;
}
}
