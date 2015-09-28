/****************************************************************************
** hw_caraca.c ***********************************************************
****************************************************************************
*
* routines for caraca receiver
*
* Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
*       modified for caraca RC5 receiver by Konrad Riedel <k.riedel@gmx.de>
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
static int caraca_decode(struct ir_remote* remote,
			 ir_code* prep, ir_code* codep,
			 ir_code* postp, int* repeat_flagp,
			 lirc_t* min_remaining_gapp,
			 lirc_t* max_remaining_gapp);
static int caraca_init(void);
static int caraca_deinit(void);
static char* caraca_rec(struct ir_remote* remotes);



const struct driver hw_caraca = {
	.name				= "caraca"
					  .device		  = NULL,
	.features			= LIRC_CAN_REC_LIRCCODE,
	.send_mode			= 0,
	.rec_mode			= LIRC_MODE_LIRCCODE,
	.code_length			= 16,
	.init_func			= caraca_init,
	.deinit_func			= caraca_deinit,
	.open_func			= default_open,
	.close_func			= default_close,
	.send_func			= NULL,
	.rec_func			= caraca_rec,
	.decode_func			= caraca_decode
					  .drvctl_func	   = NULL,
	.readdata			= NULL,
	.api_version			= 3,
	.driver_version			= "0.9.3",
	.info				= "No info available.",
	.device_hint                    = "auto",
};

const struct driver* hardwares[] = { &hw_caraca, (const struct driver*)NULL };

static const logchannel_t logchannel = LOG_DRIVER;


int caraca_decode(struct ir_remote* remote, ir_code* prep, ir_code* codep, ir_code* postp, int* repeat_flagp,
		  lirc_t* min_remaining_gapp, lirc_t* max_remaining_gapp)
{
	if (!map_code(remote, prep, codep, postp, 0, 0, hw_caraca.code_length, code, 0, 0))
		return 0;

	gap = 0;
	if (start.tv_sec - last.tv_sec >= 2) {  /* >1 sec */
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
	log_trace("code: %llx", (__u64) *codep);
	return 1;
}

int caraca_init(void)
{
	signal_length = drv.code_length * 1000000 / 1200;
	drv.fd = caraca_open(PACKAGE);
	if (drv.fd < 0) {
		log_error("could not open lirc");
		log_perror_err("caraca_init()");
		return 0;
	}
	/*accept IR-Messages (16 : RC5 key code) for all nodes on the bus */
	if (set_filter(drv.fd, 0x400, 0x7c0, 0) <= 0) {
		log_error("could not set filter for IR-Messages");
		caraca_deinit();
		return 0;
	}
	return 1;
}

int caraca_deinit(void)
{
	close(drv.fd);
	return 1;
}

char* caraca_rec(struct ir_remote* remotes)
{
	char* m;
	int i = 0, node, ir, t;
	int repeat, mouse_event;

	last = end;
	gettimeofday(&start, NULL);
	i = read(drv.fd, msg, NUMBYTES);
	gettimeofday(&end, NULL);

	log_trace("caraca_rec: %s", msg);
	i = sscanf(msg, "%d.%d:%d", &node, &t, &ir);
	if (i != 3)
		log_warn(
			  "caraca: Cannot decode input message (!)");

	/* transmit the node address as first byte, so we have
	 * different codes for every transmitting node (for every room
	 * of the house) */

	code = (ir_code)(node << 8) + ir;

	m = decode_all(remotes);
	return m;
}
/* Build system diff from removing caraca. */

// diff --git a/configure.ac b/configure.ac
// index aee2f83..b79549f 100644
// --- a/configure.ac
// +++ b/configure.ac
// @@ -133,9 +133,6 @@ AH_TEMPLATE([HAVE_FTDI],
//  AH_TEMPLATE([HAVE_LIBALSA],
//          [Define if the ALSA library is installed])
//
// -AH_TEMPLATE([HAVE_LIBCARACA],
// -        [Define if the caraca library is installed])
// -
//  AH_TEMPLATE([HAVE_LIBIRMAN],
//          [Define if the libirman library is installed])
//
// @@ -178,11 +175,6 @@ AH_TEMPLATE([LIRC_LOCKDIR],
//  AH_TEMPLATE([SH_PATH], [Path to shell, usually /bin/sh or /usr/bin/sh])
//
//
// -AC_CHECK_LIB(caraca_client, caraca_init,[
// -  AM_CONDITIONAL([BUILD_CARACA],[true])
// -  ],[
// -  AM_CONDITIONAL([BUILD_CARACA],[false])]
// -)
//  AC_CHECK_HEADER(usb.h,[
//    AC_DEFINE(HAVE_LIBUSB)
//    AM_CONDITIONAL([BUILD_USB],[true])
// @@ -406,7 +398,6 @@ Conditionals:
//  ])
//
//  AC_REPORT_CONDITIONAL([BUILD_ALSA_SB_RC])
// -AC_REPORT_CONDITIONAL([BUILD_CARACA])
//  AC_REPORT_CONDITIONAL([BUILD_DSP])
//  AC_REPORT_CONDITIONAL([BUILD_FTDI])
//  AC_REPORT_CONDITIONAL([BUILD_HIDDEV])
// diff --git a/plugins/Makefile.am b/plugins/Makefile.am
// index d5642f9..1d21b11 100644
// --- a/plugins/Makefile.am
// +++ b/plugins/Makefile.am
// @@ -12,12 +12,6 @@ plugindir                   =  $(pkglibdir)/plugins
//  EXTRA_DIST                  = pluginlist.am make-pluginlist.sh
//  plugin_LTLIBRARIES          =
//
// -if BUILD_CARACA
// -plugin_LTLIBRARIES          += caraca.la
// -caraca_la_SOURCES           = caraca.c
// -caraca_la_LDFLAGS           = $(AM_LDFLAGS) -lcaraca_client
// -endif
// -
//  if BUILD_USB
//  plugin_LTLIBRARIES          += atilibusb.la
//  atilibusb_la_SOURCES        = atilibusb.c
