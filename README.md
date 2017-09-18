# uTox for Raspberry PI

Build Status
=
**CircleCI:** [![CircleCI](https://circleci.com/gh/zoff99/uTox/tree/zoff99%2Fraspi.png?style=badge)](https://circleci.com/gh/zoff99/uTox)


RASPI version
=
the latest Development Snapshot can be downloaded from CircleCI, [here](https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/RASPI/utox?filter=successful&branch=zoff99%2Fraspi)

<!--
and you will need the libraries as well

[c-toxcore](https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/RASPI/pkg_c-toxcore.tar.gz?filter=successful&branch=zoff99%2Fraspi)

[libsodium](https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/RASPI/pkg_libsodium.tar.gz?filter=successful&branch=zoff99%2Fraspi)
-->

then run on the PI:
```
sudo apt-get install qrencode pqiv libz1
```

if you want to connect via Tor:
```
sudo apt-get install tor tor-arm
```


Linux Version
=
the latest Development Snapshot can be downloaded from CircleCI, [here](https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/ubuntu_14_04_binaries/utox?filter=successful&branch=zoff99%2Fraspi)

<img src="https://circleci.com/api/v1/project/zoff99/uTox/latest/artifacts/0/$CIRCLE_ARTIFACTS/capture_app_running.png?filter=successful&branch=zoff99%2Fraspi" width="148">
