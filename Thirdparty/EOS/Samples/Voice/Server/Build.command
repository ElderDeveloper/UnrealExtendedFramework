#!/bin/bash
cd -- "$(dirname "$BASH_SOURCE")"
rm -rf Build
mkdir Build/
cd Build
cmake ..
make -j4
