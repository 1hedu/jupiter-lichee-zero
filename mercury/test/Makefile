# Host-side tests for the Mercury rasterizer + display list parser.
# No pico-sdk needed — hardware/sync.h here stubs __dmb().
#
#   make && ./test_mercury
#
# Writes mercury_selftest.png (the self-test cube rendered through the
# display-list path) and exits non-zero on any mismatch. Run under ASan
# by default so hostile display lists that walk out of bounds fail loudly.

CC      ?= gcc
CFLAGS  ?= -O1 -g -fsanitize=address -Wall -Wextra
INC      = -I. -I../include

test_mercury: test_mercury.c ../src/mercury_raster.c ../src/mercury_displaylist.c
	$(CC) $(CFLAGS) $(INC) $^ -lz -lpthread -o $@

clean:
	rm -f test_mercury mercury_selftest.png
