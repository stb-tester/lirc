
/****************************************************************************
 ** hw_irman.c **********************************************************
 ****************************************************************************
 *
 * routines for Irman
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <irman.h>

#include "lirc_driver.h"*
#include "lirc/serial.h"

extern struct ir_remote *repeat_remote, *last_remote;

unsigned char *codestring;
struct timeval start, end, last;
lirc_t gap;
ir_code code;

#define CODE_LENGTH 64

//Forwards:
int irman_decode(struct ir_remote *remote,
                 ir_code * prep, ir_code * codep, ir_code * postp,
                 int *repeat_flagp,
		 lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
int irman_init(void);
int irman_deinit(void);
char *irman_rec(struct ir_remote *remotes);


const struct hardware hw_irman = {
	.name		=	"irman",
	.device		=	LIRC_IRTTY,
	.features	=	LIRC_CAN_REC_LIRCCODE,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	CODE_LENGTH,
	.init_func	=	irman_init,
	.deinit_func	=	irman_deinit,
	.send_func	=	NULL,
	.rec_func	=	irman_rec,
	.decode_func	=	irman_decode,
	.ioctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2"
};

const struct hardware* hardwares[] = { &hw_irman, (const struct hardware*)NULL };


int irman_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		 lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	if (!map_code(remote, prep, codep, postp, 0, 0, CODE_LENGTH, code, 0, 0)) {
		return 0;
	}

	map_gap(remote, &start, &last, 0, repeat_flagp, min_remaining_gapp, max_remaining_gapp);

	return (1);
}

int irman_init(void)
{
	if (!tty_create_lock(hw.device)) {
		logprintf(LOG_ERR, "could not create lock files");
		return (0);
	}
	if ((hw.fd = ir_init(hw.device)) < 0) {
		logprintf(LOG_ERR, "could not open %s", hw.device);
		logperror(LOG_ERR, "irman_init()");
		tty_delete_lock();
		return (0);
	}
	return (1);
}

int irman_deinit(void)
{
	ir_finish();
	sleep(1);		/* give hardware enough time to reset */
	close(hw.fd);
	tty_delete_lock();
	return (1);
}

char *irman_rec(struct ir_remote *remotes)
{
	static char *text = NULL;
	int i;

	last = end;
	gettimeofday(&start, NULL);
	codestring = ir_get_code();
	gettimeofday(&end, NULL);
	if (codestring == NULL) {
		if (errno == IR_EDUPCODE) {
			LOGPRINTF(1, "received \"%s\" (dup)", text ? text : "(null - bug)");
		} else if (errno == IR_EDISABLED) {
			LOGPRINTF(1, "irman not initialised (this is a bug)");
		} else {
			LOGPRINTF(1, "error reading code: \"%s\"", ir_strerror(errno));
		}
		if (errno == IR_EDUPCODE) {
			return decode_all(remotes);
		}
		return NULL;
	}

	text = ir_code_to_text(codestring);
	LOGPRINTF(1, "received \"%s\"", text);

	/* this is only historical but it's necessary for
	   compatibility to older versions and it's handy to
	   recognize Irman config files */
	code = 0xffff;

	for (i = 0; i < IR_CODE_LEN; i++) {
		code = code << 8;
		code = code | (ir_code) (unsigned char)codestring[i];
	}

	return decode_all(remotes);
}