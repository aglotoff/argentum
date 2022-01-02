# OSDev-PBX-A9

Toy UNIX-like operating system written for RealView Platform Baseboard Explore
for Cortex-A9.

## Building the Cross-Toolchain

Download the following packages:

* https://ftp.gnu.org/gnu/binutils/binutils-2.37.tar.bz2
* https://ftp.gnu.org/gnu/gcc/gcc-11.2.0/gcc-11.2.0.tar.gz

Unpack and build the packages:

```
tar xf binutils-2.37.tar.bz2
cd binutils-2.37
mkdir build
cd build
../configure --prefix=/usr/local \
             --target=arm-none-eabi \
             --disable-werror
make
make install                  # This command may require privileges
cd ..
rm -rf build
cd ..

tar xf gcc-11.2.0.tar.gz
cd gcc-11.2.0
./contrib/download_prerequisites
mkdir build
cd build
../configure --prefix=/usr/local \
             --target=arm-none-eabi \
             --disable-werror \
             --disable-libssp \
             --disable-libmudflap \
             --with-newlib \
             --without-headers \
             --enable-languages=c \
             MAKEINFO=missing
make all-gcc
make install-gcc
make all-target-libgcc        # This command may require privileges
make install-target-libgcc    # This command may require privileges
cd ..
rm -rf build
cd ..
```

## Building and Running the Kernel

To build the OS code, execute `make`.

To run the kernel, install **qemu-system-arm** and execute `make qemu`.
