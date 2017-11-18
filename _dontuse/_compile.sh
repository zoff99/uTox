#! /bin/bash

cd $(dirname "$0")
export _HOME_=$(pwd)
echo $_HOME_


# sudo apt install libfreetype6-dev
# sudo apt install x11proto-render-dev
# sudo apt install libxrender-dev
# sudo apt install libopenal-dev
# sudo apt install libfontconfig-dev
# sudo apt install libxshmfence-dev
# sudo apt install libxext-dev
# sudo apt install libdbus-1-dev

export _SRC_=$_HOME_/src/
export _INST_=$_HOME_/inst/

if [ "$1""x" == "fx" ]; then
	rm -Rf $_SRC_
	rm -Rf $_INST_
fi

mkdir -p $_SRC_
mkdir -p $_INST_

export LD_LIBRARY_PATH=$_INST_/lib/




cd $_SRC_
git clone --depth=1 --branch=1.0.13 https://github.com/jedisct1/libsodium.git
cd libsodium
./autogen.sh
export CFLAGS=" -static --static -fPIC "
export LDFLAGS=" -static --static "
./configure --prefix=$_INST_ --disable-shared --disable-soname-versions # --enable-minimal
make -j 4
make install

cd $_SRC_
# git clone --depth=1 --branch=v1.4.0 https://github.com/webmproject/libvpx.git
# git clone --depth=1 --branch=v1.5.0 https://github.com/webmproject/libvpx.git
# git clone --depth=1 --branch=v1.6.1 https://github.com/webmproject/libvpx.git
git clone --depth=1 --branch=master https://github.com/webmproject/libvpx.git
cd libvpx
export CFLAGS=" -fPIC "
export LDFLAGS=" -fPIC "

./configure --prefix=$_INST_ --disable-shared --disable-examples \
  --disable-unit-tests \
  --size-limit=16384x16384 \
  --enable-postproc --enable-multi-res-encoding \
  --enable-temporal-denoising --enable-vp9-temporal-denoising \
  --enable-vp9-postproc

make -j 4
make install

cd $_SRC_
git clone --depth=1 --branch=v1.2.1 https://github.com/xiph/opus.git
cd opus
./autogen.sh
export CFLAGS=" -fPIC "
./configure --prefix=$_INST_ --disable-shared
make -j 4
make install

cd $_SRC_
# git clone https://github.com/TokTok/c-toxcore
git clone https://github.com/zoff99/c-toxcore
cd c-toxcore

# git checkout 6c88bd0811a499ee72229e28796121e501a25245
# git checkout 9b89a3f77bf6a88646ca8e9caa8fcd5074ac7280
git checkout f7aee7deef0af235e1113411b83a0921ba057ecb

./autogen.sh

export CFLAGS=" -I$_INST_/include/ -fPIC "
export LDFLAGS=-L$_INST_/lib
./configure \
--prefix=$_INST_ \
--enable-logging \
--disable-soname-versions --disable-testing --disable-shared
make -j 4
make install


cd $_HOME_

git clone https://github.com/zoff99/uTox
cd uTox
git checkout zoff99/raspi

# cp -av CMakeLists.txt CMakeLists_raspi.txt

cd ..
mkdir build2
cd build2

export CFLAGS=" -g " ; cmake \
      -DENABLE_AUTOUPDATE=OFF \
      -DENABLE_DBUS=OFF \
      -DASAN=OFF \
      -DENABLE_ASAN=OFF \
      -DTOXCORE_STATIC=ON \
      -DUTOX_STATIC=ON \
      -DENABLE_FILTERAUDIO=OFF \
      -P CMakeLists_raspi.txt \
../uTox


make VERBOSE=1 -j 8

cc -g -O3 \
      -Wall -Wextra -Wpointer-arith -Werror=implicit-function-declaration \
      -Wformat=0 -Wno-misleading-indentation -fno-strict-aliasing -fPIC -flto \
      -fno-omit-frame-pointer   -Wl,-z,noexecstack CMakeFiles/utox.dir/src/avatar.c.o \
      CMakeFiles/utox.dir/src/chatlog.c.o CMakeFiles/utox.dir/src/chrono.c.o CMakeFiles/utox.dir/src/command_funcs.c.o \
      CMakeFiles/utox.dir/src/commands.c.o CMakeFiles/utox.dir/src/devices.c.o CMakeFiles/utox.dir/src/file_transfers.c.o \
      CMakeFiles/utox.dir/src/filesys.c.o CMakeFiles/utox.dir/src/flist.c.o CMakeFiles/utox.dir/src/friend.c.o \
      CMakeFiles/utox.dir/src/groups.c.o CMakeFiles/utox.dir/src/inline_video.c.o CMakeFiles/utox.dir/src/logging.c.o \
      CMakeFiles/utox.dir/src/main.c.o CMakeFiles/utox.dir/src/messages.c.o CMakeFiles/utox.dir/src/notify.c.o \
      CMakeFiles/utox.dir/src/screen_grab.c.o CMakeFiles/utox.dir/src/self.c.o \
      CMakeFiles/utox.dir/src/settings.c.o CMakeFiles/utox.dir/src/stb.c.o CMakeFiles/utox.dir/src/text.c.o \
      CMakeFiles/utox.dir/src/theme.c.o CMakeFiles/utox.dir/src/tox.c.o CMakeFiles/utox.dir/src/tox_callbacks.c.o \
      CMakeFiles/utox.dir/src/ui.c.o CMakeFiles/utox.dir/src/ui_i18n.c.o CMakeFiles/utox.dir/src/updater.c.o \
      CMakeFiles/utox.dir/src/utox.c.o CMakeFiles/utox.dir/src/window.c.o \
      -o utox \
      -rdynamic \
      src/av/libutoxAV.a src/xlib/libutoxNATIVE.a src/ui/libutoxUI.a \
      $_INST_/lib/libsodium.a \
      $_INST_/lib/libtoxav.a $_INST_/lib/libtoxcore.a $_INST_/lib/libtoxdns.a \
      $_INST_/lib/libtoxencryptsave.a $_INST_/lib/libopus.a \
      -lrt -lpthread -lm \
 /usr/lib/x86_64-linux-gnu/libvpx.a \
      -lopenal \
      src/xlib/libicon.a \
      -lv4lconvert -lSM -lICE -lX11 -lXext -lXrender -lfontconfig \
      -lfreetype -lresolv -ldl -ldbus-1 \
      src/layout/libutoxLAYOUT.a \
      -Wl,-rpath,/usr/local/lib

#      $_INST_/lib/libvpx.a \
# /usr/lib/x86_64-linux-gnu/libvpx.a \


cd $_HOME_
