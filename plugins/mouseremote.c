/****************************************************************************
** hw_mouseremote.c ********************************************************
****************************************************************************
*
* routines for X10 Mouse Remote
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
*	modified for X10 receiver by Shawn Nycz <dscordia@eden.rutgers.edu>
*	modified for X10 MouseRemote by Brian Craft <bcboy@thecraftstudio.com>
*	removed dependency on multimouse by Geoffrey Hausheer <zcke0au02@sneakemail.com>
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

#include <termios.h>
#include <signal.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

#define TIMEOUT 50000


static const logchannel_t logchannel = LOG_DRIVER;

static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;
static int serial_input;

//Forwards:
static int mouseremote_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int mouseremote_init(void);
static int mouseremote_ps2_init(void);
static int mouseremote_deinit(void);
static char* mouseremote_rec(struct ir_remote* remotes);


const struct driver hw_mouseremote = {
	.name		= "mouseremote",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= mouseremote_init,
	.deinit_func	= mouseremote_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= mouseremote_rec,
	.decode_func	= mouseremote_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver hw_mouseremote_ps2 = {
	.name		= "mouseremote_ps2",
	.device		= "/dev/psaux",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= mouseremote_ps2_init,
	.deinit_func	= mouseremote_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= mouseremote_rec,
	.decode_func	= mouseremote_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_mouseremote,
				     &hw_mouseremote_ps2,
				     (const struct driver*)NULL };


int mouseremote_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 8, 0x08, 16, code, 8, 0x7f))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int mouseremote_init(void)
{
	serial_input = 1;
	signal_length = drv.code_length * 1000000 / 1200;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("mouseremote_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		mouseremote_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 1200)) {
		log_error("could not set baud rate");
		mouseremote_deinit();
		return 0;
	}
	if (!tty_setcsize(drv.fd, 7)) {
		log_error("could not set character size");
		mouseremote_deinit();
		return 0;
	}
	return 1;
}

int mouseremote_ps2_init(void)
{
	serial_input = 0;
	signal_length = drv.code_length * 1000000 / 1200;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("mouseremote_ps2_init()");
		tty_delete_lock();
		return 0;
	}
	return 1;
}

int mouseremote_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* mouseremote_rec(struct ir_remote* remotes)
{
	char* m;
	int i = 0, dx = 0, dy = 0, stat = 0;

#define NUMBYTES 3
	unsigned char b[NUMBYTES];

	b[0] = 0x08;
	b[2] = 0x7f;

	pre = 0x08;

	last = end;
	gettimeofday(&start, NULL);
	while (i < 3) {
		int val;

		if (!waitfordata(TIMEOUT)) {
			log_trace("timeout reading byte %d", i);
			return NULL;
		}
		val = read(drv.fd, &b[i], 1);
		if (val != 1) {
			log_error("reading of byte %d (%d) failed", i, val);
			log_perror_err(NULL);
			return NULL;
		}
		if (i == 0 && ((serial_input && (b[i] & 0xC0) != 0x40) || (!serial_input && (b[i] & 0x0C) != 0x08)))
			continue;
		if (serial_input && i && ((b[i] & 0x40) || (b[i] == 0x80))) {
			/* the PS/2 initialization isn't unique
			 * enough to check the stream for
			 */
			i = 0;
			continue;
		}
		log_trace("byte %d: %02x", i, b[i]);
		++i;
	}
	gettimeofday(&end, NULL);

	if (serial_input) {
		if (((char)(b[0]) & 0x0c) != 0x0c && (char)(b[2]) == 0x3f && ((char)(b[2]) & 0x07)) {
			code = (ir_code)(char)(b[1]) | (((char)(b[0]) & 0x03) << 6);
			log_trace("result %llx", (__u64)code);
			m = decode_all(remotes);
			return m;
		}
		stat = ((b[0] & 0x20) >> 3) | ((b[0] & 0x10) >> 4);
		dx = (char)(((b[0] & 0x03) << 6) | (b[1] & 0x3F));
		dy = -((char)(((b[0] & 0x0C) << 4) | (b[2] & 0x3F)));
	} else {
		if ((char)b[2] == 0x7f) {
			if ((char)b[0] != 0x08) {
				log_trace("Bad data");
				return NULL;
			}
			code = (ir_code)b[1];
			log_trace("result %llx", (__u64)code);
			m = decode_all(remotes);
			return m;
		}
		stat = ((b[0] & 0x01) << 2) | ((b[0] & 0x06) >> 1);
		dx = (char)b[1];
		dy = (char)b[2];
	}
	code = 0;
	if (dy < 0) {
		dy = -dy;
		code |= 0x80;
	}
	if (dx < 0) {
		dx = -dx;
		code |= 0x08;
	}
	if (dy == 1 || dy == 2 || dy == 8) {
		code |= 0x10;
		if (dy == 2 && dx != 1)
			code |= 0x0200;
		else if (dy == 8)
			code |= 0x0400;
	}
	if (dx == 1 || dx == 2 || dx == 8) {
		code |= 0x01;
		if (dx == 2 && dy != 1)
			code |= 0x0200;
		else if (dx == 8)
			code |= 0x0400;
	}
	if (dy == 4 || dy == 16)
		code |= 0x30;
	else if (dx == 4 || dx == 16)
		code |= 0x03;
	if (code != 0) {
		code |= 0x0100;
		log_trace("result %llx", (__u64)code);
		m = decode_all(remotes);
		return m;
	} else if (dx == 0 && dy == 0) {
		code = 0x0800 | stat;
		log_trace("result %llx", (__u64)code);
		m = decode_all(remotes);
		return m;
	}
	log_trace("fallthrough is bad!%d %d %d", dx, dy, stat);
	return NULL;
}
