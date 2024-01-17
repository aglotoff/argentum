PACKAGE_NAME := binutils-2.41
SRC_DIR      := $(PACKAGE_NAME)
BUILD_DIR    := build
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/osdev/sysroot

all: config
	make -C $(BUILD_DIR)

config: $(BUILD_DIR)/Makefile

install: all
	make -C $(BUILD_DIR) DESTDIR=$(SYSROOT) install
	ln -f $(SYSROOT)/usr/bin/dash $(SYSROOT)/bin/sh

$(TARBALL_NAME):
	wget "https://ftp.gnu.org/gnu/binutils/${TARBALL_NAME}"

$(SRC_DIR): $(TARBALL_NAME)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(BUILD_DIR)/Makefile: $(SRC_DIR)
	mkdir -p $(@D)
	cd $(@D) && \
	 ../$(SRC_DIR)/configure --target=arm-none-osdev \
	                                             --host=arm-none-osdev \
	                                             --prefix=/usr \
																							 --disable-nls \
                                               --disable-werror

clean:
	rm -rf $(SRC_DIR) $(BUILD_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
