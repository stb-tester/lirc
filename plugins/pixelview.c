/****************************************************************************
** hw_pixelview.c **********************************************************
****************************************************************************
*
* routines for PixelView Play TV receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
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


static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b[3];
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;

//forwards:
static int pixelview_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int pixelview_init(void);
static int pixelview_deinit(void);
static char* pixelview_rec(struct ir_remote* remotes);


const struct driver hw_pixelview = {
	.name		= "pixelview",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 30,
	.init_func	= pixelview_init,
	.deinit_func	= pixelview_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= pixelview_rec,
	.decode_func	= pixelview_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_pixelview, (const struct driver*)NULL };


int pixelview_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
#if 0
	if (remote->pone != 0 || remote->sone != 833)
		return 0;
	if (remote->pzero != 833 || remote->szero != 0)
		return 0;
#endif

	if (!map_code(remote, ctx, 10, pre, 20, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int pixelview_init(void)
{
	signal_length = drv.code_length * 1000000 / 1200;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("pixelview_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		pixelview_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 1200)) {
		log_error("could not set baud rate");
		pixelview_deinit();
		return 0;
	}
	return 1;
}

int pixelview_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* pixelview_rec(struct ir_remote* remotes)
{
	char* m;
	int i;

	last = end;
	gettimeofday(&start, NULL);
	for (i = 0; i < 3; i++) {
		if (i > 0) {
			if (!waitfordata(50000)) {
				log_error("timeout reading byte %d", i);
				return NULL;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			log_error("reading of byte %d failed", i);
			log_perror_err(NULL);
			return NULL;
		}
		log_trace("byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	pre = (reverse((ir_code)b[0], 8) << 1) | 1;
	code = (reverse((ir_code)b[1], 8) << 1) | 1;
	code = code << 10;
	code |= (reverse((ir_code)b[2], 8) << 1) | 1;

	m = decode_all(remotes);
	return m;
}
