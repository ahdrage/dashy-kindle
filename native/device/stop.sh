#!/bin/sh
set -eu
. "$(dirname "$0")/common.sh"
check_device
check_runtime
marker="$RUNTIME/run/current"
if [ -f "$marker" ]; then
    [ "$(cat "$marker")" = dashy ] || die "another Kinduino app is active; close it first"
fi

# Match full argv, not a substring that might signal an unrelated process.
supervisors=""
for entry in "$ROOT"/proc/[0-9]*; do
    [ -r "$entry/cmdline" ] || continue
    command=$(tr '\000' ' ' < "$entry/cmdline" 2>/dev/null) || continue
    case "$command" in
        "$RUNTIME/bin/kinduino-supervisor dashy "|"/bin/sh $RUNTIME/bin/kinduino-supervisor dashy "|"sh $RUNTIME/bin/kinduino-supervisor dashy ")
            supervisors="$supervisors ${entry##*/}" ;;
    esac
done
for pid in $supervisors; do
    # The runtime can fork shell helpers with identical argv. Signal their parent
    # once and let its existing cleanup release the screen service and children.
    parent=$(awk '/^PPid:/ {print $2}' "$ROOT/proc/$pid/status" 2>/dev/null) || continue
    case " $supervisors " in *" $parent "*) continue ;; esac
    echo "Requesting clean Dashy shutdown: $pid"
    kill -TERM "$pid" 2>/dev/null || true
done

runtime_processes() {
    for entry in "$ROOT"/proc/[0-9]*; do
        [ -r "$entry/cmdline" ] || continue
        command=$(tr '\000' ' ' < "$entry/cmdline" 2>/dev/null) || continue
        case " $command" in *" $RUNTIME/"*) echo "${entry##*/}" ;; esac
    done
    return 0
}

timeout=${DASHY_STOP_TIMEOUT:-45}
case "$timeout" in ''|*[!0-9]*) die "invalid shutdown timeout" ;; esac
elapsed=0
while :; do
    remaining=$(runtime_processes)
    [ -n "$remaining" ] || break
    if [ "$elapsed" -ge "$timeout" ]; then
        echo "Runtime processes still present: $remaining"
        die "the previous session could not close cleanly; restart the Kindle before reopening Dashy"
    fi
    sleep 1
    elapsed=$((elapsed + 1))
done
# A marker survives a device reboot because the runtime stores it in /var/local.
# Remove it only after every runtime process is absent; never bypass a live app.
if [ -f "$marker" ]; then
    [ "$(cat "$marker")" = dashy ] || die "the active app changed; no update applied"
    rm "$marker"
    echo 'Cleared the inactive Dashy session marker'
fi
echo 'Previous Dashy session is closed'
