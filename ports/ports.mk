ports-%:
	make -C ports/$* install

clean-ports:
	make -C ports/bash clean
	make -C ports/binutils clean
	make -C ports/coreutils clean
	make -C ports/dash clean
	make -C ports/diffutils clean
	make -C ports/file clean
	make -C ports/findutils clean
	make -C ports/gawk clean
	make -C ports/gcc clean
	make -C ports/grep clean
	make -C ports/gzip clean
	make -C ports/iana-etc clean
	make -C ports/inetutils clean
	make -C ports/less clean
	make -C ports/m4 clean
	make -C ports/make clean
	make -C ports/ncurses clean
	make -C ports/sed clean
	make -C ports/vim clean
