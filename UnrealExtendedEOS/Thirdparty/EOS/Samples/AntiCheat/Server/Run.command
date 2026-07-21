#!/bin/bash
cd -- "$(dirname "$BASH_SOURCE")"
cd Build
./AntiCheatServer.app/Contents/MacOS/AntiCheatServer "$@"
