#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Lobbies.app/Contents/MacOS/Lobbies
else
    ./Lobbies
fi
