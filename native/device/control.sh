#!/bin/sh
set -eu
. "$(dirname "$0")/common.sh"
check_device

case "${1:-}" in
    install)
        check_runtime
        [ ! -f "$RUNTIME/run/current" ] || die "exit the running Kinduino app before installing"
        [ -f "$USB/.kinduino/READY" ] || [ -f "$RUNTIME/sketches/dashy/sketch.elf" ] || die "copy the complete Dashy bundle first"
        # The runtime verifies checksums, architecture, API version, and every asset.
        "$RUNTIME/bin/kinduino-watch" --install-slot --dir "$USB/.kinduino" \
            --arch armv7 --exec-dir "$RUNTIME" --ext-dir "$USB/extensions/kinduino"
        [ -f "$RUNTIME/sketches/dashy/sketch.elf" ] || die "Dashy installation did not complete"
        mkdir -p "$HOME_DIR" "$CONTROL"
        cp "$EXT/common.sh" "$EXT/boot.sh" "$EXT/dashy.conf" "$HOME_DIR/"
        chmod 0755 "$HOME_DIR/boot.sh"
        chmod 0644 "$HOME_DIR/common.sh" "$HOME_DIR/dashy.conf"
        echo "Dashy installed. Reopen KUAL, then Kinduino > Installed Sketches > dashy."
        ;;
    enable)
        [ "${2:-}" = --tested ] || die "first verify the display, corner exit, and sleep/wake; then use enable --tested"
        check_runtime
        [ -f "$HOME_DIR/boot.sh" ] && [ -f "$HOME_DIR/dashy.conf" ] \
            && [ -f "$RUNTIME/sketches/dashy/sketch.elf" ] || die "install and test Dashy first"
        job="$ROOT/etc/upstart/dashy.conf"
        if [ -e "$job" ]; then
            grep -Fq '# Dashy Paperwhite 1 startup' "$job" || die "an unrelated dashy.conf already exists; it has been preserved"
        fi
        mkdir -p "$CONTROL"
        # Leave startup disabled if copying or remounting fails partway through.
        touch "$CONTROL/DISABLE"
        mntroot rw
        trap 'mntroot ro >/dev/null 2>&1 || true' EXIT
        trap 'exit 1' HUP INT TERM
        cp "$HOME_DIR/dashy.conf" "$job.tmp"
        chmod 0644 "$job.tmp"
        mv "$job.tmp" "$job"
        mntroot ro
        trap - EXIT HUP INT TERM
        touch "$CONTROL/autostart-enabled"
        rm -f "$CONTROL/DISABLE"
        echo "Dashy startup enabled for the next reboot."
        ;;
    disable)
        mkdir -p "$CONTROL"
        touch "$CONTROL/DISABLE"
        rm -f "$CONTROL/autostart-enabled"
        "$INITCTL" stop dashy 2>/dev/null || true
        echo "Dashy startup disabled. A manually launched demo exits with the top-left corner hold."
        ;;
    *) die "usage: control.sh install | enable --tested | disable" ;;
esac
