/****************************************************************************
 ** hw_devinput.c ***********************************************************
 ****************************************************************************
 *
 * receive keycodes input via /dev/input/...
 *
 * Copyright (C) 2002 Oliver Endriss <o.endriss@gmx.de>
 *
 * Distribute under GPL version 2 or later.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>

#include <linux/input.h>
#include <linux/uinput.h>

#ifndef EV_SYN
/* previous name */
#define EV_SYN EV_RST
#endif

#include "lirc_driver.h"

/* from evtest.c - Copyright (c) 1999-2000 Vojtech Pavlik */
#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)
/* NBITS was defined in linux/uinput.h */
#undef NBITS
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

static int devinput_init();
static int devinput_init_fwd();
static int devinput_deinit(void);
static int devinput_decode(struct ir_remote *remote, struct decode_ctx_t* ctx);
static char *devinput_rec(struct ir_remote *remotes);

enum locate_type {
	locate_by_name,
	locate_by_phys,
};

const struct driver hw_devinput = {
	.name		=	"devinput",
	.device		=	"/dev/input/event0",
	.features	=	LIRC_CAN_REC_LIRCCODE,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	64,
	.init_func	=	devinput_init_fwd,
	.deinit_func	=	devinput_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,
	.rec_func	=	devinput_rec,
	.decode_func	=	devinput_decode,
	.drvctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};


const struct driver* hardwares[] = { &hw_devinput, (const struct driver*)NULL };

static ir_code code;
static ir_code code_compat;
static int exclusive = 0;
static int uinputfd = -1;
static struct timeval start, end, last;

enum {
	RPT_UNKNOWN = -1,
	RPT_NO = 0,
	RPT_YES = 1,
};

static int repeat_state = RPT_UNKNOWN;

static int is_rc(const char* s)
{
	if (s == NULL)
		return -1;
	return (s[0] == 'r' && s[1] == 'c' && s[2] >='0' && s[2] <= '9');
}


/*
 * Given a directory in /sys/class/rc, check if it contains
 * a file called device. If so, write "lirc" to the protocols
 * file in same directory.
 *
 * rc_dir: directory specification like rc0, rc1, etc.
 * device: Device given to lirc,  like 'lirc0' (or /dev/lirc0).
 * Returns: 0 if OK, else -1.
 *
 */
static int visit_rc(const char* rc_dir, const char* device)
{
	char path[64];
	int fd;

	snprintf(path, sizeof(path), "/sys/class/rc/%s", rc_dir);
	if (access(path, F_OK) != 0){
		logprintf(LIRC_NOTICE, "Cannot open rc directory: %s", path);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/rc/%s/%s", rc_dir, device);
	if (access(path, F_OK) != 0) {
		logprintf(LIRC_DEBUG, "No device found: %s", path);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/rc/%s/protocols", rc_dir);
	fd = open(path, O_WRONLY);
	if ( fd < 0 ){
		logprintf(LIRC_DEBUG, "Cannot open protocol file: %s", path);
		return -1;
	}
 	chk_write(fd, "lirc\n" , 5);
	logprintf(LIRC_NOTICE, "'lirc' written to protocols file %s", path);
	close(fd);
	return 0;
}

/*
 * Try to set the 'lirc' protocol for the device  we are using. Returns
 * 0 on success for at least one device, otherwise -1.
 */
static int set_rc_protocol(const char* device)
{
	struct dirent* ent;
	DIR* dir;
	int r = -1;

	if (strrchr(device, '/') != NULL)
		device = strrchr(device, '/') + 1;
	if ((dir = opendir("/sys/class/rc")) == NULL){
		logprintf(LIRC_NOTICE, "Cannot open /sys/class/rc\n");
		return -1;
	}
	while ((ent = readdir(dir)) != NULL) {
		if (!is_rc(ent->d_name))
			continue;
		if (visit_rc(ent->d_name, device) == 0)
			r = 0;
	}
	closedir(dir);
	return r;
}


static int setup_uinputfd(const char *name, int source)
{
	int fd;
	int key;
	struct uinput_user_dev dev;
	long events[NBITS(EV_MAX)];
	long bits[NBITS(KEY_MAX)];

	if (ioctl(source, EVIOCGBIT(0, EV_MAX), events) == -1) {
		return -1;
	}
	if (!test_bit(EV_REL, events) && !test_bit(EV_ABS, events)) {
		/* no move events, don't forward anything */
		return -1;
	}
	fd = open("/dev/input/uinput", O_RDWR);
	if (fd == -1) {
		fd = open("/dev/uinput", O_RDWR);
		if (fd == -1) {
			fd = open("/dev/misc/uinput", O_RDWR);
			if (fd == -1) {
				logperror(LIRC_WARNING, "could not open %s\n", "uinput");
				return -1;
			}
		}
	}
	memset(&dev, 0, sizeof(dev));
	if (ioctl(source, EVIOCGNAME(sizeof(dev.name)), dev.name) >= 0) {
		dev.name[sizeof(dev.name) - 1] = 0;
		if (strlen(dev.name) > 0) {
			strncat(dev.name, " ", sizeof(dev.name) - strlen(dev.name));
			dev.name[sizeof(dev.name) - 1] = 0;
		}
	}
	strncat(dev.name, name, sizeof(dev.name) - strlen(dev.name));
	dev.name[sizeof(dev.name) - 1] = 0;

	if (write(fd, &dev, sizeof(dev)) != sizeof(dev)) {
		goto setup_error;
	}

	if (test_bit(EV_KEY, events)) {
		if (ioctl(source, EVIOCGBIT(EV_KEY, KEY_MAX), bits) == -1) {
			goto setup_error;
		}

		if (ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1) {
			goto setup_error;
		}

		/* only forward mouse button events */
		for (key = BTN_MISC; key <= BTN_GEAR_UP; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_KEYBIT, key) == -1) {
					goto setup_error;
				}
			}
		}
	}
	if (test_bit(EV_REL, events)) {
		if (ioctl(source, EVIOCGBIT(EV_REL, REL_MAX), bits) == -1) {
			goto setup_error;
		}
		if (ioctl(fd, UI_SET_EVBIT, EV_REL) == -1) {
			goto setup_error;
		}
		for (key = 0; key <= REL_MAX; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_RELBIT, key) == -1) {
					goto setup_error;
				}
			}
		}
	}
	if (test_bit(EV_ABS, events)) {
		if (ioctl(source, EVIOCGBIT(EV_ABS, ABS_MAX), bits) == -1) {
			goto setup_error;
		}
		if (ioctl(fd, UI_SET_EVBIT, EV_ABS) == -1) {
			goto setup_error;
		}
		for (key = 0; key <= ABS_MAX; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_ABSBIT, key) == -1) {
					goto setup_error;
				}
			}
		}
	}

	if (ioctl(fd, UI_DEV_CREATE) == -1) {
		goto setup_error;
	}
	return fd;

