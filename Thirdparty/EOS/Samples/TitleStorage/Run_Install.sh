#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./TitleStorage.app/Contents/MacOS/TitleStorage
else
    ./TitleStorage
fi
