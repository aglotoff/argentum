PACKAGE_NAME := binutils-2.41
SRC_DIR      := $(PACKAGE_NAME)
BUILD_DIR    := build
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(BUILD_DIR)

config: $(BUILD_DIR)/Makefile

install: all
	make -C $(BUILD_DIR) DESTDIR=$(SYSROOT) install
	rm -fv $(SYSROOT)/usr/lib/lib{bfd,ctf,ctf-nobfd,opcodes,sframe}.la

$(TARBALL_FILE):
	wget "https://ftp.gnu.org/gnu/binutils/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(BUILD_DIR)/Makefile: $(SRC_DIR)
	mkdir -p $(@D)
	cd $(@D) && ../$(SRC_DIR)/configure --prefix=/usr \
	                                    --build=$(shell $(SRC_DIR)/config.guess) \
																			--host=$(HOST) \
																		  --disable-nls \
                                      --disable-werror \
																		  --disable-shared \

clean:
	rm -rf $(SRC_DIR) $(BUILD_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
