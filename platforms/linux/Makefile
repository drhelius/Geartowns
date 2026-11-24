include ../shared/makefiles/Makefile.sources

SOURCES_CXX += $(SRC_DIR)/cdrom/cdrom_drive.cpp
SOURCES_CXX += $(SRC_DIR)/cdrom/cdrom_physical_image.cpp
SOURCES_CXX += $(SRC_DIR)/cdrom/cdrom_drive_linux.cpp
SOURCES_CXX += $(DESKTOP_SRC_DIR)/midi_port_linux.cpp

CPPFLAGS += -DGT_ENABLE_PHYSICAL_CDROM
CPPFLAGS += -DGT_ENABLE_MIDI_PORT
CPPFLAGS += `pkg-config --cflags alsa`
LDFLAGS += `pkg-config --libs alsa`

include ../shared/makefiles/Makefile.common