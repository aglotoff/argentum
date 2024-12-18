# QEMU executable
QEMU := qemu-system-arm

#QEMUOPTS := -M realview-pb-a8 -m 256
QEMUOPTS := -M realview-pbx-a9 -m 256 -smp $(CPUS)
QEMUOPTS += -kernel $(KERNEL).bin
QEMUOPTS += -drive if=sd,format=raw,file=$(OBJ)/fs.img
QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

$(KERNEL).bin: $(KERNEL)
	$(V)$(OBJCOPY) -O binary $^ $@

qemu: $(OBJ)/fs.img $(KERNEL).bin
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(OBJ)/fs.img $(KERNEL).bin
	$(QEMU) $(QEMUOPTS) -s -S

.PHONY: qemu qemu-gdb
