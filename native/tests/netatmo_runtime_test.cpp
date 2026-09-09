#include "NetatmoRuntime.h"
#include "NetatmoHttp.h"
#include "NetatmoStore.h"
#include <WiFi.h>
#include <Lifecycle.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unistd.h>

static std::atomic<bool> entered{false}, releaseRequest{false}, exiting{false};
WiFiClass WiFi;
uint8_t WiFiClass::status() { return WL_CONNECTED; }
bool WiFiClass::lipcGet(const char*,const char*,std::string&) { return false; }
bool WiFiClass::ifaceIPv4(const char*,uint32_t&) { return false; }
bool WiFiClass::ifaceMac(const char*,uint8_t[6]) { return false; }
bool WiFiClass::wpaStatus(std::string&) { return false; }
bool WiFiClass::ioctlEssid(const char*,std::string&) { return false; }
int kinduinoExitRequested() { return exiting ? 1 : 0; }
dashy::HttpResponse dashy::NetatmoHttps::request(const std::string& method,const std::string&,
                                               const std::string&,const std::string&) {
    entered=true;
    while (!releaseRequest) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (method=="POST") return {200,R"({"access_token":"access-test","refresh_token":"rotated-test","expires_in":10800})",""};
    return {200,R"({"body":{"devices":[{"dashboard_data":{"Temperature":22.5,"CO2":701},"modules":[]}]}})",""};
}
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr,"Runtime check failed at line %d\n",__LINE__); std::exit(1); } } while(0)
int main(int argc,char**argv) {
    if (argc!=2 || chdir(argv[1])!=0) return 2;
    CHECK(dashy::startNetatmo());
    for (int i=0;i<500 && !entered;++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(entered);
    auto begin=std::chrono::steady_clock::now();
    auto initial=dashy::netatmoReadings();
    CHECK(std::chrono::steady_clock::now()-begin<std::chrono::milliseconds(100));
    CHECK(initial.footer.find("HENTER")!=std::string::npos);
    releaseRequest=true;
    dashy::Readings live;
    for (int i=0;i<1000;++i) {
        live=dashy::netatmoReadings();
        if (live.co2=="701 ppm") break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(live.indoor=="22.5°C" && live.co2=="701 ppm");
    dashy::FileNetatmoStore store("files/netatmo.json"); dashy::NetatmoConfig config;
    CHECK(store.load(config) && config.refreshToken=="rotated-test");
    exiting=true;
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    std::puts("Netatmo worker: display reads stay responsive during a blocked request; token rotation persists.");
}
