#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Achievements.app/Contents/MacOS/Achievements
else
    ./Achievements
fi
