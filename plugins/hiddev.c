/****************************************************************************
** hw_hiddev.c *************************************************************
****************************************************************************
*
* receive keycodes input via /dev/usb/hiddev...
*
* Copyright (C) 2002 Oliver Endriss <o.endriss@gmx.de>
* Copyright (C) 2004 Chris Pascoe <c.pascoe@itee.uq.edu.au>
* Copyright (C) 2005 William Uther <william.uther@nicta.com.au>
* Copyright (C) 2007 Brice DUBOST <ml@braice.net>
* Copyright (C) 2007 Benjamin Drung <benjamin.drung@gmail.com>
* Copyright (C) 2007 Stephen Williams <stephen.gw@gmail.com>
*
* Distribute under GPL version 2 or later.
*
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include <linux/types.h>
#include <linux/hiddev.h>

#include "lirc_driver.h"

#define TIMEOUT 20000

static int hiddev_init(void);
static int hiddev_deinit(void);
static int hiddev_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);
static char* hiddev_rec(struct ir_remote* remotes);
static int sb0540_init(void);
static char* sb0540_rec(struct ir_remote* remotes);
static char* macmini_rec(struct ir_remote* remotes);
static int samsung_init(void);
static char* samsung_rec(struct ir_remote* remotes);
static char* sonyir_rec(struct ir_remote* remotes);

const struct driver hw_dvico = {
	.name		= "dvico",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 64,
	.init_func	= hiddev_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= hiddev_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};

static const logchannel_t logchannel = LOG_DRIVER;

static int dvico_repeat_mask = 0x8000;

static int pre_code_length = 32;
static int main_code_length = 32;

static unsigned int pre_code;
static signed int main_code = 0;

static struct timeval start, end, last;

enum {
	RPT_UNKNOWN = -1,
	RPT_NO = 0,
	RPT_YES = 1,
};

static int repeat_state = RPT_UNKNOWN;

/* Remotec Mediamaster specific */
const struct driver hw_bw6130 = {
	.name		= "bw6130",
	.device		= "/dev/usb/hid/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 64,
	.init_func	= hiddev_init,
	.deinit_func	= hiddev_deinit,
	.send_func	= NULL,
	.rec_func	= hiddev_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};

const struct driver hw_asusdh = {
	.name		= "asusdh",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 64,
	.init_func	= hiddev_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= hiddev_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};

#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
/* Creative USB IR Receiver (SB0540) */
const struct driver hw_sb0540 = {
	.name		= "sb0540",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= sb0540_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= sb0540_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};
#endif

/* Apple Mac mini USB IR Receiver */
const struct driver hw_macmini = {
	.name		= "macmini",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= hiddev_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= macmini_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};

#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
/* Samsung USB IR Receiver */
const struct driver hw_samsung = {
	.name		= "samsung",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= samsung_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= samsung_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};
#endif

/* Sony IR Receiver */
const struct driver hw_sonyir = {
	.name		= "sonyir",
	.device		= "/dev/usb/hiddev0",
	.features	= LIRC_CAN_REC_LIRCCODE,
	.send_mode	= 0,
	.rec_mode	= LIRC_MODE_LIRCCODE,
	.code_length	= 32,
	.init_func	= hiddev_init,
	.deinit_func	= hiddev_deinit,
	.open_func	= default_open,
	.close_func	= default_close,
	.send_func	= NULL,
	.rec_func	= sonyir_rec,
	.decode_func	= hiddev_decode,
	.drvctl_func	= NULL,
	.readdata	= NULL,
	.api_version	= 3,
	.driver_version = "0.9.3",
	.info		= "No info available",
	.device_hint    = "/dev/usb/hiddev*",
};


static int old_main_code = 0;

static const int mousegrid[9][9] = { { 0x00, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17 },
				     { 0x05, 0x0d, 0x11, 0x12, 0x12, 0x12, 0x16, 0x17, 0x17 },
				     { 0x05, 0x09, 0x0e, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13 },
				     { 0x06, 0x0a, 0x0a, 0x0e, 0x0e, 0x12, 0x13, 0x13, 0x13 },
				     { 0x06, 0x0a, 0x0a, 0x0e, 0x0e, 0x0f, 0x13, 0x13, 0x13 },
				     { 0x06, 0x0a, 0x0a, 0x0a, 0x0f, 0x0f, 0x0f, 0x0f, 0x13 },
				     { 0x06, 0x06, 0x0b, 0x0b, 0x0b, 0x0f, 0x0f, 0x0f, 0x0f },
				     { 0x07, 0x07, 0x0b, 0x0b, 0x0b, 0x0f, 0x0f, 0x0f, 0x0f },
				     { 0x07, 0x07, 0x0b, 0x0b, 0x0b, 0x0b, 0x0f, 0x0f, 0x0f } };

