#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./AntiCheatServer.app/Contents/MacOS/AntiCheatServer "$@"
else
    ./AntiCheatServer "$@"
fi
