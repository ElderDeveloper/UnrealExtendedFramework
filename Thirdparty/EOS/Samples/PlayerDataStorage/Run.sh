#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./PlayerDataStorage.app/Contents/MacOS/PlayerDataStorage
else
    ./PlayerDataStorage
fi
