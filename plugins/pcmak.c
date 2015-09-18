/****************************************************************************
** hw_pcmak.c ***********************************************************
****************************************************************************
*
* routines for Logitech receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for pcmak serial/USB-ftdi receiver P_awe_L <pablozrudnika@wp.pl>
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef LIRC_IRTTY
/* could also be /dev/ttyS0 if it's a serial receiver */
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

#include "lirc_driver.h"
#include "lirc/serial.h"

#define TIMEOUT 50000


static const logchannel_t logchannel = LOG_DRIVER;

static unsigned char b;
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code pre, code;
static int repeat_counter, pressed_key;

//Forwards:
static int pcmak_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static int pcmak_init(void);
static int pcmak_deinit(void);
static char* pcmak_rec(struct ir_remote* remotes);


const struct driver hw_pcmak = {
	.name		= "pcmak",
	.device		= LIRC_IRTTY,
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 16,
	.init_func	= pcmak_init,
	.deinit_func	= pcmak_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= pcmak_rec,
	.decode_func	= pcmak_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/tty[0-9]*",
};

const struct driver* hardwares[] = { &hw_pcmak, (const struct driver*)NULL };


int pcmak_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	if (!map_code(remote, ctx, 8, pre, 8, code, 0, 0))
		return 0;

	map_gap(remote, ctx, &start, &last, signal_length);

	return 1;
}

int pcmak_init(void)
{
	signal_length = drv.code_length * 1000000 / 1200;

	if (!tty_create_lock(drv.device)) {
		log_error("could not create lock files");
		return 0;
	}
	drv.fd = open(drv.device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("pcmak_init()");
		tty_delete_lock();
		return 0;
	}
	if (!tty_reset(drv.fd)) {
		log_error("could not reset tty");
		pcmak_deinit();
		return 0;
	}
	if (!tty_setbaud(drv.fd, 1200)) {
		log_error("could not set baud rate");
		pcmak_deinit();
		return 0;
	}
	return 1;
}

int pcmak_deinit(void)
{
	close(drv.fd);
	tty_delete_lock();
	return 1;
}

char* pcmak_rec(struct ir_remote* remotes)
{
	char* m;
	int i = 0;

	last = end;
	gettimeofday(&start, NULL);

	while (1) {
		i++;
		if (i > 0) {
			if (!waitfordata(TIMEOUT)) {
				log_trace("timeout reading byte %d", i);
				return NULL;
			}
		}

		if (read(drv.fd, &b, 1) != 1) {
			log_error("reading of byte %d failed", i);
			log_perror_err(NULL);
			return NULL;
		}
		log_trace("byte %d: %02x", i, b);
		if (b == 0xAA) {
			repeat_counter = 0;
		} else {
			/* Range of allowed button codes */
			if (/* PCMAK codes */
				(b >= 0x01 && b <= 0x2B) ||
				/* codes with shift button */
				(b >= 0x41 && b <= 0x6B) ||
				/* MINIMAK/MINIMAK LASER codes */
				(b >= 0x2F && b <= 0x31) ||
				/* MINIMAK codes with shift */
				b == 0x5F || b == 0x79 || b == 0x75) {
				if (repeat_counter < 1) {
					repeat_counter++;
					pressed_key = b;
				} else {
					if (pressed_key == b) {
						gettimeofday(&end, NULL);
						pre = 0xAA;
						code = (ir_code)b;
						m = decode_all(remotes);
						return m;
					}
				}
			}
		}
	}
	return NULL;
}
