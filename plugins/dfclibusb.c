/****************************************************************************
** hw_dfclibusb.c **********************************************************
****************************************************************************
*  Userspace (libusb) driver for DFC USB InfraRed Remote Control, based on
*  hw_atilibusb.c
*
*  Copyright (C) 2010 Davio Franke <davio@daviofranke.com>
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

#include "lirc_driver.h"

#define CODE_BYTES 6
#define USB_TIMEOUT (5000)


static const logchannel_t logchannel = LOG_DRIVER;

static char device_path[PATH_MAX + 1] = {0};

static int dfc_init(void);
static int dfc_deinit(void);
static char* dfc_rec(struct ir_remote* remotes);
static void usb_read_loop(int fd);
static struct usb_device* find_usb_device(void);

const struct driver hw_dfclibusb = {
	.name		= "dfclibusb",
	.device		= NULL,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= CODE_BYTES * CHAR_BIT,
	.init_func	= dfc_init,
	.deinit_func	= dfc_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= dfc_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/ttyUSB*",
};

const struct driver* hardwares[] = { &hw_dfclibusb, (const struct driver*)NULL };

typedef struct {
	u_int16_t	vendor;
	u_int16_t	product;
} usb_device_id;

/* table of compatible remotes -- from lirc_dfcusb */
static usb_device_id usb_remote_id_table[] = {
	{ 0x20a0, 0x410b },     /* DFC USB InfraRed Remote Control */
	{ 0x0dfc, 0x0001 },     /* DFC USB InfraRed Remote Control (for compatibility with first release only) */
	{ 0,	  0	 }      /* Terminating entry */
};

static struct usb_dev_handle* dev_handle = NULL;
static pid_t child = -1;

/****/

/* initialize driver -- returns 1 on success, 0 on error */
static int dfc_init(void)
{
	struct usb_device* usb_dev;
	int pipe_fd[2] = { -1, -1 };

	log_trace("initializing USB receiver");

	rec_buffer_init();

	usb_dev = find_usb_device();
	snprintf(device_path, sizeof(device_path),
		 "/dev/bus/usb/%s/%s",
		 usb_dev->bus->dirname, usb_dev->filename);
	drv.device = device_path;
	if (usb_dev == NULL) {
		log_error("couldn't find a compatible USB device");
		return 0;
	}

	/* A separate process will be forked to read data from the USB
	 * receiver and write it to a pipe. drv.fd is set to the readable
	 * end of this pipe. */
	if (pipe(pipe_fd) != 0) {
		log_perror_err("couldn't open pipe");
		return 0;
	}
	drv.fd = pipe_fd[0];

	dev_handle = usb_open(usb_dev);
	if (dev_handle == NULL) {
		log_perror_err("couldn't open USB receiver");
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
static int dfc_deinit(void)
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

	if (child > 1)
		if ((kill(child, SIGTERM) == -1) || (waitpid(child, NULL, 0) == 0))
			err = 1;

	return !err;
}

static char* dfc_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear()) {
		dfc_deinit();
		return NULL;
	}
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

/* this function is run in a forked process to read data from the USB
 * receiver and write it to the given fd. it calls exit() with result
 * code 0 on success, or 1 on failure. */
static void usb_read_loop(int fd)
{
	int err = 0;
	char rcv_code[6];
	int ptr = 0, count;

	alarm(0);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);

	for (;; ) {
		char buf[16];   // CODE_BYTES
		int bytes_r, bytes_w, pos;

		/* read from the USB device */
		bytes_r =
			usb_control_msg(dev_handle,
					USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN, 3, 0, 0, &buf[0],
					sizeof(buf), USB_TIMEOUT);

		if (bytes_r < 0) {
			if (errno == EAGAIN || errno == ETIMEDOUT)
				continue;
			log_error("can't read from USB device: %s", strerror(errno));
			err = 1;
			goto done;
		}

		if (bytes_r > 1) {
			for (count = 1; count < bytes_r; count++) {
				rcv_code[ptr++] = buf[count];
				if (ptr == 6) {
					/* write to the pipe */
					for (pos = 0; pos < ptr; pos += bytes_w) {
						bytes_w = write(fd, rcv_code + pos, ptr - pos);
						if (bytes_w < 0) {
							log_error(
								  "can't write to pipe: %s", strerror(errno));
							err = 1;
							goto done;
						}
					}

					ptr = 0;
				}
			}
		}
	}

done:
	close(fd);
	if (!usb_close(dev_handle))
		err = 1;
	_exit(err);
}
