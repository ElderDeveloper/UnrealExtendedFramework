#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./SessionMatchmaking.app/Contents/MacOS/SessionMatchmaking
else
    ./SessionMatchmaking
fi
