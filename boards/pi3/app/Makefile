CIRCLEHOME ?= ../vendor/circle
CAMLIB = ../vendor/libcamera/lib
OBJS = main.o kernel.o $(CAMLIB)/cameradevice.o $(CAMLIB)/csi2cameradevice.o $(CAMLIB)/camerabuffer.o $(CAMLIB)/cameracontrol.o
LIBS = $(CIRCLEHOME)/lib/usb/libusb.a $(CIRCLEHOME)/lib/input/libinput.a $(CIRCLEHOME)/lib/fs/libfs.a $(CIRCLEHOME)/lib/libcircle.a
include $(CIRCLEHOME)/Rules.mk
INCLUDE += -I../vendor/libcamera/include
# Bare metal has no libm errno; permit hardware sqrt for unused Bayer helpers.
CFLAGS += -fno-math-errno
CPPFLAGS += -fno-math-errno
-include $(DEPS)
