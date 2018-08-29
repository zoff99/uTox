#! /bin/bash



echo "starting ..."

START_TIME=$SECONDS

## ----------------------
full="1"
O_OPTIONS=" -O3 "
opus_sys=1
vpx_sys=1
x264_sys=1
ffmpeg_sys=1
numcpus_=12
quiet_=1
## ----------------------


_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

export qqq=""

if [ "$quiet_""x" == "1x" ]; then
	export qqq=" -qq "
fi


redirect_cmd() {
    if [ "$quiet_""x" == "1x" ]; then
        "$@" > /dev/null 2>&1
    else
        "$@"
    fi
}

echo "installing system packages ..."

redirect_cmd apt-get update $qqq

redirect_cmd apt-get install $qqq -y --force-yes lsb-release
system__=$(lsb_release -i|cut -d ':' -f2|sed -e 's#\s##g')
version__=$(lsb_release -r|cut -d ':' -f2|sed -e 's#\s##g')
echo "compiling for: $system__ $version__"



syslibs_str__="_"

if [ $opus_sys == 1 ]; then
    syslibs_str__="$syslibs_str__""o"
fi
if [ $vpx_sys == 1 ]; then
    syslibs_str__="$syslibs_str__""v"
fi
if [ $x264_sys == 1 ]; then
    syslibs_str__="$syslibs_str__""x"
fi
if [ $ffmpeg_sys == 1 ]; then
    syslibs_str__="$syslibs_str__""f"
fi

echo "with system libs for: $syslibs_str__"



echo "installing more system packages ..."

redirect_cmd apt-get install $qqq -y --force-yes software-properties-common wget git cmake

#dpkg -l|grep ffmpeg
#dpkg -l|grep libav
#dpkg -l|grep x264


redirect_cmd apt-get install $qqq -y --force-yes \
    automake \
    autotools-dev \
    build-essential \
    check \
    checkinstall \
    libtool \
    libfreetype6-dev \
    libopus-dev \
    libopenal-dev \
    fontconfig-config libfontconfig1-dev \
    libv4l-dev \
    pkg-config


pkgs="
libice-dev
libsm-dev
libx11-dev
libxau-dev
libxrandr-dev
libxss-dev
libxt-dev
x11proto-composite-dev
x11proto-core-dev
x11proto-damage-dev
x11proto-dri2-dev
x11proto-fixes-dev
x11proto-gl-dev
x11proto-input-dev
x11proto-kb-dev
x11proto-randr-dev
x11proto-render-dev
x11proto-scrnsaver-dev
x11proto-xext-dev
x11proto-xf86vidmode-dev
x11proto-xinerama-dev
"

for i in $pkgs ; do
    redirect_cmd apt-get install $qqq -y --force-yes $i
done


# cmake3 ?
type -a cmake
cmake --version
#add-apt-repository -y ppa:george-edison55/cmake-3.x
#apt-get update $qqq > /dev/null 2>&1
#apt-get install $qqq -y --force-yes cmake > /dev/null 2>&1
#type -a cmake
#cmake --version



# echo $_HOME_
cd $_HOME_
mkdir -p build

export _SRC_=$_HOME_/build/
export _INST_=$_HOME_/inst/

# echo $_SRC_
# echo $_INST_

mkdir -p $_SRC_
mkdir -p $_INST_

export PKG_CONFIG_PATH=$_INST_/lib/pkgconfig



if [ "$full""x" == "1x" ]; then


rm -Rf $_INST_

echo "NASM ..."


#nasm
cd $_HOME_/build

mkdir -p nasm
cd nasm
rm -f nasm.tar.bz2
redirect_cmd wget 'https://www.nasm.us/pub/nasm/releasebuilds/2.13.02/nasm-2.13.02.tar.bz2' -O nasm.tar.bz2
echo '8d3028d286be7c185ba6ae4c8a692fc5438c129b2a3ffad60cbdcedd2793bbbe  nasm.tar.bz2'|sha256sum -c

res=$?

if [ $res -ne 0 ]; then
    echo "checksum error in nasm source code!!"
    exit 2
fi

tar -xjf nasm.tar.bz2 || true
cd nasm-*

redirect_cmd bash autogen.sh
redirect_cmd ./configure
redirect_cmd make -j"$numcpus_"
make install
nasm -v


echo "LIBAV ..."

if [ $ffmpeg_sys == 1 ] ; then
    apt-get install $qqq -y --force-yes libavutil-dev libavfilter-dev libavcodec-dev # > /dev/null 2>&1
else

cd $_HOME_/build
rm -Rf libav
redirect_cmd git clone https://github.com/libav/libav
cd libav
git checkout v12.3
redirect_cmd ./configure --prefix=$_INST_ --disable-devices --disable-programs \
--disable-doc --disable-avdevice --disable-avformat \
--disable-swscale \
--disable-avfilter --disable-network --disable-everything \
--disable-bzlib \
--disable-libxcb-shm \
--disable-libxcb-xfixes \
--enable-parser=h264 \
--enable-runtime-cpudetect \
--enable-gpl --enable-decoder=h264
redirect_cmd make -j"$numcpus_"
make install > /dev/null 2>&1

