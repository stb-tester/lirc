/****************************************************************************
** hw_uirt2.c **************************************************************
****************************************************************************
*
* routines for UIRT2 receiver using the UIR mode
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
*      modified for UIRT2 receiver by
*      Mikael Magnusson <mikma@users.sourceforge.net>
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
# include <config.h>
#endif

#ifndef LIRC_IRTTY
#define LIRC_IRTTY "/dev/ttyS0"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "lirc_driver.h"
#include "lirc/serial.h"

#define NUMBYTES 6
#define TIMEOUT 20000

static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b[NUMBYTES];
static struct timeval start, end, last;
static ir_code code;

static int uirt2_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int uirt2_init(void);
static int uirt2_deinit(void);
static char* uirt2_rec(struct ir_remote* remotes);

const struct driver hw_uirt2 = {
	.name		= "uirt2",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 8 * NUMBYTES,
	.init_func	= uirt2_init,
	.deinit_func	= uirt2_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= uirt2_rec,
	.decode_func	= uirt2_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_uirt2, (const struct driver*)NULL };


static int uirt2_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 0, 0, 8 * NUMBYTES, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, 0);

	return 1;
}

static int uirt2_init(void)
{
	if (!tty_create_lock(drv.device)) {
		log_error("uirt2: could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("uirt2: could not open %s", drv.device);
		log_perror_err("uirt2: ");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("uirt2: could not reset tty");
		uirt2_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 115200)) {
		log_error("uirt2: could not set baud rate");
		uirt2_deinit();
		return 0;
	}
	if (!tty_setcsize(drv.fd, 8)) {
		log_error("uirt2: could not set csize");
		uirt2_deinit();
		return 0;
	}
	if (!tty_setrtscts(drv.fd, 1)) {
		log_error("uirt2: could not enable hardware flow");
		uirt2_deinit();
		return 0;
	}
	return 1;
}

static int uirt2_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

static char* uirt2_rec(struct ir_remote* remotes)
{
	char* m;
	int i;

	last = end;
	gettimeofday(&start, NULL);
	for (i = 0; i < NUMBYTES; i++) {
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				log_error("uirt2: timeout reading byte %d", i);
				return NULL;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			log_error("uirt2: reading of byte %d failed", i);
			log_perror_err(NULL);
			return NULL;
		}
		log_trace("byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	/* mark as Irman */
	code = 0xffff;
	code <<= 16;

	code = ((ir_code)b[0]);
	code = code << 8;
	code |= ((ir_code)b[1]);
	code = code << 8;
	code |= ((ir_code)b[2]);
	code = code << 8;
	code |= ((ir_code)b[3]);
	code = code << 8;
	code |= ((ir_code)b[4]);
	code = code << 8;
	code |= ((ir_code)b[5]);

	log_trace("code: %llx", (__u64)code);

	m = decode_all(remotes);
	return m;
}
