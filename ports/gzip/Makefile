PACKAGE_NAME := gzip-1.12
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
PATCH_FILE   := $(PACKAGE_NAME).patch

CFLAGS := -O2 -mcpu=cortex-a9 -mhard-float

SYSROOT := $(HOME)/osdev/sysroot

all: config
	make -C $(SRC_DIR) CFLAGS="$(CFLAGS)"

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) CFLAGS="$(CFLAGS)" DESTDIR=$(SYSROOT) install
	ln -f $(SYSROOT)/usr/bin/dash $(SYSROOT)/bin/sh

$(TARBALL_NAME):
	wget "https://ftp.gnu.org/gnu/binutils/${TARBALL_NAME}"

$(SRC_DIR): $(TARBALL_NAME)
	tar xf $<

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && \
	  CFLAGS="$(CFLAGS)" ./configure --host=arm-none-osdev \
	                                 --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
