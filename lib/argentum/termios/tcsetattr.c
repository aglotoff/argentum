#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

int
tcsetattr(int fildes, int optional_actions, const struct termios *termios_p)
{
	switch (optional_actions) {
	case TCSANOW:
		return (ioctl(fildes, TIOCSETA, termios_p));
	case TCSADRAIN:
		return (ioctl(fildes, TIOCSETAW, termios_p));
	case TCSAFLUSH:
		return (ioctl(fildes, TIOCSETAF, termios_p));
	default:
		errno = EINVAL;
		return -1;
	}
}
