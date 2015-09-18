/****************************************************************************
** hw_creative.c ***********************************************************
****************************************************************************
*
* routines for Creative receiver
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
#include <sys/ioctl.h>

#include "lirc_driver.h"
#include "lirc/serial.h"


#define NUMBYTES 6
#define TIMEOUT 20000

static const logchannel_t logchannel = LOG_DRIVER;

unsigned char b[NUMBYTES];
struct timeval start, end, last;
lirc_t gap, signal_length;
ir_code pre, code;

//Forwards:
static int creative_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int creative_init(void);
static int creative_deinit(void);
static char* creative_rec(struct ir_remote* remotes);


const struct driver hw_creative = {
	.name		= "creative",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= creative_init,
	.deinit_func	= creative_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= creative_rec,
	.decode_func	= creative_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available.",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_creative, (const struct driver*)NULL };


int creative_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 16, pre, 16, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int creative_init(void)
{
	signal_length = 108000;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("creative_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		creative_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 2400)) {
		log_error("could not set baud rate");
		creative_deinit();
		return 0;
	}
	return 1;
}

int creative_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* creative_rec(struct ir_remote* remotes)
{
	char* m;
	int i;

	b[0] = 0x4d;
	b[1] = 0x05;
	b[4] = 0xac;
	b[5] = 0x21;

	last = end;
	gettimeofday(&start, NULL);
	for (i = 0; i < NUMBYTES; i++) {
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				log_error("timeout reading byte %d", i);
				return NULL;
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			log_perror_err("reading of byte %d failed", i);
			return NULL;
		}
		if (b[0] != 0x4d || b[1] != 0x05 /* || b[4]!=0xac || b[5]!=0x21 */) {
			log_error("bad envelope");
			return NULL;
		}
		if (i == 5) {
			if (b[2] != ((~b[3]) & 0xff)) {
				log_error("bad checksum");
				return NULL;
			}
		}
		log_trace("byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	/* pre=0x8435; */
	pre = reverse((((ir_code)b[4]) << 8) | ((ir_code)b[5]), 16);
	code = reverse((((ir_code)b[2]) << 8) | ((ir_code)b[3]), 16);

	m = decode_all(remotes);
	return m;
}
