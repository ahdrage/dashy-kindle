#pragma once
#include "Netatmo.h"
namespace dashy {
class NetatmoHttps : public NetatmoTransport {
public:
    explicit NetatmoHttps(std::string certificate) : certificate_(std::move(certificate)) {}
    HttpResponse request(const std::string& method, const std::string& path,
                         const std::string& bearer, const std::string& body) override;
private:
    std::string certificate_;
};
}
