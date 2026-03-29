#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./P2PNAT.app/Contents/MacOS/P2PNAT
else
    ./P2PNAT
fi