setup_error:
	logperror(LIRC_ERROR, "could not setup %s\n", "uinput");
	close(fd);
	return -1;
}

#if 0
/* using fnmatch */
static int do_match(const char *text, const char *wild)
{
	while (*wild) {
		if (*wild == '*') {
			const char *next = text;
			wild++;
			while (*next) {
				if (do_match(next, wild)) {
					return 1;
				}
				next++;
			}
			return *wild ? 0 : 1;
		} else if (*wild == '?') {
			wild++;
			if (!*text++)
				return 0;
		} else if (*wild == '\\') {
			if (!wild[1]) {
				return 0;
			}
			if (wild[1] != *text++) {
				return 0;
			}
			wild += 2;
		} else if (*wild++ != *text++) {
			return 0;
		}
	}
	return *text ? 0 : 1;
}
#endif

static int locate_dev(const char *pattern, enum locate_type type)
{
	static char devname[FILENAME_MAX];
	char ioname[255];
	DIR *dir;
	struct dirent *obj;
	int request;

	dir = opendir("/dev/input");
	if (!dir) {
		return 1;
	}

	devname[0] = 0;
	switch (type) {
	case locate_by_name:
		request = EVIOCGNAME(sizeof(ioname));
		break;
#ifdef EVIOCGPHYS
	case locate_by_phys:
		request = EVIOCGPHYS(sizeof(ioname));
		break;
#endif
	default:
		closedir(dir);
		return 1;
	}

	while ((obj = readdir(dir))) {
		int fd;
		if (obj->d_name[0] == '.' && (obj->d_name[1] == 0 || (obj->d_name[1] == '.' && obj->d_name[2] == 0))) {
			continue;	/* skip "." and ".." */
		}
		sprintf(devname, "/dev/input/%s", obj->d_name);
		fd = open(devname, O_RDONLY);
		if (!fd) {
			continue;
		}
		if (ioctl(fd, request, ioname) >= 0) {
			int ret;
			close(fd);

			ioname[sizeof(ioname) - 1] = 0;
			//ret = !do_match (ioname, pattern);
			ret = fnmatch(pattern, ioname, 0);
			if (ret == 0) {
				drv.device = devname;
				closedir(dir);
				return 0;
			}
		}
		close(fd);
	}

	closedir(dir);
	return 1;
}

