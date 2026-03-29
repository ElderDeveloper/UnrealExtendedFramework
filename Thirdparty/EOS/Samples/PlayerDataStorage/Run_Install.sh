#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./PlayerDataStorage.app/Contents/MacOS/PlayerDataStorage
else
    ./PlayerDataStorage
fi