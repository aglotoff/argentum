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

## Resources

### ARM architecture

  * [ARM Category](https://wiki.osdev.org/Category:ARM) in OSDev Wiki
  * [ARM System Developer's Guide](https://www.amazon.in/ARM-System-Developers-Guide-Architecture/dp/1558608745), Andrew Sloss, Dominic Symes, Chris Wright, 2004
  * [Embedded and Real-Time Operating Systems](https://link.springer.com/book/10.1007/978-3-319-51517-5), K.C. Wang, 2017
  * [ARM GCC Inline Assembler Cookbook](http://www.ethernut.de/en/documents/arm-inline-asm.html)

### Hardware Programming

* Generic Interrupt Controller - `kernel/gic.c`
  * [Cortex-A9 MPCore Technical Reference Manual](https://developer.arm.com/documentation/ddi0407/g/DDI0407G_cortex_a9_mpcore_r3p0_trm.pdf)
* Keyboard - `kernel/kbd.c`
  * [PS/2 Keyboard](https://wiki.osdev.org/PS/2_Keyboard) in OSDev Wiki
  * [ARM PrimeCell PS2 Keyboard/Mouse Interface (PL050) Technical Reference Manual](https://developer.arm.com/documentation/ddi0143/latest)
* LCD - `kernel/lcd.c`
  * [PrimeCell Color LCD Controller (PL111) Technical Reference Manual](https://developer.arm.com/documentation/ddi0293/c)
* Maxim DS1338 RTC - `kernel/rtc.c`
  * [DS1338 data sheet](https://datasheets.maximintegrated.com/en/ds/DS1338-DS1338Z.pdf)
  * [Using the I2C Bus](https://www.robot-electronics.co.uk/i2c-tutorial)
* SD card - `kernel/sd.c`
  * [Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/pdf?p=Part1_Physical_Layer_Simplified_Specification_Ver8.00.jpg&f=Part1_Physical_Layer_Simplified_Specification_Ver8.00.pdf&e=EN_SS1_8)
  * [ARM PrimeCell Multimedia Card Interface (PL180) Technical Reference Manual](https://developer.arm.com/documentation/ddi0172/a)
* UART - `kernel/uart.c`
  * [PrimeCell UART (PL011) Technical Reference Manual](https://developer.arm.com/documentation/ddi0183/g/)
