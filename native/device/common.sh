#!/bin/sh
# DASHY_ROOT is an off-device test seam; it is empty on the Kindle.
ROOT=${DASHY_ROOT:-}
RUNTIME="$ROOT/var/local/kinduino"
HOME_DIR="$ROOT/var/local/dashy"
USB="$ROOT/mnt/us"
CONTROL="$USB/dashy"
EXT="$USB/extensions/dashy"
INITCTL="$ROOT/sbin/initctl"

die() { echo "Dashy: $*" >&2; exit 1; }

check_device() {
    [ "$(id -u)" = 0 ] || die "requires root on the unlocked Kindle"
    case "$(uname -m)" in armv7*) ;; *) die "this installer is only for Paperwhite 1" ;; esac
    serial=$(cat "$ROOT/proc/usid" 2>/dev/null) || die "cannot identify the device"
    case "$serial" in B024*) ;; *) die "expected Paperwhite 1 Wi-Fi (B024)" ;; esac
    grep -Fq 'Kindle 5.6.1.1 (' "$USB/system/version.txt" 2>/dev/null || die "expected firmware 5.6.1.1; inspect the device before proceeding"
    [ -x "$INITCTL" ] && [ -d "$ROOT/etc/upstart" ] || die "expected Kindle upstart services"
}

check_runtime() {
    marker="$USB/extensions/kinduino/.installed"
    [ "$(sed -n 's/^version=//p' "$marker" 2>/dev/null)" = 0.5.2 ] || die "install Kinduino 0.5.2 first"
    [ "$(sed -n 's/^api=//p' "$marker" 2>/dev/null)" = 2.0.0 ] || die "incompatible Kinduino runtime"
    [ -x "$RUNTIME/bin/kinduino-supervisor" ] && [ -x "$RUNTIME/bin/kinduino-watch" ] || die "runtime tools are missing"
}
