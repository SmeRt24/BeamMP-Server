#!/bin/bash

set -ex

if [ -ne ./deps/commandline/CMakeLists.txt ]
then
    rm -rf deps/commandline
    git clone https://github.com/lionkor/commandline/ deps/commandline
fi

if [ -ne ./deps/commandline/CMakeLists.txt ]
then
    rm -rf deps/commandline
    git clone https://github.com/microsoft/vcpkg/ deps/commandline
fi

cmake ${1:-.} -B bin -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -s -Wl,-z,norelro -Wl,--hash-style=gnu -Wl,--build-id=none -Wl,-z,noseparate-code -ffunction-sections -fdata-sections -Wl,--gc-sections" -DBeamMP-Server_ENABLE_LTO=ON || cat "$SOURCE"/server/bin/vcpkg-bootstrap.log
