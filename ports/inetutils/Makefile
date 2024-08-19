PACKAGE_NAME := inetutils-2.5
SRC_DIR      := $(PACKAGE_NAME)
TARBALL_NAME := $(PACKAGE_NAME).tar.xz
TARBALL_FILE := ../../tarballs/$(TARBALL_NAME)
PATCH_FILE   := $(PACKAGE_NAME).patch

SYSROOT := $(HOME)/argentum/sysroot

all: config
	make -C $(SRC_DIR)

config: $(SRC_DIR)/Makefile

install: all
	make -C $(SRC_DIR) DESTDIR=$(SYSROOT) install
	chmod +x $(SYSROOT)/usr/bin/ping
	chmod +x $(SYSROOT)/usr/bin/traceroute

$(TARBALL_FILE):
	wget "https://ftp.gnu.org/gnu/inetutils/${TARBALL_NAME}" -O $@

$(SRC_DIR): $(TARBALL_FILE)
	tar xf $<
	cd $@ && patch -p1 -i ../$(PATCH_FILE)

$(SRC_DIR)/Makefile: | $(SRC_DIR)
	cd $(@D) && sed -i 's/def HAVE_TERMCAP_TGETENT/ 1/' telnet/telnet.c
	cd $(@D) && ./configure --host=arm-none-argentum \
													--prefix=/usr \
													--bindir=/usr/bin \
													--localstatedir=/var \
													--disable-logger     \
													--disable-whois      \
													--disable-rcp        \
													--disable-rexec      \
													--disable-rlogin     \
													--disable-rsh        \
													--disable-servers    \
													--disable-ifconfig

clean:
	rm -rf $(SRC_DIR)

distclean: clean
	rm -rf $(TARBALL_NAME)

.PHONY: all config install clean distclean
