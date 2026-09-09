#pragma once

#include <ctime>
#include <string>
#include <Display.h>
#include <KindleFont.h>
#include "Readings.h"

namespace dashy {
struct Frame {
    std::string clock;
    std::string date;
};
void useOsloTime();
Frame frameAt(std::time_t now);

class Dashboard {
public:
    Dashboard(Display& display, KindleFont& regular, KindleFont& medium, Readings readings);
    // Returns true only when new pixels were sent to the display backend.
    bool update(std::time_t now);
    void setReadings(Readings readings);
private:
    Display& display_;
    KindleFont& regular_;
    KindleFont& medium_;
    Readings readings_;
    Frame previous_;
    std::time_t lastRefresh_ = 0;
    std::time_t lastFull_ = 0;
    bool started_ = false;
    bool readingsChanged_ = false;
    void text(const char* value, double center, double top, double size,
              bool medium = false, unsigned char gray = 0, double maxWidth = 360);
    void draw(const Frame& frame, bool full, bool clockChanged);
};
}
