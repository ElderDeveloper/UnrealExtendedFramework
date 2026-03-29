#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Mods.app/Contents/MacOS/Mods
else
    ./Mods
fi
