#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./AntiCheat.app/Contents/MacOS/AntiCheat
else
    ./AntiCheat
fi
