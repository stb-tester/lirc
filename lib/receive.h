
/****************************************************************************
 ** receive.h ***************************************************************
 ****************************************************************************
 *
 * functions that decode IR codes
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

/**
 * @file receive.h
 * @author Christoph Bartelmus
 * @brief Functions that decode IR codes.
 * @ingroup driver_api
 */

#ifndef _RECEIVE_H
#define _RECEIVE_H

#include "ir_remote.h"

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * @addtogroup driver_api
 * @{
 */


/** Min value returned by receive_timeout. */
#define MIN_RECEIVE_TIMEOUT 100000

/** 
 * Set a file logging input from driver in same format as mode2(1).
 * @param f Open file to write on or NULL to disable logging.
 */
void rec_buffer_set_logfile(FILE* f);

/** Return actual timeout to use given MIN_RECEIVE_TIMEOUT limitation. */
static inline lirc_t receive_timeout(lirc_t usec)
{
	return 2 * usec < MIN_RECEIVE_TIMEOUT ? MIN_RECEIVE_TIMEOUT : 2 * usec;
}

/**
 * Wait until data is available to read, or timeout.
 *
 * @param maxusec Mac number of microseconda to wait.
 * @returns non-zero if the driver.fd is ready to read,
 *       or 0 indicating timeout
 */
int waitfordata(__u32 maxusec);

/** Clear internal buffer to pristine state. */
void rec_buffer_init();

/**
 * Flush the internal fifo and store a single code read
 * from the driver in it.
 */
int rec_buffer_clear(void);

/**
 * Decode data from remote
 *
 * @param ctx Undefined on enter. On exit, the fields in the
 *     structure are defined.
 */
int receive_decode(struct ir_remote* remote, struct decode_ctx_t* ctx);

/**
 * Reset the modules's internal fifo's read state to initial values
 * where the nothing is read. The write pointer is not affected.
 */
void rec_buffer_rewind(void);

/** Reset internal fifo's write pointer.  */
void rec_buffer_reset_wptr(void);


/** @} */
#ifdef	__cplusplus
}
#endif

#endif
