
/****************************************************************************
 ** hw_irman.h **************************************************************
 ****************************************************************************
 *
 * routines for Irman
 *
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *
 */

#ifndef _HW_IRMAN_H
#define _HW_IRMAN_H

int irman_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		 lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
int irman_init(void);
int irman_deinit(void);
char *irman_rec(struct ir_remote *remotes);

#endif
