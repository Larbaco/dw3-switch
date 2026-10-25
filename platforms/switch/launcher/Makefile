#---------------------------------------------------------------------------------
# P6.24 slice 2: the region launcher NRO (no game code, no PSY-Z).
# Build with tools/build_switch_launcher.sh (devkitpro/devkita64 container).
# Standalone on purpose: platforms/switch/Makefile and the CMake game image do
# not know it.
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
TARGET		:=	dw3
BUILD		:=	build
SOURCES		:=	source ../../../port/region ../../../port/options
INCLUDES	:=	../../../port/region ../../../port/include
# Artifacts (.nro/.elf/.nacp). Override with OUTDIR=...
OUTDIR		?=	$(CURDIR)

APP_TITLE	:=	Digimon World 3 launcher
APP_AUTHOR	:=	dw3-switch
APP_VERSION	?=	0.1.0
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -Wextra -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx -lm

LIBDIRS	:= $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(OUTDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

# Explicit list: port/options holds more files than the launcher needs.
CFILES		:=	main.c dw3_region.c dw3_launcher.c options_cfg.c

export LD	:=	$(CC)

export OFILES	:=	$(CFILES:.c=.o)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_TITLE APP_AUTHOR APP_VERSION

# switch_rules' %.nro rule never passes --nacp itself (see
# platforms/switch/README.md, "The .nro must embed its NACP").
export NROFLAGS	:=	--nacp=$(OUTPUT).nacp

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ -d $(OUTDIR) ] || mkdir -p $(OUTDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(OUTPUT).nro $(OUTPUT).nacp $(OUTPUT).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS	:=	$(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
