#!/bin/sh
rm -rf Build
mkdir Build/
rm -rf Install
mkdir Install/
mkdir Install/Assets/
cp ../../Shared/Assets/*.* Install/Assets/
cd Build
cmake -DCMAKE_INSTALL_PREFIX=../Install ..
make -j4 install
