#/usr/bin/env zsh

if ! [ -d x264 ]; then
  git clone git://git.videolan.org/x264.git
fi
cd x264
git checkout stable

CROSS=x86_64-w64-mingw32-  ./configure --target=x86_64-win64-gcc \
          --prefix="${CACHE_DIR}/usr" \
          --disable-opencl --enable-shared --enable-static \
          --disable-avs --disable-cli


make -j8
make install

cd ..
rm -rf x264
