#!/bin/sh
rm -f CMakeCache.txt
/opt/local/bin/cmake  -DCMAKE_CROSSCOMPILING_EMULATOR="/usr/bin/node" -Dminimal="ON" -Dwasm="ON" /home/wdconinc/git/root 
