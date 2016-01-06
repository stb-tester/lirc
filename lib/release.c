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
#include "include/media/lirc.h"
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
static const char* release_suffix = LIRC_RELEASE_SUFFIX;
static char message[PACKET_SIZE + 1];

void register_input(void)
{
	struct timeval gap;

	if (release_remote == NULL)
		return;

	timerclear(&gap);
	gap.tv_usec = release_gap;

	gettimeofday(&release_time, NULL);
	timeradd(&release_time, &gap, &release_time);
}

void register_button_press(struct ir_remote* remote, struct ir_ncode* ncode, ir_code code, int reps)
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
				  remote->max_total_signal_length - remote->min_gap_length)
		      + receive_timeout(upper_limit(remote, remote->min_gap_length)) + 10000;

	log_trace("release_gap: %lu", release_gap);

	register_input();
}

void get_release_data(const char** remote_name, const char** button_name, int* reps)
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

void set_release_suffix(const char* s)
{
	release_suffix = s;
}

void get_release_time(struct timeval* tv)
{
	*tv = release_time;
}

const char* check_release_event(const char** remote_name, const char** button_name)
{
	int len = 0;

	if (release_remote2 != NULL) {
		*remote_name = release_remote2->name;
		*button_name = release_ncode2->name;
		len = write_message(message,
				    PACKET_SIZE + 1,
				    release_remote2->name,
				    release_ncode2->name,
				    release_suffix,
				    release_code2,
				    0);
		release_remote2 = NULL;
		release_ncode2 = NULL;
		release_code2 = 0;

		if (len >= PACKET_SIZE + 1) {
			log_error("message buffer overflow");
			return NULL;
		}

		log_trace2("check");
		return message;
	}
	return NULL;
}

const char* trigger_release_event(const char** remote_name, const char** button_name)
{
	int len = 0;

	if (release_remote != NULL) {
		release_remote->release_detected = 1;
		*remote_name = release_remote->name;
		*button_name = release_ncode->name;
		len = write_message(message,
				    PACKET_SIZE + 1,
				    release_remote->name,
				    release_ncode->name,
				    release_suffix,
				    release_code,
				    0);
		timerclear(&release_time);
		release_remote = NULL;
		release_ncode = NULL;
		release_code = 0;

		if (len >= PACKET_SIZE + 1) {
			log_error("message buffer overflow");
			return NULL;
		}
		log_trace2("trigger");
		return message;
	}
	return NULL;
}

const char* release_map_remotes(struct ir_remote* old, struct ir_remote* new, const char** remote_name,
				const char** button_name)
{
	struct ir_remote* remote;
	struct ir_ncode* ncode = NULL;

	if (release_remote2 != NULL) {
		/* should not happen */
		log_error("release_remote2 still in use");
		release_remote2 = NULL;
	}
	if (release_remote && is_in_remotes(old, release_remote)) {
		remote = get_ir_remote(new, release_remote->name);
		if (remote)
			ncode = get_code_by_name(remote, release_ncode->name);
		if (remote && ncode) {
			release_remote = remote;
			release_ncode = ncode;
		} else {
			return trigger_release_event(remote_name, button_name);
		}
	}
	return NULL;
}
