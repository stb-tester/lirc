
/****************************************************************************
 ** driver.h **************************************************************
 ****************************************************************************
 *
 * driver.h - interface to the userspace drivers.
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

/**
 * @file driver.h
 */

#ifndef _HARDWARE_H
#define _HARDWARE_H

#include "include/media/lirc.h"
#include "lirc/ir_remote_types.h"

#ifdef	__cplusplus
extern "C" {
#endif

/** drvctl definitions */
#define DRV_ERR_NOT_IMPLEMENTED		1

/** Stores path in drv.device. */
int default_open(const char* path);

/** For now, a placeholder. */
int default_close(void);

int default_drvctl(int cmd, void* arg);

struct driver {
// Old-style implicit API version 1:

	/** The name of the device as text string. Set by lirc before init. */
	const char* device;

	/** Set by the driver after init(). */
	int fd;

	/** Code for the features of the present device, valid after init(). */
	__u32 features;

	/**
 	 * Possible values are: LIRC_MODE_RAW, LIRC_MODE_PULSE, LIRC_MODE_MODE2,
         * LIRC_MODE_LIRCCODE. These can be combined using bitwise or.
         */
	__u32 send_mode;

	/**
         * Possible values are: LIRC_MODE_RAW, LIRC_MODE_PULSE, LIRC_MODE_MODE2,
         * LIRC_MODE_LIRCCODE. These can be combined using bitwise or.
         */
	__u32 rec_mode;

	/** Length in bits of the code. */
	const __u32 code_length;

        /**
         *  Function called to do basic driver setup.
         *  @param device String describing what device driver should
         *      communicate with. Often (but not always) a /dev/... path.
         *  @return 0 if everything is fine, else positive error code.
         */
	int (*const open_func) (const char* device);

	/**
	 * Function called for initializing the driver and the hardware.
	 * Zero return value indicates failure, all other return values success.
	 */
	int (*const init_func) (void);

	/**
        * Function called when transmitting/receiving stops. Zero return value
 	*  indicates failure, all other return values success.
 	*/
	int (*const deinit_func) (void);

	/**
	 * Send data to the remote.
	 * @param remote The remote used to send.
	 * @param code Code(s) to send, a single code or the head of a
	 *             list of codes.
	 */
	int (*const send_func)(struct ir_remote*  remote,
			       struct ir_ncode*  code);

	/**
	 * Receive data from remote.
	 * @param The remote to read from.
	 * @return Formatted, statically allocated string with decoded
	 *         data: "remote-name code-name code repetitions"
	 */
	char* (*const rec_func)(struct ir_remote* remotes);

	/**
	 * TODO
	 */
	int (*const decode_func)(struct ir_remote* remote,
			   	 struct decode_ctx_t* ctx);

	/**
	* Generic driver control function with semantics as defined by driver.
	*
	*/
	int (*const drvctl_func)(unsigned int cmd, void* arg);

	/**
	 * Get length of next pulse/space from hardware.
	 * @param timeout Max time to wait (us).
	 * @return Length of pulse in lower 24 bits (us). PULSE_BIT
	 * is set to reflect if this is a pulse or space. 0
	 * indicates errors.
	 */
	 lirc_t (*const readdata)(lirc_t timeout);

	 /**
	  * Driver name, as listed by -H help and used as argument to i
	  * --driver.
	  */
	const char* name;

	/**
	 * The resolution in microseconds of the recorded durations when
	 * reading signals.
	 */
	unsigned int resolution;

// API version 2 addons:

	const int api_version;           /**< API version (from version 2+).*/
	const char* driver_version;      /**< Driver version (free text). */
	int (*const close_func)(void);   /**< Hard closing. */

};

extern struct driver drv;

#ifdef	__cplusplus
}
#endif

#endif
