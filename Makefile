#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	channel-blue
BUILD		:=	build
SOURCES		:=	source source/app source/integration source/navigation source/components source/render
DATA		:=	data
PROJECT_ROOT	:=	$(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
WOLFRAM_DIR	?=	$(abspath $(PROJECT_ROOT)/../wolfram)
WOLFRAM_BUILD	?=	$(WOLFRAM_DIR)/build-wii
WOLFRAM_PORTLIBS ?= $(WOLFRAM_DIR)/build-wii-mbedtls
HOST_WOLFRAM_BUILD ?= $(WOLFRAM_DIR)/build
INCLUDES	:=

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
CFLAGS	= -g -O2 -Wall $(MACHDEP) $(INCLUDE) \
	-I$(WOLFRAM_DIR)/include -I$(WOLFRAM_BUILD)/_deps/cjson-src \
	-I$(WOLFRAM_PORTLIBS)/include \
	-I$(DEVKITPRO)/portlibs/ppc/include \
	-I$(DEVKITPRO)/portlibs/ppc/include/freetype2
CXXFLAGS	=	$(CFLAGS)

LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	$(WOLFRAM_BUILD)/libwolfram.a \
		$(WOLFRAM_BUILD)/_deps/cjson-build/libcjson.a \
		$(WOLFRAM_BUILD)/_deps/libcbor-build/src/libcbor.a \
		$(WOLFRAM_PORTLIBS)/lib/libmbedtls.a \
		$(WOLFRAM_PORTLIBS)/lib/libmbedx509.a \
		$(WOLFRAM_PORTLIBS)/lib/libmbedcrypto.a \
		$(WOLFRAM_PORTLIBS)/lib/libmbedx509.a \
		$(WOLFRAM_PORTLIBS)/lib/libmbedcrypto.a \
		-lfreetype -lpng -ljpeg -lz -lbz2 -lbrotlidec -lbrotlicommon \
		-lfat -lwiikeyboard -lwiiuse -lbte -logc -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(DEVKITPRO)/portlibs/ppc $(WOLFRAM_PORTLIBS) $(WOLFRAM_BUILD) \
		$(WOLFRAM_BUILD)/_deps/cjson-build \
		$(WOLFRAM_BUILD)/_deps/libcbor-build/src

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)

#---------------------------------------------------------------------------------
# release artifacts: copy the full Homebrew Channel bundle to DISTDIR with a
# vX.X.X suffix. Version comes from the latest git tag, falling back to the
# short commit hash when the repo has no tags.
#---------------------------------------------------------------------------------
VERSION		:=	$(shell git -C $(CURDIR) describe --tags 2>/dev/null || git -C $(CURDIR) rev-parse --short HEAD)
DISTDIR		:=	/Volumes/Storage/Wii software

BUNDLEDIR	:=	dist/apps/channel-blue

.PHONY: $(BUILD) clean bundle dolphin release test

#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol

#---------------------------------------------------------------------------------
run:
	wiiload $(TARGET).dol

dolphin: $(OUTPUT).dol
	@mkdir -p build-dolphin-user
	@arch -x86_64 /Applications/Dolphin.app/Contents/MacOS/Dolphin \
		-u build-dolphin-user -C Dolphin.Core.WiiSDCard=True \
		-e $(OUTPUT).dol

# Build an installable Homebrew Channel tree. The entropy seed is generated
# once and deliberately retained across rebuilds; replacing it outside the
# application's rotation protocol could repeat the DRBG state.
bundle: $(OUTPUT).dol
	@mkdir -p "$(BUNDLEDIR)"
	@cp $(OUTPUT).dol "$(BUNDLEDIR)/boot.dol"
	@cp meta.xml icon.png "$(BUNDLEDIR)/"
	@if [ ! -f "$(BUNDLEDIR)/entropy.bin" ]; then \
		openssl rand 64 > "$(BUNDLEDIR)/entropy.bin"; \
	fi
	@echo bundle: ready at "$(BUNDLEDIR)"

#---------------------------------------------------------------------------------
release: $(OUTPUT).dol
	@echo release: copying bundle to "$(DISTDIR)"
	@mkdir -p "$(DISTDIR)"
	@cp $(OUTPUT).dol "$(DISTDIR)/$(TARGET) v$(VERSION).dol"
	@cp meta.xml "$(DISTDIR)/meta v$(VERSION).xml"
	@cp icon.png "$(DISTDIR)/icon v$(VERSION).png"
	@echo release: done "$(TARGET) v$(VERSION)"

# Host-side tests cover pure application modules without requiring Wii hardware.
test:
	@mkdir -p build-host
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_entropy_seed.c source/app/entropy_seed.c \
		-o build-host/test_entropy_seed
	@build-host/test_entropy_seed
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_avatar_key.c source/app/avatar_key.c \
		-o build-host/test_avatar_key
	@build-host/test_avatar_key
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_retry.c source/app/retry.c \
		-o build-host/test_retry
	@build-host/test_retry
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_session_store.c source/app/session_store.c \
		-o build-host/test_session_store
	@build-host/test_session_store
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_timeline.c source/app/timeline.c \
		-o build-host/test_timeline
	@build-host/test_timeline
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_compose.c source/app/compose.c source/app/timeline.c \
		-o build-host/test_compose
	@build-host/test_compose
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_auth.c source/app/auth.c source/app/session_store.c \
		-o build-host/test_auth
	@build-host/test_auth
	@cc -std=c99 -Wall -Wextra -Werror -Isource \
		tests/test_login.c source/app/login.c source/app/auth.c \
		source/app/session_store.c -o build-host/test_login
	@build-host/test_login
	@cc -std=c11 -Wall -Wextra -Werror -Isource -I$(WOLFRAM_DIR)/include \
		-I$(HOST_WOLFRAM_BUILD)/_deps/cjson-src \
		tests/test_wolfram_backend.c source/integration/wolfram_backend.c \
		source/app/auth.c source/app/session_store.c source/app/timeline.c \
		source/app/retry.c \
		-L$(HOST_WOLFRAM_BUILD) -lwolfram \
		-L$(HOST_WOLFRAM_BUILD)/_deps/cjson-build -lcjson \
		-o build-host/test_wolfram_backend
	@DYLD_LIBRARY_PATH=$(HOST_WOLFRAM_BUILD):$(HOST_WOLFRAM_BUILD)/_deps/cjson-build:$(HOST_WOLFRAM_BUILD)/_deps/libcbor-build/src \
	 LD_LIBRARY_PATH=$(HOST_WOLFRAM_BUILD):$(HOST_WOLFRAM_BUILD)/_deps/cjson-build:$(HOST_WOLFRAM_BUILD)/_deps/libcbor-build/src \
	 build-host/test_wolfram_backend

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin extension
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
