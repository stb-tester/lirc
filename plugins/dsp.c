/****************************************************************************
** dsp.c ****************************************************************
****************************************************************************
*
* routines for diode in microphone input
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
* Copyright (C) 2001, 2002 Pavel Machek <pavel@ucw.cz>
*
* Distribute under GPL version 2 or later.
*
* This is hardware for "simplest ir receiver". Simplest ir receiver
* consists of BPW34 receiving diode connected to your microphone
* port. (Find a way where it generates loudest noise when you press
* transmit ir near it).
*
* BPW34 is not good selection (range is about meter, I can get better
* results with other diode), but at least its tested. If you know
* better hw to use, let me know.
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
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
#include <sys/soundcard.h>

#include "lirc_driver.h"

static const logchannel_t logchannel = LOG_DRIVER;

/*
 * decoding stuff
 */

static int myfd = -1;

#define BUFSIZE 20
#define SAMPLE 47999

lirc_t dsp_readdata(lirc_t timeout)
{
	lirc_t data;
	static int lastlength, laststate;
	int i;
	signed short buf[BUFSIZE];
	double energy = 0.0;
	int state;

	while (1) {
		if (read(myfd, buf, BUFSIZE * 2) != BUFSIZE * 2)
			log_perror_err("could not read in simple...");

		for (i = 0; i < BUFSIZE - 1; i++)
			energy += ((double)buf[i] - buf[i + 1]) * ((double)buf[i] - buf[i + 1]);
		energy /= BUFSIZE;
		energy /= 2E4;

		state = (energy > 2.0);
		if (state == laststate) {
			lastlength += ((1000000 / SAMPLE) * BUFSIZE);
		} else {
			data = lastlength | (laststate ? PULSE_BIT : 0);
			lastlength = ((1000000 / SAMPLE) * BUFSIZE);
			laststate = state;
			log_trace("Pulse came %8x,  %8d...", data, data & ~PULSE_BIT);
			return data;
		}

		timeout -= BUFSIZE * 1000000 / SAMPLE;
		if (timeout <= 0)
			return 0;
	}
	return 0;
}

/*
 * interface functions
 */
int dsp_init(void)
{
	int speed = SAMPLE, fmt = AFMT_S16_LE;

	log_info("Initializing %s...", drv.device);
	rec_buffer_init();
	drv.fd = open(drv.device, O_RDONLY);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		log_perror_err("dsp_init()");
		return 0;
	}

	if (ioctl(drv.fd, SNDCTL_DSP_SPEED, &speed) < 0) {
		log_error("could not ioctl(SPEED) on %s", drv.device);
		log_perror_err("dsp_init()");
		return 0;
	}
	if (speed != SAMPLE) {
		log_error("wrong speed handshaked on %s", drv.device);
		log_perror_err("dsp_init()");
		return 0;
	}
	if (ioctl(drv.fd, SNDCTL_DSP_SETFMT, &fmt) < 0) {
		log_error("could not ioctl(SETFMT) on %s", drv.device);
		log_perror_err("dsp_init()");
		return 0;
	}
	if (fmt != AFMT_S16_LE) {
		log_error("wrong format handshaked on %s", drv.device);
		log_perror_err("dsp_init()");
		return 0;
	}
	myfd = drv.fd;
	/* select on soundcard does not work */
	drv.fd = open("/dev/zero", O_RDONLY);
	return 1;
}

int dsp_deinit(void)
{
	close(drv.fd);
	close(myfd);
	return 1;
}

char* dsp_rec(struct ir_remote* remotes)
{
	if (!rec_buffer_clear())
		return NULL;
	return decode_all(remotes);
}

const struct driver hw_dsp = {
	.name		= "dsp",
	.device		= "/dev/dsp",
	.features	= LIRC_CAN_REC_MODE2,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_MODE2,
	.code_length	= 0,
	.init_func	= dsp_init,
	.deinit_func	= dsp_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= dsp_rec,
	.decode_func	= receive_decode,
	.drvctl_func	= NULL,
	.readdata	= dsp_readdata,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/dsp",
};


const struct driver* hardwares[] = { &hw_dsp, (const struct driver*)NULL };
