#include "Dashboard.h"
#include "LandscapeBackend.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static int failures = 0;
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); ++failures; } } while (0)

class Panel : public DisplayBackend {
public:
    int calls = 0, x = 0, y = 0, w = 0, h = 0;
    RefreshMode mode = RefreshMode::CONTENT;
    std::vector<uint8_t> pixels = std::vector<uint8_t>(758 * 1024);
    int panelWidth() const override { return 758; }
    int panelHeight() const override { return 1024; }
    void present(const uint8_t* buf, int stride, int px, int py, int width, int height, RefreshMode m) override {
        ++calls; x = px; y = py; w = width; h = height; mode = m;
        CHECK(px >= 0 && py >= 0 && px + width <= 758 && py + height <= 1024);
        if (px < 0 || py < 0 || px + width > 758 || py + height > 1024) return;
        for (int row = 0; row < height; ++row)
            for (int col = 0; col < width; ++col)
                pixels[(py + row) * 758 + px + col] = buf[row * stride + col];
    }
};

static bool fontFromFile(KindleFont& font, const char* path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    return font.loadFromMemory(bytes.data(), bytes.size());
}

int main(int argc, char** argv) {
    if (argc != 5) return 2;
    dashy::useOsloTime();
    // Epochs are UTC, independent of the Mac's local timezone.
    CHECK(dashy::frameAt(1788964320).clock == "16:32");
    CHECK(dashy::frameAt(1788964320).date == "Onsdag 9. september 2026");
    CHECK(dashy::frameAt(1767222000).date == "Torsdag 1. januar 2026");
    CHECK(dashy::frameAt(1767268800).clock == "13:00");
    CHECK(dashy::frameAt(1774745940).clock == "01:59");
    CHECK(dashy::frameAt(1774746000).clock == "03:00");
    CHECK(dashy::frameAt(1792889940).clock == "02:59");
    CHECK(dashy::frameAt(1792890000).clock == "02:00");
    CHECK(dashy::frameAt(0).clock == "--:--");
    CHECK(dashy::frameAt(0).date == "Still klokken på Kindle");

    KindleFont regular, medium;
    CHECK(fontFromFile(regular, argv[1]));
    CHECK(fontFromFile(medium, argv[2]));
    if (!regular.ok() || !medium.ok()) return 2;
    Panel panel;
    Display display;
    display.begin(758, 1024, Geometry::identity(758, 1024), &panel);
    dashy::Dashboard dashboard(display, regular, medium, {"21.3°C", "623 ppm", "14.2°C"});
    CHECK(dashboard.update(1788964320));
    CHECK(panel.calls == 1 && panel.w == 758 && panel.h == 1024);
    CHECK(panel.mode == RefreshMode::FULL_FLASH);
    std::vector<uint8_t> first(758 * 1024);
    display.readRect(0, 0, 758, 1024, first.data());
    int ink = 0;
    for (auto p : first) { if (p < 0x80) ++ink; CHECK(p % 16 == 0); }
    CHECK(ink > 10000 && ink < 200000);
    CHECK(display.pixel(0, 0) == 0xF0);
    std::ofstream pgm(argv[3], std::ios::binary);
    pgm << "P5\n758 1024\n255\n";
    pgm.write(reinterpret_cast<const char*>(first.data()), first.size());
    CHECK(!dashboard.update(1788964379));
    CHECK(panel.calls == 1);
    CHECK(dashboard.update(1788964380));
    CHECK(panel.calls == 2 && panel.h < 1024 && panel.mode == RefreshMode::CONTENT);
    int changed = 0;
    for (int y = 0; y < 1024; ++y) for (int x = 0; x < 758; ++x) {
        if (display.pixel(x, y) != first[y * 758 + x]) {
            ++changed;
            CHECK(x >= panel.x && x < panel.x + panel.w && y >= panel.y && y < panel.y + panel.h);
        }
    }
    CHECK(changed > 100);
    CHECK(dashboard.update(1788965220)); // Fifteen minutes after initial frame.
    CHECK(panel.mode == RefreshMode::FULL_FLASH && panel.h == 1024);
    CHECK(dashboard.update(1788964320)); // Clock correction backwards.
    CHECK(panel.mode == RefreshMode::FULL_FLASH);
    CHECK(dashboard.update(1788991140)); // 23:59 Oslo.
    CHECK(dashboard.update(1788991200)); // Midnight: redraw date as well.
    CHECK(panel.mode == RefreshMode::FULL_FLASH && panel.h == 1024);
    CHECK(dashboard.update(0));
    CHECK(!dashboard.update(1)); // Bad device clock must not cause a refresh loop.

    // Render landscape through the actual device adapter, then check panel pixels.
    Panel physical;
    dashy::LandscapeBackend landscape(physical);
    Display wide;
    wide.begin(&landscape);
    CHECK(wide.width() == 1024 && wide.height() == 758);
    if (wide.width() == 1024 && wide.height() == 758) {
        dashy::Dashboard horizontal(wide, regular, medium, {"21.3°C", "623 ppm", "14.2°C"});
        CHECK(horizontal.update(1788964320));
        CHECK(physical.w == 758 && physical.h == 1024 && physical.mode == RefreshMode::FULL_FLASH);
        auto checkRotation = [&]() {
            int mismatches = 0;
            for (int y = 0; y < 758; ++y) for (int x = 0; x < 1024; ++x)
                if (wide.pixel(x, y) != physical.pixels[(1023 - x) * 758 + y]) ++mismatches;
            CHECK(mismatches == 0);
        };
        checkRotation();
        std::vector<uint8_t> landscapePixels(1024 * 758);
        wide.readRect(0, 0, 1024, 758, landscapePixels.data());
        std::ofstream preview(argv[4], std::ios::binary);
        preview << "P5\n1024 758\n255\n";
        preview.write(reinterpret_cast<const char*>(landscapePixels.data()), landscapePixels.size());
        auto before = physical.pixels;
        CHECK(horizontal.update(1788964380));
        CHECK(physical.mode == RefreshMode::CONTENT && physical.w < 758 && physical.h == 1024);
        int changedPixels = 0;
        for (int y = 0; y < 1024; ++y) for (int x = 0; x < 758; ++x) {
            if (before[y * 758 + x] != physical.pixels[y * 758 + x]) {
                ++changedPixels;
                CHECK(x >= physical.x && x < physical.x + physical.w && y >= physical.y && y < physical.y + physical.h);
            }
        }
        CHECK(changedPixels > 100);
        checkRotation(); // A minute update must not damage the date or readings.
    }
    std::printf("Native dashboard checks: %d failures\n", failures);
    return failures ? 1 : 0;
}
