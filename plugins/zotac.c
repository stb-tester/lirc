/****************************************************************************
 ** hw_zotac.c *************************************************************
 ****************************************************************************
 *
 * Lirc driver for Zotac remote
 *
 * Copyright (C) 2010 Rainer Hochecker
 *
 * Distribute under GPL version 2 or later.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <linux/hiddev.h>
#include <sys/ioctl.h>

#include "lirc_private.h"

enum {
	RPT_NO = 0,
	RPT_YES = 1,
};

static int zotac_init();
static int zotac_deinit();
static char *zotac_rec(struct ir_remote *remotes);
static int zotac_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
			 lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
static void *zotac_repeat();
static int zotac_getcode();

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
static int error_state = 0;
static int probe_code = 0;

#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
/* Zotac USB iR Receiver */
struct hardware hw_zotac = {
	"/dev/usb/hiddev0",	/* "device" */
	-1,			/* fd (device) */
	LIRC_CAN_REC_LIRCCODE,	/* features */
	0,			/* send_mode */
	LIRC_MODE_LIRCCODE,	/* rec_mode */
	32,			/* code_length */
	zotac_init,		/* init_func */
	zotac_deinit,		/* deinit_func */
	NULL,			/* send_func */
	zotac_rec,		/* rec_func */
	zotac_decode,		/* decode_func */
	NULL,			/* ioctl_func */
	NULL,			/* readdata */
	"zotac"		/* name */
};
#endif

struct hardware* hardwares[] = {&hw_zotac, (struct hardware*) NULL };

static int zotac_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		  lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp)
{
	LOGPRINTF(1, "zotac_decode");

	if (!map_code(remote, prep, codep, postp, 0, 0, main_code_length, main_code, 0, 0)) {
		return 0;
	}

	map_gap(remote, &start, &last, 0, repeat_flagp, min_remaining_gapp, max_remaining_gapp);
	/* override repeat */
	*repeat_flagp = repeat_state;

	return 1;
}

static int zotac_getcode() {

	ssize_t rd;
	struct hiddev_usage_ref uref;
	struct hiddev_report_info rinfo;
	struct hiddev_field_info finfo;
	int shift = 0;

	rd = read(fd_hidraw, &uref, sizeof(uref));
	if (rd < 0) {
		logprintf(LOG_ERR, "error reading '%s'", hw.device);
		logperror(LOG_ERR, NULL);
		zotac_deinit();
		error_state = 1;
		return -1;
	}

	if (uref.field_index == HID_FIELD_INDEX_NONE) {
		/*
		 * we get this when the new report has been send from
		 * device at this point we have the uref structure
		 * prefilled with correct report type and id
		 *
		 */

		switch (uref.report_id) {
		case 1:	/* USB standard keyboard usage page */
			{
				/* This page reports cursor keys */
				LOGPRINTF(3, "Keyboard (standard)\n");

				/* check for special codes */
				uref.field_index = 0;
				uref.usage_index = 1;
				/* fetch the usage code for given indexes */
				ioctl(fd_hidraw, HIDIOCGUCODE, &uref, sizeof(uref));
				/* fetch the value from report */
				ioctl(fd_hidraw, HIDIOCGUSAGE, &uref, sizeof(uref));

				if (uref.value)
					shift = 1;

				/* populate required field number */
				uref.field_index = 1;
				uref.usage_index = 0;
				/* fetch the usage code for given indexes */
				ioctl(fd_hidraw, HIDIOCGUCODE, &uref, sizeof(uref));
				/* fetch the value from report */
				ioctl(fd_hidraw, HIDIOCGUSAGE, &uref, sizeof(uref));
				/* now we have the key */

				LOGPRINTF(3, "usage: %x   value: %x   shift: %d\n",uref.usage_code, uref.value, shift);

				/* now we have the key */
				if (uref.value) {
					probe_code = (uref.usage_code | uref.value);
					if (shift)
						probe_code |= 0x10000000;
					LOGPRINTF(3, "Main code 1: %x\n", probe_code);
					return 1;
				}
				else {
					LOGPRINTF(3, "rel button\n");
					probe_code = release_code;
					return 2;
				}
			}
			break;

		case 2:
		case 3:	/* USB generic desktop usage page */
		case 4:
			{
				/* This page reports power key
				 * (via SystemControl SLEEP)
				 */
				LOGPRINTF(3, "Generic desktop (standard)\n");


				/* traverse report descriptor */
				rinfo.report_type = HID_REPORT_TYPE_INPUT;
				rinfo.report_id = HID_REPORT_ID_FIRST;
				rd = ioctl(fd_hidraw, HIDIOCGREPORTINFO, &rinfo);

				unsigned int i,j;
				while (rd >= 0) {
					for (i = 0; i < rinfo.num_fields; i++) {
						finfo.report_type = rinfo.report_type;
					finfo.report_id = rinfo.report_id;
					finfo.field_index = i;
					ioctl(fd_hidraw, HIDIOCGFIELDINFO, &finfo);
					for (j = 0; j < finfo.maxusage; j++) {
						uref.field_index = i;
					    uref.usage_index = j;
					    ioctl(fd_hidraw, HIDIOCGUCODE, &uref);
					    ioctl(fd_hidraw, HIDIOCGUSAGE, &uref);

					    if (uref.value != 0) {
						LOGPRINTF(3, "field: %d, idx: %d, usage: %x   value: %x\n",i, j, uref.usage_code, uref.value);
						probe_code = uref.usage_code;
						return 1;
					    }
					}
					}
					rinfo.report_id |= HID_REPORT_ID_NEXT;
					rd = ioctl(fd_hidraw, HIDIOCGREPORTINFO, &rinfo);
				}
				return 2;
			}
			break;
		default:
			/* Unknown/unsupported report id.
			 * Should not happen because remaining reports
			 * from report descriptor seem to be unused by remote.
			 */
			logprintf(LOG_ERR, "Unexpected report id %d", uref.report_id);
			break;
		}
	}
	else {
			/* This page reports power key
			 * (via SystemControl SLEEP)
			 */
			LOGPRINTF(3, "Same Event ...\n");

			/* traverse report descriptor */
			rinfo.report_type = HID_REPORT_TYPE_INPUT;
			rinfo.report_id = HID_REPORT_ID_FIRST;
			rd = ioctl(fd_hidraw, HIDIOCGREPORTINFO, &rinfo);

			unsigned int i,j;
			while (rd >= 0) {
				for (i = 0; i < rinfo.num_fields; i++) {
					finfo.report_type = rinfo.report_type;
				    finfo.report_id = rinfo.report_id;
				    finfo.field_index = i;
				    ioctl(fd_hidraw, HIDIOCGFIELDINFO, &finfo);
				    for (j = 0; j < finfo.maxusage; j++) {
					uref.field_index = i;
					uref.usage_index = j;
					ioctl(fd_hidraw, HIDIOCGUCODE, &uref);
					ioctl(fd_hidraw, HIDIOCGUSAGE, &uref);

					if (uref.value != 0) {
						LOGPRINTF(3, "usage: %x   value: %x\n",uref.usage_code, uref.value);
						//probe_code = uref.usage_code;
						return 0;
						}
					}
				}
				rinfo.report_id |= HID_REPORT_ID_NEXT;
				rd = ioctl(fd_hidraw, HIDIOCGREPORTINFO, &rinfo);
			}
			return 2;
	}
	return 0;
}

