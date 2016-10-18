/****************************************************************************
** serial.c ****************************************************************
****************************************************************************
*
* common routines for hardware that uses the standard serial port driver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

/**
 * @file serial.c
 * @author Christoph Bartelmus
 * @brief Implements serial.h
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef LIRC_LOCKDIR
#define LIRC_LOCKDIR "/var/lock/lockdev"
#endif


#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#if defined __linux__
#include <linux/serial.h>	/* for 'struct serial_struct' to set custom
				 * baudrates */
#endif

#include "lirc/lirc_log.h"
#include "lirc/curl_poll.h"


static const logchannel_t logchannel = LOG_LIB;

int tty_reset(int fd)
{
	struct termios options;

	if (tcgetattr(fd, &options) == -1) {
		log_trace("tty_reset(): tcgetattr() failed");
		log_perror_debug("tty_reset()");
		return 0;
	}
	cfmakeraw(&options);
	if (tcsetattr(fd, TCSAFLUSH, &options) == -1) {
		log_trace("tty_reset(): tcsetattr() failed");
		log_perror_debug("tty_reset()");
		return 0;
	}
	return 1;
}

int tty_setrtscts(int fd, int enable)
{
	struct termios options;

	if (tcgetattr(fd, &options) == -1) {
		log_trace("%s: tcgetattr() failed", __func__);
		log_perror_debug(__func__);
		return 0;
	}
	if (enable)
		options.c_cflag |= CRTSCTS;
	else
		options.c_cflag &= ~CRTSCTS;
	if (tcsetattr(fd, TCSAFLUSH, &options) == -1) {
		log_trace("%s: tcsetattr() failed", __func__);
		log_perror_debug(__func__);
		return 0;
	}
	return 1;
}

int tty_setdtr(int fd, int enable)
{
	int cmd, sts;

	if (ioctl(fd, TIOCMGET, &sts) < 0) {
		log_trace("%s: ioctl(TIOCMGET) failed", __func__);
		log_perror_debug(__func__);
		return 0;
	}
	if (((sts & TIOCM_DTR) == 0) && enable) {
		log_trace("%s: 0->1", __func__);
	} else if ((!enable) && (sts & TIOCM_DTR)) {
		log_trace("%s: 1->0", __func__);
	}
	if (enable)
		cmd = TIOCMBIS;
	else
		cmd = TIOCMBIC;
	sts = TIOCM_DTR;
	if (ioctl(fd, cmd, &sts) < 0) {
		log_trace("%s: ioctl(TIOCMBI(S|C)) failed", __func__);
		log_perror_debug(__func__);
		return 0;
	}
	return 1;
}

int tty_setbaud(int fd, int baud)
{
	struct termios options;
	int speed;

#if defined __linux__
	int use_custom_divisor = 0;
	struct serial_struct serinfo;
#endif

	switch (baud) {
	case 300:
		speed = B300;
		break;
	case 1200:
		speed = B1200;
		break;
	case 2400:
		speed = B2400;
		break;
	case 4800:
		speed = B4800;
		break;
	case 9600:
		speed = B9600;
		break;
	case 19200:
		speed = B19200;
		break;
	case 38400:
		speed = B38400;
		break;
	case 57600:
		speed = B57600;
		break;
	case 115200:
		speed = B115200;
		break;
#ifdef B230400
	case 230400:
		speed = B230400;
		break;
#endif
#ifdef B460800
	case 460800:
		speed = B460800;
		break;
#endif
#ifdef B500000
	case 500000:
		speed = B500000;
		break;
#endif
#ifdef B576000
	case 576000:
		speed = B576000;
		break;
#endif
#ifdef B921600
	case 921600:
		speed = B921600;
		break;
#endif
#ifdef B1000000
	case 1000000:
		speed = B1000000;
		break;
#endif
#ifdef B1152000
	case 1152000:
		speed = B1152000;
		break;
#endif
#ifdef B1500000
	case 1500000:
		speed = B1500000;
		break;
#endif
#ifdef B2000000
	case 2000000:
		speed = B2000000;
		break;
#endif
#ifdef B2500000
	case 2500000:
		speed = B2500000;
		break;
#endif
#ifdef B3000000
	case 3000000:
		speed = B3000000;
		break;
#endif
#ifdef B3500000
	case 3500000:
		speed = B3500000;
		break;
#endif
#ifdef B4000000
	case 4000000:
		speed = B4000000;
		break;
#endif
	default:
#if defined __linux__
		speed = B38400;
		use_custom_divisor = 1;
		break;
#else
		log_trace("tty_setbaud(): bad baud rate %d", baud);
		return 0;
#endif
	}
	if (tcgetattr(fd, &options) == -1) {
		log_trace("tty_setbaud(): tcgetattr() failed");
		log_perror_debug("tty_setbaud()");
		return 0;
	}
	(void)cfsetispeed(&options, speed);
	(void)cfsetospeed(&options, speed);
	if (tcsetattr(fd, TCSAFLUSH, &options) == -1) {
		log_trace("tty_setbaud(): tcsetattr() failed");
		log_perror_debug("tty_setbaud()");
		return 0;
	}
#if defined __linux__
	if (use_custom_divisor) {
		if (ioctl(fd, TIOCGSERIAL, &serinfo) < 0) {
			log_trace("tty_setbaud(): TIOCGSERIAL failed");
			log_perror_debug("tty_setbaud()");
			return 0;
		}
		serinfo.flags &= ~ASYNC_SPD_MASK;
		serinfo.flags |= ASYNC_SPD_CUST;
		serinfo.custom_divisor = serinfo.baud_base / baud;
		if (ioctl(fd, TIOCSSERIAL, &serinfo) < 0) {
			log_trace("tty_setbaud(): TIOCSSERIAL failed");
			log_perror_debug("tty_setbaud()");
			return 0;
		}
	}
#endif
	return 1;
}

