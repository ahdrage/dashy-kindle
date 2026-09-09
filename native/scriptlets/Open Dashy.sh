#!/bin/sh
set -eu
. "${DASHY_ROOT:-}/mnt/us/extensions/dashy/common.sh"
check_device
check_runtime
[ -f "$RUNTIME/sketches/dashy/sketch.elf" ] || die "open Install Dashy first"
export KINDUINO_DIR="$RUNTIME"
export KINDUINO_NO_SPLASH=1
"$RUNTIME/bin/kinduino-detach" "$RUNTIME/bin/kinduino-supervisor" dashy