static int zotac_init()
{
	logprintf(LOG_INFO, "zotac initializing '%s'", hw.device);
	if ((fd_hidraw = open(hw.device, O_RDONLY)) < 0) {
		logprintf(LOG_ERR, "unable to open '%s'", hw.device);
		return 0;
	}
	int flags = HIDDEV_FLAG_UREF | HIDDEV_FLAG_REPORT;
	if (ioctl(fd_hidraw, HIDIOCSFLAG, &flags)) {
		return 0;
	}
	hw.fd = fd_hidraw;

	/* Create pipe so that events sent by the repeat thread will
	   trigger main thread */
	if (pipe(fd_pipe) != 0) {
		logperror(LOG_ERR, "couldn't open pipe");
		close(fd_hidraw);
		return 0;
	}
	hw.fd = fd_pipe[0];
	/* Create thread to simulate repetitions */
	if (pthread_create(&repeat_thread, NULL, zotac_repeat, NULL)) {
		logprintf(LOG_ERR, "Could not create \"repeat thread\"");
		return 0;
	}
	return 1;
}

static int zotac_deinit()
{
	pthread_cancel(repeat_thread);
	if (fd_hidraw != -1) {
		// Close device if it is open
		logprintf(LOG_INFO, "closing '%s'", hw.device);
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
	hw.fd = -1;
	return 1;
}

/**
 *	Runtime that reads device, forwards codes to main thread
 *	and simulates repetitions.
 */
static void *zotac_repeat()
{
	int repeat_count = 0;
	unsigned current_code;
	int ret;
	int sel;
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
			ret = zotac_getcode();

			if (ret < 0) {
				// Error
				logprintf(LOG_ERR, "(%s) Could not read %s", __FUNCTION__, hw.device);
				goto exit_loop;
			}
			if (ret == 1) {
				// Key code : forward it to main thread
				pressed = 1;
				repeat_count = 0;
				delay.tv_sec = 0;
				delay.tv_usec = repeat_time1_us;
				current_code = probe_code;
			} else if (ret == 2) {
				// Release code : stop repetitions
				pressed = 0;
				current_code = release_code;
			} else if (ret == 0) {
				continue;
			}
			break;
		case 0:
			repeat_count++;
			if (repeat_count >= max_repeat_count) {
				// Too many repetitions, something must have gone wrong
				logprintf(LOG_ERR,"(%s) too many repetitions", __FUNCTION__);
				goto exit_loop;
			}
			// Timeout : send current_code again to main
			//           thread to simulate repetition
			delay.tv_sec = 0;
			delay.tv_usec = repeat_time2_us;
			break;
		default:
			// Error
			logprintf(LOG_ERR, "(%s) select() failed", __FUNCTION__);
			goto exit_loop;
		}
		// Send code to main thread through pipe
		write(fd, &current_code, sizeof(current_code));
	}
exit_loop:

	// Wake up main thread with special key code
	current_code = remove_code;
	write(fd, &current_code, sizeof(current_code));
	return NULL;
}

/*
*  Aureal Technology ATWF@83 cheap remote
*  specific code.
*/

static char *zotac_rec(struct ir_remote *remotes)
{
	unsigned ev;
	int rd;
	last = end;
	gettimeofday(&start, NULL);
	rd = read(hw.fd, &ev, sizeof(ev));

	if (rd == -1) {
		// Error
		logprintf(LOG_ERR, "(%s) could not read pipe", __FUNCTION__);
		zotac_deinit();
		return 0;
	}

	if (ev == release_code) {
		// Release code
		main_code = 0;
		return 0;
	} else if (ev == remove_code) {
		// Device has been removed
		zotac_deinit();
		return 0;
	}

	LOGPRINTF(1, "zotac : %x", ev);
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