int tty_setcsize(int fd, int csize)
{
	struct termios options;
	int size;

	switch (csize) {
	case 5:
		size = CS5;
		break;
	case 6:
		size = CS6;
		break;
	case 7:
		size = CS7;
		break;
	case 8:
		size = CS8;
		break;
	default:
		log_trace("tty_setcsize(): bad csize rate %d", csize);
		return 0;
	}
	if (tcgetattr(fd, &options) == -1) {
		log_trace("tty_setcsize(): tcgetattr() failed");
		log_perror_debug("tty_setcsize()");
		return 0;
	}
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= size;
	if (tcsetattr(fd, TCSAFLUSH, &options) == -1) {
		log_trace("tty_setcsize(): tcsetattr() failed");
		log_perror_debug("tty_setcsize()");
		return 0;
	}
	return 1;
}

int tty_create_lock(const char* name)
{
	char filename[FILENAME_MAX + 1];
	char symlink[FILENAME_MAX + 1];
	char cwd[FILENAME_MAX + 1];
	const char* last;
	const char* s;
	char id[10 + 1 + 1];
	int lock;
	int len;

	strcpy(filename, LIRC_LOCKDIR "/LCK..");

	last = strrchr(name, '/');
	if (last != NULL)
		s = last + 1;
	else
		s = name;

	if (strlen(filename) + strlen(s) > FILENAME_MAX) {
		log_error("invalid filename \"%s%s\"", filename, s);
		return 0;
	}
	strcat(filename, s);

tty_create_lock_retry:
	len = snprintf(id, 10 + 1 + 1, "%10d\n", getpid());
	if (len == -1) {
		log_error("invalid pid \"%d\"", getpid());
		return 0;
	}
	lock = open(filename, O_CREAT | O_EXCL | O_WRONLY, 0644);
	if (lock == -1) {
		log_perror_err("could not create lock file \"%s\"", filename);
		lock = open(filename, O_RDONLY);
		if (lock != -1) {
			pid_t otherpid;

			id[10 + 1] = 0;
			if (read(lock, id, 10 + 1) == 10 + 1 && read(lock, id, 1) == 0
			    && sscanf(id, "%d\n", &otherpid) > 0) {
				if (kill(otherpid, 0) == -1 && errno == ESRCH) {
					log_warn("detected stale lockfile %s", filename);
					close(lock);
					if (unlink(filename) != -1) {
						log_warn("stale lockfile removed");
						goto tty_create_lock_retry;
					} else {
						log_perror_err(
							  "could not remove stale lockfile");
					}
					return 0;
				}
				log_error("%s is locked by PID %d", name, otherpid);
			} else {
				log_error("invalid lockfile %s encountered", filename);
			}
			close(lock);
		}
		return 0;
	}
	if (write(lock, id, len) != len) {
		log_perror_err("could not write pid to lock file");
		close(lock);
		if (unlink(filename) == -1)
			log_perror_err("could not delete file \"%s\"", filename);
		/* FALLTHROUGH */
		return 0;
	}
	if (close(lock) == -1) {
		log_perror_err("could not close lock file");
		if (unlink(filename) == -1)
			log_perror_err("could not delete file \"%s\"", filename);
		/* FALLTHROUGH */
		return 0;
	}

	len = readlink(name, symlink, FILENAME_MAX);
	if (len == -1) {
		if (errno != EINVAL) {  /* symlink */
			log_perror_err("readlink() failed for \"%s\"", name);
			if (unlink(filename) == -1) {
				log_perror_err("could not delete file \"%s\"",
					       filename);
				/* FALLTHROUGH */
			}
			return 0;
		}
	} else {
		symlink[len] = 0;

		if (last) {
			char dirname[FILENAME_MAX + 1];

			if (getcwd(cwd, FILENAME_MAX) == NULL) {
				log_perror_err("getcwd() failed");
				if (unlink(filename) == -1) {
					log_perror_err(
						  "could not delete file \"%s\"",
						  filename);
					/* FALLTHROUGH */
				}
				return 0;
			}

			strcpy(dirname, name);
			dirname[strlen(name) - strlen(last)] = 0;
			if (chdir(dirname) == -1) {
				log_perror_err(
					  "chdir() to \"%s\" failed", dirname);
				if (unlink(filename) == -1) {
					log_perror_err(
						  "could not delete file \"%s\"",
						  filename);
					/* FALLTHROUGH */
				}
				return 0;
			}
		}
		if (tty_create_lock(symlink) == -1) {
			if (unlink(filename) == -1) {
				log_perror_err(
					  "could not delete file \"%s\"", filename);
				/* FALLTHROUGH */
			}
			return 0;
		}
		if (last) {
			if (chdir(cwd) == -1) {
				log_perror_err("chdir() to \"%s\" failed", cwd);
				if (unlink(filename) == -1) {
					log_perror_err(
						  "could not delete file \"%s\"",
						  filename);
					/* FALLTHROUGH */
				}
				return 0;
			}
		}
	}
	return 1;
}

