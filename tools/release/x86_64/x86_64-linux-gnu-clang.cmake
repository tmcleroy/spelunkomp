# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of spelunkomp.
# The cross build for Linux x86_64 with clang (tools/release/linux.sh's default): clang targeting x86_64-linux-gnu,
# with the cross gcc's C++ library and headers (crossbuild-essential-amd64), linked by lld
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-linux-gnu)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)
set(PKG_CONFIG_EXECUTABLE /usr/bin/pkg-config CACHE FILEPATH "")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")
