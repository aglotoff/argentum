#ifndef __KERNEL_MCI_H__
#define __KERNEL_MCI_H__

struct Buf;

void mci_init(void);
void mci_intr(void);
void mci_request(struct Buf *);

#endif  // !__KERNEL_MCI_H__
