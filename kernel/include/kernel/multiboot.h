#ifndef AG_KERNEL_MULTIBOOT_H
#define AG_KERNEL_MULTIBOOT_H

#define MULTIBOOT_HEADER_MAGIC  0x1BADB002

/* Align all boot modules on page (4KB) boundaries */
#define MULTIBOOT_PAGE_ALIGN                    (1 << 0)
/* Pass information on available memory */
#define MULTIBOOT_MEMORY_INFO                   (1 << 1)

#endif  // !AG_KERNEL_MULTIBOOT_H
