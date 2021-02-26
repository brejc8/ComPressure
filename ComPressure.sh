#!/bin/bash

cd "`dirname "$0"`"

ARCH=`uname -m`
PLATFORM=`uname -s`

if [ "$ARCH" = "x86_64" ]; then
    if [ "$PLATFORM" = "Linux" ]; then
        export LD_LIBRARY_PATH=".:$LD_LIBRARY_PATH"
        exec ./ComPressure.linux
    elif  [ "$PLATFORM" = "Darwin" ]; then
        export DYLD_LIBRARY_PATH=".:$DYLD_LIBRARY_PATH"
        exec ./ComPressure.macos
    fi
else
    echo "Currently only supporting x86 64bit systems (sorry)"
fi
