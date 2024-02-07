#ifndef _SYS_IOCCOM_H
#define _SYS_IOCCOM_H

#define _IOC_DIR_SHIFT  30
#define _IOC_DIR_MASK   0x3
#define _IOC_LEN_SHIFT  16
#define _IOC_LEN_MASK   0x3FFF
#define _IOC_GRP_SHIFT  8
#define _IOC_GRP_MASK   0xFF
#define _IOC_CMD_SHIFT  0
#define _IOC_CMD_MASK   0xFF

#define _IOC_DIR_NONE   0
#define _IOC_DIR_IN     1
#define _IOC_DIR_OUT    2
#define _IOC_DIR_INOUT  3

#define _IOC(dir, len, grp, cmd)  (             \
  (((dir) & _IOC_DIR_MASK) << _IOC_DIR_SHIFT) | \
  (((len) & _IOC_LEN_MASK) << _IOC_LEN_SHIFT) | \
  (((grp) & _IOC_GRP_MASK) << _IOC_GRP_SHIFT) | \
  (((cmd) & _IOC_CMD_MASK) << _IOC_CMD_SHIFT)   \
)

#define _IO(grp, cmd, type)   _IOC(_IOC_DIR_NONE,  sizeof(type), (grp), (cmd))
#define _IOR(grp, cmd, type)  _IOC(_IOC_DIR_IN,    sizeof(type), (grp), (cmd))
#define _IOW(grp, cmd, type)  _IOC(_IOC_DIR_OUT,   sizeof(type), (grp), (cmd))
#define _IOWR(grp, cmd, type) _IOC(_IOC_DIR_INOUT, sizeof(type), (grp), (cmd))

#endif /* !_SYS_IOCCOM_H */
