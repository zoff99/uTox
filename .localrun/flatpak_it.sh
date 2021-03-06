#! /bin/bash

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_

echo $_HOME_
cd $_HOME_


build_for='
ubuntu:20.04
'

for system_to_build_for in $build_for ; do

    system_to_build_for_orig="$system_to_build_for"
    system_to_build_for=$(echo "$system_to_build_for_orig" 2>/dev/null|tr ':' '_' 2>/dev/null)"_flatpak"

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

    cp -v utox.json $_HOME_/"$system_to_build_for"/workspace/
    chmod a+rwx $_HOME_/"$system_to_build_for"/workspace/utox.json

    echo '#! /bin/bash


pkgs_Ubuntu_18_04="
    :u:
    ca-certificates
    shtool
    elfutils
    xz-utils
    patch
    bzip2
    librsvg2-2
    librsvg2-common
    flatpak
    flatpak-builder
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

# Add "https://flathub.org" remote:
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

## Workaround for Flathub download issues: https://github.com/flathub/flathub/issues/845
# Pre download org.kde.Sdk because it fails often
for i in {1..5}
do
    echo "Download try $i"
    flatpak --system install flathub -y org.kde.Sdk/x86_64/5.15 || true
    flatpak --system install flathub -y org.gnome.Sdk/x86_64/3.38 || true
done
## Workaround end

# Build the qTox flatpak
flatpak-builder --disable-rofiles-fuse --install-deps-from=flathub --force-clean --repo=tox-repo utox-flatpak /workspace/utox.json

# Create a bundle for distribution
flatpak build-bundle tox-repo /artefacts/utox.flatpak io.github.utox.uTox

# Chmod since everything is root:root
chmod 755 -R /artefacts/

#------------------------


' > $_HOME_/"$system_to_build_for"/script/run.sh


    # careful this uses "privileged" option for docker!!!!!!!!!!!!!!!
    # careful this uses "privileged" option for docker!!!!!!!!!!!!!!!
    # careful this uses "privileged" option for docker!!!!!!!!!!!!!!!
    # careful this uses "privileged" option for docker!!!!!!!!!!!!!!!
    docker run --privileged -ti --rm \
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

ls -alh *_flatpak/artefacts/

