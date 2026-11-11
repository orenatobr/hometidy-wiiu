#-------------------------------------------------------------------------------
# HomeTidy: Wii U Aroma app (.wuhb). Build with the Dockerfile in this folder.
#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

WUT_ROOT := $(DEVKITPRO)/wut

TARGET		:=	HomeTidy
BUILD		:=	build
SOURCES		:=	source
INCLUDES	:=	source

APP_NAME		:=	HomeTidy
APP_SHORTNAME	:=	HomeTidy
APP_AUTHOR		:=	orenatobr
APP_ICON		:=	resources/icon.png
APP_TV_SPLASH	:=	resources/tv.png
APP_DRC_SPLASH	:=	resources/drc.png

CFLAGS	:=	-Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS	+=	$(INCLUDE) -D__WIIU__ -D__WUT__ -I$(PORTLIBS_PATH)/wiiu/include/SDL2

CXXFLAGS	:= $(CFLAGS) -std=gnu++20

ASFLAGS	:=	$(ARCH)
LDFLAGS	=	$(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

LIBS	:=	-lSDL2_gfx -lSDL2_ttf -lharfbuzz -lfreetype -lbrotlidec -lbrotlicommon -lSDL2 -lpng -lz -lbz2 -lmocha -lwut

LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

export LD	:=	$(CXX)
export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# wuhbtool runs inside build/, so the resource paths must be absolute.
export APP_ICON		:= $(CURDIR)/$(APP_ICON)
export APP_TV_SPLASH	:= $(CURDIR)/$(APP_TV_SPLASH)
export APP_DRC_SPLASH	:= $(CURDIR)/$(APP_DRC_SPLASH)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf

else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT).wuhb

$(OUTPUT).wuhb	:	$(OUTPUT).rpx
$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

endif
