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

#define _GNU_SOURCE 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_LIBUDEV_H
# include <libudev.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/stat.h>

#ifndef EV_SYN
/* previous name */
#define EV_SYN EV_RST
#endif

#include "lirc_driver.h"

/* from evtest.c - Copyright (c) 1999-2000 Vojtech Pavlik */
#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)
/* NBITS was defined in linux/uinput.h */
#undef NBITS
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x)  ((x) % BITS_PER_LONG)
#define BIT(x)  (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)


static const logchannel_t logchannel = LOG_DRIVER;

static int devinput_init(void);
static int devinput_init_fwd(void);
static int devinput_deinit(void);
static int devinput_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static char* devinput_rec(struct ir_remote* remotes);
static int drvctl(unsigned int fd, void* arg);

enum locate_type {
	locate_by_name,
	locate_by_phys,
	locate_default
};

const struct driver hw_devinput = {
	.name		= "devinput",
	.device		= "/dev/input/event0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= sizeof(struct input_event) * 8,
	.init_func	= devinput_init_fwd,
	.deinit_func	= devinput_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= devinput_rec,
	.decode_func	= devinput_decode,
	.drvctl_func	= drvctl,
	.readdata	= NULL,
	.api_version	= 4,
	.driver_version = "0.9.3",
	.info		= "See file://" PLUGINDOCS "/devinput.html",
#ifdef HAVE_LIBUDEV_H
	.device_hint    = "drvctl",
#else
	.device_hint    = "/dev/input/event*",
#endif
};


const struct driver* hardwares[] = { &hw_devinput, (const struct driver*)NULL };


const char* const DEVINPUT_NOTICE1 =
	"Use the distributed devinput.lircd.dist by renaming it. ";
const char* const DEVINPUT_NOTICE2 =
	"Or use irdb-get to search for \"devinput\" and download it.";

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

static int setup_uinputfd(const char* name, int source)
{
	int fd;
	int key;
	struct uinput_user_dev dev;
	long events[NBITS(EV_MAX)];
	long bits[NBITS(KEY_MAX)];

	if (ioctl(source, EVIOCGBIT(0, EV_MAX), events) == -1)
		return -1;
	if (!test_bit(EV_REL, events) && !test_bit(EV_ABS, events))
		/* no move events, don't forward anything */
		return -1;
	fd = open("/dev/input/uinput", O_RDWR);
	if (fd == -1) {
		fd = open("/dev/uinput", O_RDWR);
		if (fd == -1) {
			fd = open("/dev/misc/uinput", O_RDWR);
			if (fd == -1) {
				log_perror_warn("could not open %s\n",
						"uinput");
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
	strncat(dev.name, name, sizeof(dev.name) - strlen(dev.name) - 1);
	dev.name[sizeof(dev.name) - 1] = 0;

	if (write(fd, &dev, sizeof(dev)) != sizeof(dev))
		goto setup_error;

	if (test_bit(EV_KEY, events)) {
		if (ioctl(source, EVIOCGBIT(EV_KEY, KEY_MAX), bits) == -1)
			goto setup_error;

		if (ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1)
			goto setup_error;

		/* only forward mouse button events */
		for (key = BTN_MISC; key <= BTN_GEAR_UP; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_KEYBIT, key) == -1)
					goto setup_error;
			}
		}
	}
	if (test_bit(EV_REL, events)) {
		if (ioctl(source, EVIOCGBIT(EV_REL, REL_MAX), bits) == -1)
			goto setup_error;
		if (ioctl(fd, UI_SET_EVBIT, EV_REL) == -1)
			goto setup_error;
		for (key = 0; key <= REL_MAX; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_RELBIT, key) == -1)
					goto setup_error;
			}
		}
	}
	if (test_bit(EV_ABS, events)) {
		if (ioctl(source, EVIOCGBIT(EV_ABS, ABS_MAX), bits) == -1)
			goto setup_error;
		if (ioctl(fd, UI_SET_EVBIT, EV_ABS) == -1)
			goto setup_error;
		for (key = 0; key <= ABS_MAX; key++) {
			if (test_bit(key, bits)) {
				if (ioctl(fd, UI_SET_ABSBIT, key) == -1)
					goto setup_error;
			}
		}
	}

	if (ioctl(fd, UI_DEV_CREATE) == -1)
		goto setup_error;
	return fd;

setup_error:
	log_perror_err("could not setup %s\n", "uinput");
	close(fd);
	return -1;
}

