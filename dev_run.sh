#!/bin/bash

LIB_PATH="/usr/local/lib"
export LD_LIBRARY_PATH="$LIB_PATH:$LD_LIBRARY_PATH"

echo "运行tiley..."
if [ -f "./build/tiley" ]; then
    ./build/tiley
else
    echo "错误: 没有找到./build/tiley 可执行文件。是否忘记了先构建一下?"
    exit 1
fi
