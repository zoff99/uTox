#! /bin/bash

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_


build_for='
alpine:3.12.0
ubuntu:18.04
debian:10
ubuntu:20.04
debian:9
ubuntu:16.04
'

# archlinux:20200605

for system_to_build_for in $build_for ; do

    system_to_build_for_orig="$system_to_build_for"
    system_to_build_for=$(echo "$system_to_build_for_orig" 2>/dev/null|tr ':' '_' 2>/dev/null)

    cd $_HOME_/
    mkdir -p $_HOME_/"$system_to_build_for"/

    # rm -Rf $_HOME_/"$system_to_build_for"/script 2>/dev/null
    # rm -Rf $_HOME_/"$system_to_build_for"/workspace 2>/dev/null

    mkdir -p $_HOME_/"$system_to_build_for"/artefacts
    mkdir -p $_HOME_/"$system_to_build_for"/script
    mkdir -p $_HOME_/"$system_to_build_for"/workspace

    ls -al $_HOME_/"$system_to_build_for"/

    rsync -a ../ --exclude=.localrun $_HOME_/"$system_to_build_for"/workspace/uTox
    chmod a+rwx -R $_HOME_/"$system_to_build_for"/workspace/uTox

    echo '#! /bin/bash


pkgs_Ubuntu_18_04="
    :u:
    vim
    devscripts
    debhelper
    libconfig-dev
    cmake
    wget
    unzip
    zip
    automake
    autotools-dev
    build-essential
    check
    checkinstall
    libtool
    pkg-config
    rsync
    git
    libx11-dev
    x11-common
    x11-utils
    libxrender-dev
    libfreetype6-dev
    libfontconfig1-dev
    libxext-dev
    ffmpeg
    libasound2-dev
    libopenal-dev
    alsa-utils
    libv4l-dev
    v4l-conf
    v4l-utils
    libjpeg8-dev
    libavcodec-dev
    libavdevice-dev
    libsodium-dev
    libvpx-dev
    libopus-dev
    libx264-dev
"

pkgs_Ubuntu_16_04="
    :u:
    software-properties-common
    :c:add-apt-repository\sppa:jonathonf/ffmpeg-4\s-y
    :u:
    devscripts
    debhelper
    libconfig-dev
    cmake
    wget
    unzip
    zip
    passwd
    ffmpeg
    automake
    autotools-dev
    build-essential
    check
    checkinstall
    libtool
    pkg-config
    rsync
    git
    libx11-dev
    x11-common
    x11-utils
    libxrender-dev
    libfreetype6-dev
    libfontconfig1-dev
    libxext-dev
    libasound2-dev
    libopenal-dev
    libv4l-dev
    v4l-conf
    v4l-utils
    libjpeg8-dev
    libavcodec-dev
    libavdevice-dev
    libsodium-dev
    libvpx-dev
    libopus-dev
    libx264-dev
"

    pkgs_AlpineLinux_3_12_0="
        :c:apk\supdate
        :c:apk\sadd\sshadow
        :c:apk\sadd\sgit
        :c:apk\sadd\sunzip
        :c:apk\sadd\szip
        :c:apk\sadd\smake
        :c:apk\sadd\scmake
        :c:apk\sadd\sgcc
        :c:apk\sadd\slinux-headers
        :c:apk\sadd\smusl-dev
        :c:apk\sadd\sautomake
        :c:apk\sadd\sautoconf
        :c:apk\sadd\scheck
        :c:apk\sadd\slibtool
        :c:apk\sadd\srsync
        :c:apk\sadd\sgit
        :c:apk\sadd\slibx11-dev
        :c:apk\sadd\slibxext-dev
        :c:apk\sadd\sfreetype-deb
        :c:apk\sadd\sfontconfig-dev
        :c:apk\sadd\sopenal-soft-dev
        :c:apk\sadd\slibxrender-dev
        :c:apk\sadd\sffmpeg
        :c:apk\sadd\sffmpeg-dev
        :c:apk\sadd\salsa-lib
        :c:apk\sadd\salsa-lib-dev
        :c:apk\sadd\sv4l-utils
        :c:apk\sadd\sv4l-utils-dev
        :c:apk\sadd\slibjpeg
        :c:apk\sadd\slibsodium
        :c:apk\sadd\slibsodium-dev
        :c:apk\sadd\slibsodium-static
        :c:apk\sadd\slibvpx
        :c:apk\sadd\slibvpx-dev
        :c:apk\sadd\sopus
        :c:apk\sadd\sopus-dev
        :c:apk\sadd\sx264
        :c:apk\sadd\sx264-dev
"

    pkgs_ArchLinux_="
        :c:pacman\s-Sy
        :c:pacman\s-S\s--noconfirm\sbase-devel
        :c:pacman\s-S\s--noconfirm\sglibc
        :c:pacman\s-S\s--noconfirm\score/make
        :c:pacman\s-S\s--noconfirm\scmake
        :c:pacman\s-S\s--noconfirm\sopenal
        :c:pacman\s-S\s--noconfirm\sffmpeg
        :c:pacman\s-S\s--noconfirm\slibsodium
        :c:pacman\s-S\s--noconfirm\sv4l-utils
        :c:pacman\s-S\s--noconfirm\sautomake
        :c:pacman\s-S\s--noconfirm\slibx11
        :c:pacman\s-S\s--noconfirm\slibxext
        :c:pacman\s-S\s--noconfirm\slibxrender
        :c:pacman\s-S\s--noconfirm\sextra/check
        :c:pacman\s-S\s--noconfirm\sautoconf
        :c:pacman\s-S\s--noconfirm\sgit
