/****************************************************************************
** hw_default.c ************************************************************
****************************************************************************
*
* routines for hardware that supports ioctl() interface
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "lirc_driver.h"



static const logchannel_t logchannel = LOG_DRIVER;

static __u32 supported_send_modes[] = {
	/* LIRC_CAN_SEND_LIRCCODE, */
	/* LIRC_CAN_SEND_MODE2, this one would be very easy */
	LIRC_CAN_SEND_PULSE,
	/* LIRC_CAN_SEND_RAW, */
	0
};

static __u32 supported_rec_modes[] = {
	LIRC_CAN_REC_LIRCCODE,
	LIRC_CAN_REC_MODE2,
	/* LIRC_CAN_REC_PULSE, shouldn't be too hard */
	/* LIRC_CAN_REC_RAW, */
	0
};

//Forwards:
static int default_init(void);
static int default_deinit(void);
static int default_send(struct ir_remote* remote, struct ir_ncode* code);
static char* default_rec(struct ir_remote* remotes);
static int default_ioctl(unsigned int cmd, void* arg);
static lirc_t default_readdata(lirc_t timeout);
static int my_open(const char* path);



static const const struct driver hw_default = {
	.name		= "default",
	.device		= LIRC_DRIVER_DEVICE,
	.features	= 0,
	.send_mode	= 0,
	.rec_mode	= 0,
	.code_length	= 0,
	.init_func	= default_init,
	.deinit_func	= default_deinit,
	.open_func	= my_open,
	.close_func	= default_close,
	.send_func	= default_send,
	.rec_func	= default_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= default_ioctl,
	.readdata	= default_readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/lirc*",
};


const struct driver* hardwares[] = { &hw_default, (const struct driver*)NULL };


/**********************************************************************
*
* internal function prototypes
*
**********************************************************************/

static int write_send_buffer(int lirc);

/***************************************************
*
* /sys/class/rc  stuff.
*
****************************************************/

static int is_rc(const char* s)
{
	if (s == NULL)
		return -1;
	return s[0] == 'r' && s[1] == 'c' && s[2] >= '0' && s[2] <= '9';
}


/*
 * Given a directory in /sys/class/rc, check if it contains
 * a file called device. If so, write "lirc" to the protocols
 * file in same directory unless the 'lirc' protocol already
 * is enabled.
 *
 * rc_dir: directory specification like rc0, rc1, etc.
 * device: Device given to lirc,  like 'lirc0' (or /dev/lirc0).
 * Returns: 0 if OK, else -1.
 *
 */
