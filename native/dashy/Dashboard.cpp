#include "Dashboard.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dashy {
void useOsloTime() {
    // POSIX rule is bundled by value: the old Kindle need not have a zoneinfo database.
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
}

Frame frameAt(std::time_t now) {
    if (now < 1577836800) return {"--:--", "Still klokken på Kindle"};
    std::tm local{};
    if (!localtime_r(&now, &local)) return {"--:--", "Still klokken på Kindle"};
    static const char* days[] = {"Søndag", "Mandag", "Tirsdag", "Onsdag", "Torsdag", "Fredag", "Lørdag"};
    static const char* months[] = {"januar", "februar", "mars", "april", "mai", "juni",
                                   "juli", "august", "september", "oktober", "november", "desember"};
    char clock[16], date[96];
    std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
    std::snprintf(date, sizeof(date), "%s %d. %s %d", days[local.tm_wday], local.tm_mday,
                  months[local.tm_mon], local.tm_year + 1900);
    return {clock, date};
}

Dashboard::Dashboard(Display& display, KindleFont& regular, KindleFont& medium, Readings readings)
    : display_(display), regular_(regular), medium_(medium), readings_(readings) {}

void Dashboard::text(const char* value, double center, double top, double size,
                     bool medium, unsigned char gray, double maxWidth) {
    KindleFont& font = medium ? medium_ : regular_;
    double scale = display_.width() / 400.0;
    // KindleFont sizes the full ascender/descender span; Montserrat's span is 1.219 em.
    int height = static_cast<int>(std::lround(size * scale * 1.219));
    int width = font.textWidth(value, height);
    if (width > maxWidth * scale) {
        height = std::max(10, static_cast<int>(height * maxWidth * scale / width));
        width = font.textWidth(value, height);
    }
    int x = static_cast<int>(std::lround(center * scale)) - width / 2;
    int baseline = static_cast<int>(std::lround(top * scale + height * 0.74 / 1.219));
    font.drawText(display_, x, baseline, value, height, gray);
}

void Dashboard::draw(const Frame& frame, bool full) {
    double scale = display_.width() / 400.0;
    auto px = [scale](double value) { return static_cast<int>(std::lround(value * scale)); };
    bool portrait = display_.height() > display_.width();
    int clockY = portrait ? 80 : 20;
    int clockH = portrait ? 125 : 82;
    if (full) display_.clear(0xF0);
    else display_.fillRect(0, px(clockY), display_.width(), px(clockH), 0xF0);
    text(frame.clock.c_str(), 200, portrait ? 92 : 30, portrait ? 108 : 78);
    if (full) {
        text(frame.date.c_str(), 200, portrait ? 212 : 105, 19, true);
        int separatorY = portrait ? 277 : 141;
        int titleY = portrait ? 308 : 163;
        int valueY = portrait ? 347 : 194;
        int dividerBottom = portrait ? 387 : 229;
        display_.fillRect(px(30), px(separatorY), px(340), std::max(1, px(1)), 0x50);
        const char* titles[] = {"INNE", "CO2", "UTE"};
        const char* values[] = {readings_.indoor, readings_.co2, readings_.outdoor};
        const int centers[] = {67, 200, 333};
        for (int i = 0; i < 3; ++i) {
            text(titles[i], centers[i], titleY, 14, true, 0x40);
            text(values[i], centers[i], valueY, 29, true, 0, 117);
        }
        for (int x : {133, 267})
            display_.fillRect(px(x), px(titleY + 1), std::max(1, px(1)), px(dividerBottom - titleY - 1), 0x50);
        text("DEMOVISNING · EKSEMPELDATA", 200, portrait ? 487 : 255, 9, true, 0x50);
        display_.fillRect(px(170), px(portrait ? 516 : 280), px(60), px(3), 0);
        display_.refresh(RefreshMode::FULL_FLASH, 0, 0, display_.width(), display_.height());
    } else {
        display_.refresh(RefreshMode::CONTENT, 0, px(clockY), display_.width(), px(clockH));
    }
}

bool Dashboard::update(std::time_t now) {
    Frame frame = frameAt(now);
    bool full = !started_ || frame.date != previous_.date || now < lastRefresh_
                || now - lastRefresh_ > 120 || now - lastFull_ >= 15 * 60;
    if (!full && frame.clock == previous_.clock) return false;
    draw(frame, full);
    previous_ = frame;
    lastRefresh_ = now;
    if (full) lastFull_ = now;
    started_ = true;
    return true;
}
}
