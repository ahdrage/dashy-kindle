#!/bin/sh
set -eu
. "$(dirname "$0")/common.sh"
check_device
check_runtime
mkdir -p "$CONTROL"
exec > "$CONTROL/launch.log" 2>&1
save_log() {
    status=$?
    trap - EXIT
    if [ -f "$RUNTIME/launcher.log" ]; then
        tail -n 160 "$RUNTIME/launcher.log" > "$CONTROL/runtime-launch.log" || true
    fi
    if [ "$status" -ne 0 ]; then
        echo "Dashy launch stopped with status $status"
        eips 0 6 'Dashy stopped. Please reconnect USB.' 2>/dev/null || true
    fi
    exit "$status"
}
trap save_log EXIT
echo 'Closing the previous session'
sh "$EXT/stop.sh"
echo 'Applying any pending dashboard update'
sh "$EXT/control.sh" install
echo 'Opening Dashy'
sh "$USB/documents/Open Dashy.sh"
sleep 5
echo 'Launch requested; the screen still needs physical confirmation'
