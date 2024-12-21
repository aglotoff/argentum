# QEMU executable
QEMU := qemu-system-i386

QEMUOPTS := -m 256
QEMUOPTS += -kernel $(KERNEL)
# QEMUOPTS += -drive if=sd,format=raw,file=$(OBJ)/fs.img
# QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

qemu: $(KERNEL)
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL)
	$(QEMU) $(QEMUOPTS) -s -S

.PHONY: qemu qemu-gdb
