#/usr/bin/env zsh

OPUS_VERSION="1.2.1"

# install libopus, needed for audio encoding/decoding
if ! [ -f "$CACHE_DIR/usr/lib/pkgconfig/opus.pc" ]; then
  wget 'https://github.com/xiph/opus/releases/download/v1.1.2/opus-1.1.2.tar.gz' -O opus.tar.gz
  # curl https://ftp.osuosl.org/pub/xiph/releases/opus/opus-${OPUS_VERSION}.tar.gz -o opus.tar.gz
  ls -al opus.tar.gz
  tar -xzvf opus.tar.gz
  cd opus-${OPUS_VERSION}
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
