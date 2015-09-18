/*****************************************************************************
** hw_usbx.c ****************************************************************
*****************************************************************************
* Routines for the ADSTech USBX-707 USB IR Blaster
*
* Only receiving is implemented.
*
* It uses a baudrate of 300kps on a USB serial device which, currently, is
* only supported by Linux.
* If someone knows how to set such a baudrate under other OS's, please add
* that functionality to daemons/serial.c to make this driver work for those
* OS's.
*
* Information on how to send with this device is greatly appreciated...
*
* Copyright (C) 2007 Jelle Foks <jelle@foks.8m.com>
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
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyUSB0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <termios.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b[6];
static ir_code code;

#define REPEAT_FLAG ((ir_code)0x1)
#define CODE_LENGTH 48

//Forwards:
static int usbx_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int usbx_init(void);
static int usbx_deinit(void);
static char* usbx_rec(struct ir_remote* remotes);


const struct driver hw_usbx = {
	.name		= "usbx",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= CODE_LENGTH,
	.init_func	= usbx_init,
	.deinit_func	= usbx_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= usbx_rec,
	.decode_func	= usbx_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/ttyUSB*",
};

const struct driver* hardwares[] = { &hw_usbx, (const struct driver*)NULL };


int usbx_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (remote->flags & CONST_LENGTH
	    || !map_code(remote, ctx, 0, 0, CODE_LENGTH, code & (~REPEAT_FLAG), 0, 0))
		return 0;
	/* the lsb in the code is the repeat flag */
	ctx->repeat_flag = code & REPEAT_FLAG ? 1 : 0;
	ctx->min_remaining_gap = min_gap(remote);
	ctx->max_remaining_gap = max_gap(remote);

	log_trace("repeat_flagp: %d", ctx->repeat_flag);
	log_trace("remote->gap range:      %lu %lu\n", (__u32)min_gap(remote), (__u32)max_gap(remote));
	log_trace("rem: %lu %lu", (__u32)remote->min_remaining_gap, (__u32)remote->max_remaining_gap);
	return 1;
}

int usbx_init(void)
{
	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files for '%s'", drv.device);
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		tty_delete_lock();
		log_error("Could not open the '%s' device", drv.device);
		return 0;
	}
	log_trace("device '%s' opened", drv.device);

	if (!tty_reset(drv.fd) || !tty_setbaud(drv.fd, 300000) || !tty_setrtscts(drv.fd, 1)) {
		log_error("could not configure the serial port for '%s'", drv.device);
		usbx_deinit();
		return 0;
	}

	return 1;
}

int usbx_deinit(void)
{
	close(drv.fd);
	drv.fd = -1;
	tty_delete_lock();
	return 1;
}

char* usbx_rec(struct ir_remote* remotes)
{
	char* m;
	int i, x;

	x = 0;
	for (i = 0; i < 6; i++) {
		if (i > 0) {
			if (!waitfordata(20000)) {
				log_trace("timeout reading byte %d", i);
				break;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			log_trace("reading of byte %d failed.", i);
			usbx_deinit();
			return NULL;
		}
		log_trace("byte %d: %02x", i, b[i]);
		x++;
	}
	code = 0;
	for (i = 0; i < x; i++) {
		code = code << 8;
		code |= ((ir_code)b[i]);
	}

	log_trace(" -> %0llx", (__u64)code);

	m = decode_all(remotes);
	return m;
}
