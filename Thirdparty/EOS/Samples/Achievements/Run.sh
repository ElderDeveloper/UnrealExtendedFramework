#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Achievements.app/Contents/MacOS/Achievements
else
    ./Achievements
fi
