#!/bin/sh
set -eu
. "${DASHY_ROOT:-}/var/local/dashy/common.sh"
# Both controls live on USB storage so startup can be disabled without a shell.
[ -f "$CONTROL/autostart-enabled" ] || exit 0
[ ! -e "$CONTROL/DISABLE" ] || exit 0
check_device
check_runtime
[ -f "$RUNTIME/sketches/dashy/sketch.elf" ] || die "Dashy has not been installed"
# /var/run is volatile. Keep this marker after exit: restoring the framework emits
# the same start event, and must never trigger another launch in this boot.
mkdir "$ROOT/var/run/dashy-boot-attempted" 2>/dev/null || exit 0
export KINDUINO_DIR="$RUNTIME"
export KINDUINO_NO_SPLASH=1
exec "$RUNTIME/bin/kinduino-supervisor" dashy