static int visit_rc(const char* rc_dir, const char* device)
{
	char path[64];
	char buff[128];
	char* enabled = NULL;
	int fd;
	int r;

	snprintf(path, sizeof(path), "/sys/class/rc/%s", rc_dir);
	if (access(path, F_OK) != 0) {
		log_notice("Cannot open rc directory: %s", path);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/rc/%s/%s", rc_dir, device);
	if (access(path, F_OK) != 0) {
		log_debug("No device found: %s", path);
		return -1;
	}
	snprintf(path, sizeof(path), "/sys/class/rc/%s/protocols", rc_dir);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		log_debug(
			  "Cannot open protocol file: %s for read", path);
		return -1;
	}
	r = read(fd, buff, sizeof(buff));
	if (r < 0) {
		log_debug("Cannot read from %s", path);
		return -1;
	}
	if (strchr(buff, '[') != NULL) {
		enabled = strchr(buff, '[') + 1;
		if (strchr(buff, ']') != NULL)
			*strchr(buff, ']') = '\0';
		else
			enabled = NULL;
	}
	if (enabled == NULL) {
		log_warn("Cannot parse protocols %s", buff);
	} else  if (strcmp(enabled, "lirc") == 0) {
		log_info("[lirc] protocol is enabled");
		return 0;
	}
	fd = open(path, O_WRONLY);
	if (fd < 0) {
		log_debug(
			  "Cannot open protocol file for write: %s", path);
		return -1;
	}
	chk_write(fd, "lirc\n", 5);
	log_notice("'lirc' written to protocols file %s", path);
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
	dir = opendir("/sys/class/rc");
	if (dir == NULL) {
		log_notice("Cannot open /sys/class/rc\n");
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


int my_open(const char* path)
{
	default_open(path);
	set_rc_protocol(drv.device);
	return 0;
}


/**********************************************************************
*
* decode stuff
*
**********************************************************************/
int default_readdata(lirc_t timeout)
{
	int data, ret;

	static int last_data = PULSE_BIT; // See kernel bug below

	if (!waitfordata((long)timeout))
		return 0;

	ret = read(drv.fd, &data, sizeof(data));
	if (ret != sizeof(data)) {
		logperror(LIRC_ERROR,
			  "error reading from %s (ret %d, expected %d)",
			  drv.device, ret, sizeof(data));
		default_deinit();

		return 0;
	}

	if (data == 0) {
		static int data_warning = 1;

		if (data_warning) {
			log_warn(
				  "read invalid data from device %s",
				  drv.device);
			data_warning = 0;
		}
		data = 1;
	}
	// FIXME: https://bugzilla.kernel.org/show_bug.cgi?id=102971 fix.
	if (getenv("LIRC_REGRESSION_TEST") != NULL)
		return data;
	if (is_space(data) && is_space(last_data)) {
		last_data = data;
		return  default_readdata(timeout);
	}
	last_data = data;

	return data;
}

/*
 * interface functions
 */
int default_init(void)
{
	struct stat s;
	int i;

	/* FIXME: other modules might need this, too */
	rec_buffer_init();
	send_buffer_init();

	if (set_rc_protocol(drv.device) != 0)
		log_info("Cannot configure the rc device for %s",
			  drv.device);

	if (stat(drv.device, &s) == -1) {
		log_error("could not get file information for %s", drv.device);
		logperror(LIRC_ERROR, "default_init()");
		return 0;
	}

	/* file could be unix socket, fifo and native lirc device */
	if (S_ISSOCK(s.st_mode)) {
		struct sockaddr_un addr;

		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, drv.device, sizeof(addr.sun_path) - 1);

		drv.fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (drv.fd == -1) {
			log_error("could not create socket");
			logperror(LIRC_ERROR, "default_init()");
			return 0;
		}

		if (connect(drv.fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			log_error("could not connect to unix socket %s", drv.device);
			logperror(LIRC_ERROR, "default_init()");
			default_deinit();
			close(drv.fd);
			return 0;
		}

		log_trace("using unix socket lirc device");
		drv.features = LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE;
		drv.rec_mode = LIRC_MODE_MODE2; /* this might change in future */
		drv.send_mode = LIRC_MODE_PULSE;
		return 1;
	}
	drv.fd = open(drv.device, O_RDWR);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		logperror(LIRC_ERROR, "default_init()");
		return 0;
	}
	if (S_ISFIFO(s.st_mode)) {
		log_trace("using defaults for the Irman");
		drv.features = LIRC_CAN_REC_MODE2;
		drv.rec_mode = LIRC_MODE_MODE2; /* this might change in future */
		return 1;
	} else if (!S_ISCHR(s.st_mode)) {
		default_deinit();
		log_error("%s is not a character device!!!", drv.device);
		logperror(LIRC_ERROR, "something went wrong during installation");
		return 0;
	} else if (default_ioctl(LIRC_GET_FEATURES, &drv.features) == -1) {
		log_error("could not get hardware features");
		log_error("this device driver does not support the LIRC ioctl interface");
		if (major(s.st_rdev) == 13) {
			log_error("did you mean to use the devinput driver instead of the %s driver?",
				  drv.name);
		} else {
			log_error("major number of %s is %lu", drv.device, (__u32)major(s.st_rdev));
			log_error("make sure %s is a LIRC device and use a current version of the driver",
				  drv.device);
		}
		default_deinit();
		return 0;
	}
	if (!(LIRC_CAN_SEND(drv.features) || LIRC_CAN_REC(drv.features)))
		log_trace("driver supports neither sending nor receiving of IR signals");
	if (LIRC_CAN_SEND(drv.features) && LIRC_CAN_REC(drv.features)) {
		log_trace("driver supports both sending and receiving");
	} else if (LIRC_CAN_SEND(drv.features)) {
		log_trace("driver supports sending");
	} else if (LIRC_CAN_REC(drv.features)) {
		log_trace("driver supports receiving");
	}

	/* set send/receive method */
	drv.send_mode = 0;
	if (LIRC_CAN_SEND(drv.features)) {
		for (i = 0; supported_send_modes[i] != 0; i++) {
			if (LIRC_CAN_SEND(drv.features) == supported_send_modes[i]) {
				drv.send_mode = LIRC_SEND2MODE(supported_send_modes[i]);
				break;
			}
		}
		if (supported_send_modes[i] == 0)
			log_notice("the send method of the driver is not yet supported by lircd");
	}
	drv.rec_mode = 0;
	if (LIRC_CAN_REC(drv.features)) {
		for (i = 0; supported_rec_modes[i] != 0; i++) {
			if (LIRC_CAN_REC(drv.features) == supported_rec_modes[i]) {
				drv.rec_mode = LIRC_REC2MODE(supported_rec_modes[i]);
				break;
			}
		}
		if (supported_rec_modes[i] == 0)
			log_notice("the receive method of the driver is not yet supported by lircd");
	}
	if (drv.rec_mode == LIRC_MODE_MODE2) {
		/* get resolution */
		drv.resolution = 0;
		if ((drv.features & LIRC_CAN_GET_REC_RESOLUTION)
		    && (default_ioctl(LIRC_GET_REC_RESOLUTION, &drv.resolution) != -1))
			log_trace("resolution of receiver: %d", drv.resolution);

	} else if (drv.rec_mode == LIRC_MODE_LIRCCODE) {
		if (default_ioctl(LIRC_GET_LENGTH, (void*)&drv.code_length) == -1) {
			log_error("could not get code length");
			logperror(LIRC_ERROR, "default_init()");
			default_deinit();
			return 0;
		}
		if (drv.code_length > sizeof(ir_code) * CHAR_BIT) {
			log_error("lircd can not handle %lu bit codes", drv.code_length);
			default_deinit();
			return 0;
		}
	}
	if (!(drv.send_mode || drv.rec_mode)) {
		default_deinit();
		return 0;
	}
	return 1;
}

int default_deinit(void)
{
	if (drv.fd != -1) {
		close(drv.fd);
		drv.fd = -1;
	}
	return 1;
}

static int write_send_buffer(int lirc)
{
	if (send_buffer_length() == 0) {
		log_trace("nothing to send");
		return 0;
	}
	return write(lirc, send_buffer_data(), send_buffer_length() * sizeof(lirc_t));
}

int default_send(struct ir_remote* remote, struct ir_ncode* code)
{
	/* things are easy, because we only support one mode */
	if (drv.send_mode != LIRC_MODE_PULSE)
		return 0;

	if (drv.features & LIRC_CAN_SET_SEND_CARRIER) {
		unsigned int freq;

		freq = remote->freq == 0 ? DEFAULT_FREQ : remote->freq;
		if (default_ioctl(LIRC_SET_SEND_CARRIER, &freq) == -1) {
			log_error("could not set modulation frequency");
			logperror(LIRC_ERROR, NULL);
			return 0;
		}
	}
	if (drv.features & LIRC_CAN_SET_SEND_DUTY_CYCLE) {
		unsigned int duty_cycle;

		duty_cycle = remote->duty_cycle == 0 ? 50 : remote->duty_cycle;
		if (default_ioctl(LIRC_SET_SEND_DUTY_CYCLE, &duty_cycle) == -1) {
			log_error("could not set duty cycle");
			logperror(LIRC_ERROR, NULL);
			return 0;
		}
	}
	if (!send_buffer_put(remote, code))
		return 0;
	if (write_send_buffer(drv.fd) == -1) {
		log_error("write failed");
		logperror(LIRC_ERROR, NULL);
		return 0;
	}
	return 1;
}

char* default_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear()) {
		default_deinit();
		return NULL;
	}
	return decode_all(remotes);
}

int default_ioctl(unsigned int cmd, void* arg)
{
	return ioctl(drv.fd, cmd, arg);
}