int hiddev_init(void)
{
	log_info("initializing '%s'", drv.device);

	drv.fd = open(drv.device, O_RDONLY);
	if (drv.fd < 0) {
		log_error("unable to open '%s'", drv.device);
		return 0;
	}

	return 1;
}

int hiddev_deinit(void)
{
	if (drv.fd != -1) {
		log_info("closing '%s'", drv.device);
		close(drv.fd);
		drv.fd = -1;
	}
	return 1;
}

int hiddev_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	log_trace("hiddev_decode");

	if (!map_code(remote, ctx, pre_code_length, pre_code, main_code_length, main_code, 0, 0))
		return 0;

	log_trace("lirc code: 0x%X", ctx->code);

	map_gap(remote, ctx, &start, &last, 0);
	/* override repeat */
	switch (repeat_state) {
	case RPT_NO:
		ctx->repeat_flag = 0;
		break;
	case RPT_YES:
		ctx->repeat_flag = 1;
		break;
	default:
		break;
	}

	return 1;
}

char* hiddev_rec(struct ir_remote* remotes)
{
	struct hiddev_event event;
	struct hiddev_event asus_events[8];
	int rd;
	/* Remotec Mediamaster specific */
	static int wheel_count = 0;
	static int x_movement = 0;
	static struct timeval time_of_last_code;
	int y_movement = 0;
	int x_direction = 0;
	int y_direction = 0;
	int i;

	log_trace("hiddev_rec");

	last = end;
	gettimeofday(&start, NULL);
	rd = read(drv.fd, &event, sizeof(event));
	if (rd != sizeof(event)) {
		log_error("error reading '%s'", drv.device);
		log_perror_err(NULL);
		hiddev_deinit();
		return 0;
	}

	log_trace("hid 0x%X  value 0x%X", event.hid, event.value);

	pre_code = event.hid;
	main_code = event.value;

	/*
	 * This stuff is probably dvico specific.
	 * I don't have any other hid devices to test...
	 *
	 * See further for the Asus DH specific code
	 *
	 */

	if (event.hid == 0x90001) {
		/* This is the DVICO Remote. It actually sends two hid
		 * events, the first of which has 0 as the hid.value and
		 * is of no use in decoding the remote code. If we
		 * receive this type of event, read the next event
		 * (which should be immediately available) and
		 * use it to obtain the remote code.
		 */

		log_trace("This is another type Dvico - sends two codes");
		if (!waitfordata(TIMEOUT)) {
			log_error("timeout reading next event");
			return NULL;
		}
		rd = read(drv.fd, &event, sizeof(event));
		if (rd != sizeof(event)) {
			log_error("error reading '%s'", drv.device);
			return 0;
		}
		pre_code = event.hid;
		main_code = event.value;
	}
	gettimeofday(&end, NULL);

	if (event.hid == 0x10046) {
		struct timeval now;

		repeat_state = (main_code & dvico_repeat_mask) ? RPT_YES : RPT_NO;
		main_code = (main_code & ~dvico_repeat_mask);

		gettimeofday(&now, NULL);

		/* The hardware dongle for the dvico remote sends spurious */
		/* repeats of the last code received it it gets a false    */
		/* trigger from some other IR source, or if it misses      */
		/* receiving the first code of a new button press. To      */
		/* minimise the impact of this hardware bug, ignore any    */
		/* repeats that occur more than half a second after the    */
		/* previous valid code because it is likely that they are  */
		/* spurious.                                               */

		if (repeat_state == RPT_YES) {
			if (time_elapsed(&time_of_last_code, &now) > 500000)
				return NULL;
		}
		time_of_last_code = now;

		log_trace("main 0x%X  repeat state 0x%X", main_code, repeat_state);
		return decode_all(remotes);
#if 0
		/* the following code could be used to recreate the
		 * real codes of the remote control (currently
		 * verified for the MCE remote only) */
		ir_code pre, main;

		pre_code_length = 16;
		main_code_length = 16;

		pre = event.value & 0xff;
		pre_code = reverse(~pre, 8) << 8 | reverse(pre, 8);

		repeat_state = (event.value & dvico_repeat_mask) ? RPT_YES : RPT_NO;

		main = (event.value & 0x7f00) >> 8;
		main_code = reverse(main, 8) << 8 | reverse(~main, 8);
		return decode_all(remotes);
#endif
	}
	/* Asus DH remote specific code */
	else if (event.hid == 0xFF000000) {
		log_trace("This is an asus P5 DH remote, we read the other events");
		asus_events[0] = event;
		for (i = 1; i < 8; i++) {
			if (!waitfordata(TIMEOUT)) {
				log_error("timeout reading byte %d", i);
				return NULL;
			}
			rd = read(drv.fd, &asus_events[i], sizeof(event));
			if (rd != sizeof(event)) {
				log_error("error reading '%s'", drv.device);
				return 0;
			}
		}
		for (i = 0; i < 8; i++)
			log_trace("Event number %d hid 0x%X  value 0x%X", i, asus_events[i].hid,
				  asus_events[i].value);
		pre_code = asus_events[1].hid;
		main_code = asus_events[1].value;
		if (main_code)
			return decode_all(remotes);
	}

	/* Remotec Mediamaster specific code */
	/* Y-Coordinate,
	 * second event field after button code (wheel_count==2) */
	if (wheel_count == 2) {
		y_movement = event.value & 0x0000000F;
		y_direction = (event.value & 0x000000F0) >> 2;
		x_direction = (x_movement & 0x000000F0) >> 1;
		x_movement &= 0x0000000F;

		if (x_movement > 8 || y_movement > 8) {
			log_error("unexpected coordinates: %u,%u", x_movement, y_movement);
			return NULL;
		}

		main_code = mousegrid[x_movement][y_movement];
		main_code |= x_direction;
		main_code |= y_direction;
		main_code |= 0x00000080;        //just to make it unique

		wheel_count = 0;
		pre_code = 0xFFA10003;  //so it gets recognized
		return decode_all(remotes);
	}
	/* X-Coordinate,
	 * first event field after button code (wheel_count==1) */
	else if (wheel_count == 1) {
		x_movement = event.value;

		wheel_count = 2;
		return NULL;
	}

	if ((event.hid == 0xFFA10003) && (event.value != 0xFFFFFFFF) && (event.value != 0xFFFFFFAA)) {
		if (old_main_code == main_code)
			repeat_state = RPT_YES;
		old_main_code = main_code;
		if (main_code == 0x40) {        /* the mousedial has been touched */
			wheel_count = 1;
			return 0;
		}
		return decode_all(remotes);
	} else if ((event.hid == 0xFFA10003) && (event.value == 0xFFFFFFAA)) {
		repeat_state = RPT_NO;
		old_main_code = 0;
	}

	/* insert decoding logic for other hiddev remotes here */

	return 0;
}

