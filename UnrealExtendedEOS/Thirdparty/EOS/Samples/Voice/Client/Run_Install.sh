#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Voice.app/Contents/MacOS/Voice
else
    ./Voice
fi