#if 0
/* using fnmatch */
static int do_match(const char* text, const char* wild)
{
	while (*wild) {
		if (*wild == '*') {
			const char* next = text;

			wild++;
			while (*next) {
				if (do_match(next, wild))
					return 1;
				next++;
			}
			return *wild ? 0 : 1;
		} else if (*wild == '?') {
			wild++;
			if (!*text++)
				return 0;
		} else if (*wild == '\\') {
			if (!wild[1])
				return 0;
			if (wild[1] != *text++)
				return 0;
			wild += 2;
		} else if (*wild++ != *text++) {
			return 0;
		}
	}
	return *text ? 0 : 1;
}
#endif

static int locate_default_device(char* errmsg, size_t size)
{

	static char devname[256];

	static const char* const DEV_PATTERN =
		"/sys/class/rc/rc0/input[0-9]*/event[0-9]*";
	glob_t matches;
	int r;
	char* event;

	r = glob("/sys/class/rc/rc0/input[0-9]*/event[0-9]*",
		 0, NULL, &matches);
	if (r != 0) {
		log_perror_warn("Cannot run glob %s", DEV_PATTERN);
		snprintf(errmsg, size, "Cannot glob %s", DEV_PATTERN);
		return 0;
	}
	if (matches.gl_pathc == 0) {
		strncpy(errmsg, "No /sys/class/rc/ devices found", size - 1);
		return 0;
	}
	if (matches.gl_pathc > 1) {
		strncpy(errmsg,
			"Multiple /sys/class/rc/ devices found",
			size - 1);
		return 0;
	}
	event = basename(strdupa(matches.gl_pathv[0]));
	snprintf(devname, sizeof(devname), "/dev/input/%s", event);
	drv.device = devname;
	return 1;
}


