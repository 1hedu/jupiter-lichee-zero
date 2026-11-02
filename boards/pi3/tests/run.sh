#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
M=../../mercury    # this repo's Mercury sources (were vendor/mercury)
export ASAN_OPTIONS=detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=1
flags='-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all'
c++ -std=c++17 $flags -I tests/mock tests/bridge_test.cpp -o tests/bridge_test
./tests/bridge_test
cc $flags -I $M/include -I $M/test -c $M/src/mercury_raster.c -o tests/raster.o
cc $flags -I $M/include -I $M/test -c $M/src/mercury_displaylist.c -o tests/displaylist.o
c++ $flags -I $M/include tests/packet_test.cpp tests/raster.o tests/displaylist.o -o tests/packet_test
./tests/packet_test

c++ -std=c++17 $flags tests/layout_test.cpp -o tests/layout_test
./tests/layout_test
c++ -std=c++17 $flags -DRASPPI=3 -I tests/mmio tests/i2c_test.cpp vendor/circle/lib/i2cmaster.cpp -o tests/i2c_test
./tests/i2c_test

c++ -std=c++17 $flags tests/keyboard_test.cpp -o tests/keyboard_test
./tests/keyboard_test

c++ -std=c++17 $flags tests/input_test.cpp -o tests/input_test
./tests/input_test
