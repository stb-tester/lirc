/****************************************************************************
** hw_mp3anywhere.c ********************************************************
****************************************************************************
*
* routines for X10 MP3 Anywhere receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
*	modified for X10 receiver by Shawn Nycz <dscordia@eden.rutgers.edu>
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

//Forwards:
static int mp3anywhere_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int mp3anywhere_init(void);
static int mp3anywhere_deinit(void);
static char* mp3anywhere_rec(struct ir_remote* remotes);


const struct driver hw_mp3anywhere = {
	.name		= "mp3anywhere",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 8,
	.init_func	= mp3anywhere_init,
	.deinit_func	= mp3anywhere_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= mp3anywhere_rec,
	.decode_func	= mp3anywhere_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_mp3anywhere, (const struct driver*)NULL };


int mp3anywhere_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 24, pre, 8, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int mp3anywhere_init(void)
{
	signal_length = drv.code_length * 1000000 / 9600;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("mp3anywhere_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		mp3anywhere_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 9600)) {
		log_error("could not set baud rate");
		mp3anywhere_deinit();
		return 0;
	}
	return 1;
}

int mp3anywhere_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* mp3anywhere_rec(struct ir_remote* remotes)
{
	char* m;
	int i = 0;

	b[0] = 0x00;
	b[1] = 0xd5;
	b[2] = 0xaa;
	b[3] = 0xee;
	b[5] = 0xad;

	last = end;
	gettimeofday(&start, NULL);
	while (b[i] != 0xAD) {
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
		if (b[1] != 0xd5 || b[2] != 0xaa || b[3] != 0xee || b[5] != 0xad) {
			log_error("bad envelope");
			return NULL;
		}
		log_trace("byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	pre = 0xD5AAEE;
	code = (ir_code)b[4];

	m = decode_all(remotes);
	return m;
}
