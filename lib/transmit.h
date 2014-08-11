
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

/**
 * Struct for the global sending buffer.
 */
struct sbuf {
	lirc_t *data;

	lirc_t _data[WBUF_SIZE]; /**< Actual sending data. */
	int wptr;
	int too_long;
	int is_biphase;
	lirc_t pendingp;
	lirc_t pendings;
	lirc_t sum;
};

void init_send_buffer(void);
int init_send(struct ir_remote *remote, struct ir_ncode *code);
/** @cond */
int init_sim(struct ir_remote *remote, struct ir_ncode *code, int repeat_preset);
/** @endcond */

extern struct sbuf send_buffer;

#ifdef	__cplusplus
}
#endif

#endif
