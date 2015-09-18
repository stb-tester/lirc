/****************************************************************************
** hw_awlibusb.c ***********************************************************
****************************************************************************
*
*  Userspace Lirc plugin for Awox RF/IR usb remote
*
*  Copyright (C) 2008 Arif <azeemarif@gmail.com>
*  Copyright (C) 2008 Awox Pte Ltd <marif@awox.com>
*
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Library General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
*
*  July 2008:    Created along with corresponding kernel space driver
*  August 2008:  Modified it to work in userspace using libusb.
*                No kernel driver is needed anymore.
*               (reference taken from atilibusb)
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "lirc_driver.h"

#define AW_MODE_LIRCCODE 1

#define AWUSB_RECEIVE_BYTES 5
#define USB_TIMEOUT (1000 * 60)
#define AW_VENDOR_THOMSON 0x069b
#define AW_DEVICE_THOMSON 0x1111

#define AW_KEY_GAP 0            /* Original value=200000. Made it 0 to handle it in userspace */

#if !defined(AW_MODE_LIRCCODE)
static ir_code code;
static ir_code code_last;
static struct timeval time_current = { 0 };
static struct timeval time_last = { 0 };
#endif
static char device_path[PATH_MAX + 1] = {0};

static int awlibusb_init(void);
static int awlibusb_deinit(void);
static char* awlibusb_rec(struct ir_remote* remotes);
static void usb_read_loop(int fd);
static struct usb_device* find_usb_device(void);
static int find_device_endpoints(struct usb_device* dev);

#ifdef AW_MODE_LIRCCODE
const struct driver hw_awlibusb = {
	.name		= "awlibusb",
	.device		= NULL,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= (AWUSB_RECEIVE_BYTES - 1) * CHAR_BIT,
	.init_func	= awlibusb_init,
	.deinit_func	= awlibusb_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= awlibusb_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available.",
	.device_hint    = "/dev/ttyUSB*",
};
#else
const struct driver hw_awlibusb = {
	.name		= "awlibusb",
	.device		= NULL,
	.features	= LIRC_CAN_REC_CODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_CODE,
	.code_length	= CHAR_BIT,
	.init_func	= awlibusb_init,
	.deinit_func	= awlibusb_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= awlibusb_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.info		= "No info available.",
	.device_hint    = "/dev/ttyUSB*",
};
#endif

const struct driver* hardwares[] = { &hw_awlibusb, (const struct driver*)NULL };

typedef struct {
	u_int16_t	vendor;
	u_int16_t	product;
} usb_device_id;

static const logchannel_t logchannel = LOG_DRIVER;

/* table of compatible remotes -- from lirc_awusb */
static usb_device_id usb_remote_id_table[] = {
	/* Awox RF/Infrared Transciever */
	{ AW_VENDOR_THOMSON, AW_DEVICE_THOMSON },
	/* Terminating entry */
	{}
};

static struct usb_dev_handle* dev_handle = NULL;
static struct usb_endpoint_descriptor* dev_ep_in = NULL;
static pid_t child = -1;

/****/

/* initialize driver -- returns 1 on success, 0 on error */
static int awlibusb_init(void)
{
	struct usb_device* usb_dev;
	int pipe_fd[2] = { -1, -1 };

	log_trace("initializing USB receiver");

	rec_buffer_init();

	/* A separate process will be forked to read data from the USB
	 * receiver and write it to a pipe. drv.fd is set to the readable
	 * end of this pipe. */
	if (pipe(pipe_fd) != 0) {
		log_perror_err("couldn't open pipe");
		return 0;
	}
	drv.fd = pipe_fd[0];

	usb_dev = find_usb_device();
	if (usb_dev == NULL) {
		log_error("couldn't find a compatible USB device");
		goto fail;
	}

	if (!find_device_endpoints(usb_dev)) {
		log_error("couldn't find device endpoints");
		goto fail;
	}

	dev_handle = usb_open(usb_dev);
	if (dev_handle == NULL) {
		log_perror_err("couldn't open USB receiver");
		goto fail;
	}

	if (usb_claim_interface(dev_handle, 0) != 0) {
		log_perror_err("couldn't claim USB interface");
		goto fail;
	}

	snprintf(device_path, sizeof(device_path),
		 "/dev/bus/usb/%s/%s",
		 usb_dev->bus->dirname, usb_dev->filename);
	drv.device = device_path;
	log_debug("atilibusb: using device: %s", device_path);

	child = fork();
	if (child == -1) {
		log_perror_err("couldn't fork child process");
		goto fail;
	} else if (child == 0) {
		usb_read_loop(pipe_fd[1]);
	}

	log_trace("USB receiver initialized");
	return 1;

fail:
	if (dev_handle) {
		usb_close(dev_handle);
		dev_handle = NULL;
	}
	if (pipe_fd[0] >= 0)
		close(pipe_fd[0]);
	if (pipe_fd[1] >= 0)
		close(pipe_fd[1]);
	return 0;
}

