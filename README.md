# Codename Argentum

Toy UNIX-like operating system written for RealView Platform Baseboard Explore
for Cortex-A9.

## Prerequisites

In order to build and run the OS, you'll need the following packages:

* `arm-none-eabi-binutils`
* `arm-none-eabi-gcc`
* `qemu-system-arm`
* `arm-none-eabi-gdb` (optional, required to run the OS in debug mode)

## Building and Running the Kernel

To build the OS code, execute `make`.

To run the kernel, execute `make qemu`.

## Resources

### General OS Development Information

* [OSDev Wiki](https://wiki.osdev.org/Expanded_Main_Page)
* [xv6, a simple Unix-like teaching OS](https://pdos.csail.mit.edu/6.828/2018/xv6.html) by MIT

### C Library

The C Library code is heavily based on the following reference implementation:
* [The Standard C Library](https://www.amazon.com/Standard-C-Library-P-J-Plauger/dp/0131315099), P.J. Plauger, 1992

### ARM architecture

* [ARM Category](https://wiki.osdev.org/Category:ARM) in OSDev Wiki
* [ARM System Developer's Guide](https://www.amazon.in/ARM-System-Developers-Guide-Architecture/dp/1558608745), Andrew Sloss, Dominic Symes, Chris Wright, 2004
* [Embedded and Real-Time Operating Systems](https://link.springer.com/book/10.1007/978-3-319-51517-5), K.C. Wang, 2017
* [ARM GCC Inline Assembler Cookbook](http://www.ethernut.de/en/documents/arm-inline-asm.html)

### Hardware Programming

* ARM RealView Platform Baseboard Explore for Cortex-A9
  * [RealView Platform Baseboard Explore for Cortex-A9 User Guide](https://developer.arm.com/documentation/dui0440/b/)
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
  * [Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/pdf/?p=Part1_Physical_Layer_Simplified_Specification_Ver1.10.jpg&f=Part1_Physical_Layer_Simplified_Specification_Ver1.10.pdf&e=EN_P1110)
  * [ARM PrimeCell Multimedia Card Interface (PL180) Technical Reference Manual](https://developer.arm.com/documentation/ddi0172/a)
* UART - `kernel/uart.c`
  * [PrimeCell UART (PL011) Technical Reference Manual](https://developer.arm.com/documentation/ddi0183/g/)
* Ethernet - `kernel/eth.c`
  * [LAN9118 Datasheet](http://ww1.microchip.com/downloads/en/DeviceDoc/00002266B.pdf)
