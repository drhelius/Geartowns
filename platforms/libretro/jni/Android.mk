LOCAL_PATH := $(call my-dir)

ROOT_DIR   := $(LOCAL_PATH)/../../..
CORE_DIR   := $(ROOT_DIR)/platforms/libretro
SOURCE_DIR := $(ROOT_DIR)/src
DEPS_DIR   := $(ROOT_DIR)/platforms/shared/dependencies
INCLUDES   := -I$(CORE_DIR) -I$(SOURCE_DIR) -I$(DEPS_DIR)/minivorbis -I$(DEPS_DIR)/miniz -I$(DEPS_DIR)/libchdr/include -I$(DEPS_DIR)/zstd -I$(DEPS_DIR)/lzma/include -I$(DEPS_DIR)/lightning/include -I$(DEPS_DIR)/bochs_fpu -I$(DEPS_DIR)/mt32emu/src -I$(DEPS_DIR)/fluidsynth/include -I$(DEPS_DIR)/fluidsynth/src -I$(DEPS_DIR)/fluidsynth/src/bindings -I$(DEPS_DIR)/fluidsynth/src/drivers -I$(DEPS_DIR)/fluidsynth/src/midi -I$(DEPS_DIR)/fluidsynth/src/rvoice -I$(DEPS_DIR)/fluidsynth/src/sfloader -I$(DEPS_DIR)/fluidsynth/src/synth -I$(DEPS_DIR)/fluidsynth/src/utils

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DHAVE_STDINT_H -DHAVE_INTTYPES_H -D__LIBRETRO__ -DGT_DISABLE_DISASSEMBLER -DGT_DISABLE_VGMRECORDER -DZ7_ST -DZSTD_DISABLE_ASM -DGT_ENABLE_CDROM_CUEBIN_READAHEAD $(INCLUDES)

ifeq ($(DISABLE_MT32), 1)
   COREFLAGS += -DGT_DISABLE_MT32
endif

ifeq ($(DISABLE_SOUNDFONT), 1)
   COREFLAGS += -DGT_DISABLE_SOUNDFONT
endif

GIT_VERSION ?= " $(shell git -c safe.directory="$(abspath $(ROOT_DIR))" describe --abbrev=7 --dirty --always --tags || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
   COREFLAGS += -DEMULATOR_BUILD=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE    := retro
LOCAL_SRC_FILES := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CFLAGS    := $(COREFLAGS)
LOCAL_CXXFLAGS  := $(COREFLAGS)
LOCAL_LDFLAGS   := -Wl,-version-script=$(CORE_DIR)/link.T
LOCAL_LDLIBS 	:= -llog
include $(BUILD_SHARED_LIBRARY)
