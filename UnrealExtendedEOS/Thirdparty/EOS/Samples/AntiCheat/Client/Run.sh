#!/bin/sh
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./start_protected_game.app/Contents/MacOS/start_protected_game "$@"
else
    ./start_protected_game "$@"
fi