int devinput_init()
{
	logprintf(LIRC_INFO, "initializing '%s'", drv.device);

	if (!strncmp(drv.device, "name=", 5)) {
		if (locate_dev(drv.device + 5, locate_by_name)) {
			logprintf(LIRC_ERROR, "unable to find '%s'", drv.device);
			return 0;
		}
	} else if (!strncmp(drv.device, "phys=", 5)) {
		if (locate_dev(drv.device + 5, locate_by_phys)) {
			logprintf(LIRC_ERROR, "unable to find '%s'", drv.device);
			return 0;
		}
	}

	if ((drv.fd = open(drv.device, O_RDONLY)) < 0) {
		logprintf(LIRC_ERROR, "unable to open '%s'", drv.device);
		return 0;
	}
	if (set_rc_protocol(drv.device) != 0) {
		logprintf(LIRC_INFO, "Cannot configure the rc device for %s",
			  drv.device);
	}
#ifdef EVIOCGRAB
	exclusive = 1;
	if (ioctl(drv.fd, EVIOCGRAB, 1) == -1) {
		exclusive = 0;
		logprintf(LIRC_WARNING, "can't get exclusive access to events coming from `%s' interface", drv.device);
	}
#endif
	return 1;
}

int devinput_init_fwd()
{
	if (!devinput_init())
		return 0;

	if (exclusive) {
		uinputfd = setup_uinputfd("(lircd bypass)", drv.fd);
	}

	return 1;
}

int devinput_deinit(void)
{
	logprintf(LIRC_INFO, "closing '%s'", drv.device);
	if (uinputfd != -1) {
		ioctl(uinputfd, UI_DEV_DESTROY);
		close(uinputfd);
		uinputfd = -1;
	}
	close(drv.fd);
	drv.fd = -1;
	return 1;
}

int devinput_decode(struct ir_remote *remote, struct decode_ctx_t* ctx)
{
	LOGPRINTF(1, "devinput_decode");

	if (!map_code(remote, ctx, 0, 0, hw_devinput.code_length, code, 0, 0)) {
		static int print_warning = 1;

		if (!map_code(remote, ctx, 0, 0, 32, code_compat, 0, 0)) {
			return (0);
		}
		if (print_warning) {
			print_warning = 0;
			logprintf(LIRC_WARNING, "you are using an obsolete devinput config file");
			logprintf(LIRC_WARNING,
				  "get the new version at http://lirc.sourceforge.net/remotes/devinput/lircd.conf.devinput");
		}
	}

	map_gap(remote, ctx, &start, &last, 0);
	/* override repeat */
	switch (repeat_state) {
	case RPT_NO:
		ctx->repeat_flag = 0;
		break;
	case RPT_YES:
		ctx->repeat_flag = 1;
		break;
	default:
		break;
	}

	return 1;
}

char *devinput_rec(struct ir_remote *remotes)
{
	struct input_event event;
	int rd;
	ir_code value;

	LOGPRINTF(1, "devinput_rec");

	last = end;
	gettimeofday(&start, NULL);

	rd = read(drv.fd, &event, sizeof event);
	if (rd != sizeof event) {
		logprintf(LIRC_ERROR, "error reading '%s'", drv.device);
		if (rd <= 0 && errno != EINTR) {
			devinput_deinit();
		}
		return 0;
	}

	LOGPRINTF(1, "time %ld.%06ld  type %d  code %d  value %d", event.time.tv_sec, event.time.tv_usec, event.type,
		  event.code, event.value);

	value = (unsigned)event.value;
#ifdef EV_SW
	if (value == 2 && (event.type == EV_KEY || event.type == EV_SW)) {
		value = 1;
	}
	code_compat = ((event.type == EV_KEY || event.type == EV_SW) && event.value != 0) ? 0x80000000 : 0;
#else
	if (value == 2 && event.type == EV_KEY) {
		value = 1;
	}
	code_compat = ((event.type == EV_KEY) && event.value != 0) ? 0x80000000 : 0;
#endif
	code_compat |= ((event.type & 0x7fff) << 16);
	code_compat |= event.code;

	if (event.type == EV_KEY) {
		if (event.value == 2) {
			repeat_state = RPT_YES;
		} else {
			repeat_state = RPT_NO;
		}
	} else {
		repeat_state = RPT_UNKNOWN;
	}

	code = ((ir_code) (unsigned)event.type) << 48 | ((ir_code) (unsigned)event.code) << 32 | value;

	LOGPRINTF(1, "code %.8llx", code);

	if (uinputfd != -1) {
		if (event.type == EV_REL || event.type == EV_ABS
		    || (event.type == EV_KEY && event.code >= BTN_MISC && event.code <= BTN_GEAR_UP)
		    || event.type == EV_SYN) {
			LOGPRINTF(1, "forwarding: %04x %04x", event.type, event.code);
			if (write(uinputfd, &event, sizeof(event)) != sizeof(event)) {
				logperror(LIRC_ERROR, "writing to uinput failed");
			}
			return NULL;
		}
	}

	/* ignore EV_SYN */
	if (event.type == EV_SYN)
		return NULL;

	gettimeofday(&end, NULL);
	return decode_all(remotes);
}
