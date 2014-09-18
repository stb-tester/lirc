
/****************************************************************************
 ** hw_caraca.c ***********************************************************
 ****************************************************************************
 *
 * routines for caraca receiver
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 * 	modified for caraca RC5 receiver by Konrad Riedel <k.riedel@gmx.de>
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
#include <caraca/caraca_client.h>

#include "lirc_driver.h"

#define NUMBYTES 34
#define TIMEOUT 20000


static unsigned char msg[NUMBYTES];
static struct timeval start, end, last;
static lirc_t signal_length;
static ir_code code;

// Forwards:
int caraca_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		  lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
int caraca_init(void);
int caraca_deinit(void);
char *caraca_rec(struct ir_remote *remotes);



const struct driver hw_caraca = {
	.name		=	"caraca"
	.device		=	NULL,
	.features	=	LIRC_CAN_REC_LIRCCODE,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	16,
	.init_func	=	caraca_init,
	.deinit_func	=	caraca_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,
	.rec_func	=	caraca_rec,
	.decode_func	=	caraca_decode
	.drvctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=  	"No info available."
};

const struct driver* hardwares[] = { &hw_caraca, (const struct driver*)NULL };


int caraca_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		  lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	if (!map_code(remote, prep, codep, postp, 0, 0, hw_caraca.code_length, code, 0, 0)) {
		return (0);
	}

	gap = 0;
	if (start.tv_sec - last.tv_sec >= 2) {	/* >1 sec */
		*repeat_flagp = 0;
	} else {
		gap = (start.tv_sec - last.tv_sec) * 1000000 + start.tv_usec - last.tv_usec;

		if (gap < 120000)
			*repeat_flagp = 1;
		else
			*repeat_flagp = 0;
	}

	*min_remaining_gapp = 0;
	*max_remaining_gapp = 0;
	LOGPRINTF(1, "code: %llx", (__u64) * codep);
	return (1);
}

int caraca_init(void)
{
	signal_length = drv.code_length * 1000000 / 1200;
	if ((drv.fd = caraca_open(PACKAGE)) < 0) {
		logprintf(LIRC_ERROR, "could not open lirc");
		logperror(LIRC_ERROR, "caraca_init()");
		return (0);
	}
	/*accept IR-Messages (16 : RC5 key code) for all nodes on the bus */
	if (set_filter(drv.fd, 0x400, 0x7c0, 0) <= 0) {
		logprintf(LIRC_ERROR, "could not set filter for IR-Messages");
		caraca_deinit();
		return (0);
	}
	return (1);
}

int caraca_deinit(void)
{
	close(drv.fd);
	return (1);
}

char *caraca_rec(struct ir_remote *remotes)
{
	char *m;
	int i = 0, node, ir, t;
	int repeat, mouse_event;

	last = end;
	gettimeofday(&start, NULL);
	i = read(drv.fd, msg, NUMBYTES);
	gettimeofday(&end, NULL);

	LOGPRINTF(1, "caraca_rec: %s", msg);
	sscanf(msg, "%d.%d:%d", &node, &t, &ir);

	/* transmit the node address as first byte, so we have
	 * different codes for every transmitting node (for every room
	 * of the house) */

	code = (ir_code) (node << 8) + ir;

	m = decode_all(remotes);
	return (m);
}
