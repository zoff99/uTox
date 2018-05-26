#/usr/bin/env zsh

if ! [ -d libav ]; then
  https://github.com/libav/libav
fi
cd libav
git checkout v12.3

CROSS=x86_64-w64-mingw32-  ./configure --target=x86_64-win64-gcc \
          --prefix="${CACHE_DIR}/usr" \
          --disable-devices --disable-programs \
          --disable-doc --disable-avdevice --disable-avformat \
          --disable-swscale \
          --disable-avfilter --disable-network --disable-everything \
          --disable-bzlib \
          --disable-libxcb-shm \
          --disable-libxcb-xfixes \
          --enable-parser=h264 \
          --enable-runtime-cpudetect \
          --enable-gpl --enable-decoder=h264

make -j8
make install

cd ..
rm -rf libav
