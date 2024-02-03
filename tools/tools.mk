tools-libstdc++-v3: all-lib
	make -C tools/gcc install-target-libstdc++-v3

tools-%:
	make -C tools/$* install

tools: tools-binutils tools-gcc
