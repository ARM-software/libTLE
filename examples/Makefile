# Copyright (C) 2020 by Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Define target architecture (ARCH).
#
ifndef ARCH
  ARCH := $(shell uname -m)
  ifeq ($(ARCH),)
    $(error "Cannot detect architecture")
  endif
endif
ifeq ($(ARCH), arm64)
  override ARCH := aarch64
endif
ifndef ARCH
  ARCH := x86_64
endif


#
# Define static compilation or not (STATIC).
#
ifneq ($(findstring $(STATIC), 0 no No NO false False FALSE),)
  override STATIC = 0
else
  override STATIC = 1
endif

#
# Define compilation for debug or not (DEBUG).
#
ifneq ($(findstring $(DEBUG), 1 y Y yes Yes YES true True TRUE),)
  override DEBUG = 1
else
  override DEBUG = 0
endif


#
# Define target system (OS).
#
ifndef OS
  # Windows sets environment variable OS; for other systems, ask uname
  OS := $(shell uname -s)
  ifeq ($(OS),)
    $(error "Cannot detect operating system")
  endif
endif
ifeq ($(OS), Windows_NT)
  $(error "Cannot compiler for Windows")
endif


#
# Common compilation flags
#
CPPFLAGS = -DHAVE_THREADS -I..
CXXFLAGS = -pthread -std=c++11 -Wall -Werror
CFLAGS   = -pthread -std=c11 -Wall -Werror
ifeq ($(DEBUG),0)
  CPPFLAGS += -DNDEBUG=1
  CXXFLAGS += -O2
  CFLAGS   += -O2
else
  CXXFLAGS += -O0 -g
  CFLAGS   += -O0 -g
endif
LDFLAGS =
LDLIBS =


#
# Per-architecture compilation flags
#
ifeq ($(ARCH),aarch64)
  CXXFLAGS += -march=armv8-a+tme+nolse
  CFLAGS   += -march=armv8-a+tme+nolse
else
  CXXFLAGS += -mrtm
  CFLAGS   += -mrtm
endif


ifeq ($(OS),Darwin)
  #
  # Configure x86_64 macOS compilation flags.
  #
  CC  := clang
  CXX := clang++
  OD  := otool -tv

  # Clang on macOS does not need -pthread for linking, but macOS cannot compile
  # statically
  override STATIC = 0
else
  ifeq ($(ARCH),aarch64)
    #
    # Configure AArch64 Linux compilation flags.
    #

    # Look at the user's path for an ARM compiler
    ifeq ($(gccpath),)
      gccpath := $(abspath $(dir $(shell which aarch64-none-linux-gnu-g++)))
      gccprfx := aarch64-none-linux-gnu-
    endif

    # Look at the user's path for a Linaro compiler
    ifeq ($(gccpath),)
      gccpath := $(abspath $(dir $(shell which aarch64-linux-gnu-g++)))
      gccprfx := aarch64-linux-gnu-
    endif
  else
    #
    # Configure x86_64 Linux compilation flags.
    #

    # Look at the user's path for a compiler
    ifeq ($(gccpath),)
      gccpath := $(abspath $(dir $(shell which g++)))
    endif
  endif

  ifeq ($(gccpath),)
    $(error "Cannot find a valid compiler")
  else
    export PATH := $(gccpath):$(PATH)
    ifneq ($(binutilspath),)
      export PATH := $(binutilspath):$(PATH)
    endif
    AS  := $(gccpath)/$(gccprfx)as
    LD  := $(gccpath)/$(gccprfx)ld
    CC  := $(gccpath)/$(gccprfx)gcc
    CXX := $(gccpath)/$(gccprfx)g++
    OD  := $(gccpath)/$(gccprfx)objdump -d
  endif

  LDFLAGS = -pthread
  LDLIBS = -lrt
  ifeq ($(STATIC),1)
    LDFLAGS += -static
    LDLIBS += -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
  endif

  # Fix brocken compiler installs
  gccpathlib   := $(abspath $(wildcard $(gccpath)/../lib))
  gccpathlib64 := $(abspath $(wildcard $(gccpath)/../lib64))
  ifneq ($(gccpathlib64),)
    LDFLAGS += -Wl,-rpath,$(gccpathlib64)
  endif
  ifneq ($(gccpathlib),)
    LDFLAGS += -Wl,-rpath,$(gccpathlib)
  endif
endif

# Set shell commands
RM ?= rm -f
RD ?= rm -rf
MD ?= mkdir -p

TARGETS = spin_mutex_c \
	  spin_mutex_cxx \
	  spin_shared_mutex_c \
	  spin_shared_mutex_cxx

all: $(TARGETS)

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

LINK.c  = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
LINK.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(TARGET_ARCH)
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

% : %.c
% : %.c $(DEPDIR)/%.d
	$(LINK.c) $< $(LOADLIBES) $(LDLIBS) -o $@
	$(POSTCOMPILE)

% : %.cpp
% : %.cpp $(DEPDIR)/%.d
	$(LINK.cc) $< $(LOADLIBES) $(LDLIBS) -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

-include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(TARGETS)))

clean:
	-$(RM) $(TARGETS)
ifeq ($(OS),Darwin)
	-$(RD) $(addsuffix .dSYM, $(TARGETS))
endif

distclean: clean
	-$(RD) $(DEPDIR)
	-$(RD) $(addsuffix .asm, $(TARGETS))

%.asm: %
	$(OD) $^ > $@
