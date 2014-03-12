
/****************************************************************************
 ** hw_default.h ************************************************************
 ****************************************************************************
 *
 * routines for hardware that supports ioctl() interface
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef _HW_DEFAULT_H
#define _HW_DEFAULT_H

#include "lirc/ir_remote.h"

int default_init(void);
int default_config(struct ir_remote *remotes);
int default_deinit(void);
int default_send(struct ir_remote *remote, struct ir_ncode *code);
char *default_rec(struct ir_remote *remotes);
int default_ioctl(unsigned int cmd, void *arg);
lirc_t default_readdata(lirc_t timeout);

#endif
