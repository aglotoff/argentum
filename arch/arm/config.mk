# The recommended target for cross compiling is "arm-none-eabi-". To use the
# host compiler, comment the following line out
TOOLPREFIX := arm-none-eabi-

ARCH_CFLAGS := -mcpu=cortex-a9 -mapcs-frame -fno-omit-frame-pointer
ARCH_CFLAGS += -mhard-float -mfpu=vfp

ARCH_LDFLAGS := -z max-page-size=0x1000
