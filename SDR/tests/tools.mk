define ALLOWED_BUILD_CONFIGS
    aarch64.native
    aarch64.gcc6.3
    x86_32.gcc
    x86_64.gcc
    arm_cortex-a9.gcc4.8_uclibc_openwrt
    arm_cortex-a9.gcc5.2_glibc_openwrt
    arm_cortex-a9.gcc4.8_gnueabihf_linux
    arm_cortex-a9.gcc4.9.2_gnueabi
    arm_cortex-a9.gcc7.2.1_gnueabihf
endef

define ALLOWED_PLATFORMS
    msiq-sx
    z2
    z2-armhf
    z3u
endef

BUILD_CONFIG?= undefined
PLATFORM=z2-armhf # was undefined, but by default we want SidekiqZ2

# CFLAGS common to all build configurations
CFLAGS+= -fstrict-aliasing -fPIC -Wall -O3

ifeq ($(PLATFORM),msiq-sx)
    # The 'msiq-sx' PLATFORM targets the Matchstiq S1x/S2x at Release 9 or later (OpenWrt 16.02)
    BUILD_CONFIG=arm_cortex-a9.gcc5.2_glibc_openwrt
endif

ifeq ($(PLATFORM),z2)
    # The 'z2' PLATFORM targets the Sidekiq Z2
    BUILD_CONFIG=arm_cortex-a9.gcc4.9.2_gnueabi
endif

ifeq ($(PLATFORM),z2-armhf)
    # The 'z2-armhf' PLATFORM targets the Sidekiq Z2 (with hard-float extensions enabled)
    $(info z2-armhf selected:)
    BUILD_CONFIG=arm_cortex-a9.gcc7.2.1_gnueabihf
endif

ifneq ($(filter $(PLATFORM),z3u),)
    BUILD_CONFIG=aarch64.gcc6.3
endif

ifeq ($(BUILD_CONFIG),x86_32.gcc)
    TOOL_DIR=/usr/bin
    CROSS_PREFIX=
    CFLAGS+=-m32
    ARCH=x86_32
endif

ifeq ($(BUILD_CONFIG),x86_64.gcc)
    TOOL_DIR=/usr/bin
    CROSS_PREFIX=
    CFLAGS+= -g -gdwarf-3
    ARCH=x86_64
endif

ifeq ($(BUILD_CONFIG),aarch64.gcc6.3)
    TOOL_DIR=/opt/toolchains/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin
    CROSS_PREFIX=aarch64-linux-gnu-
    CONFIG_FLAGS="--host=aarch64-linux-gnu"
    ARCH=aarch64.gcc6.3
endif

ifeq ($(BUILD_CONFIG),aarch64.native)
    TOOL_DIR=/usr/bin
    CROSS_PREFIX=
    CONFIG_FLAGS=
    ARCH=aarch64.gcc6.3
endif

ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc4.8_uclibc_openwrt)
    TOOL_DIR=/opt/toolchains/arm_cortex-a9.gcc4.8_uclibc_openwrt/bin
    CROSS_PREFIX=arm-openwrt-linux-
    CFLAGS+=-mfpu=neon -mfloat-abi=hard -mno-unaligned-access -ggdb
    export STAGING_DIR=$(TOOL_DIR)
    CONFIG_FLAGS="--host=arm-openwrt-linux"
    ARCH=arm_cortex-a9
endif

ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc5.2_glibc_openwrt)
    TOOL_DIR=/opt/toolchains/arm_cortex-a9.gcc5.2_glibc_openwrt/bin
    CROSS_PREFIX=arm-openwrt-linux-
    CFLAGS+=-mfpu=neon -mfloat-abi=hard -mno-unaligned-access -ggdb
    export STAGING_DIR=$(TOOL_DIR)
    CONFIG_FLAGS="--host=arm-openwrt-linux"
    ARCH=arm_cortex-a9.glibc
endif

ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc4.8_gnueabihf_linux)
    TOOL_DIR=/opt/toolchains/arm_cortex-a9.gcc4.8_gnueabihf_linux/bin
    CROSS_PREFIX=arm-linux-gnueabihf-
    CFLAGS+=-g -fomit-frame-pointer -ffast-math -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard
    ARCH=arm_cortex-a9.jhf
endif

ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc4.9.2_gnueabi)
     TOOL_VERSION=2016.2
     # Xilinx Tools installed by default to this location
     TOOL_DIR = /opt/Xilinx/SDK/$(TOOL_VERSION)/gnu/arm/lin/bin
     CROSS_PREFIX=arm-xilinx-linux-gnueabi-
     VIVADO_SETTINGS=/opt/Xilinx/Vivado/$(TOOL_VERSION)/settings64.sh
     CROSS_COMPILE=$(CROSS_PREFIX)
    CONFIG_FLAGS="--host=arm-linux-gnueabi"
endif

ifeq ($(BUILD_CONFIG),arm_cortex-a9.gcc7.2.1_gnueabihf)
     TOOL_VERSION=2018.2
     # Xilinx Tools installed by default to this location
     TOOL_DIR = /opt/Xilinx/SDK/$(TOOL_VERSION)/gnu/aarch32/lin/gcc-arm-linux-gnueabi/bin
     CROSS_PREFIX=arm-linux-gnueabihf-
     VIVADO_SETTINGS=/opt/Xilinx/Vivado/$(TOOL_VERSION)/settings64.sh
     CROSS_COMPILE=$(CROSS_PREFIX)
     CONFIG_FLAGS="--host=arm-linux-gnueabihf"
endif

ifneq ($(PLATFORM),undefined)
  ifeq ($(filter $(PLATFORM),$(strip $(ALLOWED_PLATFORMS))),)
    $(info PLATFORM is not available, use one of the following:)
    $(info )
    $(info $(ALLOWED_PLATFORMS))
    $(info )
    $(error choose a supported PLATFORM (or BUILD_CONFIG))
  endif
endif

ifeq ($(filter $(BUILD_CONFIG),$(strip $(ALLOWED_BUILD_CONFIGS))),)
$(info BUILD_CONFIG is not defined or not available, use one of the following:)
$(info )
$(info $(ALLOWED_BUILD_CONFIGS))
$(info )
$(error choose a supported BUILD_CONFIG)
endif

##############################################
#
# Define tool chain based on TOOL_DIR and CROSS_PREFIX
#
##############################################

CC     := $(TOOL_DIR)/$(CROSS_PREFIX)gcc
CXX    := $(TOOL_DIR)/$(CROSS_PREFIX)g++
LD     := $(TOOL_DIR)/$(CROSS_PREFIX)ld
NM     := $(TOOL_DIR)/$(CROSS_PREFIX)nm
OBJCOPY:= $(TOOL_DIR)/$(CROSS_PREFIX)objcopy
AR     := $(TOOL_DIR)/$(CROSS_PREFIX)ar
RANLIB := $(TOOL_DIR)/$(CROSS_PREFIX)ranlib
STRIP  := $(TOOL_DIR)/$(CROSS_PREFIX)strip
