#! /bin/bash

#####################################################
# update utox binary from Circle CI (zoff99/raspi branch)
#####################################################

cd $(dirname "$0")

pkill utox # will stop utox
cp -av utox utox__BACKUP
wget -O utox 'https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/RASPI/utox?filter=successful&branch=zoff99%2Fraspi'
chmod u+rwx utox