/*
 * Creative USB IR Receiver specific code
 *
 * based on creative_rm1500_usb-0.1 from http://ecto.teftin.net/rm1500.html
 * which is written by Stan Sawa teftin(at)gmail.com
 *
 */

#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
int sb0540_init(void)
{
	int rv = hiddev_init();

	if (rv == 1) {
		/* we want to get info on each report received from device */
		int flags = HIDDEV_FLAG_UREF | HIDDEV_FLAG_REPORT;

		if (ioctl(drv.fd, HIDIOCSFLAG, &flags))
			return 0;
	}

	return rv;
}

char* sb0540_rec(struct ir_remote* remotes)
{
	/*
	 * at this point, each read from opened file/device should return
	 * hiddev_usage_ref structure
	 *
	 */

	ir_code code;
	ssize_t rd;
	struct hiddev_usage_ref uref;

	log_trace("sb0540_rec");

	pre_code_length = 16;
	main_code_length = 16;
	pre_code = 0x8322;
	repeat_state = RPT_NO;

	last = end;
	gettimeofday(&start, NULL);

	rd = read(drv.fd, &uref, sizeof(uref));
	if (rd < 0) {
		log_error("error reading '%s'", drv.device);
		log_perror_err(NULL);
		hiddev_deinit();
		return 0;
	}

	gettimeofday(&end, NULL);

	if (uref.field_index == HID_FIELD_INDEX_NONE) {
		/*
		 * we get this when the new report has been send from
		 * device at this point we have the uref structure
		 * prefilled with correct report type and id
		 *
		 */

		/* this got guessed by getting the device report
		 * descriptor (function devinfo in source) */
		uref.field_index = 0;   /* which field of report */
		/* this got guessed by taking all values from device
		 * and checking which ones are changing ;) */
		uref.usage_index = 3;   /* which usage entry of field */

		/* fetch the usage code for given indexes */
		ioctl(drv.fd, HIDIOCGUCODE, &uref, sizeof(uref));
		/* fetch the value from report */
		ioctl(drv.fd, HIDIOCGUSAGE, &uref, sizeof(uref));
		/* now we have the key */

		code = reverse(uref.value, 8);
		main_code = (code << 8) + ((~code) & 0xff);

		return decode_all(remotes);
	}
	/*
	 * we are not interested in any other events, as they are only
	 * giving info what changed in report and this not always
	 * works, is complicated, doesn't output anything sensible on
	 * repeated key and we already have all the info from real
	 * report
	 *
	 */

	return 0;
}
#endif

