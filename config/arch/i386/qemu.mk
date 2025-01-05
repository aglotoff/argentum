# QEMU executable
QEMU := qemu-system-x86_64

QEMUOPTS := -m 256
QEMUOPTS += -kernel $(KERNEL)
QEMUOPTS += -drive file=$(OBJ)/fs.img,index=1,media=disk,format=raw
# QEMUOPTS += -nic user,hostfwd=tcp::8080-:80
QEMUOPTS += -serial mon:stdio

qemu: $(KERNEL) $(OBJ)/fs.img
	$(QEMU) $(QEMUOPTS)

qemu-gdb: $(KERNEL)
	$(QEMU) $(QEMUOPTS) -s -S

.PHONY: qemu qemu-gdb
