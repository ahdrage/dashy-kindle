#include "NightMode.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace dashy {
uint32_t nightSleepSeconds(std::time_t now) {
    if (now<1577836800) return 0;
    std::tm local{};
    if (!localtime_r(&now,&local) || (local.tm_hour>=7 && local.tm_hour<23)) return 0;
    if (local.tm_hour>=23) ++local.tm_mday;
    local.tm_hour=7; local.tm_min=0; local.tm_sec=0;
    local.tm_isdst=-1; // Recompute DST at 07:00, including a clock change overnight.
    const auto wake=std::mktime(&local);
    const auto seconds=std::difftime(wake,now);
    return seconds>0 && seconds<=10*3600 ? static_cast<uint32_t>(seconds) : 0;
}
NightMode::NightMode(NightActions& actions, std::string path) : actions_(actions), path_(std::move(path)) {
    int fd=open(path_.c_str(),O_RDONLY|O_NOFOLLOW);
    if (fd<0) { if (errno==ENOENT) state_=State::Pending; return; }
    struct stat info{}; char text[32]{};
    if (fstat(fd,&info)==0 && S_ISREG(info.st_mode) && info.st_size<31) {
        const auto n=read(fd,text,sizeof(text)-1);
        if (n==8 && std::string(text)=="enabled\n") state_=State::Enabled;
    }
    close(fd); // Interrupted tests and unknown states stay disabled.
}
bool NightMode::save(const char* value) {
    struct stat info{};
    if (lstat(path_.c_str(),&info)==0) { if (!S_ISREG(info.st_mode)) return false; }
    else if (errno!=ENOENT) return false;
    const auto temp=path_+".tmp";
    int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if (fd<0) return false;
    const std::string text=std::string(value)+"\n";
    bool ok=write(fd,text.data(),text.size())==static_cast<ssize_t>(text.size()) && fsync(fd)==0;
    if (close(fd)!=0) ok=false;
    if (ok) ok=rename(temp.c_str(),path_.c_str())==0;
    if (!ok) { unlink(temp.c_str()); return false; }
    auto parent=path_.substr(0,path_.find_last_of('/'));
    int dir=open(parent.c_str(),O_RDONLY|O_DIRECTORY);
    ok=dir>=0 && fsync(dir)==0;
    if (dir>=0) close(dir);
    return ok;
}
void NightMode::fail() {
    actions_.restore(suspended_);
    suspended_=0; dark_=false; pausing_=false; state_=State::Failed;
    save("failed");
    std::fprintf(stderr,"Dashy night: disabled; sleep/wake verification failed\n");
}
bool NightMode::update(std::time_t now, uint64_t uptime) {
    if (!started_) { started_=true; testAt_=uptime+10; }
    if (state_==State::Failed) return false;
    if (state_==State::Pending) {
        if (uptime<testAt_ || now<1577836800 || !actions_.pauseNetwork()) return false;
        if (!save("testing") || !actions_.prepareSleep()) { fail(); return false; }
        std::fprintf(stderr,"Dashy night: starting 60-second RTC sleep test\n"); std::fflush(stderr);
        const auto result=actions_.sleep(60);
        actions_.restore(result.suspended);
        // A USB-inhibited suspend, simulated wait, or early button wake is not a pass.
        bool passed=result.resumed && result.suspended>=45 && result.elapsed>=55 && result.elapsed<=120;
        state_=passed && save("enabled") ? State::Enabled : State::Failed;
        if (state_==State::Failed) save("failed");
        std::fprintf(stderr,"Dashy night: test %s; elapsed=%u suspended=%u seconds\n",
                     enabled() ? "PASSED (23:00-07:00 Oslo enabled)" : "FAILED",result.elapsed,result.suspended);
        return false;
    }
    auto seconds=nightSleepSeconds(now);
    if (!seconds) {
        if (pausing_) { actions_.restore(suspended_); suspended_=0; dark_=false; pausing_=false; }
        return false;
    }
    if (uptime<retryAt_) return dark_;
    if (!dark_) {
        pausing_=true;
        if (!actions_.pauseNetwork()) return false;
        if (!actions_.prepareSleep()) { fail(); return false; }
        dark_=true;
    }
    std::fprintf(stderr,"Dashy night: sleeping until 07:00 (%u seconds)\n",seconds); std::fflush(stderr);
    const auto result=actions_.sleep(seconds);
    suspended_+=result.suspended;
    if (!result.resumed || (seconds>5 && result.suspended<1)) { fail(); return false; }
    // Leave a short window for the existing corner-exit gesture on an early wake.
    retryAt_=uptime+(result.elapsed>result.suspended ? result.elapsed-result.suspended : 0)+5;
    return true;
}
const char* NightMode::notice() const {
    if (state_==State::Pending) return "NATTTEST · SKJERMEN BLIR BLANK I ETT MINUTT";
    if (state_==State::Failed) return "NATTMODUS IKKE AKTIV";
    return "NATT 23–07";
}
bool NightMode::enabled() const { return state_==State::Enabled; }
}
