#! /bin/bash


#sudo apt-get update
#sudo apt-get install git
#sudo apt-get install autoconf
#sudo apt-get install make
#sudo apt-get install checkinstall
#sudo apt-get install libtool
#sudo apt-get install cmake
#sudo apt-get install yasm



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


