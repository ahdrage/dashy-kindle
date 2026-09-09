#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sdk=.deps/kinduino/arduino
mkdir -p output/native
c++ -O2 -std=gnu++17 -fno-exceptions -fno-rtti -Wall -Wextra \
    -I"$sdk/cores/kindle" -I"$sdk/libraries/Display" -I"$sdk/libraries/Font" \
    -I"$sdk/libraries/GFX" -Inative/dashy \
    native/tests/dashboard_test.cpp native/dashy/Dashboard.cpp \
    "$sdk/libraries/Font/KindleFont.cpp" "$sdk/libraries/Font/stb_truetype_impl.cpp" \
    "$sdk/libraries/Display/Display.cpp" "$sdk/libraries/Display/Scale.cpp" \
    "$sdk/libraries/Display/font5x7.cpp" "$sdk/cores/kindle/Print.cpp" "$sdk/cores/kindle/geometry.cpp" \
    -o output/native/dashboard-test
output/native/dashboard-test output/native/data/DashySans-Regular.ttf \
    output/native/data/DashySans-Medium.ttf output/native/dashy-native.pgm \
    output/native/dashy-native-landscape.pgm
