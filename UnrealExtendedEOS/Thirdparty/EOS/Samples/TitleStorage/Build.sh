#!/bin/sh
rm -rf Build
mkdir Build/
mkdir Build/Assets/
cp ../Shared/Assets/*.* Build/Assets/
cd Build
cmake ..
make -j4
