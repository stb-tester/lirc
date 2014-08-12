
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

struct hardware {
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
	__u32 code_length;

        /** Function called for initializing the driver and the hardware. Zero return value indicates failure, all other return values success. */
        int (*init_func) (void);

        /** Function called when terminating the driver. Zero return value indicates failure, all other return values success. */
        int (*deinit_func) (void);

        /**
         * TODO
         */
        int (*send_func) (struct ir_remote * remote, struct ir_ncode * code);

        /**
         *
         */
        char *(*rec_func) (struct ir_remote * remotes);

        /**
         * TODO
         */
        int (*decode_func) (struct ir_remote * remote, ir_code * prep, ir_code * codep, ir_code * postp,
			    int *repeat_flag, lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);

        /**
         * Depending on the particular driver and hardware, other functionality can be implemented here, with syntax and semantic to be determined by the driver.
         */
         int (*ioctl_func) (unsigned int cmd, void *arg);

        /**
         * TODO
         */
	 lirc_t(*readdata) (lirc_t timeout);

         /**
          * The name of the driver, as to be used as argument to --driver.
          */
	char *name;

        /**
         * The resolution in microseconds of the recorded durations when reading signals.
         */
	unsigned int resolution;
};

extern struct hardware hw;

#ifdef	__cplusplus
}
#endif

#endif
