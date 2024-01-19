PACKAGE_NAME    := gcc-13.2.0
SRC_DIR         := $(PACKAGE_NAME)
BUILD_DIR       := build
TARBALL_NAME    := $(PACKAGE_NAME).tar.xz
GCC_PATCH_FILE  := $(PACKAGE_NAME).patch
GMP_PATCH_FILE  := gmp-6.2.1.patch
MPFR_PATCH_FILE := mpfr-4.1.0.patch
MPC_PATCH_FILE  := mpc-1.2.1.patch
ISL_PATCH_FILE  := isl-0.24.patch

SYSROOT := $(HOME)/osdev/sysroot

all: config
	make -C $(BUILD_DIR)

config: $(BUILD_DIR)/Makefile

install: all
	make -C $(BUILD_DIR) DESTDIR=$(SYSROOT) install
	make -C $(BUILD_DIR) DESTDIR=$(SYSROOT) install-strip

$(TARBALL_NAME):
	wget "https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/${TARBALL_NAME}"

$(SRC_DIR): $(TARBALL_NAME)
	tar xf $<
	cd $@ && patch -p1 -i ../$(GCC_PATCH_FILE) && ./contrib/download_prerequisites
	cd $@/gmp && patch -p1 -i ../../$(GMP_PATCH_FILE)
	cd $@/mpfr && patch -p1 -i ../../$(MPFR_PATCH_FILE)
	cd $@/mpc && patch -p1 -i ../../$(MPC_PATCH_FILE)
	cd $@/isl && patch -p1 -i ../../$(ISL_PATCH_FILE)

$(BUILD_DIR)/Makefile: $(SRC_DIR)
	mkdir -p $(@D)
	cd $(@D) && ../$(SRC_DIR)/configure --prefix=/usr \
	                                    --build=$(shell $(SRC_DIR)/config.guess) \
																			--host=arm-none-osdev \
																			--target=arm-none-osdev \
																		  --with-build-sysroot=$(SYSROOT) \
																			--with-cpu=cortex-a9 \
																			--with-float=hard \
																			--with-fpu=vfp \
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
																			--disable-tls

clean:
	rm -rf $(SRC_DIR) $(BUILD_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
