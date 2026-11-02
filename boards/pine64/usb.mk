# TinyUSB 0.18.0, pinned subset and local OHCI adaptation (see UPSTREAM.txt).
CPPFLAGS += -Ithird_party/tinyusb/src
USB_SRCS := boards/pine64/usb.c boards/pine64/usb_keyboard.c \
 third_party/tinyusb/src/tusb.c third_party/tinyusb/src/common/tusb_fifo.c \
 third_party/tinyusb/src/host/usbh.c third_party/tinyusb/src/host/hub.c \
 third_party/tinyusb/src/class/hid/hid_host.c \
 third_party/tinyusb/src/portable/ohci/ohci.c
