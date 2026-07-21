#!/bin/sh
rm -rf Build
mkdir Build/
cd Build
cmake ..
make -j4
