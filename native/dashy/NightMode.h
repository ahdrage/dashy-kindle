#pragma once
#include <cstdint>
#include <ctime>
#include <string>

namespace dashy {
uint32_t nightSleepSeconds(std::time_t now);
struct SleepResult {
    bool resumed = false;
    uint32_t elapsed = 0, suspended = 0;
};
class NightActions {
public:
    virtual ~NightActions() = default;
    virtual bool pauseNetwork() = 0;
    // Save the light level, switch it off, and clear the screen.
    virtual bool prepareSleep() = 0;
    virtual SleepResult sleep(uint32_t seconds) = 0;
    // Restore the light, invalidate the screen, and resume network requests.
    virtual void restore(uint32_t suspendedSeconds) = 0;
};
class NightMode {
public:
    NightMode(NightActions& actions, std::string statePath);
    // True while the screen belongs to night mode. uptime excludes suspend.
    bool update(std::time_t now, uint64_t uptime);
    const char* notice() const;
    bool enabled() const;
private:
    NightActions& actions_;
    std::string path_;
    enum class State { Pending, Enabled, Failed } state_ = State::Failed;
    bool started_ = false, dark_ = false, pausing_ = false;
    uint64_t testAt_ = 0, retryAt_ = 0;
    uint32_t suspended_ = 0;
    bool save(const char* value);
    void fail();
};
}
