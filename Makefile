# ----------------------------
# Makefile Options
# ----------------------------

NAME=ade

LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0

include $(shell agondev-config --makefile)

# aed's two libraries, from the aed release pinned in aed-libs.pin --
# tools/fetch-aed-libs.sh downloads, checks and unpacks it. AED_LIBS=<dir>
# builds against an unpacked package instead, such as one made with aed's
# mklibs.sh from an unreleased aed.
#
# After the include, on purpose: makefile.inc assigns CFLAGS and PROJECTLIBDIR
# with plain `=`, so anything set above it is overwritten. The UI is linked
# before the core, which it uses.
AED_LIBS ?= $(shell tools/fetch-aed-libs.sh)
ifeq ($(AED_LIBS),)
$(error no aed libraries: see the message above, or set AED_LIBS)
endif

# hub's client library, for building: its header and libhub.a, from a hub
# checkout -- HUB=<dir>, ~/code/devel by default.
HUB ?= $(HOME)/code/devel

CFLAGS += -I$(AED_LIBS)/include/core -I$(AED_LIBS)/include/ui -I$(HUB)/include
PROJECTLIBDIR := $(AED_LIBS)/lib
LIBS := -ledui -ledcore $(HUB)/build/lib/agondev/libhub.a

# Header dependencies, which AgonDev's makefile does not track -- see aed's
# Makefile for what that cost it. The libraries' headers count too: a new
# package with a changed struct has to rebuild everything that includes it.
CFLAGS += -MMD -MP
-include $(shell find $(OBJDIR) -name '*.d' 2>/dev/null)
