ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPRO")
endif

#---------------------------------------------------------------------------------
# BUILD_FLAGS: List of extra build flags to add.
# ENABLE_EXCEPTIONS: Enable C++ exceptions.
# NO_CITRUS: Do not include citrus.
#---------------------------------------------------------------------------------

include $(DEVKITPRO)/citrus/tools/make_base