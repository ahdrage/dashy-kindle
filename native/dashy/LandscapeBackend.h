#pragma once
#include <DisplayBackend.h>
#include <algorithm>
#include <vector>

namespace dashy {
class LandscapeBackend : public DisplayBackend {
public:
    explicit LandscapeBackend(DisplayBackend& panel) : panel_(panel) {}
    int panelWidth() const override { return std::max(panel_.panelWidth(), panel_.panelHeight()); }
    int panelHeight() const override { return std::min(panel_.panelWidth(), panel_.panelHeight()); }
    int panelDpi() const override { return panel_.panelDpi(); }
    bool lastOk() const override { return panel_.lastOk(); }
    void present(const uint8_t* buf, int stride, int x, int y, int w, int h, RefreshMode mode) override {
        if (panel_.panelWidth() >= panel_.panelHeight()) {
            panel_.present(buf, stride, x, y, w, h, mode);
            return;
        }
        // Rotate both pixels and refresh bounds counterclockwise. The Kindle is
        // held horizontally with its original bottom (USB/power edge) on the left.
        rotated_.resize(static_cast<size_t>(w) * h);
        for (int row = 0; row < h; ++row)
            for (int col = 0; col < w; ++col)
                rotated_[static_cast<size_t>(w - 1 - col) * h + row] = buf[static_cast<size_t>(row) * stride + col];
        panel_.present(rotated_.data(), h, y, panel_.panelHeight() - x - w, h, w, mode);
    }
private:
    DisplayBackend& panel_;
    std::vector<uint8_t> rotated_;
};
}
