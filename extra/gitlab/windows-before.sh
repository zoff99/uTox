#!/bin/sh
set -eux

. ./extra/gitlab/env.sh

export TARGET_HOST="--host=x86_64-w64-mingw32"
export TARGET_TRGT="--target=x86_64-win32-gcc"
export CROSS="x86_64-w64-mingw32-"

echo "==============================="
echo "==============================="
echo "==============================="

. ./extra/common/build_x264.sh

echo "==============================="
echo "==============================="
echo "==============================="

. ./extra/common/build_libav.sh

echo "==============================="
echo "==============================="
echo "==============================="

. ./extra/common/build_nacl.sh

echo "==============================="
echo "==============================="
echo "==============================="

. ./extra/common/build_opus.sh

echo "==============================="
echo "==============================="
echo "==============================="

. ./extra/common/build_vpx.sh

echo "==============================="
echo "==============================="
echo "==============================="



# CMake 3.2 or higher is required. for c-toxcore :-(
# sudo apt-get install cmake3

# install toxcore
# git clone --depth=1 --branch=$TOXCORE_REPO_BRANCH $TOXCORE_REPO_URI toxcore
git clone $CTOXCORE_URL toxcore
cd toxcore
git checkout "$CTOXCORE_VERSION_HASH"
# git rev-parse HEAD > toxcore.sha
if ! ([ -f "$CACHE_DIR/toxcore.sha" ] && diff "$CACHE_DIR/toxcore.sha" toxcore.sha); then
  mkdir _build
  ./autogen.sh
  cd _build
  export PKG_CONFIG_PATH="$CACHE_DIR/usr/lib/pkgconfig/"
  export LIBSODIUM_CFLAGS="-I$CACHE_DIR/usr/include/"
  export LIBSODIUM_LIBS="-L$CACHE_DIR/usr/lib/"
  export CFLAGS=" -g -O3 -fomit-frame-pointer -I$CACHE_DIR/usr/include -I/usr/share/mingw-w64/include/ "
  export CXXFLAGS=" -g -O3 -fomit-frame-pointer -I$CACHE_DIR/usr/include -I/usr/share/mingw-w64/include/ "
  CROSS=x86_64-w64-mingw32- ../configure --prefix=$CACHE_DIR/usr --enable-logging \
  --disable-soname-versions --host="x86_64-w64-mingw32" \
  --with-sysroot="$CACHE_DIR/" --disable-testing \
  --disable-rt --disable-shared
 
 make VERBOSE=1 V=1 -j8
 make install
 cd ..
 
#  cmake -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
#        -DCMAKE_SYSTEM_NAME=Windows \
#        -DCMAKE_CROSSCOMPILING=1 \
#        -B_build \
#        -H. \
#        -DCMAKE_INSTALL_PREFIX:PATH=$CACHE_DIR/usr \
#        -DENABLE_SHARED=OFF \
#        -DENABLE_STATIC=ON
#  make -C_build -j`nproc`
#  make -C_build install
#  mv toxcore.sha "$CACHE_DIR/toxcore.sha"
fi
cd ..
# rm -rf toxcore

echo "==============================="
echo "==============================="
echo "==============================="


if ! [ -d openal ]; then
  git clone --depth=1 https://github.com/irungentoo/openal-soft-tox.git openal
fi
cd openal
git rev-parse HEAD > openal.sha
if ! ([ -f "$CACHE_DIR/openal.sha" ] && diff "$CACHE_DIR/openal.sha" openal.sha ); then
  mkdir -p build
  cd build
  echo "
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH $CACHE_DIR )
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)" > ./Toolchain-x86_64-w64-mingw32.cmake
  cmake ..  -DCMAKE_TOOLCHAIN_FILE=./Toolchain-x86_64-w64-mingw32.cmake \
            -DCMAKE_PREFIX_PATH="$CACHE_DIR/usr" \
            -DCMAKE_INSTALL_PREFIX="$CACHE_DIR/usr" \
            -DLIBTYPE="STATIC" \
            -DCMAKE_BUILD_TYPE=Debug \
            -DDSOUND_INCLUDE_DIR=/usr/x86_64-w64-mingw32/include \
            -DDSOUND_LIBRARY=/usr/x86_64-w64-mingw32/lib/libdsound.a
  make
  make install
  cd ..
  mv -v openal.sha "$CACHE_DIR/openal.sha"
fi

cd ..
rm -rf openal

echo "==============================="
echo "==============================="
echo "==============================="

pwd

echo "==============================="
echo "==============================="
echo "==============================="


export CC=x86_64-w64-mingw32-gcc
. ./extra/common/filter_audio.sh
x86_64-w64-mingw32-ranlib $CACHE_DIR/usr/lib/libfilteraudio.a
unset CC

cp -av $CACHE_DIR/usr/lib/libOpenAL32.a $CACHE_DIR/usr/lib/libopenal.a || true

if [ ! -f "$CACHE_DIR/usr/lib/libshell32.a" ]; then
    curl https://cmdline.org/travis/64/shell32.a > "$CACHE_DIR/usr/lib/libshell32.a"
fi



