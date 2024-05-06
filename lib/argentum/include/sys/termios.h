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
  speed_t   c_ispeed;
  speed_t   c_ospeed;
};

#define BRKINT		  0x0001	/* signal interrupt on break */
#define ICRNL		    0x0002
#define IGNBRK		0x0004
#define INLCR		    0x0020
#define IGNCR       0x0008
#define IGNPAR		0x0010
#define INPCK		0x0040	
#define ISTRIP		  0x0080
#define IXOFF		0x0100	
#define IXON		    0x0200	/* enable start/stop output control */
#define PARMRK		  0x0400

#define OPOST		  0x0001
#define ONLCR		  0x0002
#define ONLRET    0x0004
#define OCRNL     0x0008

#define TCSANOW     0x0001
#define TCSADRAIN   0x0002
#define TCSAFLUSH   0x0004

#define TCIFLUSH    1	/* flush accumulated input data */

#define CLOCAL		0x0001	/* ignore modem status lines */
#define CREAD		0x0002	/* enable receiver */
#define CSIZE		    0x000C
#define   CS5 0x0000
#define   CS6 0x0004
#define   CS7 0x0008
#define		CS8	0x000C
#define CSTOPB		0x0010
#define HUPCL	  	0x0020
#define PARENB		0x0040	/* enable parity on output */
#define PARODD		0x0080

#define ECHO        0x0001
#define ECHOE	    	0x0002
#define ECHOK		    0x0004	/* echo KILL */
#define ECHONL      0x0008	/* echo NL */
#define ICANON		0x0010
#define IEXTEN		0x0020	/* enable extended functions */
#define ISIG		0x0040
#define NOFLSH   0x0080
#define ECHOCTL 0x0100
#define ECHOKE  0x0200

int     tcgetattr(int, struct termios *);
int     tcsetattr(int, int, const struct termios *);
speed_t cfgetispeed(const struct termios *);
speed_t cfgetospeed(const struct termios *);
int     cfsetispeed(struct termios *, speed_t);
int     cfsetospeed(struct termios *, speed_t);

#define IXANY		0x0800	/* allow any key to continue ouptut */

#define VEOF                 0	/* cc_c[VEOF] = EOF char (^D) */
#define VEOL                 1	/* cc_c[VEOL] = EOL char (undef) */
#define VERASE               2	/* cc_c[VERASE] = ERASE char (^H) */
#define VINTR                3	/* cc_c[VINTR] = INTR char (DEL) */
#define VKILL                4	/* cc_c[VKILL] = KILL char (^U) */
#define VMIN                 5	/* cc_c[VMIN] = MIN value for timer */
#define VQUIT                6	/* cc_c[VQUIT] = QUIT char (^\) */
#define VTIME                7	/* cc_c[VTIME] = TIME value for timer */
#define VSUSP                8	/* cc_c[VSUSP] = SUSP (^Z, ignored) */
#define VSTART               9	/* cc_c[VSTART] = START char (^S) */
#define VSTOP               10	/* cc_c[VSTOP] = STOP char (^Q) */

#define B0		  0x0000	/* hang up the line */
#define B50		  0x1000	/* 50 baud */
#define B75		  0x2000	/* 75 baud */
#define B110		0x3000	/* 110 baud */
#define B134		0x4000	/* 134.5 baud */
#define B150		0x5000	/* 150 baud */
#define B200		0x6000	/* 200 baud */
#define B300		0x7000	/* 300 baud */
#define B600		0x8000	/* 600 baud */
#define B1200		0x9000	/* 1200 baud */
#define B1800		0xA000	/* 1800 baud */
#define B2400		0xB000	/* 2400 baud */
#define B4800		0xC000	/* 4800 baud */
#define B9600		0xD000	/* 9600 baud */
#define B19200	0xE000	/* 19200 baud */
#define B38400	0xF000	/* 38400 baud */

#ifndef _POSIX_VDISABLE
#define	_POSIX_VDISABLE		((unsigned char)'\377')
#endif /* POSIX_VDISABLE */

#endif
