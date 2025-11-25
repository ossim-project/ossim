# Makefile for ossimctl - Command-line control tool for ossimd
#
# This tool links against libossim to communicate with the ossimd daemon

# Support out-of-tree builds
ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(abspath $(O))
else
  KBUILD_OUTPUT := $(CURDIR)/build
endif

# Always redirect to out-of-tree build directory unless we're already there
ifneq ($(KBUILD_OUTPUT), $(CURDIR))
# Only redirect if SRC_DIR is not set (i.e., we're not already redirected)
ifeq ($(SRC_DIR),)

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out sub-make,$(MAKECMDGOALS)): sub-make
	@:

sub-make:
	@mkdir -p $(KBUILD_OUTPUT)
	@$(MAKE) --no-print-directory -C $(KBUILD_OUTPUT) -f $(CURDIR)/Makefile \
		SRC_DIR=$(abspath $(CURDIR)) \
		$(MAKECMDGOALS)

.PHONY: $(PHONY)

# Skip the rest of the makefile when redirecting
skip-makefile := 1
endif
endif

ifeq ($(skip-makefile),)

# Build configuration
SRC_DIR ?= $(CURDIR)
PREFIX ?= /usr/local

CC := gcc
CFLAGS := -std=gnu11 -Wall -O2 -I$(PREFIX)/include
GRPC_LDFLAGS ?= $(shell pkg-config --libs grpc++ protobuf)
LDFLAGS := -L$(PREFIX)/lib -l:libossim.a $(GRPC_LDFLAGS) -lgrpc++_reflection -lstdc++ -lm

TARGET := ossimctl

all: $(TARGET)

$(TARGET): $(SRC_DIR)/main.c
	@echo "Building ossimctl: $@"
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

install: $(TARGET)
	@echo "Installing ossimctl to $(PREFIX)/bin"
	@mkdir -p $(PREFIX)/bin
	install -m 755 $(TARGET) $(PREFIX)/bin/

clean:
	rm -f $(TARGET)

.PHONY: all install clean

endif  # End of ifeq ($(skip-makefile),)
