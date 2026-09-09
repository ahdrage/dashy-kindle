#pragma once
#include "Netatmo.h"
namespace dashy {
class FileNetatmoStore : public NetatmoStore {
public:
    explicit FileNetatmoStore(std::string path) : path_(std::move(path)) {}
    bool load(NetatmoConfig& config) override;
    bool save(const NetatmoConfig& config) override;
private:
    std::string path_;
};
}
