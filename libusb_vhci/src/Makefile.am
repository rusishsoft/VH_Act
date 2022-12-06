lib_LTLIBRARIES = libusb_vhci.la
libusb_vhci_la_SOURCES = \
libusb_vhci.c \
urb.cpp \
port_stat.cpp \
work.cpp \
hcd.cpp \
local_hcd.cpp

# set the include path found by configure
INCLUDES = $(all_includes)

# the library search path.
libusb_vhci_la_LDFLAGS = $(all_libraries) -lpthread

include_HEADERS = libusb_vhci.h

libusb_vhci_la_CFLAGS_common = -pthread -Wall
libusb_vhci_la_CXXFLAGS_common = -pthread -Wall -Weffc++ -Wold-style-cast -Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel
if ENABLE_DEBUG
libusb_vhci_la_CFLAGS = $(libusb_vhci_la_CFLAGS_common)
libusb_vhci_la_CXXFLAGS = $(libusb_vhci_la_CXXFLAGS_common)
endif
if ENABLE_RELEASE
libusb_vhci_la_CFLAGS = $(libusb_vhci_la_CFLAGS_common)
libusb_vhci_la_CXXFLAGS = $(libusb_vhci_la_CXXFLAGS_common) -fno-enforce-eh-specs
endif
