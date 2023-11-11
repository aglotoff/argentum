# QEMU executable
QEMU := qemu-system-arm

ifndef CPUS
  CPUS := 2
endif

# Qemu options
QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS) -nographic
QEMUOPTS += -kernel $(KERNEL).bin
QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

$(KERNEL).bin: $(KERNEL)
	$(V)$(OBJCOPY) -O binary $^ $@

qemu: $(KERNEL).bin
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL).bin
	$(QEMU) $(QEMUOPTS) -s -S
