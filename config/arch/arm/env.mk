# Cross-compiler toolchain
#
# The recommended target for the cross-compiler toolchain is "arm-none-eabi-".
# If you want to use the host tools (i.e. binutils, gcc, gdb, etc.), comment
# this line out.
TOOLPREFIX := arm-none-argentum-

# BASE_FLAGS += -mcpu=cortex-a9 -mhard-float -mfpu=vfp