/*
 * Apple Mac mini USB IR Receiver specific code.
 *
 */
char* macmini_rec(struct ir_remote* remotes)
{
	static struct timeval time_of_last_code;
	struct hiddev_event ev[4];
	int rd;
	int i;

	log_trace("macmini_rec");

	last = end;
	gettimeofday(&start, NULL);
	for (i = 0; i < 4; i++) {
		if (i > 0 && !waitfordata(TIMEOUT)) {
			log_error("timeout reading byte %d", i);
			return NULL;
		}
		rd = read(drv.fd, &ev[i], sizeof(ev[i]));
		if (rd != sizeof(ev[i])) {
			log_error("error reading '%s'", drv.device);
			hiddev_deinit();
			return 0;
		}
	}
	gettimeofday(&end, NULL);

	/* Record the code */
	pre_code_length = 0;
	pre_code = 0;
	main_code = (ev[0].value << 24) + (ev[1].value << 16) + (ev[2].value << 8) + (ev[3].value << 0);
	repeat_state = RPT_UNKNOWN;
	if (main_code == 0) {
		/* some variants seem to send 0 to indicate repeats */
		if (time_elapsed(&time_of_last_code, &end) > 500000)
			/* but some send 0 if they receive codes from
			 * a different remote, so only send repeats if
			 * close to the original code */
			return NULL;
		main_code = old_main_code;
		repeat_state = RPT_YES;
	}
	old_main_code = main_code;
	time_of_last_code = end;

	return decode_all(remotes);
}

/*
 * Samsung/Cypress USB IR Receiver specific code
 * (e.g. used in Satelco EasyWatch remotes)
 *
 * Based on sb0540 code.
 * Written by r.schedel (at)yahoo.de
 *
 */

#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
int samsung_init(void)
{
	int rv = hiddev_init();

	if (rv == 1) {
		/* we want to get info on each report received from device */
		int flags = HIDDEV_FLAG_UREF | HIDDEV_FLAG_REPORT;

		if (ioctl(drv.fd, HIDIOCSFLAG, &flags))
			return 0;
	}

	return rv;
}

