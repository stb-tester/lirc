
/****************************************************************************
 ** transmit.h **************************************************************
 ****************************************************************************
 *
 * functions that prepare IR codes for transmitting.
 *
 * Operations in this module applies to the transmit buffer. The buffer
 * is initiated using init_send_buffer(), filled with data using init_send()
 * and accessed using  send_buffer_data() and send_buffer_length().
 *
 * A prepared buffer contains an even number of unsigned ints, each of
 * which representing a pulse width in microseconds. The first item represents
 * a space and the last thus a pulse.
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef _TRANSMIT_H
#define _TRANSMIT_H

#include "ir_remote.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define WBUF_SIZE 256

/** Clear and re-initiate the buffer. */
void init_send_buffer(void);

/**
 * Prepare the buffer.
 * @param remote Parsed lircd.conf data.
 * @param code Data item to be represented in buff.
 * @return 0 on errors, else 1
 */
int init_send(struct ir_remote* remote, struct ir_ncode* code);

/** @cond */
int init_sim(struct ir_remote* remote,
             struct ir_ncode* code,
             int repeat_preset);
/** @endcond */

/** @return Number of items accessible in array send_buffer_data(). */
int send_buffer_length();

/** @return Pointer to timing data in microseconds for pulses/spaces. */
const lirc_t* send_buffer_data();

/** @return Total length of buffer in microseconds. */
lirc_t send_buffer_sum();

#ifdef	__cplusplus
}
#endif

#endif
