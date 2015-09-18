/*
 * hw_livedrive.c - lirc routines for a Creative Labs LiveDrive.
 *
 *     Copyright (C) 2003 Stephen Beahm <stephenbeahm@adelphia.net>
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301 USA.
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

#include "lirc_driver.h"

#include "livedrive_common.h"

static const logchannel_t logchannel = LOG_DRIVER;

struct timeval start, end, last;
ir_code pre, code;

int livedrive_init(void)
{
	drv.fd = open(drv.device, O_RDONLY, 0);
	if (drv.fd < 0) {
		log_error("could not open %s", drv.device);
		return 0;
	}

	return 1;
}

int livedrive_deinit(void)
{
	close(drv.fd);
	return 1;
}

int livedrive_decode(struct ir_remote* remote, struct decode_ctx_t* ctx)
{
	lirc_t gap;

	if (!map_code(remote, ctx, 16, pre, 16, code, 0, 0))
		return 0;

	gap = 0;
	if (start.tv_sec - last.tv_sec >= 2) {
		ctx->repeat_flag = 0;
	} else {
		gap = time_elapsed(&last, &start);

		if (gap < 300000)
			ctx->repeat_flag = 1;
		else
			ctx->repeat_flag = 0;
	}

	log_trace("repeat_flag: %d", ctx->repeat_flag);
	log_trace("gap: %lu", (__u32)gap);

	return 1;
}
