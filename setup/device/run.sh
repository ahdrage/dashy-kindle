#!/bin/sh
# PW1 retry: the upstream payload is predecoded on the Mac.
exec > /mnt/us/winterbreak2/jailbreak-retry.log 2>&1 || exit 1
[ "$(id -u)" = 0 ] || { echo 'Unlock did not obtain root access'; exit 1; }
case "$(cat /proc/usid 2>/dev/null)" in
    B024*) ;;
    *) echo 'Unexpected device; this package was prepared for Paperwhite 1 Wi-Fi'; exit 1 ;;
esac
grep -Fq 'Kindle 5.6.1.1 (' /mnt/us/system/version.txt || exit 1
mkdir /tmp/dashy-unlock-retry.lock || exit 1
trap 'rmdir /tmp/dashy-unlock-retry.lock' EXIT
export JB_HEADER='Winterbreak2 Jailbreak'
export RUN_MODE=1 JB_SH_DEBUG=0
echo 'PW1 retry: unpack, unlock, install demo, open demo'
df -k /var/local /tmp
if ! sh /mnt/us/winterbreak2/jb.sh; then
    echo 'STOP: bootstrap returned an error; Dashy has not been launched'
    exit 1
fi
for required in /var/local/kmc/bin/sh_integration_launcher /var/local/kmc/bin/kmc_system_patcher /usr/lib/ccat/sh_integration_extractor.so /etc/uks/pubdevkey01.pem; do
    [ -s "$required" ] || { echo "STOP: unlock verification failed: $required"; exit 1; }
done
echo 'Unlock files verified; installing Dashy'
df -k /var/local
if ! sh '/mnt/us/documents/Install Dashy.sh'; then
    echo 'STOP: Dashy installation failed; see /mnt/us/dashy/install.log'
    eips 0 4 'Dashy install stopped. Reconnect USB.' 2>/dev/null
    exit 1
fi
echo 'Dashy installed; opening the demo after the interface restart'
sleep 10
if ! sh '/mnt/us/documents/Open Dashy.sh'; then
    echo 'STOP: Dashy launch failed'
    exit 1
fi
echo 'Dashboard launch requested; physical screen verification is still required'
