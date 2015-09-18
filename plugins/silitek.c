/****************************************************************************
** hw_silitek.c ***********************************************************
****************************************************************************
*
* routines for Silitek receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
*	        modified for silitek receiver by Krister Wicksell
*	        <krister.wicksell@spray.se>
* */

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
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "lirc_driver.h"
#include "lirc/serial.h"

#define NUMBYTES 3
#define TIMEOUT 20000


static const logchannel_t logchannel = LOG_DRIVER;

unsigned char b[NUMBYTES];
ir_code code;
struct timeval current, last;
int do_repeat;

//Forwards:
static int silitek_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int silitek_init(void);
static int silitek_deinit(void);
static char* silitek_rec(struct ir_remote* remotes);


const struct driver hw_silitek = {
	.name		= "silitek",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 24,
	.init_func	= silitek_init,
	.deinit_func	= silitek_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= silitek_rec,
	.decode_func	= silitek_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_silitek, (const struct driver*)NULL };


int silitek_read(int fd, unsigned char* data, long timeout)
{
	if (!waitfordata(timeout))
		return 0;

	if (!read(fd, data, 1))
		return 0;

	return 1;
}

int silitek_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 0, 0, 24, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &current, &last, 0);

	if (!do_repeat)
		ctx->repeat_flag = 0;

	log_trace("repeat_flagp:           %d", ctx->repeat_flag);

	return 1;
}

int silitek_init(void)
{
	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}

	drv.fd = open(drv.device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("silitek_init()");
		tty_delete_lock();
		return 0;
	}

	if (!tty_reset(drv.fd)) {
		log_error("could not reset %s", drv.device);
		silitek_deinit();
		return 0;
	}

	if (!tty_setbaud(drv.fd, 1200)) {
		log_error("could not set baud rate on %s", drv.device);
		silitek_deinit();
		return 0;
	}

	return 1;
}

int silitek_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* silitek_rec(struct ir_remote* remotes)
{
	char* m;
	int mouse_x;
	int mouse_y;
	char sign = 0x00;       /* store sign of mouse direction. */
	char pos = 0x00;        /* store mouse direction. */

	do_repeat = 1;

	if (!silitek_read(drv.fd, &b[0], TIMEOUT)) {
		log_error("reading of byte 0 failed");
		log_perror_err(NULL);
		return NULL;
	}

	if ((b[0] != 0x3f) &&   /* button down */
	    (b[0] != 0x31) &&   /* button still down */
	    (b[0] != 0x2a) &&   /* button up */
	    (b[0] != 0x7c) &&   /* mouse event */
	    (b[0] != 0x7f) &&   /* mouse event, l+r-mouse button down */
	    (b[0] != 0xfd) &&   /* mouse event, r-mouse button down */
	    (b[0] != 0xfe))     /* mouse event, l-mouse button down */
		return NULL;

	last = current;

	if (!silitek_read(drv.fd, &b[1], TIMEOUT)) {
		log_error("reading of byte 1 failed");
		log_perror_err(NULL);
		return NULL;
	}

	if (!silitek_read(drv.fd, &b[2], TIMEOUT)) {
		log_error("reading of byte 2 failed");
		log_perror_err(NULL);
		return NULL;
	}

	/* mouse event ? */
	if ((b[0] == 0x7c) || (b[0] == 0x7f) || (b[0] == 0xfd) || (b[0] == 0xfe)) {
		/* if mouse was not moved check mouse-button state. */
		if ((b[1] == 0x80) && (b[2] == 0x80)) {
			switch (b[0]) {
			case 0xfd:
				b[1] = 0xa0;    /* r-mouse button */
				b[2] = 0xbb;
				break;
			case 0xfe:
				b[1] = 0x0a;    /* l-mouse button */
				b[2] = 0xbb;
				break;
			case 0x7f:
				b[1] = 0xaa;    /* l+r-mouse button */
				b[2] = 0xbb;
				break;
			}
		} else {
			/* calc mouse x movement */
			mouse_x = b[1] & 0x1f;
			/* calc mouse y movement */
			mouse_y = b[2] & 0x1f;

			/* if the joystick is pulled to the left */
			if ((b[1] & 0x20) == 0x20) {
				mouse_x = 32 - mouse_x;
				sign |= 0x10;
			}

			/* if the joystick is pulled up */
			if ((b[2] & 0x20) == 0x20) {
				mouse_y = 32 - mouse_y;
				sign |= 0x01;
			}

			/* calc mouse direction */
			if ((mouse_x > 0) && (mouse_y == 0))
				pos = 0x01;
			if ((mouse_x > mouse_y) && (mouse_y > 0))
				pos = 0x02;
			if ((mouse_x == mouse_y) && (mouse_x > 0))
				pos = 0x03;
			if ((mouse_y > mouse_x) && (mouse_x > 0))
				pos = 0x04;
			if ((mouse_y > 0) && (mouse_x == 0))
				pos = 0x05;

			b[1] = sign;
			b[2] = pos;

			/* if only a small mouse movement don't set
			 * repeat flag gets better control in lircmd
			 * this way */
			if ((mouse_x < 4) && (mouse_y < 4))
				do_repeat = 0;
		}
		b[0] = 0xaa;    /* set to indicate mouse event */
	} else {
		if (b[0] != 0x2a)
			b[0] = 0xbb;    /* set to indicate button down event */
		else
			b[0] = 0xcc;    /* set to indicate button up event */
	}

	code = ((ir_code)b[0] << 16) + ((ir_code)b[1] << 8) + (ir_code)b[2];
	gettimeofday(&current, NULL);

	m = decode_all(remotes);
	return m;
}