fi


echo "X264 ..."

if [ $x264_sys == 1 ] ; then
    apt-get install $qqq -y --force-yes libx264-dev # > /dev/null 2>&1
else


cd $_HOME_/build
rm -Rf x264
git clone git://git.videolan.org/x264.git > /dev/null 2>&1
cd x264
git checkout stable
redirect_cmd ./configure --prefix=$_INST_ --disable-opencl --enable-shared --enable-static \
--disable-avs --disable-cli
redirect_cmd make -j"$numcpus_"
make install > /dev/null 2>&1


fi


echo "SODIUM ..."


cd $_HOME_/build
rm -Rf libsodium
git clone https://github.com/jedisct1/libsodium > /dev/null 2>&1
cd libsodium
git checkout 1.0.13
autoreconf -fi > /dev/null 2>&1
redirect_cmd ./configure --prefix=$_INST_ --disable-shared --disable-soname-versions
redirect_cmd make -j"$numcpus_"
make install > /dev/null 2>&1




echo "OPUS ..."

if [ $opus_sys == 1 ] ; then
    apt-get install -y --force-yes libopus-dev # > /dev/null 2>&1
else


cd $_HOME_/build
rm -Rf opus
git clone https://github.com/xiph/opus.git > /dev/null 2>&1
cd opus
git checkout v1.2.1
./autogen.sh > /dev/null 2>&1
redirect_cmd ./configure --prefix=$_INST_ --disable-shared
redirect_cmd make -j"$numcpus_"
make install > /dev/null 2>&1

fi



echo "VPX ..."

if [ $vpx_sys == 1 ] ; then
    apt-get install $qqq -y --force-yes libvpx-dev # > /dev/null 2>&1
else

cd $_HOME_/build
rm -Rf libvpx
git clone https://github.com/webmproject/libvpx.git > /dev/null 2>&1
cd libvpx > /dev/null 2>&1
git checkout v1.7.0
export CFLAGS=" -g -O3 -I$_INST_/include/ -Wall -Wextra "
export LDFLAGS=" -O3 -L$_INST_/lib "
redirect_cmd ./configure --prefix=$_INST_ \
  --disable-examples \
  --disable-unit-tests \
  --enable-shared \
  --size-limit=16384x16384 \
  --enable-onthefly-bitpacking \
  --enable-runtime-cpu-detect \
  --enable-multi-res-encoding \
  --enable-error-concealment \
  --enable-better-hw-compatibility \
  --enable-postproc \
  --enable-vp9-postproc
  --enable-temporal-denoising \
  --enable-vp9-temporal-denoising
redirect_cmd make -j"$numcpus_"
make install > /dev/null 2>&1
unset CFLAGS
unset LDFLAGS


fi

echo "filteraudio ..."

cd $_HOME_/build
rm -Rf filter_audio
git clone https://github.com/irungentoo/filter_audio.git > /dev/null 2>&1
cd filter_audio
export DESTDIR=$_INST_
export PREFIX=""
redirect_cmd make
make install > /dev/null 2>&1
export DESTDIR=""
unset DESTDIR
export PREFIX=""
unset PREFIX


fi



echo "c-toxcore ..."


cd $_HOME_/build


rm -Rf c-toxcore
git clone https://github.com/Zoxcore/c-toxcore c-toxcore > /dev/null 2>&1
cd c-toxcore
git checkout toxav-multi-codec
./autogen.sh > /dev/null 2>&1
export CFLAGS=" -D_GNU_SOURCE -g -O3 -I$_INST_/include/ -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable "
export LDFLAGS=" -O3 -L$_INST_/lib "
redirect_cmd ./configure \
  --prefix=$_INST_ \
  --disable-soname-versions --disable-testing --disable-shared
unset CFLAGS
unset LDFLAGS

redirect_cmd make -j"$numcpus_" || exit 1


make install > /dev/null 2>&1




echo "compiling uTox ..."


cd $_HOME_/build

rm -Rf utox_build
mkdir -p utox_build
cd utox_build/

