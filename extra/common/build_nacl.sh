#/usr/bin/env zsh

# install libsodium, needed for crypto
git clone https://github.com/jedisct1/libsodium.git
cd libsodium
git checkout 1.0.13

./autogen.sh
./configure "$TARGET_HOST" \
            --prefix="$CACHE_DIR/usr"
make -j`nproc`
make install

touch libsodium.sha
mv libsodium.sha "$CACHE_DIR/libsodium.sha"

cd ..
rm -rf libsodium
