/****************************************************************************
 ** hw_mp3anywhere.c ********************************************************
 ****************************************************************************
 *
 * routines for X10 MP3 Anywhere receiver
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * 	modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
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


static unsigned char b[NUMBYTES];
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;

//Forwards:
int mp3anywhere_decode(struct ir_remote *remote, struct decode_ctx_t* ctx);
int mp3anywhere_init(void);
int mp3anywhere_deinit(void);
char *mp3anywhere_rec(struct ir_remote *remotes);


const struct driver hw_mp3anywhere = {
	.name		=	"mp3anywhere",
	.device		=	LIRC_IRTTY,
	.features	=	LIRC_CAN_REC_LIRCCODE,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	8,
	.init_func	=	mp3anywhere_init,
	.deinit_func	=	mp3anywhere_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,
	.rec_func	=	mp3anywhere_rec,
	.decode_func	=	mp3anywhere_decode,
	.drvctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=	"No info available"
};

const struct driver* hardwares[] = { &hw_mp3anywhere, (const struct driver*)NULL };


int mp3anywhere_decode(struct ir_remote *remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 24, pre, 8, code, 0, 0)) {
		return (0);
	}

	map_gap(remote, ctx, &start, &last, signal_length);

	return (1);
}

int mp3anywhere_init(void)
{
	signal_length = drv.code_length * 1000000 / 9600;

	if (!tty_create_lock(drv.device)) {
		logprintf(LIRC_ERROR, "could not create lock files");
		return (0);
	}
	if ((drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0) {
		logprintf(LIRC_ERROR, "could not open %s", drv.device);
		logperror(LIRC_ERROR, "mp3anywhere_init()");
		tty_delete_lock();
		return (0);
	}
	if (!tty_reset(drv.fd)) {
		logprintf(LIRC_ERROR, "could not reset tty");
		mp3anywhere_deinit();
		return (0);
	}
	if (!tty_setbaud(drv.fd, 9600)) {
		logprintf(LIRC_ERROR, "could not set baud rate");
		mp3anywhere_deinit();
		return (0);
	}
	return (1);
}

int mp3anywhere_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return (1);
}

char *mp3anywhere_rec(struct ir_remote *remotes)
{
	char *m;
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
			LOGPRINTF(0, "buffer overflow at byte %d", i);
			break;
		}
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				LOGPRINTF(0, "timeout reading byte %d", i);
				return (NULL);
			}
		}
		if (read(drv.fd, &b[i], 1) != 1) {
			logprintf(LIRC_ERROR, "reading of byte %d failed", i);
			logperror(LIRC_ERROR, NULL);
			return (NULL);
		}
		if (b[1] != 0xd5 || b[2] != 0xaa || b[3] != 0xee || b[5] != 0xad) {
			logprintf(LIRC_ERROR, "bad envelope");
			return (NULL);
		}
		LOGPRINTF(1, "byte %d: %02x", i, b[i]);
	}
	gettimeofday(&end, NULL);

	pre = 0xD5AAEE;
	code = (ir_code) b[4];

	m = decode_all(remotes);
	return (m);
}
