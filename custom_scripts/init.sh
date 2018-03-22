#! /bin/bash


#sudo apt-get update
#sudo apt-get install git
#sudo apt-get install autoconf
#sudo apt-get install make
#sudo apt-get install checkinstall
#sudo apt-get install libtool
#sudo apt-get install yasm
#sudo apt-get install pkg-config
# sudo apt-get install cmake

# sudo apt-get install libfreetype6-dev
# sudo apt-get install x11-common libx11-dev x11proto-render-dev x11proto-core-dev
# sudo apt-get install 
# sudo apt-get install libxrender-dev

# sudo apt-get install libopus-dev libvpx-dev libopenal-dev

# sudo apt-get install fontconfig libfontconfig-dev
# sudo apt-get install libxext-dev
# sudo apt-get install libv4l-dev
# sudo apt-get install libxt6 libxrender1 libxext6 libice6 libxaw7 libx11-6 libxi6 libxmu6
# sudo apt-get install libice-dev libsm-dev

_HOME2_=$(dirname $0)
export _HOME2_
_HOME_=$(cd $_HOME2_;pwd)
export _HOME_


echo $_HOME_
cd $_HOME_

git clone https://github.com/zoff99/uTox
cd uTox
git checkout "zoff99/linux_custom_001"

cd $_HOME_
git clone https://github.com/zoff99/c-toxcore
cd c-toxcore
git checkout "zoff99/_0.1.10_2017_video_fix_10p"

cd $_HOME_