int tty_delete_lock(void)
{
	DIR* dp;
	struct dirent* ep;
	int lock;
	int len;
	char id[20] = { '\0' };
	char filename[FILENAME_MAX + 1];
	long pid;
	int retval = 1;

	dp = opendir(LIRC_LOCKDIR);
	if (dp != NULL) {
		while ((ep = readdir(dp))) {
			if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) {
				retval = 0;
				continue;
			}
			strcpy(filename, LIRC_LOCKDIR "/");
			if (strlen(filename) + strlen(ep->d_name) > FILENAME_MAX) {
				retval = 0;
				continue;
			}
			strcat(filename, ep->d_name);
			if (strstr(filename, "LCK..") == NULL) {
				log_debug("Ignoring non-LCK.. logfile %s",
					  filename);
				retval = 0;
				continue;
			}
			lock = open(filename, O_RDONLY);
			if (lock == -1) {
				retval = 0;
				continue;
			}
			len = read(lock, id, sizeof(id) - 1);
			close(lock);
			if (len <= 0) {
				retval = 0;
				continue;
			}
			pid = strtol(id, NULL, 10);
			if (pid == LONG_MIN || pid == LONG_MAX || pid == 0) {
				log_debug("Can't parse lockfile %s (ignored)",
					  filename);
				retval = 0;
				continue;
			}
			if (pid == getpid()) {
				if (unlink(filename) == -1) {
					log_perror_err(
						  "could not delete file \"%s\"",
						  filename);
					retval = 0;
					continue;
				}
			}
		}
		closedir(dp);
	} else {
		log_error("could not open directory \"" LIRC_LOCKDIR "\"");
		return 0;
	}
	return retval;
}

int tty_set(int fd, int rts, int dtr)
{
	int mask;

	mask = rts ? TIOCM_RTS : 0;
	mask |= dtr ? TIOCM_DTR : 0;
	if (ioctl(fd, TIOCMBIS, &mask) == -1) {
		log_trace("tty_set(): ioctl() failed");
		log_perror_warn("tty_set()");
		return 0;
	}
	return 1;
}

int tty_clear(int fd, int rts, int dtr)
{
	int mask;

	mask = rts ? TIOCM_RTS : 0;
	mask |= dtr ? TIOCM_DTR : 0;
	if (ioctl(fd, TIOCMBIC, &mask) == -1) {
		log_perror_debug("tty_clear()");
		log_trace("tty_clear(): ioctl() failed");
		return 0;
	}
	return 1;
}

int tty_write(int fd, char byte)
{
	if (write(fd, &byte, 1) != 1) {
		log_trace("tty_write(): write() failed");
		log_perror_debug("tty_write()");
		return -1;
	}
	/* wait until the stop bit of Control Byte is sent
	 * (for 9600 baud rate, it takes about 100 msec */
	usleep(100 * 1000);

	/* we don't wait because tcdrain() does this for us */
	/* tcdrain(fd); */
	/* FIXME! but unfortunately this does not seem to be
	 * implemented in 2.0.x kernels ... */
	return 1;
}

int tty_read(int fd, char* byte)
{
	struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};
	int ret;

	ret = curl_poll(&pfd, 1, 1000); /* 1 second timeout. */
	if (ret == 0) {
		log_error("tty_read(): timeout");
		return -1;      /* received nothing, bad */
	} else if (ret != 1) {
		log_perror_debug("tty_read(): curl_poll() failed");
		return -1;
	}
	if (read(fd, byte, 1) != 1) {
		log_perror_debug("tty_read(): read() failed");
		return -1;
	}
	return 1;
}

int tty_write_echo(int fd, char byte)
{
	char reply;

	if (tty_write(fd, byte) == -1)
		return -1;
	if (tty_read(fd, &reply) == -1)
		return -1;
	log_trace("sent: A%u D%01x reply: A%u D%01x", (((unsigned int)(unsigned char)byte) & 0xf0) >> 4,
		  ((unsigned int)(unsigned char)byte) & 0x0f, (((unsigned int)(unsigned char)reply) & 0xf0) >> 4,
		  ((unsigned int)(unsigned char)reply) & 0x0f);
	if (byte != reply)
		log_error("Command mismatch.");
	return 1;
}
