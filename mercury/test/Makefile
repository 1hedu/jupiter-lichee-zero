# Host-side tests for Mercury. No pico-sdk needed — hardware/sync.h
# here stubs __dmb().
#
#   make && ./test_mercury && ./test_hstx_stream
#
# test_mercury: renders the self-test cube through the display-list path
# to mercury_selftest.png, fuzzes rasterizer clipping against an int64
# reference, and throws hostile display lists at the parser.
# test_hstx_stream: decodes the RP2350 HSTX DVI stream with the command
# expander's semantics and checks every symbol period of several frames.
# Both run under ASan so out-of-bounds walks fail loudly.

CC      ?= gcc
CFLAGS  ?= -O1 -g -fsanitize=address -Wall -Wextra
INC      = -I. -I../include

all: test_mercury test_hstx_stream

test_mercury: test_mercury.c ../src/mercury_raster.c ../src/mercury_displaylist.c
	$(CC) $(CFLAGS) $(INC) $^ -lz -lpthread -o $@

test_hstx_stream: test_hstx_stream.c ../src/hstx_dvi_stream.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f test_mercury test_hstx_stream mercury_selftest.png
