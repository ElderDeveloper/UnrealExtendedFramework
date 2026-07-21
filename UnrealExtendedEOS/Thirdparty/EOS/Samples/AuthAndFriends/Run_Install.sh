#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./AuthAndFriends.app/Contents/MacOS/AuthAndFriends
else
    ./AuthAndFriends
fi
