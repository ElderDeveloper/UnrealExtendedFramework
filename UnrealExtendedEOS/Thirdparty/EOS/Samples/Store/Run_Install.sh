#!/bin/sh
cd Install
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./Store.app/Contents/MacOS/Store
else
    ./Store
fi
