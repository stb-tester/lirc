/****************************************************************************
** hw_logitech.c ***********************************************************
****************************************************************************
*
* routines for Logitech receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
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

#define NUMBYTES 16
#define TIMEOUT 50000


static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b[NUMBYTES];
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;

//Forwards
static int logitech_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int logitech_init(void);
static int logitech_deinit(void);
static char* logitech_rec(struct ir_remote* remotes);

struct driver hw_logitech = {
	.name		= "logitech",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 16,
	.init_func	= logitech_init,
	.deinit_func	= logitech_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= logitech_rec,
	.decode_func	= logitech_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_logitech, (struct driver*)NULL };


int logitech_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 8, pre, 8, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int logitech_init(void)
{
	signal_length = drv.code_length * 1000000 / 1200;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("logitech_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		logitech_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 1200)) {
		log_error("could not set baud rate");
		logitech_deinit();
		return 0;
	}
	return 1;
}

int logitech_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* logitech_rec(struct ir_remote* remotes)
{
	char* m;
	int i = 0;
	int repeat, mouse_event;

	b[i] = 0x00;

	last = end;
	gettimeofday(&start, NULL);
	while (b[i] != 0xAA) {
		i++;
		if (i >= NUMBYTES) {
			log_trace("buffer overflow at byte %d", i);
			break;
		}
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				log_trace("timeout reading byte %d", i);
				return NULL;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			log_error("reading of byte %d failed", i);
			log_perror_err(NULL);
			return NULL;
		}
		log_trace("byte %d: %02x", i, b[i]);
		if (b[i] >= 0x40 && b[i] <= 0x6F) {
			mouse_event = b[i];
			b[1] = 0xA0;
			b[2] = mouse_event;
			log_trace("mouse event: %02x", mouse_event);
			break;
		}
	}
	gettimeofday(&end, NULL);

	if (b[1] == 0xA0)
		repeat = 0;
	else
		repeat = 1;
	if (!repeat)
		pre = (ir_code)b[1];
	else
		pre = 0xA0;
	code = (ir_code)b[2];

	m = decode_all(remotes);
	return m;
}
