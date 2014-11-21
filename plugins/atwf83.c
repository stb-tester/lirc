/****************************************************************************
 ** hw_atwf83.c *************************************************************
 ****************************************************************************
 *
 * Lirc driver for Aureal remote (ATWF@83-W001 ESKY.CC USB_V3B)
 *
 * Copyright (C) 2010 Romain Henriet <romain-devel@laposte.net>
 *
 * Distribute under GPL version 2 or later.
 *
 */

#include <stdio.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <signal.h>

#include "lirc_driver.h"

enum {
	RPT_NO = 0,
	RPT_YES = 1,
};

static int atwf83_init();
static int atwf83_deinit();
static char *atwf83_rec(struct ir_remote *remotes);
static int atwf83_decode(struct ir_remote *remote, struct decode_ctx_t* ctx);
static void *atwf83_repeat();

/** Max number of repetitions */
const unsigned max_repeat_count = 500;
/** Code that triggers key release */
const unsigned release_code = 0x00000000;
/** Code that triggers device remove  */
const unsigned remove_code =0x00FFFFFF;
/** Time to wait before first repetition */
const unsigned repeat_time1_us = 500000;
/** Time to wait between two repetitions */
const unsigned repeat_time2_us = 100000;
/** Pipe between main thread and repetition thread */
static int fd_pipe[2] = { -1, -1 };

/** Thread that simulates repetitions */
static pthread_t repeat_thread;
/** File descriptor for the real device */
static int fd_hidraw;

const int main_code_length = 32;
static signed int main_code = 0;
static struct timeval start, end, last;
static int repeat_state = RPT_NO;

/* Aureal USB iR Receiver */
const struct driver hw_atwf83 = {
	.name		=	"atwf83",
	.device		=	"/dev/hidraw0",
	.fd		=	-1,
	.features	=	LIRC_CAN_REC_LIRCCODE,
	.send_mode	=	0,
	.rec_mode	=	LIRC_MODE_LIRCCODE,
	.code_length	=	32,
	.init_func	=	atwf83_init,
	.deinit_func	=	atwf83_deinit,
	.open_func	=	default_open,
	.close_func	=	default_close,
	.send_func	=	NULL,
	.rec_func	=	atwf83_rec,
	.decode_func	=	atwf83_decode,
	.drvctl_func	=	NULL,
	.readdata	=	NULL,
	.api_version	=	2,
	.driver_version = 	"0.9.2",
	.info		=  	"No info available."
};

const struct driver* hardwares[] = { &hw_atwf83, (const struct driver*)NULL };


static int atwf83_decode(struct ir_remote *remote, struct decode_ctx_t* ctx)
{
	LOGPRINTF(1, "atwf83_decode");

	if (!map_code(remote, ctx, 0, 0, main_code_length, main_code, 0, 0)) {
		return 0;
	}

	map_gap(remote, ctx, &start, &last, 0);
	/* override repeat */
	ctx->repeat_flag = repeat_state;

	return 1;
}

static int atwf83_init()
{
	logprintf(LIRC_INFO, "initializing '%s'", drv.device);
	if ((fd_hidraw = open(drv.device, O_RDONLY)) < 0) {
		logprintf(LIRC_ERROR, "unable to open '%s'", drv.device);
		return 0;
	}
	drv.fd = fd_hidraw;

	/* Create pipe so that events sent by the repeat thread will
	   trigger main thread */
	if (pipe(fd_pipe) != 0) {
		logperror(LIRC_ERROR, "couldn't open pipe");
		close(fd_hidraw);
		return 0;
	}
	drv.fd = fd_pipe[0];
	/* Create thread to simulate repetitions */
	if (pthread_create(&repeat_thread, NULL, atwf83_repeat, NULL)) {
		logprintf(LIRC_ERROR, "Could not create \"repeat thread\"");
		return 0;
	}
	return 1;
}

