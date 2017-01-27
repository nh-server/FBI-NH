# TARGET #

TARGET := 3DS
LIBRARY := 0

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    ifeq ($(strip $(DEVKITPRO)),)
        $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro")
    endif
endif

# COMMON CONFIGURATION #

NAME := FBI

BUILD_DIR := build
OUTPUT_DIR := output
INCLUDE_DIRS := include
SOURCE_DIRS := source

EXTRA_OUTPUT_FILES := servefiles

LIBRARY_DIRS :=
LIBRARIES :=

BUILD_FLAGS := -Wno-misleading-indentation -Wno-strict-aliasing
BUILD_FLAGS_CC :=
BUILD_FLAGS_CXX :=
RUN_FLAGS :=

VERSION_PARTS := $(subst ., ,$(shell git describe --tags --abbrev=0))

VERSION_MAJOR := $(word 1, $(VERSION_PARTS))
VERSION_MINOR := $(word 2, $(VERSION_PARTS))
VERSION_MICRO := $(word 3, $(VERSION_PARTS))

# 3DS/Wii U CONFIGURATION #

ifeq ($(TARGET),$(filter $(TARGET),3DS WIIU))
    TITLE := $(NAME)
    DESCRIPTION := Open source CIA installer.
    AUTHOR := Steveice10
endif

# 3DS CONFIGURATION #

ifeq ($(TARGET),3DS)
    LIBRARY_DIRS += $(DEVKITPRO)/libctru
    LIBRARIES += citro3d ctru

    PRODUCT_CODE := CTR-P-CFBI
    UNIQUE_ID := 0xF8001

    CATEGORY := Application
    USE_ON_SD := true

    MEMORY_TYPE := Application
    SYSTEM_MODE := 64MB
    SYSTEM_MODE_EXT := Legacy
    CPU_SPEED := 268MHz
    ENABLE_L2_CACHE := true

    ICON_FLAGS := --flags visible,ratingrequired,recordusage --cero 153 --esrb 153 --usk 153 --pegigen 153 --pegiptr 153 --pegibbfc 153 --cob 153 --grb 153 --cgsrr 153

    ROMFS_DIR := romfs
    BANNER_AUDIO := meta/audio_3ds.wav
    BANNER_IMAGE := meta/banner_3ds.cgfx
    ICON := meta/icon_3ds.png
    LOGO := meta/logo_3ds.bcma.lz
endif

# Wii U CONFIGURATION #

ifeq ($(TARGET),WIIU)
    LIBRARY_DIRS +=
    LIBRARIES +=

    LONG_DESCRIPTION := Open source CIA installer.
    ICON := meta/icon_wiiu.png
endif

# INTERNAL #

include buildtools/make_base
