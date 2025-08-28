#!/bin/bash

LIB_PATH="/usr/local/lib"
export LD_LIBRARY_PATH="$LIB_PATH:$LD_LIBRARY_PATH"

echo "Launching tiley..."
if [ -f "./build/tiley" ]; then
    ./build/tiley
else
    echo "Error: ./build/tiley executable does not exist. Did you forget to build first?"
    exit 1
fi
