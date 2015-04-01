ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPRO")
endif

#---------------------------------------------------------------------------------
# BUILD_FLAGS: List of extra build flags to add.
# NO_CTRCOMMON: Do not include ctrcommon.
# ENABLE_EXCEPTIONS: Enable C++ exceptions.
#---------------------------------------------------------------------------------

include $(DEVKITPRO)/ctrcommon/tools/make_base