#include <Arduino.h>
#include <DefaultBackend.h>
#include <Assets.h>
#include <Lifecycle.h>
#include <cstdlib>
#include <cstdio>
#include "Dashboard.h"
#include "LandscapeBackend.h"
#include "NetatmoRuntime.h"

static DefaultBackend backend;
static dashy::LandscapeBackend landscape(backend);
static Display display;
static KindleFont regular, medium;
static dashy::Dashboard dashboard(display, regular, medium, {"--.-°C", "---- ppm", "--.-°C", "NETATMO · HENTER MÅLINGER"});

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
}

void loop() {
    if (kinduinoExitRequested()) return;
    dashboard.setReadings(dashy::netatmoReadings());
    if (dashboard.update(std::time(nullptr)) && !backend.lastOk()) {
        std::fprintf(stderr, "Dashy: display connection failed\n");
        std::exit(1);
    }
    delay(200); // Responsive exit; display refresh still happens only once a minute.
}
