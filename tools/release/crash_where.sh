#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp; from cps2recomp's.
#
# A Windows crash log's calls ("ddpdoj.exe+0x40ba8") as the functions they are in, from the same build's unstripped
# program (build/release-windows-clang/symbols/<game>.exe, kept by windows.sh; -gcc for WINDOWS_CC=gcc's):
#   tools/release/crash_where.sh build/release-windows-clang/symbols/ddpdoj.exe 0x40ba8 0x1307 ...
# (in the build image, for MinGW's nm: IMAGE=spelunkomp-build PLATFORM=linux/amd64 for the amd64 one; lines of other
# programs, SDL3.dll and Windows', are not ours)
set -eu
[ $# -ge 2 ] || { echo "usage: $0 <unstripped program.exe> <offset>..." >&2; exit 2; }
program=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); shift
docker run --rm --platform "${PLATFORM:-linux/arm64}" -v "$(dirname "$program")":/p "${IMAGE:-spelunkomp-build-arm64}" sh -c '
    exe=/p/$1; shift
    base=$(x86_64-w64-mingw32-objdump -p "$exe" | awk "/^ImageBase/{print \$2}")
    x86_64-w64-mingw32-nm -C --defined-only "$exe" | grep -E " [tT] " | grep -v " [.]text" | sort > /tmp/symbols
    for offset in "$@"; do
        at=$(printf "%016x" $((0x$base + offset)))
        awk -v a="$at" -v o="$offset" "\$1 <= a { l = \$0 } END { sub(/^[0-9a-f]+ [tT] /, \"\", l); print o \"  \" l }" /tmp/symbols
    done' sh "$(basename "$program")" "$@"