git clone https://github.com/zoff99/uTox xx > /dev/null 2>&1
mv xx/* . > /dev/null 2>&1
mv xx/.??* . > /dev/null 2>&1
rm -Rf xx > /dev/null 2>&1

git checkout zoff99/linux_custom_003

git submodule update --init --recursive > /dev/null 2>&1

mkdir build2
cd build2/



export PKG_CONFIG_PATH=$_INST_/lib/pkgconfig
export CFLAGS=" -g $O_OPTIONS -I$_INST_/include/ -L$_INST_/lib -Wl,-Bstatic -lsodium -ltoxav -lopus -lvpx -lm -ltoxcore -ltoxencryptsave -Wl,-Bdynamic "
export LDFLAGS=" -L$_INST_/lib "


cmake \
 -DENABLE_AUTOUPDATE=OFF \
 -DENABLE_DBUS=OFF \
 -DASAN=OFF \
 -DENABLE_ASAN=OFF \
 -DTOXCORE_STATIC=ON \
 -DUTOX_STATIC=ON \
 ../

make clean > /dev/null 2> /dev/null

redirect_cmd make -j"$numcpus_"

unset CFLAGS

rm -f utox



echo "###############"
echo "##### 111 #####"
echo "###############"


syslibs_libs__=""
if [ $opus_sys == 1 ]; then
    syslibs_libs__="$syslibs_libs__"" -lopus "
else
    syslibs_libs__="$syslibs_libs__"" -l:libopus.a "
fi
if [ $vpx_sys == 1 ]; then
    syslibs_libs__="$syslibs_libs__"" -lvpx "
else
    syslibs_libs__="$syslibs_libs__"" -l:libvpx.a "
fi
if [ $x264_sys == 1 ]; then
    syslibs_libs__="$syslibs_libs__"" -lx264 "
else
    syslibs_libs__="$syslibs_libs__"" -l:libx264.a "
fi
if [ $ffmpeg_sys == 1 ]; then
    syslibs_libs__="$syslibs_libs__"" ""$(pkg-config --libs libavfilter libavutil libavcodec libavformat libswresample libavresample)"
else
    syslibs_libs__="$syslibs_libs__"" -l:libavcodec.a -l:libavutil.a "
fi


cc -g $O_OPTIONS -I$_INST_/include/   \
-L$_INST_/lib \
-Wall -Wextra -Wpointer-arith -Werror=implicit-function-declaration \
-Wformat=0 -Wno-misleading-indentation -fno-strict-aliasing -fPIC -flto  \
-Wl,-z,noexecstack CMakeFiles/utox.dir/src/avatar.c.o \
CMakeFiles/utox.dir/src/chatlog.c.o CMakeFiles/utox.dir/src/chrono.c.o \
CMakeFiles/utox.dir/src/command_funcs.c.o CMakeFiles/utox.dir/src/commands.c.o \
CMakeFiles/utox.dir/src/devices.c.o CMakeFiles/utox.dir/src/file_transfers.c.o \
CMakeFiles/utox.dir/src/filesys.c.o CMakeFiles/utox.dir/src/flist.c.o \
CMakeFiles/utox.dir/src/friend.c.o CMakeFiles/utox.dir/src/groups.c.o \
CMakeFiles/utox.dir/src/inline_video.c.o CMakeFiles/utox.dir/src/logging.c.o \
CMakeFiles/utox.dir/src/main.c.o CMakeFiles/utox.dir/src/messages.c.o \
CMakeFiles/utox.dir/src/notify.c.o CMakeFiles/utox.dir/src/screen_grab.c.o \
CMakeFiles/utox.dir/src/self.c.o CMakeFiles/utox.dir/src/settings.c.o \
CMakeFiles/utox.dir/src/stb.c.o CMakeFiles/utox.dir/src/text.c.o \
CMakeFiles/utox.dir/src/theme.c.o CMakeFiles/utox.dir/src/tox.c.o \
CMakeFiles/utox.dir/src/tox_callbacks.c.o CMakeFiles/utox.dir/src/ui.c.o \
CMakeFiles/utox.dir/src/ui_i18n.c.o CMakeFiles/utox.dir/src/updater.c.o \
CMakeFiles/utox.dir/src/utox.c.o CMakeFiles/utox.dir/src/window.c.o  \
-o utox \
-rdynamic src/av/libutoxAV.a \
src/xlib/libutoxNATIVE.a src/ui/libutoxUI.a \
-lrt -lpthread -lm -lopenal \
src/xlib/libicon.a -lv4lconvert \
-lSM -lICE -lX11 -lXext -lXrender \
-lfontconfig -lfreetype -lresolv -ldl \
-Wl,-Bstatic $_INST_/lib/libtoxcore.a -Wl,-Bdynamic \
-Wl,-Bstatic $_INST_/lib/libtoxav.a -Wl,-Bdynamic \
-Wl,-Bstatic $_INST_/lib/libtoxencryptsave.a -Wl,-Bdynamic \
-Wl,-Bstatic $_INST_/lib/libfilteraudio.a -Wl,-Bdynamic \
-l:libsodium.a \
$syslibs_libs__ \
CMakeFiles/utox.dir/third-party/qrcodegen/c/qrcodegen.c.o \
CMakeFiles/utox.dir/src/qr.c.o \
CMakeFiles/utox.dir/third-party/minini/dev/minIni.c.o \
src/layout/libutoxLAYOUT.a || exit 1

# ldd utox

echo "###############"
echo "###############"
echo "###############"

# so files can be accessed outside of docker
chmod -R a+rw /script/
chmod -R a+rw /artefacts/

pwd

ELAPSED_TIME=$(($SECONDS - $START_TIME))

echo "compile time: $(($ELAPSED_TIME/60)) min $(($ELAPSED_TIME%60)) sec"


ls -hal utox && cp -av utox /artefacts/utox_"$system__"_"$version__""$syslibs_str__"

