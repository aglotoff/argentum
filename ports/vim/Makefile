PACKAGE_NAME := vim-9.1.0405
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.gz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(SRC_DIR) 
config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) STRIP=arm-none-argentum-strip DESTDIR=$(SYSROOT) install

$(TARBALL_FILE):
	wget "https://github.com/vim/vim/archive/v9.1.0405/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && vim_cv_memcpy_handles_overlap=set \
	            vim_cv_bcopy_handles_overlap=set \
							vim_cv_memmove_handles_overlap=set \
							vim_cv_stat_ignores_slash=set \
							vim_cv_timer_create=set \
							vim_cv_getcwd_broken=set \
							vim_cv_toupper_broken="set" \
							vim_cv_terminfo="set" \
							vim_cv_tgetent=zero \
							./configure --host=arm-none-argentum \
							            --with-tlib=ncurses \
													--with-features=tiny \
													--disable-gui \
													--without-x \
													--disable-selinux \
													--disable-acl \
													--disable-gpm --prefix=/usr

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
