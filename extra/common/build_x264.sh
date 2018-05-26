#/usr/bin/env zsh


mkdir nasm
cd nasm
wget 'https://www.nasm.us/pub/nasm/releasebuilds/2.13.02/nasm-2.13.02.tar.bz2' -O nasm.tar.bz2
echo '8d3028d286be7c185ba6ae4c8a692fc5438c129b2a3ffad60cbdcedd2793bbbe  nasm.tar.bz2'|sha256sum -c

res=$?

if [ $res -ne 0 ]; then
    echo "checksum error in nasm source code!!"
    exit 2
fi

tar -xjvf nasm.tar.bz2
cd nasm-*

bash autogen.sh
./configure "$TARGET_HOST"   --prefix="${CACHE_DIR}/usr"
make -j8
make install

make clean
./configure
make -j8
sudo make install
nasm -v

cd ..
rm -Rf nasm-*
cd ..

## too old ## sudo apt-get install nasm


# ============================

if ! [ -d x264 ]; then
  git clone git://git.videolan.org/x264.git
fi

cd x264
git checkout stable

./configure "$TARGET_HOST" --cross-prefix=x86_64-w64-mingw32- \
    --prefix="${CACHE_DIR}/usr" \
    --disable-opencl --enable-shared \
    --enable-static --disable-avs --disable-cli

# --disable-asm

make -j8
make install

cd ..
rm -rf x264
