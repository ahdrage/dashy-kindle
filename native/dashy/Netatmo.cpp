#include "Netatmo.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dashy {
namespace {
constexpr uint64_t INTERVAL = 180;
bool printable(const std::string& value, bool spaces = true) {
    if (value.empty() || value.size() > 4096) return false;
    for (unsigned char c : value) if (c < (spaces ? 32 : 33) || c > 126) return false;
    return true;
}
std::string encoded(const std::string& value) {
    const char* hex="0123456789ABCDEF";
    std::string result;
    for (unsigned char c : value) {
        if ((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~') result+=c;
        else { result+='%'; result+=hex[c>>4]; result+=hex[c&15]; }
    }
    return result;
}
bool number(JsonVariantConst value, double& out, double minimum, double maximum) {
    if (!value.is<double>() || value.is<bool>()) return false;
    out=value.as<double>();
    return std::isfinite(out) && out>=minimum && out<=maximum;
}
std::string temperature(JsonVariantConst value) {
    double n;
    if (!number(value,n,-100,100)) return "--.-°C";
    char text[32]; std::snprintf(text,sizeof(text),"%.1f°C",n); return text;
}
std::time_t measured(JsonVariantConst value) {
    if (!value.is<int64_t>()) return 0;
    int64_t n=value.as<int64_t>();
    return n>=1577836800 && n<=2147483647 ? static_cast<std::time_t>(n) : 0;
}
}

bool NetatmoConfig::valid() const {
    return printable(clientId) && printable(clientSecret) && printable(refreshToken,false)
        && (stationId.empty() || (stationId.size()<=128 && printable(stationId,false)));
}

void NetatmoClient::begin() {
    authorized_=store_.load(config_) && config_.valid();
    status_=authorized_ ? NetatmoStatus::Connecting : NetatmoStatus::Unconfigured;
}
void NetatmoClient::afterSleep(uint64_t tick) {
    if (status_!=NetatmoStatus::RateLimited) nextPoll_=tick;
}

void NetatmoClient::failed(const HttpResponse& response, uint64_t tick, bool tokenEndpoint) {
    if (response.code==429) {
        status_=NetatmoStatus::RateLimited;
        uint64_t wait=INTERVAL;
        if (!response.retryAfter.empty() && response.retryAfter.size()<9
            && response.retryAfter.find_first_not_of("0123456789")==std::string::npos)
            wait=std::strtoul(response.retryAfter.c_str(),nullptr,10);
        nextPoll_=tick+std::max<uint64_t>(INTERVAL,std::min<uint64_t>(3600,wait));
    } else if (response.code==401 || response.code==403 || (tokenEndpoint && response.code==400)) {
        status_=NetatmoStatus::Authorization;
        authorized_=false; // A rejected grant needs new credentials, not repeated refresh requests.
    } else status_=NetatmoStatus::Offline;
}

bool NetatmoClient::refresh(uint64_t tick) {
    std::string body="grant_type=refresh_token&client_id="+encoded(config_.clientId)
        +"&client_secret="+encoded(config_.clientSecret)+"&refresh_token="+encoded(config_.refreshToken);
    auto response=http_.request("POST","/oauth2/token","",body);
    if (response.code!=200) { failed(response,tick,true); return false; }
    JsonDocument doc;
    if (response.body.size()>32768 || deserializeJson(doc,response.body,DeserializationOption::NestingLimit(12))) {
        status_=NetatmoStatus::NoData; return false;
    }
    const std::string access=doc["access_token"].is<const char*>() ? doc["access_token"].as<std::string>() : "";
    std::string refresh=config_.refreshToken;
    if (!doc["refresh_token"].isNull()) refresh=doc["refresh_token"].is<const char*>() ? doc["refresh_token"].as<std::string>() : "";
    if (!printable(access,false) || !printable(refresh,false) || !doc["expires_in"].is<uint32_t>()
        || doc["expires_in"].as<uint32_t>()==0 || doc["expires_in"].as<uint32_t>()>86400) {
        status_=NetatmoStatus::NoData; return false;
    }
    // Persist rotation before any station request, including on insufficient scope.
    pendingSave_=refresh!=config_.refreshToken;
    config_.refreshToken=refresh;
    accessToken_=access;
    const uint64_t expiry=doc["expires_in"].as<uint32_t>();
    expires_=tick+(expiry>60 ? expiry-60 : 1);
    if (pendingSave_) {
        if (!store_.save(config_)) { status_=NetatmoStatus::Storage; return false; }
        pendingSave_=false;
    }
    if (doc["scope"].is<JsonArray>()) {
        bool allowed=false;
        for (JsonVariantConst scope : doc["scope"].as<JsonArrayConst>())
            if (scope.is<const char*>() && scope.as<std::string>()=="read_station") allowed=true;
        if (!allowed) { status_=NetatmoStatus::Authorization; authorized_=false; return false; }
    }
    return true;
}

bool NetatmoClient::parseStation(const std::string& body) {
    JsonDocument doc;
    if (body.size()>256*1024 || deserializeJson(doc,body,DeserializationOption::NestingLimit(16))) return false;
    const JsonArrayConst devices=doc["body"]["devices"].as<JsonArrayConst>();
    JsonObjectConst station;
    for (JsonObjectConst candidate : devices) {
        if (config_.stationId.empty() || candidate["_id"].as<std::string>()==config_.stationId) { station=candidate; break; }
    }
    if (station.isNull()) return false;
    Readings next;
    next.indoor=temperature(station["dashboard_data"]["Temperature"]);
    double co2;
    if (number(station["dashboard_data"]["CO2"],co2,0,100000) && std::floor(co2)==co2)
        next.co2=std::to_string(static_cast<int>(co2))+" ppm";
    const auto modules=station["modules"].as<JsonArrayConst>();
    JsonObjectConst outdoor;
    for (JsonObjectConst module : modules) {
        if (module["type"].as<std::string>()=="NAModule1") { outdoor=module; break; }
    }
    next.outdoor=temperature(outdoor["dashboard_data"]["Temperature"]);
    if (next.outdoor=="--.-°C" && !modules.isNull() && modules.size()>0) {
        outdoor=modules[0].as<JsonObjectConst>();
        next.outdoor=temperature(outdoor["dashboard_data"]["Temperature"]);
    }
    if (next.indoor=="--.-°C" && next.outdoor=="--.-°C" && next.co2=="---- ppm") return false;
    measuredAt_=measured(station["dashboard_data"]["time_utc"]);
    const auto outsideTime=measured(outdoor["dashboard_data"]["time_utc"]);
    if (outsideTime && next.outdoor!="--.-°C") measuredAt_=measuredAt_ ? std::min(measuredAt_,outsideTime) : outsideTime;
    unreachable_=(station["reachable"].is<bool>() && !station["reachable"].as<bool>())
        || (next.outdoor!="--.-°C" && outdoor["reachable"].is<bool>() && !outdoor["reachable"].as<bool>());
    readings_=next;
    return true;
}

bool NetatmoClient::poll(uint64_t tick, std::time_t now, bool connected) {
    if (!authorized_ || tick<nextPoll_) return false;
    nextPoll_=tick+INTERVAL;
    if (now<1577836800) { status_=NetatmoStatus::Clock; return true; }
    if (!connected) { status_=NetatmoStatus::Offline; return true; }
    if (pendingSave_) {
        if (!store_.save(config_)) { status_=NetatmoStatus::Storage; return true; }
        pendingSave_=false;
    }
    if ((accessToken_.empty() || tick>=expires_) && !refresh(tick)) return true;
    const auto path=std::string("/api/getstationsdata")+(config_.stationId.empty() ? "" : "?device_id="+encoded(config_.stationId));
    auto response=http_.request("GET",path,accessToken_,"");
    if (response.code==401 || response.code==403) {
        if (!refresh(tick)) return true;
        response=http_.request("GET",path,accessToken_,""); // Exactly one retry.
    }
    if (response.code!=200) { failed(response,tick); return true; }
    if (!parseStation(response.body)) { status_=NetatmoStatus::NoData; return true; }
    lastSuccess_=now;
    status_=NetatmoStatus::Live;
    return true;
}

Readings NetatmoClient::view(std::time_t now) const {
    auto result=readings_;
    const char* message="IKKE TILKOBLET";
    switch (status_) {
        case NetatmoStatus::Unconfigured: break;
        case NetatmoStatus::Connecting: message="HENTER MÅLINGER"; break;
        case NetatmoStatus::Live: message=""; break;
        case NetatmoStatus::Offline: message="NETTVERKSFEIL"; break;
        case NetatmoStatus::Authorization: message="KOBLE TIL PÅ NYTT"; break;
        case NetatmoStatus::Storage: message="KUNNE IKKE LAGRE"; break;
        case NetatmoStatus::NoData: message="MANGLER MÅLINGER"; break;
        case NetatmoStatus::RateLimited: message="VENTER PÅ NETATMO"; break;
        case NetatmoStatus::Clock: message="STILL KLOKKEN"; break;
    }
    const auto timestamp=measuredAt_ ? measuredAt_ : lastSuccess_;
    if (status_==NetatmoStatus::Live && (unreachable_ || (timestamp && (now<timestamp || now-timestamp>1800)))) message="ELDRE MÅLINGER";
    result.footer=std::string("NETATMO")+(*message ? std::string(" · ")+message : "");
    if (lastSuccess_) {
        char stamp[32]; std::tm local{};
        if (localtime_r(&lastSuccess_,&local) && std::strftime(stamp,sizeof(stamp),"%d.%m %H:%M",&local))
            result.footer+=" · HENTET "+std::string(stamp);
    }
    return result;
}
}
