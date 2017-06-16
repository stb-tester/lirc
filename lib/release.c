/****************************************************************************
** release.c ***************************************************************
****************************************************************************
*
* Copyright (C) 2007 Christoph Bartelmus (lirc@bartelmus.de)
*
*/

/**
 * @file release.c
 * @brief Implements release.h.
 * @author Christoph Bartelmus
 */



#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#ifdef HAVE_KERNEL_LIRC_H
#include <linux/lirc.h>
#else
#include "media/lirc.h"
#endif

#include "lirc/release.h"
#include "lirc/receive.h"
#include "lirc/lirc_log.h"

static const logchannel_t logchannel = LOG_LIB;

static struct timeval release_time;
static struct ir_remote* release_remote;
static struct ir_ncode* release_ncode;
static ir_code release_code;
static int release_reps;
static lirc_t release_gap;

static struct ir_remote* release_remote2;
static struct ir_ncode* release_ncode2;
static ir_code release_code2;

static void register_input(void)
{
	struct timeval gap;

	if (release_remote == NULL)
		return;

	timerclear(&gap);
	gap.tv_usec = release_gap;

	gettimeofday(&release_time, NULL);
	timeradd(&release_time, &gap, &release_time);
}

void register_button_press(struct ir_remote* remote,
			   struct ir_ncode*  ncode,
			   ir_code           code,
			   int               reps)
{
	if (reps == 0 && release_remote != NULL) {
		release_remote2 = release_remote;
		release_ncode2 = release_ncode;
		release_code2 = release_code;
	}

	release_remote = remote;
	release_ncode = ncode;
	release_code = code;
	release_reps = reps;
	/* some additional safety margin */
	release_gap = upper_limit(remote,
				  remote->max_total_signal_length
					- remote->min_gap_length)
		      + receive_timeout(upper_limit(remote,
						    remote->min_gap_length))
		      + 10000;
	log_trace("release_gap: %lu", release_gap);
	register_input();
}

void get_release_data(const char** remote_name,
		      const char** button_name,
		      int*         reps)
{
	if (release_remote != NULL) {
		*remote_name = release_remote->name;
		*button_name = release_ncode->name;
		*reps = release_reps;
	} else {
		*remote_name = *button_name = "(NULL)";
		*reps = 0;
	}
}


void get_release_time(struct timeval* tv)
{
	*tv = release_time;
}
