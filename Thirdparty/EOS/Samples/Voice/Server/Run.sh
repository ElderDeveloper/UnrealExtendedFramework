#!/bin/sh
cd Build
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Mac OSX
    ./VoiceServer.app/Contents/MacOS/VoiceServer
else
    ./VoiceServer
fi
