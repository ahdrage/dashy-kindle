#include <Arduino.h>
#include <DefaultBackend.h>
#include <Assets.h>
#include <Lifecycle.h>
#include <cstdlib>
#include <cstdio>
#include <Kindle.h>
#include <LinuxPlatformBackend.h>
#include <cmath>
#include "Dashboard.h"
#include "LandscapeBackend.h"
#include "NetatmoRuntime.h"
#include "NightMode.h"

static DefaultBackend backend;
static dashy::LandscapeBackend landscape(backend);
static Display display;
static KindleFont regular, medium;
static dashy::Dashboard dashboard(display, regular, medium, {"--.-°C", "---- ppm", "--.-°C", "NETATMO · HENTER MÅLINGER"});

// The SDK exposes light writes; its protected raw property helpers also let us
// preserve the exact existing level instead of guessing a daytime brightness.
class NightLight : public LinuxPlatformBackend {
    int saved_=-1;
public:
    bool off() {
        int level=-1;
        if (!lipcGetInt("com.lab126.powerd","flIntensity",level) || level<0 || level>255) return false;
        saved_=level;
        return lipcSetInt("com.lab126.powerd","flIntensity",0);
    }
    void restore() {
        if (saved_>=0) {
            if (!lipcSetInt("com.lab126.powerd","flIntensity",saved_))
                std::fprintf(stderr,"Dashy night: could not restore front light\n");
            saved_=-1;
        }
    }
};
class DeviceNight : public dashy::NightActions {
    NightLight light_;
public:
    bool pauseNetwork() override { return dashy::pauseNetatmo(); }
    bool prepareSleep() override {
        if (!Kindle.canDeepSleep() || !light_.off()) return false;
        dashboard.blank();
        return backend.lastOk();
    }
    dashy::SleepResult sleep(uint32_t seconds) override {
        timespec wallBefore{},monoBefore{},wallAfter{},monoAfter{};
        clock_gettime(CLOCK_REALTIME,&wallBefore); clock_gettime(CLOCK_MONOTONIC,&monoBefore);
        const bool resumed=Kindle.deepSleep(seconds);
        clock_gettime(CLOCK_MONOTONIC,&monoAfter); clock_gettime(CLOCK_REALTIME,&wallAfter);
        auto difference=[](timespec a,timespec b) {
            return static_cast<double>(a.tv_sec-b.tv_sec)+(a.tv_nsec-b.tv_nsec)/1e9;
        };
        const double elapsed=difference(wallAfter,wallBefore);
        const double suspended=elapsed-difference(monoAfter,monoBefore);
        auto rounded=[](double value) -> uint32_t {
            return value>0 && value<86400 ? static_cast<uint32_t>(std::lround(value)) : 0;
        };
        return {resumed,rounded(elapsed),rounded(suspended)};
    }
    void restore(uint32_t suspendedSeconds) override {
        light_.restore();
        dashy::resumeNetatmo(suspendedSeconds);
        // Dashboard::blank already invalidated the frame before suspend.
    }
};
static DeviceNight nightDevice;
static dashy::NightMode* night=nullptr;

static bool loadFont(KindleFont& font, const char* path) {
    File file = Assets.open(path);
    if (!file) return false;
    size_t size = file.size();
    if (!size || size > 1024 * 1024) return false;
    uint8_t* bytes = static_cast<uint8_t*>(std::malloc(size));
    if (!bytes) return false;
    size_t got = 0;
    while (got < size) {
        int n = file.read(bytes + got, size - got);
        if (n <= 0) break;
        got += n;
    }
    file.close();
    bool ok = got == size && font.loadFromMemory(bytes, size);
    std::free(bytes);
    return ok;
}

void setup() {
    dashy::useOsloTime();
    // Check the real panel before allocating or drawing; a missing display must fail promptly.
    if (!((backend.panelWidth() == 758 && backend.panelHeight() == 1024)
          || (backend.panelWidth() == 1024 && backend.panelHeight() == 758))) {
        std::fprintf(stderr, "Dashy: expected a Paperwhite 1 display\n");
        std::exit(1);
    }
    display.begin(&landscape);
    if (!loadFont(regular, "DashySans-Regular.ttf") || !loadFont(medium, "DashySans-Medium.ttf")) {
        std::fprintf(stderr, "Dashy: bundled fonts missing; reinstall the complete package\n");
        std::exit(1); // The supervisor restores the reader interface.
    }
    if (!dashy::startNetatmo()) {
        std::fprintf(stderr,"Dashy: Netatmo worker could not start\n");
        std::exit(1);
    }
    // The runtime preserves files/ across app updates, including the one-time test result.
    night=new dashy::NightMode(nightDevice,"files/night-mode.state");
}

void loop() {
    if (kinduinoExitRequested()) return;
    timespec mono{}; clock_gettime(CLOCK_MONOTONIC,&mono);
    if (night->update(std::time(nullptr),static_cast<uint64_t>(mono.tv_sec))) { delay(200); return; }
    if (kinduinoExitRequested()) return;
    auto readings=dashy::netatmoReadings();
    if (*night->notice()) readings.footer+=std::string(" · ")+night->notice();
    dashboard.setReadings(std::move(readings));
    if (dashboard.update(std::time(nullptr)) && !backend.lastOk()) {
        std::fprintf(stderr, "Dashy: display connection failed\n");
        std::exit(1);
    }
    delay(200); // Responsive exit; display refresh still happens only once a minute.
}
