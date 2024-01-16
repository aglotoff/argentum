#ifndef _SYS_TERMIOS_H
#define _SYS_TERMIOS_H

typedef unsigned char   cc_t;
typedef long            speed_t;
typedef unsigned long   tcflag_t;

#define NCCS    40

struct termios {
  tcflag_t  c_iflag;
  tcflag_t  c_oflag;
  tcflag_t  c_cflag;
  tcflag_t  c_lflag;
  cc_t      c_cc[NCCS];
};

#define TCSANOW     0x0001
#define TCSADRAIN   0x0002
#define TCSAFLUSH   0x0004

#define ECHO        0x0001

int     tcgetattr(int, struct termios *);
int     tcsetattr(int, int, const struct termios *);

#endif
