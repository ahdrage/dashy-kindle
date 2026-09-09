#include "NetatmoRuntime.h"
#include "Netatmo.h"
#include "NetatmoStore.h"
#include "NetatmoHttp.h"
#include <Arduino.h>
#include <Lifecycle.h>
#include <WiFi.h>
#include <fstream>
#include <iterator>
#include <pthread.h>

namespace dashy {
namespace {
struct Runtime {
    FileNetatmoStore store{"files/netatmo.json"};
    NetatmoHttps http;
    NetatmoClient client;
    Readings readings;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    explicit Runtime(std::string certificate) : http(std::move(certificate)), client(http,store) {
        client.begin(); readings=client.view(std::time(nullptr));
    }
};
// The detached worker owns process-lifetime state. Exiting the app must not wait
// for a slow DNS/TLS request; the OS reclaims this state with the process.
Runtime* runtime=nullptr;
void* run(void* value) {
    auto& state=*static_cast<Runtime*>(value);
    while (!kinduinoExitRequested()) {
        timespec mono{}; clock_gettime(CLOCK_MONOTONIC,&mono);
        const auto now=std::time(nullptr);
        const auto tick=static_cast<uint64_t>(mono.tv_sec);
        if (state.client.due(tick)) state.client.poll(tick,now,WiFi.status()==WL_CONNECTED);
        auto view=state.client.view(std::time(nullptr));
        pthread_mutex_lock(&state.mutex);
        state.readings=std::move(view);
        pthread_mutex_unlock(&state.mutex);
        delay(1000);
    }
    return nullptr;
}
}
bool startNetatmo() {
    std::ifstream cert("assets/DigiCertGlobalRootG2.crt");
    std::string pem((std::istreambuf_iterator<char>(cert)),{});
    if (pem.empty() || pem.size()>16384) return false;
    runtime=new Runtime(std::move(pem));
    millis(); // Initialize the SDK monotonic epoch before the second thread starts.
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes)!=0) return false;
    bool ok=pthread_attr_setstacksize(&attributes,1024*1024)==0
        && pthread_attr_setdetachstate(&attributes,PTHREAD_CREATE_DETACHED)==0;
    pthread_t worker;
    if (ok) ok=pthread_create(&worker,&attributes,run,runtime)==0;
    pthread_attr_destroy(&attributes);
    return ok;
}
Readings netatmoReadings() {
    if (!runtime) return {"--.-°C","---- ppm","--.-°C","NETATMO · KUNNE IKKE STARTE"};
    pthread_mutex_lock(&runtime->mutex);
    auto result=runtime->readings;
    pthread_mutex_unlock(&runtime->mutex);
    return result;
}
}
