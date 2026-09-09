#pragma once
#include "Readings.h"
#include <cstdint>
#include <ctime>

namespace dashy {
struct NetatmoConfig {
    std::string clientId, clientSecret, refreshToken, stationId;
    bool valid() const;
};
struct HttpResponse { int code; std::string body, retryAfter; };
class NetatmoTransport {
public:
    virtual ~NetatmoTransport() = default;
    virtual HttpResponse request(const std::string& method, const std::string& path,
                                 const std::string& bearer, const std::string& body) = 0;
};
class NetatmoStore {
public:
    virtual ~NetatmoStore() = default;
    virtual bool load(NetatmoConfig& config) = 0;
    virtual bool save(const NetatmoConfig& config) = 0;
};
enum class NetatmoStatus { Unconfigured, Connecting, Live, Offline, Authorization, Storage, NoData, RateLimited, Clock };
class NetatmoClient {
public:
    NetatmoClient(NetatmoTransport& http, NetatmoStore& store) : http_(http), store_(store) {}
    void begin();
    bool poll(uint64_t monotonicSeconds, std::time_t now, bool connected);
    Readings view(std::time_t now) const;
    bool due(uint64_t tick) const { return authorized_ && tick>=nextPoll_; }
    NetatmoStatus status() const { return status_; }
private:
    NetatmoTransport& http_;
    NetatmoStore& store_;
    NetatmoConfig config_;
    Readings readings_;
    NetatmoStatus status_ = NetatmoStatus::Unconfigured;
    std::string accessToken_;
    uint64_t nextPoll_ = 0, expires_ = 0;
    std::time_t lastSuccess_ = 0, measuredAt_ = 0;
    bool authorized_ = false, pendingSave_ = false, unreachable_ = false;
    bool refresh(uint64_t tick);
    bool parseStation(const std::string& body);
    void failed(const HttpResponse& response, uint64_t tick, bool tokenEndpoint = false);
};
}
