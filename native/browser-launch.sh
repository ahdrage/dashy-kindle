#!/bin/sh
set -eu
exec > /mnt/us/dashy/landscape-update.log 2>&1
. /mnt/us/extensions/dashy/common.sh
check_device
check_runtime
# Detach the entire update so a reader-interface restart cannot interrupt it.
"$RUNTIME/bin/kinduino-detach" /bin/sh "$EXT/update.sh"
echo 'Dashy reopening started. Details are in dashy/launch.log.'
