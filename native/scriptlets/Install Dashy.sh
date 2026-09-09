#!/bin/sh
set -eu
. "${DASHY_ROOT:-}/mnt/us/extensions/dashy/common.sh"
check_device
[ ! -f "$RUNTIME/run/current" ] || die "exit the running app before installing"
mkdir -p "$CONTROL"
exec > "$CONTROL/install.log" 2>&1
KINDUINO_DIR="$RUNTIME" EXT="$USB/extensions/kinduino" sh "$USB/extensions/kinduino/bin/kinduino-install"
sh "$EXT/control.sh" install
echo 'Dashy installed. Open Dashy from the Kindle library.'
if [ -x "$ROOT/usr/sbin/eips" ]; then
    "$ROOT/usr/sbin/eips" 1 2 'Dashy installed. Open Dashy in the library.' || true
fi