char* samsung_rec(struct ir_remote* remotes)
{
	/*
	 * at this point, each read from opened file/device should return
	 * hiddev_usage_ref structure
	 *
	 */

	ssize_t rd;
	struct hiddev_usage_ref uref;

	log_trace("samsung_rec");

	pre_code_length = 0;
	main_code_length = 32;
	pre_code = 0;
	repeat_state = RPT_NO;

	last = end;
	gettimeofday(&start, NULL);
	rd = read(drv.fd, &uref, sizeof(uref));
	if (rd < 0) {
		log_error("error reading '%s'", drv.device);
		log_perror_err(NULL);
		hiddev_deinit();
		return 0;
	}
	gettimeofday(&end, NULL);

	if (uref.field_index == HID_FIELD_INDEX_NONE) {
		/*
		 * we get this when the new report has been send from
		 * device at this point we have the uref structure
		 * prefilled with correct report type and id
		 *
		 */

		log_trace1(
			  "hiddev event: reptype %d, repid %d, field" " idx %d, usage idx %x, usage code %x, val %d\n",
			  uref.report_type, uref.report_id, uref.field_index, uref.usage_index, uref.usage_code,
			  uref.value);

		switch (uref.report_id) {
		case 1: /* USB standard keyboard usage page */
		{
			/* This page reports cursor keys */
			log_trace2("Keyboard (standard)\n");

			/* populate required field number */
			uref.field_index = 1;
			uref.usage_index = 0;

			/* fetch the usage code for given indexes */
			ioctl(drv.fd, HIDIOCGUCODE, &uref, sizeof(uref));
			/* fetch the value from report */
			ioctl(drv.fd, HIDIOCGUSAGE, &uref, sizeof(uref));
			/* now we have the key */

			main_code = (uref.usage_code & 0xffff0000)
				    | uref.value;

			log_trace2("Main code: %x\n", main_code);
			return decode_all(remotes);
		}
		break;

		case 3: /* USB generic desktop usage page */
		{
			/* This page reports power key
			 * (via SystemControl SLEEP)
			 */
			log_trace2("Generic desktop (standard)\n");

			/* populate required field number */
			uref.field_index = 0;
			uref.usage_index = 1;           /* or 7 */

			/* fetch the usage code for given indexes */
			ioctl(drv.fd, HIDIOCGUCODE, &uref, sizeof(uref));
			/* fetch the value from report */
			ioctl(drv.fd, HIDIOCGUSAGE, &uref, sizeof(uref));
			/* now we have the key */

			main_code = (uref.usage_code & 0xffff0000)
				    | uref.value;

			log_trace2("Main code: %x\n", main_code);
			return decode_all(remotes);
		}
		break;

		case 4: /* Samsung proprietary usage page */
		{
			/* This page reports all other keys.
			 * It is the only page with keys we cannot
			 * receive via HID input layer directly.
			 * This is why we need to implement all of
			 * this hiddev stuff here.
			 */
			int maxbit, i;

			log_trace2("Samsung usage (proprietary)\n");
			/* According to tests, at most one of the
			 * 48 key bits can be set.
			 * Due to the required kernel patch, the
			 * 48 bits are received in the report as
			 * 6 usages a 8 bit.
			 * We want to avoid using a 64 bit value
			 * if max. one bit is set anyway.
			 * Therefore, we use the (highest) set bit
			 * as final key value.
			 *
			 * Now fetch each usage and
			 * combine to single value.
			 */
			for (i = 0, maxbit = 1; i < 6; i++, maxbit += 8) {
				unsigned int tmpval = 0;

				uref.field_index = 0;
				uref.usage_index = i;

				/* fetch the usage code for given indexes */
				ioctl(drv.fd, HIDIOCGUCODE, &uref, sizeof(uref));
				/* fetch the value from report */
				ioctl(drv.fd, HIDIOCGUSAGE, &uref, sizeof(uref));
				/* now we have the key byte */
				tmpval = uref.value & 0xff;             /* 8 bit */

				if (i == 0)
					/* fetch usage code from first usage
					 * (should be 0xffcc)
					 */
					main_code = (uref.usage_code & 0xffff0000);

				/* find index of highest bit with binary search */
				if (tmpval > 0) {
					if (tmpval & 0xf0) {
						maxbit += 4;
						tmpval >>= 4;
					}
					if (tmpval & 0x0c) {
						maxbit += 2;
						tmpval >>= 2;
					}
					if (tmpval & 0x02)
						maxbit += 1;
					main_code |= maxbit;
					/* We found a/the pressed key, so break out */
					break;
				}
			}

			log_trace2("Main code: %x\n", main_code);

			/* decode combined key value */
			return decode_all(remotes);
		}
		break;

		default:
			/* Unknown/unsupported report id.
			 * Should not happen because remaining reports
			 * from report descriptor seem to be unused by remote.
			 */
			log_error("Unexpected report id %d", uref.report_id);
			break;
		}
	}
	/*
	 * we are not interested in any other events, as they are only
	 * giving info what changed in report and this not always
	 * works, is complicated, doesn't output anything sensible on
	 * repeated key and we already have all the info from real
	 * report
	 *
	 */

	return 0;
}
#endif


/*
 * Sony IR Receiver  (PCVA-IR5U)
 *
 * #define USB_VENDOR_ID_SONY               0x054c
 * #define USB_DEVICE_ID_SONY_IR_RECEIVER   0x00d4
 *
 */
char* sonyir_rec(struct ir_remote* remotes)
{
	struct hiddev_event ev;
	int rd;
	unsigned char msg[16];

	log_trace("sonyir_rec");

	// Sony IR receiver has 3 reports:
	//   0x01 - 5B
	//   0x02 - 1B
	//   0x03 - 8B
	//
	rd = read(drv.fd, msg, 16);

	// Require 6-character report, usage 0x1
	if ((rd != 6) || (msg[0] != 0x1))
		return 0;

	// Ignore release message
	if ((msg[2] & 0x80) == 0x80)
		//repeat_state = RPT_NO;
		return 0;

	// Construct event
	ev.value = (msg[3] << 16) | (msg[4] << 8) | ((msg[2] & 0x7f) << 0);

	pre_code_length = 0;
	pre_code = 0;
	main_code = ev.value;
	repeat_state = RPT_NO;

	return decode_all(remotes);
}

const struct driver* hardwares[] = { &hw_dvico,
				     &hw_bw6130,
				     &hw_asusdh,
				     &hw_macmini,
				     &hw_sonyir,
#ifdef HAVE_LINUX_HIDDEV_FLAG_UREF
				     &hw_sb0540,
				     &hw_samsung,
#endif
				     (const struct driver*)NULL };
