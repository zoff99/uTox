#/usr/bin/env zsh

OPUS_VERSION="1.2.1"

# install libopus, needed for audio encoding/decoding
if ! [ -f "$CACHE_DIR/usr/lib/pkgconfig/opus.pc" ]; then
  wget 'https://github.com/xiph/opus/archive/v1.2.1.tar.gz' -O opus.tar.gz
  # curl https://ftp.osuosl.org/pub/xiph/releases/opus/opus-${OPUS_VERSION}.tar.gz -o opus.tar.gz
  ls -al opus.tar.gz
  tar -xzvf opus.tar.gz

  cd opus-${OPUS_VERSION}

  ./autogen.sh

  export CFLAGS=" -g -O3 -I$CACHE_DIR/usr/include -I/usr/share/mingw-w64/include/ "
  export CXXFLAGS=" -g -O3 -I$CACHE_DIR/usr/include -I/usr/share/mingw-w64/include/ "

  ./configure "$TARGET_HOST" \
              --prefix="$CACHE_DIR/usr" \
              --disable-extra-programs \
              --disable-doc

  make -j`nproc`
  make install
  cd ..
  rm -rf opus**
else
  echo "Have Opus"
  ls -la "${CACHE_DIR}/usr/lib/"
  ls -la "${CACHE_DIR}/usr/lib/pkgconfig/"
  ls -la "${CACHE_DIR}/usr/include/"
fi
