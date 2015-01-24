#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

include $(TOPDIR)/resources/AppInfo

#---------------------------------------------------------------------------------
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
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include

ICON            :=      resources/icon.png

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

MAKEROM    = $(TOPDIR)/tools/makerom
BANNERTOOL = $(TOPDIR)/tools/bannertool


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

null            :=
SPACE           :=      $(null) $(null)
export OUTPUT_D	:=	$(CURDIR)/output
export OUTPUT	:=	$(OUTPUT_D)/$(subst $(SPACE),,$(APP_TITLE))
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

export APP_ICON := $(TOPDIR)/$(ICON)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT_D)


#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
.PHONY: all
all	:	$(OUTPUT_D) $(OUTPUT).3dsx $(OUTPUT).smdh $(OUTPUT).cia $(OUTPUT).3ds
endif
$(OUTPUT_D)     :
	@[ -d $@ ] || mkdir -p $@

$(OUTPUT).3dsx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

banner.bnr: $(TOPDIR)/resources/banner.png $(TOPDIR)/resources/audio.wav
	$(BANNERTOOL) makebanner -i $(TOPDIR)/resources/banner.png -a $(TOPDIR)/resources/audio.wav -o banner.bnr
	@echo "built ... banner"

stripped.elf: $(OUTPUT).elf
	@cp $(OUTPUT).elf stripped.elf
	@$(PREFIX)strip stripped.elf

$(OUTPUT).cia: stripped.elf cia.rsf banner.bnr $(OUTPUT).smdh
	$(MAKEROM) -f cia -o $(OUTPUT).cia -rsf cia.rsf -target t -exefslogo -elf stripped.elf -icon $(OUTPUT).smdh -banner banner.bnr
	@echo "built ... $(notdir $@)"

$(OUTPUT).3ds: stripped.elf 3ds.rsf banner.bnr $(OUTPUT).smdh
	$(MAKEROM) -f cci -o $(OUTPUT).3ds -rsf 3ds.rsf -target d -exefslogo -elf stripped.elf -icon $(OUTPUT).smdh -banner banner.bnr
	@echo "built ... $(notdir $@)"
	
3ds.rsf:
	cat $(TOPDIR)/tools/template-3ds.rsf | sed 's/{APP_TITLE}/$(APP_TITLE)/' | sed 's/{APP_PRODUCT_CODE}/$(APP_PRODUCT_CODE)/' | sed 's/{APP_UNIQUE_ID}/$(APP_UNIQUE_ID)/' > 3ds.rsf
	
cia.rsf:
	cat $(TOPDIR)/tools/template-cia.rsf | sed 's/{APP_TITLE}/$(APP_TITLE)/' | sed 's/{APP_PRODUCT_CODE}/$(APP_PRODUCT_CODE)/' | sed 's/{APP_UNIQUE_ID}/$(APP_UNIQUE_ID)/' > cia.rsf

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
