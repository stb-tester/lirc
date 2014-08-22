
/****************************************************************************
 ** transmit.h **************************************************************
 ****************************************************************************
 *
 * functions that prepare IR codes for transmitting
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

void init_send_buffer(void);

int init_send(struct ir_remote* remote, struct ir_ncode* code);

/** @cond */
int init_sim(struct ir_remote* remote,
             struct ir_ncode* code,
             int repeat_preset);
/** @endcond */

int send_buffer_length();

const lirc_t* send_buffer_data();

lirc_t send_buffer_sum();
#ifdef	__cplusplus
}
#endif

#endif
