#pragma once
#include <string>
namespace dashy {
struct Readings {
    std::string indoor = "--.-°C";
    std::string co2 = "---- ppm";
    std::string outdoor = "--.-°C";
    std::string footer = "DEMOVISNING · EKSEMPELDATA";
    bool operator==(const Readings& other) const {
        return indoor==other.indoor && co2==other.co2 && outdoor==other.outdoor && footer==other.footer;
    }
};
}
