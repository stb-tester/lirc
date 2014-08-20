
/****************************************************************************
 ** hardware.h **************************************************************
 ****************************************************************************
 *
 * hardware.h - internal hardware interface
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

/**
 * @file hardware.h
 */

#ifndef _HARDWARE_H
#define _HARDWARE_H

#include "include/media/lirc.h"
#include "lirc/ir_remote_types.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct driver {
	// Old-style implicit API version 1:

	/** The name of the device as text string. */
	char *device;

	/** Internally used by lirc_lib, no initialization needed. */
	int fd;

	/** Code for the features of the present device. */
	__u32 features;

	/** Possible values are: LIRC_MODE_RAW, LIRC_MODE_PULSE, LIRC_MODE_MODE2, LIRC_MODE_LIRCCODE. These can be combined using bitwise or.*/
	__u32 send_mode;

	/** Possible values are: LIRC_MODE_RAW, LIRC_MODE_PULSE, LIRC_MODE_MODE2, LIRC_MODE_LIRCCODE. These can be combined using bitwise or. */
	__u32 rec_mode;

	/** Length in bits of the code. */
	const __u32 code_length;

	/** Function called for initializing the driver and the hardware. Zero return value indicates failure, all other return values success. */
	int (*const init_func) (void);

	/** Function called when terminating the driver. Zero return value indicates failure, all other return values success. */
	int (*const deinit_func) (void);

	/**
	 * TODO
	 */
	int (*const send_func) (struct ir_remote * remote,
				struct ir_ncode * code);

	/**
	 *
	 */
	char *(*const rec_func) (struct ir_remote * remotes);

	/**
	 * TODO
	 */
	int (*const decode_func)(struct ir_remote * remote,
				 ir_code * prep, ir_code * codep,
				 ir_code * postp,
				 int *repeat_flag, lirc_t * min_remaining_gapp,
				 lirc_t * max_remaining_gapp);

	/**
	* Generic driver control function with semantics as defined by driver.
	* Will eventually be removed, don't use in new code.
	*
	* @deprecated
	*/
	int (*const ioctl_func) (unsigned int cmd, void *arg);

	/**
	 * TODO
	 */
	 lirc_t(*const readdata) (lirc_t timeout);

	 /**
	  * Driver name, as listed by -H help and used as argument to --driver.
	  */
	const char *name;

	/**
	 * The resolution in microseconds of the recorded durations when reading signals.
	 */
	unsigned int resolution;

	// API version 2 addons:

	const int api_version;           /**< API version available from version 2+. */
	const char* driver_version;      /**< Driver version (free text). */
	int (*const close_func)(void);   /**< Hard closing. */

};

extern struct driver drv;

#ifdef	__cplusplus
}
#endif

#endif
