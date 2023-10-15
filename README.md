# Argentum

Toy operating system written for RealView Platform Baseboard Explore
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
