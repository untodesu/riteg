#!/bin/sh
cmake -B build -DCMAKE_BUILD_TYPE=Release || exit 1
cmake --bild build --parallel $(nproc) || exit 1
exit 0
