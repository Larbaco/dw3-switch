#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
TARGET		:=	dw3-switch
BUILD		:=	build
SOURCES		:=	source psyz_backend
DATA		:=	data
INCLUDES	:=	include psyz_backend
# Artifacts (.nro/.elf/.nacp). Override for NTFS/Docker (see tools/dk.sh).
OUTDIR		?=	$(CURDIR)

APP_TITLE	:=	dw3-switch host_hello
APP_AUTHOR	:=	dw3-switch
APP_VERSION	:=	0.4.3
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm

#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(OUTDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

# mesa/EGL pulls C++ (nouveau codegen); always link with g++.
export LD	:=	$(CXX)

export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_SRC)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_TITLE APP_AUTHOR APP_VERSION

# switch_rules' %.nro rule is `elf2nro $< $@ $(NROFLAGS)` — it never passes
# --nacp itself. Without this the .nro carries elf2nro's stock "HOMEBREW" NACP
# and no asset section: hbmenu shows no title/version/author, and LibHac-based
# loaders (Ryujinx) throw NullReferenceException in Nro.OpenNroAssetSection.
export NROFLAGS	:=	--nacp=$(OUTPUT).nacp

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@[ -d $(OUTDIR) ] || mkdir -p $(OUTDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -f $(OUTPUT).nro $(OUTPUT).nacp $(OUTPUT).elf
	@# Clear contents only — $(BUILD) may be a Docker bind mount (DW3_DK_TMPBUILD).
	@if [ -d $(BUILD) ]; then find $(BUILD) -mindepth 1 -maxdepth 1 -exec rm -rf {} +; fi
	@rmdir $(BUILD) 2>/dev/null || true

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS	:=	$(OFILES:.o=.d)

all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
