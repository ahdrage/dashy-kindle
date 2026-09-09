#include "NetatmoStore.h"
#include <ArduinoJson.h>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace dashy {
bool FileNetatmoStore::load(NetatmoConfig& config) {
    const int fd=::open(path_.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) return false;
    struct stat st{};
    if (fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<1 || st.st_size>16384) { close(fd); return false; }
    std::string bytes(static_cast<size_t>(st.st_size),'\0');
    size_t got=0;
    while (got<bytes.size()) {
        const auto n=read(fd,&bytes[got],bytes.size()-got);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) break;
        got+=static_cast<size_t>(n);
    }
    close(fd);
    if (got!=bytes.size()) return false;
    JsonDocument doc;
    if (deserializeJson(doc,bytes,DeserializationOption::NestingLimit(4))) return false;
    for (const char* field : {"client_id","client_secret","refresh_token"}) if (!doc[field].is<const char*>()) return false;
    if (!doc["station_id"].isNull() && !doc["station_id"].is<const char*>()) return false;
    NetatmoConfig next{doc["client_id"].as<std::string>(),doc["client_secret"].as<std::string>(),
                       doc["refresh_token"].as<std::string>(),doc["station_id"] | ""};
    if (!next.valid()) return false;
    config=next;
    return true;
}

bool FileNetatmoStore::save(const NetatmoConfig& config) {
    if (!config.valid()) return false;
    struct stat st{};
    if (lstat(path_.c_str(),&st)==0) { if (!S_ISREG(st.st_mode)) return false; }
    else if (errno!=ENOENT) return false;
    JsonDocument doc;
    doc["client_id"]=config.clientId; doc["client_secret"]=config.clientSecret;
    doc["refresh_token"]=config.refreshToken; doc["station_id"]=config.stationId;
    std::string bytes; serializeJson(doc,bytes);
    const auto pattern=path_+".tmp.XXXXXX";
    std::vector<char> temporary(pattern.begin(),pattern.end()); temporary.push_back('\0');
    const int fd=mkstemp(temporary.data());
    if (fd<0) return false;
    bool ok=fchmod(fd,0600)==0;
    size_t sent=0;
    while (ok && sent<bytes.size()) {
        const auto n=write(fd,bytes.data()+sent,bytes.size()-sent);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) { ok=false; break; }
        sent+=static_cast<size_t>(n);
    }
    if (ok) ok=fsync(fd)==0;
    if (close(fd)!=0) ok=false;
    if (ok) ok=rename(temporary.data(),path_.c_str())==0;
    if (!ok) { unlink(temporary.data()); return false; }
    const auto slash=path_.find_last_of('/');
    const auto parent=slash==std::string::npos ? "." : path_.substr(0,slash);
    const int directory=open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (directory<0) return false;
    const int synced=fsync(directory); const int error=errno; close(directory);
    return synced==0 || error==EINVAL; // Some host filesystems cannot fsync directories.
}
}
