#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Export exactly HEAD plus its pinned Common tree; never bundle a dirty worktree.
#
# This release-only builder is not part of the distributed installer. It emits
# a native ELF .run whose extraction/build/test/install path is implemented in C.
set -euo pipefail
[[ $# == 1 ]] || { echo 'Usage: bash packaging/build-source-assets.sh OUTPUT_DIRECTORY' >&2; exit 2; }
root=$(git rev-parse --show-toplevel)
cd "$root"
git diff --quiet HEAD --
version=$(cat VERSION)
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
common=src/vendor/infiltratr-common
pin=$(git rev-parse "HEAD:$common")
[[ "$(git -C "$common" rev-parse HEAD)" == "$pin" ]]
mkdir -p -- "$1"
output=$(cd "$1" && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
mkdir "$work/source"
git archive HEAD | tar -x -C "$work/source"
mkdir -p "$work/source/$common"
git -C "$common" archive "$pin" | tar -x -C "$work/source/$common"
tar -czf "$work/source.tar.gz" -C "$work/source" .

cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
    -o "$work/native-installer" \
    "$work/source/packaging/native-installer.c"

run="$output/System-Settings-$version-native.run"
payload_size=$(wc -c < "$work/source.tar.gz")
cat "$work/native-installer" "$work/source.tar.gz" > "$run"
printf '\nSSPAYLOAD=%020d\n' "$payload_size" >> "$run"
chmod 0755 "$run"

(cd "$work/source" && cmake -E tar cf "$output/System-Settings-$version-source.zip" --format=zip -- .)

# These modes do not install software or modify machine settings. They prove the
# published .run is a native executable and can recover its exact source payload.
readelf -h "$run" >/dev/null
"$run" --help
"$run" --extract "$work/verified"
test -s "$work/verified/$common/CMakeLists.txt"
cmp "$work/source/VERSION" "$work/verified/VERSION"
cmp "$work/source/$common/VERSION" "$work/verified/$common/VERSION"
