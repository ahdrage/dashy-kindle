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
    bool pauseRequested=false, busy=false, safe=true, resumed=false;
    uint64_t suspendedSeconds=0;
    explicit Runtime(std::string certificate) : http(std::move(certificate)), client(http,store) {
        client.begin(); readings=client.view(std::time(nullptr));
    }
};
// The detached worker owns process-lifetime state. Exiting the app must not wait
// for a slow DNS/TLS request; the OS reclaims this state with the process.
Runtime* runtime=nullptr;
void* run(void* value) {
    auto& state=*static_cast<Runtime*>(value);
    uint64_t reconnectUntil=0;
    while (!kinduinoExitRequested()) {
        timespec mono{}; clock_gettime(CLOCK_MONOTONIC,&mono);
        const auto now=std::time(nullptr);
        pthread_mutex_lock(&state.mutex);
        const auto tick=static_cast<uint64_t>(mono.tv_sec)+state.suspendedSeconds;
        const bool paused=state.pauseRequested && state.safe;
        const bool resumed=!paused && state.resumed;
        if (!paused) { state.busy=true; state.resumed=false; }
        pthread_mutex_unlock(&state.mutex);
        if (paused) { delay(1000); continue; }
        if (resumed) { state.client.afterSleep(tick); reconnectUntil=tick+30; }
        if (state.client.due(tick)) {
            const bool connected=WiFi.status()==WL_CONNECTED;
            // Let the OS reassociate after suspend before starting the 3-minute retry.
            if (connected || tick>=reconnectUntil) {
                state.client.poll(tick,now,connected);
                reconnectUntil=0;
            }
        }
        auto view=state.client.view(std::time(nullptr));
        pthread_mutex_lock(&state.mutex);
        state.readings=std::move(view);
        state.safe=state.client.safeToSleep();
        state.busy=false;
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
bool pauseNetatmo() {
    if (!runtime) return true;
    pthread_mutex_lock(&runtime->mutex);
    runtime->pauseRequested=true;
    const bool ready=!runtime->busy && runtime->safe;
    pthread_mutex_unlock(&runtime->mutex);
    return ready;
}
void resumeNetatmo(uint32_t suspendedSeconds) {
    if (!runtime) return;
    pthread_mutex_lock(&runtime->mutex);
    // CLOCK_MONOTONIC stops in suspend on this kernel; OAuth expiry must keep advancing.
    runtime->suspendedSeconds+=suspendedSeconds;
    runtime->resumed=true;
    runtime->pauseRequested=false;
    pthread_mutex_unlock(&runtime->mutex);
}
}
