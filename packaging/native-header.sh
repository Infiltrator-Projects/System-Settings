#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The release builder appends a complete source payload after the marker.
set -euo pipefail
case "${1:-}" in
    --help|-h)
        echo 'System Settings native installer'
        echo 'Usage: ./System-Settings-VERSION-native.run [--extract DIRECTORY]'
        echo 'Default: build for this CPU, test, package, then install through APT.'
        echo 'Build prerequisites: build-essential cmake pkg-config libgtk-4-dev libgeocode-glib-dev xvfb dbus-x11'
        exit 0 ;;
    --extract)
        [[ $# == 2 && -n "$2" ]] || { echo 'Supply an empty extraction directory.' >&2; exit 2; }
        mkdir -- "$2"
        payload_line=$(awk '/^__SS_SOURCE_PAYLOAD__$/ {print NR+1; exit}' "$0")
        [[ -n "$payload_line" ]]
        tail -n +"$payload_line" "$0" | tar -xz -C "$2"
        exit 0 ;;
    '') [[ $# == 0 ]] || exit 2 ;;
    *) echo 'Unknown option; use --help.' >&2; exit 2 ;;
esac
[[ $EUID != 0 ]] || { echo 'Run as your ordinary user; only final package installation uses sudo.' >&2; exit 1; }
for command_name in cmake cc c++ make pkg-config dpkg-deb sudo xvfb-run dbus-daemon; do
    command -v "$command_name" >/dev/null || { echo "Missing prerequisite: $command_name" >&2; exit 1; }
done
pkg-config --exists 'gtk4 >= 4.6' geocode-glib-2.0 || {
    echo 'Install libgtk-4-dev and libgeocode-glib-dev before building.' >&2; exit 1;
}
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
payload_line=$(awk '/^__SS_SOURCE_PAYLOAD__$/ {print NR+1; exit}' "$0")
[[ -n "$payload_line" ]]
tail -n +"$payload_line" "$0" | tar -xz -C "$work"
cores=$(getconf _NPROCESSORS_ONLN)
jobs=$((cores > 1 ? cores - 1 : 1))
cmake -S "$work" -B "$work/build" -DCMAKE_BUILD_TYPE=Release \
    -DSYSTEM_SETTINGS_BUILD_PROFILE=native -DBUILD_TESTING=ON \
    -DCMAKE_C_FLAGS_RELEASE='-O3 -DNDEBUG -march=native -mtune=native'
cmake --build "$work/build" --parallel "$jobs"
ctest --test-dir "$work/build" --output-on-failure
(cd "$work/build" && cpack -G DEB)
packages=("$work"/build/infiltrator-system-settings_*.deb)
[[ ${#packages[@]} == 1 && -s "${packages[0]}" ]]
sudo apt-get install "${packages[0]}"
exit 0
__SS_SOURCE_PAYLOAD__
