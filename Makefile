#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

APP_ID = FBI
APP_TITLE = FBI
APP_DESCRIPTION = Open source CIA installer.
APP_AUTHOR = Steveice10

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	$(APP_ID)
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include

ICON            :=      resources/icon48.png

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=softfp

CFLAGS	:=	-g -Wall -O3 -mword-relocations \
			-fomit-frame-pointer -ffast-math \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

CFLAGS +=       -std=gnu99

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) ./lib

MAKEROM = $(TOPDIR)/resources/tools/makerom
BANNER_TOOL = $(TOPDIR)/resources/tools/banner
PREPARE_BANNER = python2 banner.py
PREPARE_ICON24 = python2 icon.py
PREPARE_ICON48 = python2 icon.py
CREATE_BANNER = python2 create.py


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(TARGET).3ds $(TARGET).cia $(BANNER_TOOL)/banner.bnr $(BANNER_TOOL)/icon.icn


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
.PHONY: all
all	:	$(OUTPUT).3dsx $(OUTPUT).smdh $(OUTPUT).cia $(OUTPUT).3ds
endif
$(OUTPUT).3dsx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(BANNER_TOOL)/banner.bnr: $(TOPDIR)/resources/banner.png $(TOPDIR)/resources/icon24.png $(TOPDIR)/resources/icon48.png
	cp $(TOPDIR)/resources/banner.png $(BANNER_TOOL)/banner/banner.png
	cp $(TOPDIR)/resources/icon24.png $(BANNER_TOOL)/icon24/icon24.png
	cp $(TOPDIR)/resources/icon48.png $(BANNER_TOOL)/icon48/icon48.png
	cd $(BANNER_TOOL)/banner; $(PREPARE_BANNER)
	cd $(BANNER_TOOL)/icon24; $(PREPARE_ICON24)
	cd $(BANNER_TOOL)/icon48; $(PREPARE_ICON48)
	cd $(BANNER_TOOL); $(CREATE_BANNER)
	rm $(BANNER_TOOL)/banner/banner.png
	rm $(BANNER_TOOL)/banner/banner.cgfx
	rm $(BANNER_TOOL)/banner/compressed.cgfx
	rm $(BANNER_TOOL)/banner/banner.cbmd
	rm $(BANNER_TOOL)/icon24/icon24.png
	rm $(BANNER_TOOL)/icon24/icon24.ctpk
	rm $(BANNER_TOOL)/icon48/icon48.png
	rm $(BANNER_TOOL)/icon48/icon48.ctpk
	
$(BANNER_TOOL)/icon.icn: $(BANNER_TOOL)/banner.bnr

$(OUTPUT).cia: $(OUTPUT).elf $(TOPDIR)/resources/cia.rsf $(BANNER_TOOL)/banner.bnr $(BANNER_TOOL)/icon.icn
	@cp $(OUTPUT).elf $(TARGET)_stripped.elf
	@$(PREFIX)strip $(TARGET)_stripped.elf
	$(MAKEROM) -f cia -o $(OUTPUT).cia -rsf $(TOPDIR)/resources/cia.rsf -target t -exefslogo -elf $(TARGET)_stripped.elf -icon $(BANNER_TOOL)/icon.icn -banner $(BANNER_TOOL)/banner.bnr
	@echo "built ... $(notdir $@)"

$(OUTPUT).3ds: $(OUTPUT).elf $(TOPDIR)/resources/3ds.rsf $(BANNER_TOOL)/banner.bnr $(BANNER_TOOL)/icon.icn
	@cp $(OUTPUT).elf $(TARGET)_stripped.elf
	@$(PREFIX)strip $(TARGET)_stripped.elf
	$(MAKEROM) -f cci -o $(OUTPUT).3ds -rsf $(TOPDIR)/resources/3ds.rsf -target d -exefslogo -elf $(TARGET)_stripped.elf -icon $(BANNER_TOOL)/icon.icn -banner $(BANNER_TOOL)/banner.bnr
	@echo "built ... $(notdir $@)"

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

# WARNING: This is not the right way to do this! TODO: Do it right!
#---------------------------------------------------------------------------------
%.vsh.o	:	%.vsh
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@python $(AEMSTRO)/aemstro_as.py $< ../$(notdir $<).shbin
	@bin2s ../$(notdir $<).shbin | $(PREFIX)as -o $@
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u8" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@echo "extern const u32" `(echo $(notdir $<).shbin | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(notdir $<).shbin | tr . _)`.h
	@rm ../$(notdir $<).shbin

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