/* deinitialize driver -- returns 1 on success, 0 on error */
static int awlibusb_deinit(void)
{
	int err = 0;

	if (dev_handle) {
		if (usb_close(dev_handle) < 0)
			err = 1;
		dev_handle = NULL;
	}

	if (drv.fd >= 0) {
		if (close(drv.fd) < 0)
			err = 1;
		drv.fd = -1;
	}

	if (child > 1) {
		if ((kill(child, SIGTERM) == -1)
		    || (waitpid(child, NULL, 0) == 0))
			err = 1;
	}

	return !err;
}

static char* awlibusb_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

/* returns 1 if the given device should be used, 0 otherwise */
static int is_device_ok(struct usb_device* dev)
{
	/* TODO: allow exact device to be specified */

	/* check if the device ID is in usb_remote_id_table */
	usb_device_id* dev_id;

	for (dev_id = usb_remote_id_table; dev_id->vendor; dev_id++)
		if ((dev->descriptor.idVendor == dev_id->vendor) && (dev->descriptor.idProduct == dev_id->product))
			return 1;

	return 0;
}

/* find a compatible USB receiver and return a usb_device,
 * or NULL on failure. */
static struct usb_device* find_usb_device(void)
{
	struct usb_bus* usb_bus;
	struct usb_device* dev;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (usb_bus = usb_busses; usb_bus; usb_bus = usb_bus->next) {
		for (dev = usb_bus->devices; dev; dev = dev->next)
			if (is_device_ok(dev))
				return dev;
	}
	return NULL;            /* no suitable device found */
}

/* set dev_ep_in and dev_ep_out to the in/out endpoints of the given
 * device. returns 1 on success, 0 on failure. */
static int find_device_endpoints(struct usb_device* dev)
{
	struct usb_interface_descriptor* idesc;

	if (dev->descriptor.bNumConfigurations != 1)
		return 0;

	if (dev->config[0].bNumInterfaces != 1)
		return 0;

	if (dev->config[0].interface[0].num_altsetting != 1)
		return 0;

	idesc = &dev->config[0].interface[0].altsetting[0];
//      if (idesc->bNumEndpoints != 2) return 0;

	dev_ep_in = &idesc->endpoint[0];
	if ((dev_ep_in->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
	    != USB_ENDPOINT_IN)
		return 0;

	if ((dev_ep_in->bmAttributes & USB_ENDPOINT_TYPE_MASK)
	    != USB_ENDPOINT_TYPE_INTERRUPT)
		return 0;

	return 1;
}

/* this function is run in a forked process to read data from the USB
 * receiver and write it to the given fd. it calls exit() with result
 * code 0 on success, or 1 on failure. */
static void usb_read_loop(int fd)
{
	int inited = 0;
	int err = 0;

#if !defined(AW_MODE_LIRCCODE)
	long elapsed_seconds = 0;       /* diff between seconds counter */
	long elapsed_useconds = 0;      /* diff between microseconds counter */
	long time_diff = 0;
#endif

	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	for (;; ) {
		char buf[AWUSB_RECEIVE_BYTES];
		int bytes_r, bytes_w;

		/* read from the USB device */
		bytes_r =
			usb_interrupt_read(dev_handle, dev_ep_in->bEndpointAddress, &buf[0], sizeof(buf), USB_TIMEOUT);
		if (bytes_r < 0) {
			if (errno == EAGAIN || errno == ETIMEDOUT)
				continue;
			log_perror_err("can't read from USB device");
			err = 1;
			goto done;
		}

		/* sometimes the remote sends one byte on startup; ignore it */
		if (!inited) {
			inited = 1;
			if (bytes_r == 1)
				continue;
		}
#ifdef AW_MODE_LIRCCODE
		bytes_w = write(fd, &(buf[1]), (AWUSB_RECEIVE_BYTES - 1));
		/* ignore first byte */
		if (bytes_w < 0) {
			log_perror_err("can't write to pipe");
			err = 1;
			goto done;
		}
#else
		code = buf[AWUSB_RECEIVE_BYTES - 2];

		/* calculate time diff */
		gettimeofday(&time_current, NULL);
		elapsed_seconds = time_current.tv_sec - time_last.tv_sec;
		elapsed_useconds = time_current.tv_usec - time_last.tv_usec;
		time_diff = (elapsed_seconds) * 1000000 + elapsed_useconds;
		//printf("time_diff = %d usec\n", time_diff);

		if (!((code == code_last) && (time_diff < AW_KEY_GAP))) {
			bytes_w = write(fd, &code, 1);
			if (bytes_w < 0) {
				log_perror_err("can't write to pipe");
				err = 1;
				goto done;
			}
			code_last = code;
			memcpy(&time_last, &time_current, sizeof(struct timeval));
		}
#endif
	}

done:
	if (!usb_close(dev_handle))
		err = 1;
	_exit(err);
}