"

    pkgs_CentOSLinux_7="
        :c:yum\scheck-update
        :c:yum\sinstall\s-y\sgit
        :c:yum\sinstall\s-y\spasswd
        :c:yum\sinstall\s-y\sunzip
        :c:yum\sinstall\s-y\szip
        :c:yum\sinstall\s-y\smake
        :c:yum\sinstall\s-y\sautomake
        :c:yum\sinstall\s-y\sautoconf
        :c:yum\sinstall\s-y\scheck
        :c:yum\sinstall\s-y\scheckinstall
        :c:yum\sinstall\s-y\slibtool
        :c:yum\sinstall\s-y\spkg-config
        :c:yum\sinstall\s-y\srsync
        :c:yum\sinstall\s-y\skernel-headers
        :c:yum\sinstall\s-y\slibX11-devel
        :c:yum\sinstall\s-y\slibX11-common
        :c:yum\sinstall\s-y\sxorg-x11-utils
        :c:yum\sinstall\s-y\slibvpx-devel
        :c:yum\sinstall\s-y\sopus-devel
        :c:yum\sinstall\s-y\salsa-utils
        :c:yum\sinstall\s-y\slibv4l-devel
        :c:yum\sinstall\s-y\sv4l-utils-devel-tools
        :c:yum\sinstall\s-y\sv4l-utils
        :c:yum\sinstall\s-y\slibjpeg-turbo-devel
        :c:yum\sinstall\s-y\shttps://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
        :c:yum\scheck-update
        :c:yum\sinstall\s-y\slibsodium
        :c:yum\sinstall\s-y\slibsodium-devel
        :c:yum\slocalinstall\s-y\shttps://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm
        :c:yum\scheck-update
        :c:yum\sinstall\s-y\sffmpeg-devel
        :c:yum\sinstall\s-y\sx264-devel
        :c:yum\sinstall\s-y\salsa-lib-devel
"

pkgs_Ubuntu_20_04="$pkgs_Ubuntu_18_04"
pkgs_DebianGNU_Linux_9="$pkgs_Ubuntu_18_04"
pkgs_DebianGNU_Linux_10="$pkgs_Ubuntu_18_04"

export DEBIAN_FRONTEND=noninteractive


os_release=$(cat /etc/os-release 2>/dev/null|grep "PRETTY_NAME=" 2>/dev/null|cut -d"=" -f2)
echo "using /etc/os-release"
system__=$(cat /etc/os-release 2>/dev/null|grep "^NAME=" 2>/dev/null|cut -d"=" -f2|tr -d "\""|sed -e "s#\s##g")
version__=$(cat /etc/os-release 2>/dev/null|grep "^VERSION_ID=" 2>/dev/null|cut -d"=" -f2|tr -d "\""|sed -e "s#\s##g")

echo "compiling on: $system__ $version__"

pkgs_name="pkgs_"$(echo "$system__"|tr "." "_"|tr "/" "_")"_"$(echo $version__|tr "." "_"|tr "/" "_")
echo "PKG:-->""$pkgs_name""<--"

for i in ${!pkgs_name} ; do
    if [[ ${i:0:3} == ":u:" ]]; then
        echo "apt-get update"
        apt-get update > /dev/null 2>&1
    elif [[ ${i:0:3} == ":c:" ]]; then
        cmd=$(echo "${i:3}"|sed -e "s#\\\s# #g")
        echo "$cmd"
        $cmd > /dev/null 2>&1
    else
        echo "apt-get install -y --force-yes ""$i"
        apt-get install -qq -y --force-yes $i > /dev/null 2>&1
    fi
done

#------------------------

mkdir -p /workspace/
cd /workspace/
mkdir -p inst
git clone https://github.com/zoff99/c-toxcore
cd c-toxcore


# cmake DCMAKE_C_FLAGS=" -D_GNU_SOURCE -g -O3 -fPIC " \
#  -DCMAKE_INSTALL_PREFIX:PATH=/workspace/inst/ . || exit 1

pwd
ls -al

./autogen.sh
export CFLAGS=" -D_GNU_SOURCE -g -O3 -I$_INST_/include/ -fPIC "
export LDFLAGS=" -O3 -L$_INST_/lib -fPIC "
./configure \
  --prefix=/workspace/inst/ \
  --disable-soname-versions --disable-testing --enable-logging --disable-shared

# make VERBOSE=1 -j $(nproc) || exit 1
make -j $(nproc) || exit 1
make install || exit 1


#------------------------


cd /workspace/uTox/
git submodule update --init --recursive

rm -Rf build2
mkdir -p build2
cd build2/ 

export PKG_CONFIG_PATH=/workspace/inst/lib/pkgconfig

# --debug-output

cmake \
 -DENABLE_AUTOUPDATE=OFF \
 -DENABLE_DBUS=OFF \
 -DASAN=OFF \
 -DENABLE_ASAN=OFF \
 -DTOXCORE_STATIC=ON \
 -DUTOX_STATIC=ON \
 ../ || exit 1


make -j $(nproc) || exit 1


ls -hal utox || exit 1
cp -av utox /artefacts/

' > $_HOME_/"$system_to_build_for"/script/run.sh


    docker run -ti --rm \
      -v $_HOME_/"$system_to_build_for"/artefacts:/artefacts \
      -v $_HOME_/"$system_to_build_for"/script:/script \
      -v $_HOME_/"$system_to_build_for"/workspace:/workspace \
      --net=host \
     "$system_to_build_for_orig" \
     /bin/sh -c "apk add bash >/dev/null 2>/dev/null; /bin/bash /script/run.sh"
     if [ $? -ne 0 ]; then
        echo "** ERROR **:$system_to_build_for_orig"
        exit 1
     else
        echo "--SUCCESS--:$system_to_build_for_orig"
     fi

done

ls -alh */artefacts/utox

