#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Mods.app/Contents/MacOS/Mods
else
    ./Mods
fi
