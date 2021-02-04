#!/bin/bash

cd "`dirname "$0"`"

ARCH=`uname -m`

if [ "$ARCH" == "x86_64" ]; then
    export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:."
    exec ./ComPressure.linux
else
    echo "Currently only supporting x86 64bit systems (sorry)"
fi
