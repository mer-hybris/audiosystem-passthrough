# -*- Mode: makefile-gmake -*-

.PHONY: all debug audiosystem-passthrough clean install

#
# Required packages
#

PKGS = glib-2.0 gio-2.0 libgbinder libglibutil

PREFIX ?= /usr
LIBDIR ?= /usr/lib

EXE = audiosystem-passthrough

SRC = passthrough-helper.c impl-af.c impl-qti.c impl-hw2_0.c dbus-comms.c

all: $(EXE) pkgconfig

PCVERSION = 1.2.0

SRC_DIR = src
BUILD_DIR = build

CC = $(CROSS_COMPILE)gcc
LD = $(CC)
WARNINGS = -Wall
INCLUDES =
CFLAGS = $(DEFINES) $(WARNINGS) $(INCLUDES) -MMD -MP \
  $(shell pkg-config --cflags $(PKGS))
LDFLAGS = $(shell pkg-config --libs $(PKGS))
RELEASE_FLAGS =

ifndef KEEP_SYMBOLS
KEEP_SYMBOLS = 0
endif

ifneq ($(KEEP_SYMBOLS),0)
RELEASE_FLAGS += -g
SUBMAKE_OPTS += KEEP_SYMBOLS=1
endif

RELEASE_LDFLAGS = $(LDFLAGS) $(RELEASE_FLAGS)
RELEASE_CFLAGS = $(CFLAGS) $(RELEASE_FLAGS) -O2

RELEASE_OBJS = $(SRC:%.c=$(BUILD_DIR)/%.o)

PKGCONFIG = $(BUILD_DIR)/$(EXE).pc

DEPS = $(DEBUG_OBJS:%.o=%.d) $(RELEASE_OBJS:%.o=%.d)
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(DEPS)),)
-include $(DEPS)
endif
endif

$(RELEASE_OBJS): | $(BUILD_DIR)
$(PKGCONFIG): | $(BUILD_DIR)

RELEASE_EXE = $(BUILD_DIR)/$(EXE)

$(EXE): $(RELEASE_EXE)

pkgconfig: $(PKGCONFIG)

clean:
	rm -f *~
	rm -fr $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.c
	$(CC) -c $(RELEASE_CFLAGS) -MT"$@" -MF"$(@:%.o=%.d)" $< -o $@

$(PKGCONFIG): $(EXE).pc.in Makefile
	sed -e 's,\[version\],'$(PCVERSION),g \
		-e 's,\[includedir\],'$(PREFIX)/include,g \
		-e 's,\[helperdir\],'$(PREFIX)/libexec/$(EXE),g $< > $@

$(RELEASE_EXE): $(BUILD_DIR) $(RELEASE_OBJS)
	$(LD) $(RELEASE_OBJS) $(RELEASE_LDFLAGS) -o $@
ifeq ($(KEEP_SYMBOLS),0)
	strip $@
endif

install: $(RELEASE_EXE)
	mkdir -p $(DESTDIR)$(PREFIX)/libexec/$(EXE)
	cp $< $(DESTDIR)$(PREFIX)/libexec/$(EXE)/$(EXE)
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	cp $(BUILD_DIR)/$(EXE).pc $(DESTDIR)$(LIBDIR)/pkgconfig
	mkdir -p $(DESTDIR)$(PREFIX)/include/audiosystem-passthrough
	cp $(SRC_DIR)/common.h $(DESTDIR)$(PREFIX)/include/audiosystem-passthrough