static int locate_dev(const char* pattern, enum locate_type type)
{
	static char devname[FILENAME_MAX];
	char ioname[255];
	DIR* dir;
	struct dirent* obj;
	int request;

	dir = opendir("/dev/input");
	if (!dir)
		return 1;

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

		if (obj->d_name[0] == '.' && (obj->d_name[1] == 0 ||
		    (obj->d_name[1] == '.' && obj->d_name[2] == 0)))
			continue;       /* skip "." and ".." */
		sprintf(devname, "/dev/input/%s", obj->d_name);
		fd = open(devname, O_RDONLY);
		if (!fd)
			continue;
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

#ifdef HAVE_LIBUDEV_H

static void list_device(struct udev_device* device, glob_t* glob)
{
	char buff[256];
	const char* devnode;
	struct udev_device* usb_device;
	struct udev_list_entry* links;

	devnode = udev_device_get_devnode(device);
	if (devnode == NULL)
			return;
	if (udev_device_get_parent_with_subsystem_devtype(device,
							  "rc",
							  NULL) == NULL) {
		return;
	}
	links =  udev_device_get_devlinks_list_entry(device);
	while (links != NULL) {
		snprintf(buff, sizeof(buff), "%s  -> %s",
			 udev_list_entry_get_name(links), devnode);
		links = udev_list_entry_get_next(links);
		glob->gl_pathv[glob->gl_pathc] = strdup(buff);
		glob->gl_pathc += 1;
	}
	device = udev_device_get_parent_with_subsystem_devtype(device,
							       "input",
							       NULL);
	usb_device = udev_device_get_parent_with_subsystem_devtype(
						device, "usb", "usb_device");
	if (usb_device != NULL) {
		snprintf(buff, sizeof(buff), "%s %s [%s]",
			 devnode,
			 udev_device_get_sysattr_value(device, "name"),
			 udev_device_get_syspath(usb_device));
	} else {
		snprintf(buff, sizeof(buff), "%s %s [%s]",
			 devnode,
			 udev_device_get_sysattr_value(device, "name"),
			 udev_device_get_syspath(device));
	}
	glob_t_add_path(glob, buff);
}


static void list_devices(glob_t* glob)
{
	struct udev* udev;
	struct udev_enumerate* enumerate;
	struct udev_list_entry* devices;
	struct udev_list_entry* device;

	glob_t_init(glob);

	udev = udev_new();
	if (udev == NULL) {
		log_error("Cannot run udev_new()");
		return;
	}
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	udev_list_entry_foreach(device, devices) {
		const char* const syspath = udev_list_entry_get_name(device);
		struct udev_device* udev_device =
			udev_device_new_from_syspath(udev, syspath);
		list_device(udev_device, glob);
	}
	udev_enumerate_unref(enumerate);
	udev_unref(udev);
}
#endif  // HAVE_LIBUDEV_H


int devinput_init(void)
{
	char errmsg[256];

	log_info("initializing '%s'", drv.device);

	if (strncmp(drv.device, "name=", 5) == 0) {
		if (locate_dev(drv.device + 5, locate_by_name)) {
			log_error("Unable to find '%s'", drv.device);
			return 0;
		}
	} else if (strncmp(drv.device, "phys=", 5) == 0) {
		if (locate_dev(drv.device + 5, locate_by_phys)) {
			log_error("Unable to find '%s'", drv.device);
			return 0;
		}
	} else if (strcmp(drv.device, "auto") == 0) {
		if (locate_default_device(errmsg, sizeof(errmsg)) == 0) {
			log_error(errmsg);
			return 0;
		}
	}
	log_info("Using device: %s", drv.device);
	drv.fd = open(drv.device, O_RDONLY);
	if (drv.fd < 0) {
		log_error("unable to open '%s'", drv.device);
		return 0;
	}
#ifdef EVIOCGRAB
	exclusive = 1;
	if (ioctl(drv.fd, EVIOCGRAB, 1) == -1) {
		exclusive = 0;
		log_warn("can't get exclusive access to events coming from `%s' interface", drv.device);
	}
#endif
	return 1;
}

int devinput_init_fwd(void)
{
	if (!devinput_init())
		return 0;

	if (exclusive)
		uinputfd = setup_uinputfd("(lircd bypass)", drv.fd);

	return 1;
}

int devinput_deinit(void)
{
	log_info("closing '%s'", drv.device);
	if (uinputfd != -1) {
		ioctl(uinputfd, UI_DEV_DESTROY);
		close(uinputfd);
		uinputfd = -1;
	}
	close(drv.fd);
	drv.fd = -1;
	return 1;
}

int devinput_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	log_trace("devinput_decode");

	if (!map_code(remote, ctx, 0, 0, hw_devinput.code_length, code, 0, 0)) {
		static int print_warning = 1;

		if (!map_code(remote, ctx, 0, 0, 32, code_compat, 0, 0))
			return 0;
		if (print_warning) {
			print_warning = 0;
			log_warn("Obsolete devinput config file used");
			log_notice(DEVINPUT_NOTICE1);
			log_notice(DEVINPUT_NOTICE2);
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

char* devinput_rec(struct ir_remote* remotes)
{
	struct input_event event;
	int rd;
	ir_code value;

	log_trace("devinput_rec");

	last = end;
	gettimeofday(&start, NULL);

	rd = read(drv.fd, &event, sizeof(event));
	if (rd != sizeof(event)) {
		log_error("error reading '%s'", drv.device);
		if (rd <= 0 && errno != EINTR)
			devinput_deinit();
		return 0;
	}

	log_trace("time %ld.%06ld  type %d  code %d  value %d", event.time.tv_sec, event.time.tv_usec, event.type,
		  event.code, event.value);

	value = (unsigned)event.value;
#ifdef EV_SW
	if (value == 2 && (event.type == EV_KEY || event.type == EV_SW))
		value = 1;
	code_compat = ((event.type == EV_KEY || event.type == EV_SW) && event.value != 0) ? 0x80000000 : 0;
#else
	if (value == 2 && event.type == EV_KEY)
		value = 1;
	code_compat = ((event.type == EV_KEY) && event.value != 0) ? 0x80000000 : 0;
#endif
	code_compat |= ((event.type & 0x7fff) << 16);
	code_compat |= event.code;

	if (event.type == EV_KEY) {
		if (event.value == 2)
			repeat_state = RPT_YES;
		else
			repeat_state = RPT_NO;
	} else {
		repeat_state = RPT_UNKNOWN;
	}

	code = ((ir_code)(unsigned)event.type) << 48 | ((ir_code)(unsigned)event.code) << 32 | value;

	log_trace("code %.8llx", code);

	if (uinputfd != -1) {
		if (event.type == EV_REL || event.type == EV_ABS
		    || (event.type == EV_KEY && event.code >= BTN_MISC && event.code <= BTN_GEAR_UP)
		    || event.type == EV_SYN) {
			log_trace("forwarding: %04x %04x", event.type, event.code);
			if (write(uinputfd, &event, sizeof(event)) != sizeof(event))
				log_perror_err("writing to uinput failed");
			return NULL;
		}
	}

	/* ignore EV_SYN */
	if (event.type == EV_SYN)
		return NULL;

	gettimeofday(&end, NULL);
	return decode_all(remotes);
}


static int drvctl(unsigned int cmd, void* arg)
{
	glob_t* glob;

	switch (cmd) {
#ifdef HAVE_LIBUDEV_H
	case DRVCTL_GET_DEVICES:
		glob = (glob_t*) arg;
		list_devices(glob);
		return 0;
	case DRVCTL_FREE_DEVICES:
		glob = (glob_t*) arg;
		glob_t_free(glob);
		return 0;
#endif
	case DRVCTL_GET_RAW_CODELENGTH:
		*(unsigned int*)arg = sizeof(struct input_event) * 8;
		return 0;
	default:
		return DRV_ERR_NOT_IMPLEMENTED;
	}
}
