#!/bin/sh
set -eu
. "$(dirname "$0")/common.sh"
check_device
check_runtime
source="$CONTROL/netatmo.pending.json"
[ -e "$source" ] || [ -L "$source" ] || exit 0
[ -f "$source" ] && [ ! -L "$source" ] || die "Netatmo settings must be a regular file"
[ ! -e "$RUNTIME/run/current" ] || die "close the running app before importing Netatmo settings"
size=$(wc -c < "$source" | tr -d ' ')
[ "$size" -gt 2 ] && [ "$size" -le 16384 ] || die "invalid Netatmo settings size"
files="$RUNTIME/sketches/dashy/files"
[ -d "$RUNTIME/sketches/dashy" ] && [ ! -L "$RUNTIME/sketches/dashy" ] || die "install Dashy first"
[ ! -L "$files" ] || die "unexpected Netatmo storage path"
umask 077
mkdir -p "$files"
chown 99:99 "$files"
chmod 0700 "$files"
target="$files/netatmo.json"
[ ! -L "$target" ] || die "unexpected Netatmo settings path"
[ ! -e "$target" ] || [ -f "$target" ] || die "unexpected Netatmo settings file"
temporary="$files/netatmo.import.$$"
(set -C; : > "$temporary") || die "Netatmo import already exists"
trap 'rm -f "$temporary"' EXIT
cp "$source" "$temporary"
chmod 0600 "$temporary"
chown 99:99 "$temporary"
cmp -s "$source" "$temporary" || die "Netatmo settings copy did not verify"
mv "$temporary" "$target"
sync
rm "$source"
echo 'Netatmo settings imported into private app storage; USB staging copy removed.'
