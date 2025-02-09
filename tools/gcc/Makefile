PACKAGE_NAME    := gcc-13.2.0
SRC_DIR         := $(PACKAGE_NAME)
BUILD_DIR       := build
TARBALL_NAME    := $(PACKAGE_NAME).tar.xz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := ../../ports/gcc/$(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(BUILD_DIR) all-gcc all-target-libgcc

all-target-libstdc++-v3: config
	make -C $(BUILD_DIR) all-target-libstdc++-v3

config: $(BUILD_DIR)/Makefile

install: all
	sudo make -C $(BUILD_DIR) install-gcc install-target-libgcc

install-target-libstdc++-v3: all-target-libstdc++-v3
	sudo make -C $(BUILD_DIR) install-target-libstdc++-v3

$(TARBALL_FILE):
	wget "https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE) && ./contrib/download_prerequisites

EXTRA_OPTIONS :=

ifeq ($(HOST), arm-none-argentum)
EXTRA_OPTIONS := --with-cpu=cortex-a9 --with-float=hard --with-fpu=vfp
endif

$(BUILD_DIR)/Makefile: $(SRC_DIR)
	mkdir -p $(@D)
	cd $(@D) && ../$(SRC_DIR)/configure --prefix=/usr/local \
																			--target=$(HOST) \
																		  --with-sysroot=$(SYSROOT) \
																			--with-newlib \
																			--enable-languages=c,c++ \
																			--disable-nls \
																			--disable-werror \
																			--disable-multilib \
																			--disable-decimal-float \
																			--disable-libffi \
																			--disable-libgomp \
																			--disable-libmudflap \
																			--disable-libquadmath  \
																			--disable-libssp \
																			--disable-libstdcxx-pch \
																			--disable-shared \
																			--disable-threads \
																			--disable-tls \
																			$(EXTRA_OPTIONS)

clean:
	rm -rf $(SRC_DIR) $(BUILD_DIR)

distclean: clean
	rm -rf $(TARBALL_FILE)

.PHONY: all config install clean distclean
