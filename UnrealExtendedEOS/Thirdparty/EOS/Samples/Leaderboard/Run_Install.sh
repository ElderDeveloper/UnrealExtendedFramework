#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Leaderboard.app/Contents/MacOS/Leaderboard
else
    ./Leaderboard
fi
