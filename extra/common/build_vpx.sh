#/usr/bin/env zsh

# install libvpx, needed for video encoding/decoding
if ! [ -d libvpx ]; then
  git clone --depth=1 --branch=v1.6.1 https://github.com/webmproject/libvpx.git
fi
cd libvpx
git rev-parse HEAD > libvpx.sha
if ! ([ -f "${CACHE_DIR}/libvpx.sha" ] && diff "${CACHE_DIR}/libvpx.sha" libvpx.sha); then
  CROSS=i586-mingw32msvc-  ./configure --target=x86-win32-gcc \
              --prefix="${CACHE_DIR}/usr" \
              --enable-static \
              --disable-examples \
              --disable-unit-tests \
              --disable-shared
  make -j8
  make install
  mv libvpx.sha "${CACHE_DIR}/libvpx.sha"
fi
cd ..
rm -rf libvpx
