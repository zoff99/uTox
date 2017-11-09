#!/bin/sh
set -eux

. ./extra/gitlab/env.sh

export CFLAGS=" -g -O3 -I$CACHE_DIR/usr/include -I/usr/share/mingw-w64/include/ "

# CMake 3.2 or higher is required. for c-toxcore :-(
sudo apt-get install cmake3

mkdir build_win
cd build_win

#    -DCMAKE_BUILD_TYPE=Debug \
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-win64.cmake \
    -DCMAKE_INCLUDE_PATH="$CACHE_DIR/usr/include" \
    -DLIBTOX_INCLUDE_DIRS="$CACHE_DIR/usr/include" \
    -DLIBTOX_LIBRARIES="toxcore -ltoxencryptsave -ltoxav -ltoxdns" \
    -DCMAKE_LIBRARY_PATH="$CACHE_DIR/usr/lib" \
    -DENABLE_TESTS=OFF \
    -DENABLE_FILTERAUDIO=OFF \
    -DTOXCORE_STATIC=ON \
    -DENABLE_WERROR=OFF


make VERBOSE=1