static int atwf83_deinit()
{
	pthread_cancel(repeat_thread);
	if (fd_hidraw != -1) {
		// Close device if it is open
		logprintf(LIRC_INFO, "closing '%s'", drv.device);
		close(fd_hidraw);
		fd_hidraw = -1;
	}
	// Close pipe input
	if (fd_pipe[1] >= 0) {
		close(fd_pipe[1]);
		fd_pipe[1] = -1;
	}
	// Close pipe output
	if (fd_pipe[0] >= 0) {
		close(fd_pipe[0]);
		fd_pipe[0] = -1;
	}
	drv.fd = -1;
	return 1;
}

/**
 *	Runtime that reads device, forwards codes to main thread
 *	and simulates repetitions.
 */
static void *atwf83_repeat()
{
	int repeat_count = 0;
	unsigned ev[2];
	unsigned current_code;
	int rd, sel;
	fd_set files;
	struct timeval delay;
	int pressed = 0;
	int fd = fd_pipe[1];

	while (1) {
		// Initialize set to monitor device's events
		FD_ZERO(&files);
		FD_SET(fd_hidraw, &files);
		if (pressed) {
			sel = select(FD_SETSIZE, &files, NULL, NULL, &delay);
		} else {
			sel = select(FD_SETSIZE, &files, NULL, NULL, NULL);
		}

		switch (sel) {
		case 1:
			// Data ready in device's file
			rd = read(fd_hidraw, ev, sizeof(ev));

			if (rd == -1) {
				// Error
				logprintf(LIRC_ERROR, "(%s) Could not read %s", __FUNCTION__, drv.device);
				goto exit_loop;
			}
			if ((rd == 8 && ev[0] != 0) || (rd == 6 && ev[0] > 2)) {
				// Key code : forward it to main thread
				pressed = 1;
				repeat_count = 0;
				delay.tv_sec = 0;
				delay.tv_usec = repeat_time1_us;
				current_code = ev[0];
			} else {
				// Release code : stop repetitions
				pressed = 0;
				current_code = release_code;
			}
			break;
		case 0:
			repeat_count++;
			if (repeat_count >= max_repeat_count) {
				// Too many repetitions, something must have gone wrong
				logprintf(LIRC_ERROR,"(%s) too many repetitions", __FUNCTION__);
				goto exit_loop;
			}
			// Timeout : send current_code again to main
			//           thread to simulate repetition
			delay.tv_sec = 0;
			delay.tv_usec = repeat_time2_us;
			break;
		default:
			// Error
			logprintf(LIRC_ERROR, "(%s) select() failed", __FUNCTION__);
			goto exit_loop;
		}
		// Send code to main thread through pipe
		chk_write(fd, &current_code, sizeof(current_code));
	}
exit_loop:

	// Wake up main thread with special key code
	current_code = remove_code;
	chk_write(fd, &current_code, sizeof(current_code));
	return NULL;
}

/*
*  Aureal Technology ATWF@83 cheap remote
*  specific code.
*/

static char *atwf83_rec(struct ir_remote *remotes)
{
	unsigned ev;
	int rd;
	last = end;
	gettimeofday(&start, NULL);
	rd = read(drv.fd, &ev, sizeof(ev));

	if (rd == -1) {
		// Error
		logprintf(LIRC_ERROR, "(%s) could not read pipe", __FUNCTION__);
		atwf83_deinit();
		return 0;
	}

	if (ev == release_code) {
		// Release code
		main_code = 0;
		return 0;
	} else if (ev == remove_code) {
		// Device has been removed
		atwf83_deinit();
		return 0;
	}

	LOGPRINTF(1, "atwf83 : %x", ev);
	// Record the code and check for repetition
	if (main_code == ev) {
		repeat_state = RPT_YES;
	} else {
		main_code = ev;
		repeat_state = RPT_NO;
	}
	gettimeofday(&end, NULL);
	return decode_all(remotes);
}
